#pragma once

#include "AssistantFiles.h"

#include <string>
#include <utility>
#include <vector>

// The conversation ledger: each AI conversation is a chain of message blocks, every block committing to the one
// before it, so an edit, insertion, deletion or reordering is detected when the conversation is loaded. It is
// tamper-evident, not immutable: no proof of work, no network, no consensus. The format and the hash recipe are the
// suite's shared ones (CONVERSATION_LEDGER.md in the research IDE), so every application's conversations can be verified
// the same way. Only the visible dialogue is stored (user and assistant turns): not system prompts, secrets or
// retrieval context.
namespace creation::assistant
{
struct ConversationBlock
{
    int index = 0;
    std::string timestamp;      // ISO-8601
    std::string role;           // "user" or "assistant"
    std::string content;
    std::string previousHash;   // 64 zeroes for the first block
    std::string hash;           // lowercase SHA-256 hex
};

struct Conversation
{
    std::string id;
    std::string title;          // derived from the first user message
    std::string createdAt;
    std::string updatedAt;      // the last block's timestamp (or createdAt when there are none)
    std::vector<ConversationBlock> blocks;

    // What a file declared about itself, kept so verify() can check it. A conversation built in memory declares nothing
    // (declaredBlockCount stays -1) and these checks are skipped; toJson() always writes the true values.
    std::string schema = "djehuti-conversation-chain";
    int schemaVersion = 1;
    std::string algorithm = "SHA-256";
    int declaredBlockCount = -1;
    std::string declaredHeadHash;
};

struct ConversationSummary
{
    std::string id;
    std::string title;
    std::string updatedAt;
    int blockCount = 0;
    bool intact = false;        // false: unreadable or failed verification (shown as [ALTERED], never fed to the model)
    bool archived = false;
};

class ConversationLedger
{
public:
    // A new empty conversation with a fresh identifier and creation time.
    static Conversation create();

    // Adds a block. Only "user" and "assistant" are stored; anything else is refused.
    static bool append(Conversation& conversation, const std::string& role, const std::string& content);

    // Every rule: schema, algorithm, declared count, contiguous indexes, roles and timestamps, the first block's previous
    // hash, each link, each hash recomputed, the head hash, the title and updatedAt derived correctly. On failure `error`
    // says which rule and where.
    static bool verify(const Conversation& conversation, std::string& error);

    static std::string toJson(const Conversation& conversation);
    // Reads the file format. Does not verify; call verify() on what it returns.
    static bool fromJson(const std::string& json, Conversation& conversation, std::string& error);

    // The title the design derives from the first user message (whitespace collapsed, at most 60 characters).
    static std::string makeTitle(const std::string& firstUserMessage);

    static std::string hashOf(const Conversation& conversation, const ConversationBlock& block);

    // The conversation as readable Markdown, for exporting (title, date, then each turn).
    static std::string toMarkdown(const Conversation& conversation);

    static std::string nowIso8601();
};

// One application's conversations, saved in its assistant area of the project.
class ConversationStore
{
public:
    ConversationStore(AssistantFiles& files, AssistantArea area) : files(files), area(std::move(area)) {}

    std::vector<ConversationSummary> list();          // active conversations, newest first
    std::vector<ConversationSummary> listArchived();  // archived ones, newest first

    // Loads and verifies. A conversation that fails verification is never returned.
    bool load(const std::string& id, Conversation& conversation, std::string& error);
    // Loads from the archive, verified the same way.
    bool loadArchived(const std::string& id, Conversation& conversation, std::string& error);
    // Verifies, then writes the whole file in one step.
    bool save(const Conversation& conversation, std::string& error);
    // Moves the unchanged file to the archive (verified first), and back again.
    bool archive(const std::string& id, std::string& error);
    bool unarchive(const std::string& id, std::string& error);
    // Removes a conversation for good, active or archived.
    bool remove(const std::string& id, std::string& error);

    const AssistantArea& assistantArea() const noexcept { return area; }

private:
    std::vector<ConversationSummary> listIn(const std::string& folder, bool archived);
    bool loadFrom(const std::string& folder, const std::string& id, Conversation& conversation, std::string& error);
    bool move(const std::string& from, const std::string& to, const std::string& id, std::string& error);

    AssistantFiles& files;
    AssistantArea area;
};
}
