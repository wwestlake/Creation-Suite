#include "creation/assistant/ConversationLedger.h"

#include "MiniJson.h"
#include "Sha256.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <random>

namespace creation::assistant
{
namespace
{
constexpr const char* schemaName = "djehuti-conversation-chain";
constexpr int schemaVersion = 1;
constexpr const char* defaultTitle = "New conversation";

std::string zeroHash() { return std::string(64, '0'); }

void appendHashField(std::string& out, const std::string& field)
{
    out += std::to_string(field.size());   // the UTF-8 byte count, since strings here are UTF-8
    out.push_back(':');
    out += field;
}

bool isSpaceByte(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

// The number of bytes in the UTF-8 sequence that starts with `lead`.
size_t utf8Length(unsigned char lead)
{
    if (lead < 0x80) return 1;
    if ((lead >> 5) == 0x6) return 2;
    if ((lead >> 4) == 0xE) return 3;
    if ((lead >> 3) == 0x1E) return 4;
    return 1;
}

std::string newId()
{
    static thread_local std::mt19937_64 random { std::random_device {}() };
    unsigned char bytes[16];
    for (int i = 0; i < 2; ++i)
    {
        const auto word = random();
        for (int j = 0; j < 8; ++j)
            bytes[i * 8 + j] = (unsigned char) ((word >> (j * 8)) & 0xff);
    }
    bytes[6] = (unsigned char) ((bytes[6] & 0x0f) | 0x40);   // version 4
    bytes[8] = (unsigned char) ((bytes[8] & 0x3f) | 0x80);   // variant
    char text[40];
    std::snprintf(text, sizeof(text), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                  bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7], bytes[8], bytes[9],
                  bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
    return text;
}
}

std::string ConversationLedger::nowIso8601()
{
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    std::tm parts {};
#ifdef _WIN32
    gmtime_s(&parts, &seconds);
#else
    gmtime_r(&seconds, &parts);
#endif
    char text[40];
    std::snprintf(text, sizeof(text), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday,
                  parts.tm_hour, parts.tm_min, parts.tm_sec, (int) millis);
    return text;
}

std::string ConversationLedger::makeTitle(const std::string& content)
{
    // Line breaks and tabs become spaces, the ends are trimmed, and runs of spaces collapse to one.
    std::string spaced;
    for (char c : content)
        spaced.push_back(c == '\r' || c == '\n' || c == '\t' ? ' ' : c);

    std::string collapsed;
    size_t start = 0, end = spaced.size();
    while (start < end && isSpaceByte(spaced[start])) ++start;
    while (end > start && isSpaceByte(spaced[end - 1])) --end;
    for (size_t i = start; i < end; ++i)
        if (! (spaced[i] == ' ' && ! collapsed.empty() && collapsed.back() == ' '))
            collapsed.push_back(spaced[i]);

    // At most 60 characters (not bytes); longer keeps the first 57, trimmed, and adds "...".
    size_t characters = 0;
    for (size_t i = 0; i < collapsed.size(); i += utf8Length((unsigned char) collapsed[i]))
        ++characters;
    if (characters > 60)
    {
        size_t bytes = 0, seen = 0;
        while (bytes < collapsed.size() && seen < 57)
        {
            bytes += utf8Length((unsigned char) collapsed[bytes]);
            ++seen;
        }
        std::string head = collapsed.substr(0, bytes);
        while (! head.empty() && isSpaceByte(head.back()))
            head.pop_back();
        collapsed = head + "...";
    }
    return collapsed.empty() ? std::string(defaultTitle) : collapsed;
}

std::string ConversationLedger::hashOf(const Conversation& conversation, const ConversationBlock& block)
{
    std::string canonical;
    appendHashField(canonical, schemaName);
    appendHashField(canonical, std::to_string(schemaVersion));
    appendHashField(canonical, conversation.id);
    appendHashField(canonical, conversation.createdAt);
    appendHashField(canonical, std::to_string(block.index));
    appendHashField(canonical, block.timestamp);
    appendHashField(canonical, block.role);
    appendHashField(canonical, block.content);
    appendHashField(canonical, block.previousHash);
    return detail::sha256Hex(canonical);
}

Conversation ConversationLedger::create()
{
    Conversation conversation;
    conversation.id = newId();
    conversation.title = defaultTitle;
    conversation.createdAt = nowIso8601();
    conversation.updatedAt = conversation.createdAt;
    return conversation;
}

bool ConversationLedger::append(Conversation& conversation, const std::string& role, const std::string& content)
{
    if (role != "user" && role != "assistant")
        return false;

    ConversationBlock block;
    block.index = (int) conversation.blocks.size();
    block.timestamp = nowIso8601();
    block.role = role;
    block.content = content;
    block.previousHash = conversation.blocks.empty() ? zeroHash() : conversation.blocks.back().hash;
    block.hash = hashOf(conversation, block);

    if (role == "user" && conversation.title == defaultTitle)
    {
        bool firstUser = true;
        for (const auto& existing : conversation.blocks)
            if (existing.role == "user")
                firstUser = false;
        if (firstUser)
            conversation.title = makeTitle(content);
    }

    conversation.updatedAt = block.timestamp;
    conversation.blocks.push_back(std::move(block));
    conversation.declaredBlockCount = -1;   // built in memory: nothing declared to check
    conversation.declaredHeadHash.clear();
    return true;
}

bool ConversationLedger::verify(const Conversation& conversation, std::string& error)
{
    if (conversation.id.empty())
    {
        error = "The conversation has no identifier.";
        return false;
    }
    if (conversation.schema != schemaName || conversation.schemaVersion != schemaVersion)
    {
        error = "The conversation uses a format this version does not support.";
        return false;
    }
    if (conversation.algorithm != "SHA-256")
    {
        error = "The conversation declares an integrity algorithm other than SHA-256.";
        return false;
    }
    if (conversation.declaredBlockCount >= 0 && conversation.declaredBlockCount != (int) conversation.blocks.size())
    {
        error = "The declared block count does not match the blocks in the file.";
        return false;
    }

    std::string expectedPrevious = zeroHash();
    std::string expectedTitle = defaultTitle;
    for (size_t i = 0; i < conversation.blocks.size(); ++i)
    {
        const auto& block = conversation.blocks[i];
        if (block.index != (int) i)
        {
            error = "Block " + std::to_string(block.index) + " is out of sequence.";
            return false;
        }
        if ((block.role != "user" && block.role != "assistant") || block.timestamp.empty())
        {
            error = "Block " + std::to_string(block.index) + " has invalid data.";
            return false;
        }
        if (block.previousHash != expectedPrevious)
        {
            error = "The chain is broken at block " + std::to_string(block.index) + ".";
            return false;
        }
        if (block.hash != hashOf(conversation, block))
        {
            error = "Block " + std::to_string(block.index) + " has been altered.";
            return false;
        }
        expectedPrevious = block.hash;
        if (expectedTitle == defaultTitle && block.role == "user")
            expectedTitle = makeTitle(block.content);
    }

    if (! conversation.declaredHeadHash.empty() && conversation.declaredHeadHash != (conversation.blocks.empty() ? zeroHash() : conversation.blocks.back().hash))
    {
        error = "The declared head hash does not match the last block.";
        return false;
    }
    if (conversation.title != expectedTitle)
    {
        error = "The title does not match the chained content.";
        return false;
    }
    const auto expectedUpdated = conversation.blocks.empty() ? conversation.createdAt : conversation.blocks.back().timestamp;
    if (conversation.createdAt.empty() || conversation.updatedAt != expectedUpdated)
    {
        error = "The timestamps do not match the chained content.";
        return false;
    }
    return true;
}

std::string ConversationLedger::toJson(const Conversation& conversation)
{
    using detail::quoteJson;
    std::string json = "{\n";
    json += "  \"schema\": " + quoteJson(schemaName) + ",\n";
    json += "  \"schemaVersion\": " + std::to_string(schemaVersion) + ",\n";
    json += "  \"id\": " + quoteJson(conversation.id) + ",\n";
    json += "  \"title\": " + quoteJson(conversation.title) + ",\n";
    json += "  \"createdAt\": " + quoteJson(conversation.createdAt) + ",\n";
    json += "  \"updatedAt\": " + quoteJson(conversation.updatedAt) + ",\n";
    json += "  \"blocks\": [";
    for (size_t i = 0; i < conversation.blocks.size(); ++i)
    {
        const auto& b = conversation.blocks[i];
        json += i == 0 ? "\n" : ",\n";
        json += "    {\n";
        json += "      \"index\": " + std::to_string(b.index) + ",\n";
        json += "      \"timestamp\": " + quoteJson(b.timestamp) + ",\n";
        json += "      \"role\": " + quoteJson(b.role) + ",\n";
        json += "      \"content\": " + quoteJson(b.content) + ",\n";
        json += "      \"previousHash\": " + quoteJson(b.previousHash) + ",\n";
        json += "      \"hash\": " + quoteJson(b.hash) + "\n";
        json += "    }";
    }
    json += conversation.blocks.empty() ? "],\n" : "\n  ],\n";
    json += "  \"integrity\": {\n";
    json += "    \"algorithm\": \"SHA-256\",\n";
    json += "    \"blockCount\": " + std::to_string(conversation.blocks.size()) + ",\n";
    json += "    \"headHash\": " + quoteJson(conversation.blocks.empty() ? zeroHash() : conversation.blocks.back().hash) + "\n";
    json += "  }\n}\n";
    return json;
}

bool ConversationLedger::fromJson(const std::string& text, Conversation& conversation, std::string& error)
{
    using detail::JsonValue;
    JsonValue root;
    if (! detail::parseJson(text, root, error))
        return false;
    if (root.type != JsonValue::Type::object)
    {
        error = "The file is not a conversation.";
        return false;
    }

    auto stringOf = [&](const JsonValue& object, const char* name, std::string& out) {
        const auto* v = object.find(name);
        if (v == nullptr || v->type != JsonValue::Type::string)
        {
            error = std::string("Missing \"") + name + "\".";
            return false;
        }
        out = v->string;
        return true;
    };
    auto integerOf = [&](const JsonValue& object, const char* name, int& out) {
        const auto* v = object.find(name);
        if (v == nullptr || v->type != JsonValue::Type::integer)
        {
            error = std::string("Missing \"") + name + "\".";
            return false;
        }
        out = (int) v->integer;
        return true;
    };

    Conversation read;
    if (! stringOf(root, "schema", read.schema) || ! integerOf(root, "schemaVersion", read.schemaVersion) || ! stringOf(root, "id", read.id)
        || ! stringOf(root, "title", read.title) || ! stringOf(root, "createdAt", read.createdAt) || ! stringOf(root, "updatedAt", read.updatedAt))
        return false;

    const auto* blocks = root.find("blocks");
    if (blocks == nullptr || blocks->type != JsonValue::Type::array)
    {
        error = "Missing \"blocks\".";
        return false;
    }
    for (const auto& item : blocks->items)
    {
        if (item.type != JsonValue::Type::object)
        {
            error = "A block is not an object.";
            return false;
        }
        ConversationBlock block;
        if (! integerOf(item, "index", block.index) || ! stringOf(item, "timestamp", block.timestamp) || ! stringOf(item, "role", block.role)
            || ! stringOf(item, "content", block.content) || ! stringOf(item, "previousHash", block.previousHash) || ! stringOf(item, "hash", block.hash))
            return false;
        read.blocks.push_back(std::move(block));
    }

    const auto* integrity = root.find("integrity");
    if (integrity == nullptr || integrity->type != JsonValue::Type::object)
    {
        error = "Missing \"integrity\".";
        return false;
    }
    if (! stringOf(*integrity, "algorithm", read.algorithm) || ! integerOf(*integrity, "blockCount", read.declaredBlockCount)
        || ! stringOf(*integrity, "headHash", read.declaredHeadHash))
        return false;

    conversation = std::move(read);
    return true;
}

std::string ConversationLedger::toMarkdown(const Conversation& conversation)
{
    std::string text = "# " + conversation.title + "\n\n";
    text += "Started " + conversation.createdAt + ", last updated " + conversation.updatedAt + ".  \n";
    text += "Conversation " + conversation.id + ", " + std::to_string(conversation.blocks.size()) + " message(s).\n";
    for (const auto& block : conversation.blocks)
    {
        text += "\n---\n\n**" + std::string(block.role == "user" ? "You" : "Assistant") + "** (" + block.timestamp + ")\n\n";
        text += block.content;
        text += "\n";
    }
    return text;
}
}
