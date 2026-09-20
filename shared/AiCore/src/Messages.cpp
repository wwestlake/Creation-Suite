#include "creation/ai/Messages.h"

namespace creation::ai
{
Message Message::text(Role role, std::string text)
{
    Message message;
    message.role = role;
    message.content.push_back(TextBlock { std::move(text) });
    return message;
}

Message Message::toolResult(std::string callId, std::string content, bool isError)
{
    Message message;
    message.role = Role::tool;
    message.content.push_back(ToolResultBlock { std::move(callId), std::move(content), isError });
    return message;
}

std::string Message::plainText() const
{
    std::string joined;
    for (const auto& block : content)
        if (const auto* text = std::get_if<TextBlock>(&block))
            joined += text->text;
    return joined;
}

std::vector<ToolCallBlock> Message::toolCalls() const
{
    std::vector<ToolCallBlock> calls;
    for (const auto& block : content)
        if (const auto* call = std::get_if<ToolCallBlock>(&block))
            calls.push_back(*call);
    return calls;
}
}
