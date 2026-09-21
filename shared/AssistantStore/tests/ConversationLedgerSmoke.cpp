#include "creation/assistant/ConversationLedger.h"

#include "Sha256.h"

#include <iostream>
#include <chrono>
#include <map>
#include <thread>

using namespace creation::assistant;

namespace
{
int failures = 0;

void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "PASS  " : "FAIL  ") << what << "\n";
    if (! ok)
        ++failures;
}

// A place to keep files for the tests: a map, like a project's storage.
class MemoryFiles : public AssistantFiles
{
public:
    std::map<std::string, std::string> entries;

    bool exists(const std::string& path) override { return entries.count(path) != 0; }
    bool read(const std::string& path, std::string& text) override
    {
        auto it = entries.find(path);
        if (it == entries.end())
            return false;
        text = it->second;
        return true;
    }
    bool write(const std::string& path, const std::string& text) override { entries[path] = text; return true; }
    bool remove(const std::string& path) override { return entries.erase(path) != 0; }
    std::vector<std::string> list(const std::string& prefix) override
    {
        std::vector<std::string> found;
        for (const auto& [path, text] : entries)
            if (path.compare(0, prefix.size(), prefix) == 0)
                found.push_back(path);
        return found;
    }
};

Conversation sample()
{
    auto c = ConversationLedger::create();
    ConversationLedger::append(c, "user", "How is the tempo on track 2?");
    ConversationLedger::append(c, "assistant", "It is about 120 BPM.\nSteady.");
    ConversationLedger::append(c, "user", "Thanks");
    return c;
}

bool verifies(const Conversation& c)
{
    std::string error;
    return ConversationLedger::verify(c, error);
}

std::string whyNot(const Conversation& c)
{
    std::string error;
    ConversationLedger::verify(c, error);
    return error;
}
}

int main()
{
    // ---- SHA-256 against known answers (computed independently) ----
    check(detail::sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA-256 of nothing");
    check(detail::sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 of abc");
    check(detail::sha256Hex(std::string(1000, 'a')) == "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3", "SHA-256 of a 1000 byte message (several blocks)");

    // ---- The hash recipe, against a value worked out independently ----
    {
        Conversation c;
        c.id = "11111111-2222-4333-8444-555555555555";
        c.createdAt = "2026-01-02T03:04:05.000Z";
        ConversationBlock b0;
        b0.index = 0; b0.timestamp = "2026-01-02T03:04:06.000Z"; b0.role = "user";
        b0.content = "H\xC3\xA9llo\n\"w\xC3\xB6rld\" \xF0\x9F\x8E\xB8";   // Héllo, quotes, ö and a guitar emoji
        b0.previousHash = std::string(64, '0');
        check(ConversationLedger::hashOf(c, b0) == "3b314a6aac30fb1fb749355b1dd7f19949eca834ef68e7975d40bd177698a54b", "a block's hash matches the recipe, with accents and an emoji");
        b0.hash = ConversationLedger::hashOf(c, b0);
        ConversationBlock b1;
        b1.index = 1; b1.timestamp = "2026-01-02T03:04:07.000Z"; b1.role = "assistant"; b1.content = "Hi."; b1.previousHash = b0.hash;
        check(ConversationLedger::hashOf(c, b1) == "30802426c23c29ec36b9d26bd1ac1545f2c42b3fac2547d27717b5f679119496", "the next block commits to the one before");
    }

    // ---- Titles ----
    check(ConversationLedger::makeTitle("  hello \n\t  world  ") == "hello world", "title: whitespace collapsed and trimmed");
    check(ConversationLedger::makeTitle("") == "New conversation", "title: empty");
    check(ConversationLedger::makeTitle(std::string(60, 'x')) == std::string(60, 'x'), "title: exactly 60 is kept");
    check(ConversationLedger::makeTitle(std::string(61, 'x')) == std::string(57, 'x') + "...", "title: 61 is cut to 57 plus ...");
    {
        std::string accents;
        for (int i = 0; i < 70; ++i)
            accents += "\xC3\xA9";   // 70 e-acute, 2 bytes each
        const auto title = ConversationLedger::makeTitle(accents);
        check(title.size() == 57 * 2 + 3, "title: cut on a character, never in the middle of one");
    }

    // ---- A good chain verifies; edits do not ----
    auto good = sample();
    check(verifies(good), "a conversation built with append verifies");
    check(good.title == "How is the tempo on track 2?", "the title comes from the first user message");
    check(good.blocks[0].previousHash == std::string(64, '0'), "the first block starts from 64 zeroes");
    check(good.blocks[1].previousHash == good.blocks[0].hash, "each block links to the one before");

    { auto c = good; c.blocks[1].content += " (edited)"; check(! verifies(c), "editing a block's content is detected: " + whyNot(c)); }
    { auto c = good; c.blocks[1].role = "user"; check(! verifies(c), "changing a block's role is detected"); }
    { auto c = good; c.blocks[2].timestamp = "2030-01-01T00:00:00.000Z"; check(! verifies(c), "changing a block's time is detected"); }
    { auto c = good; c.blocks.erase(c.blocks.begin() + 1); check(! verifies(c), "deleting a block is detected: " + whyNot(c)); }
    { auto c = good; std::swap(c.blocks[1], c.blocks[2]); check(! verifies(c), "reordering blocks is detected"); }
    { auto c = good; c.blocks.pop_back(); c.updatedAt = c.blocks.back().timestamp; check(verifies(c), "dropping the last block still leaves a valid, shorter chain (tamper-evident, not immutable)"); }
    { auto c = good; c.id = "99999999-2222-4333-8444-555555555555"; check(! verifies(c), "a different identifier is detected"); }
    { auto c = good; c.title = "Something else"; check(! verifies(c), "a title that does not match the content is detected"); }
    { auto c = good; c.updatedAt = "2031-01-01T00:00:00.000Z"; check(! verifies(c), "a wrong updatedAt is detected"); }
    check(! ConversationLedger::append(good, "system", "no"), "only user and assistant turns are stored");

    // ---- The file format: written, read back, and the declared values are checked ----
    {
        const auto json = ConversationLedger::toJson(good);
        Conversation back;
        std::string error;
        check(ConversationLedger::fromJson(json, back, error), "the file reads back: " + error);
        check(verifies(back), "and verifies: " + whyNot(back));
        check(back.blocks.size() == good.blocks.size() && back.blocks[1].content == "It is about 120 BPM.\nSteady.", "with the same content");
        check(back.declaredBlockCount == 3, "the declared block count is read");

        auto wrongCount = back; wrongCount.declaredBlockCount = 2;
        check(! verifies(wrongCount), "a wrong declared block count is detected");
        auto wrongHead = back; wrongHead.declaredHeadHash = std::string(64, 'a');
        check(! verifies(wrongHead), "a wrong head hash is detected");
        auto wrongAlgorithm = back; wrongAlgorithm.algorithm = "MD5";
        check(! verifies(wrongAlgorithm), "another integrity algorithm is refused");
        auto wrongSchema = back; wrongSchema.schemaVersion = 2;
        check(! verifies(wrongSchema), "an unknown schema version is refused");

        Conversation bad;
        check(! ConversationLedger::fromJson("{ not json", bad, error), "broken JSON is refused");
        check(! ConversationLedger::fromJson("[]", bad, error), "JSON that is not a conversation is refused");
        check(! ConversationLedger::fromJson("{\"schema\":\"x\"}", bad, error), "a file missing its fields is refused");

        // Text that needs escaping in the file, and text a JSON writer might escape differently.
        auto tricky = ConversationLedger::create();
        ConversationLedger::append(tricky, "user", std::string("quote \" backslash \\ tab \t control \x01 unicode \xC3\xA9 emoji \xF0\x9F\x8E\xB8 slash /"));
        Conversation trickyBack;
        check(ConversationLedger::fromJson(ConversationLedger::toJson(tricky), trickyBack, error) && verifies(trickyBack)
                  && trickyBack.blocks[0].content == tricky.blocks[0].content,
              "awkward characters survive the file exactly: " + error);

        // The same content, escaped the way another JSON writer would (\u escapes and a surrogate pair), reads the same.
        std::string other = ConversationLedger::toJson(tricky);
        const auto pos = other.find("\xF0\x9F\x8E\xB8");
        other.replace(pos, 4, "\\ud83c\\udfb8");
        Conversation otherBack;
        check(ConversationLedger::fromJson(other, otherBack, error) && verifies(otherBack), "\\u escapes and surrogate pairs read as the same text: " + error);
    }

    // ---- The store ----
    {
        MemoryFiles files;
        ConversationStore store(files, AssistantArea { "Station" });

        auto a = sample();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));   // so the second is clearly newer
        auto b = ConversationLedger::create();
        ConversationLedger::append(b, "user", "A second conversation");
        std::string error;
        check(store.save(a, error) && store.save(b, error), "conversations save: " + error);
        check(files.exists("Assistants/Station/conversations/" + a.id + ".json"), "under the assistant's own area, one file per conversation");

        const auto listed = store.list();
        check(listed.size() == 2 && listed[0].id == b.id, "the list has both, newest first");
        check(listed.size() == 2 && listed[0].intact && listed[0].title == "A second conversation" && listed[1].blockCount == 3, "with title, count and integrity");

        Conversation loaded;
        check(store.load(a.id, loaded, error) && loaded.blocks.size() == 3, "a conversation loads and verifies");

        auto more = loaded;
        ConversationLedger::append(more, "assistant", "One more");
        check(store.save(more, error) && store.load(a.id, loaded, error) && loaded.blocks.size() == 4, "a conversation keeps growing");

        // A file edited on disk.
        auto& text = files.entries["Assistants/Station/conversations/" + b.id + ".json"];
        const auto at = text.find("A second conversation", text.find("\"content\""));
        text.replace(at, 1, "B");
        check(! store.load(b.id, loaded, error), "an edited file is refused: " + error);
        const auto afterEdit = store.list();
        bool sawAltered = false;
        for (const auto& s : afterEdit)
            if (s.id == b.id && ! s.intact && s.title.find("[ALTERED]") == 0)
                sawAltered = true;
        check(sawAltered && afterEdit.back().id == b.id, "it is listed as [ALTERED], last, so it can still be removed");

        // Archive, restore, delete.
        check(store.archive(a.id, error), "archive: " + error);
        check(! files.exists("Assistants/Station/conversations/" + a.id + ".json") && files.exists("Assistants/Station/archive/" + a.id + ".json"), "the file moves to the archive");
        check(store.listArchived().size() == 1 && store.listArchived()[0].id == a.id && store.listArchived()[0].archived, "the archive lists it");
        check(store.loadArchived(a.id, loaded, error) && loaded.blocks.size() == 4, "and it still verifies there");
        check(store.unarchive(a.id, error) && files.exists("Assistants/Station/conversations/" + a.id + ".json"), "restore: " + error);
        check(! store.archive(b.id, error), "a conversation that fails verification is not archived");
        check(store.remove(b.id, error) && ! files.exists("Assistants/Station/conversations/" + b.id + ".json"), "an altered conversation can be deleted");
        check(store.archive(a.id, error) && store.remove(a.id, error) && store.listArchived().empty(), "an archived conversation can be deleted");
        check(! store.remove(a.id, error), "deleting what is not there says so");
        check(! store.load("../secrets", loaded, error), "an identifier that is a path is refused");
        check(! store.remove("a/b", error), "and so is one on delete");
    }

    // ---- Export ----
    {
        const auto markdown = ConversationLedger::toMarkdown(good);
        check(markdown.find("# How is the tempo on track 2?") == 0 && markdown.find("**You**") != std::string::npos
                  && markdown.find("**Assistant**") != std::string::npos && markdown.find("It is about 120 BPM.") != std::string::npos,
              "export gives a readable document with the title and each turn");
    }

    std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: " + std::to_string(failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
