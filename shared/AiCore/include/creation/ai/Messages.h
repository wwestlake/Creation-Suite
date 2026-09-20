#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// The vocabulary every AI provider adapter and the agent engine share: a conversation is a list of messages,
// each made of typed content blocks (text, a tool call the model made, the result of a tool). These are
// plain standard types so the engine and its tests need no GUI or network code; the adapters turn them into
// each provider's own wire format and back.
namespace creation::ai
{
enum class Role
{
    system,
    user,
    assistant,
    tool     // results of tool calls, sent back to the model
};

struct TextBlock
{
    std::string text;
};

// A tool the model asked to run. `argumentsJson` is the JSON object the model produced for the tool's schema.
struct ToolCallBlock
{
    std::string id;              // the provider's id for this call; the result must quote it
    std::string name;
    std::string argumentsJson;
};

struct ToolResultBlock
{
    std::string callId;
    std::string content;         // what the tool returned, as text the model reads
    bool isError = false;
};

using ContentBlock = std::variant<TextBlock, ToolCallBlock, ToolResultBlock>;

struct Message
{
    Role role = Role::user;
    std::vector<ContentBlock> content;

    static Message text(Role role, std::string text);
    static Message toolResult(std::string callId, std::string content, bool isError = false);

    // All the text blocks joined.
    std::string plainText() const;
    std::vector<ToolCallBlock> toolCalls() const;
};

// A tool the model may call. `parametersSchemaJson` is a JSON Schema object describing the arguments.
struct ToolSpec
{
    std::string name;
    std::string description;
    std::string parametersSchemaJson = "{\"type\":\"object\",\"properties\":{}}";
};

struct Usage
{
    std::int64_t inputTokens = 0;
    std::int64_t outputTokens = 0;

    Usage& operator+=(const Usage& other)
    {
        inputTokens += other.inputTokens;
        outputTokens += other.outputTokens;
        return *this;
    }
};

enum class StopReason
{
    endOfTurn,      // the model finished its answer
    toolUse,        // the model wants tools run; see the message's tool calls
    maxTokens,      // cut off by the output limit
    cancelled,
    error
};

enum class ErrorKind
{
    none,
    authentication,     // bad or missing key: not worth retrying
    rateLimited,        // retry after a delay
    network,            // could not reach the provider: retry
    serverError,        // the provider failed: retry
    badRequest,         // this request is wrong: do not retry unchanged
    contextTooLong,     // the conversation does not fit the model
    unsupported,        // the provider or model cannot do what was asked (for example tools)
    cancelled
};

struct ProviderError
{
    ErrorKind kind = ErrorKind::none;
    std::string message;
    int httpStatus = 0;
    double retryAfterSeconds = 0.0;   // when the provider said how long to wait

    bool retryable() const noexcept
    {
        return kind == ErrorKind::rateLimited || kind == ErrorKind::network || kind == ErrorKind::serverError;
    }
};

struct ChatRequest
{
    std::string model;
    std::string system;                 // the system prompt (sent in whatever way the provider wants)
    std::vector<Message> messages;      // the conversation so far, oldest first, without the system prompt
    std::vector<ToolSpec> tools;        // empty: the model may not call tools
    double temperature = 0.2;
    int maxOutputTokens = 4096;
};

struct ChatResponse
{
    Message assistant;                  // what the model said and any tool calls it made
    StopReason stopReason = StopReason::endOfTurn;
    Usage usage;
    std::optional<ProviderError> error; // set when stopReason is error or cancelled

    bool ok() const noexcept { return ! error.has_value(); }
};
}
