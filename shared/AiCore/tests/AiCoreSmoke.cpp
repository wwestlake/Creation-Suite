// Checks the message model and the scripted provider: the foundation the provider adapters and the agent
// engine are tested against.

#include <creation/ai/Messages.h>
#include <creation/ai/ScriptedProvider.h>

#include <iostream>

namespace
{
int failures = 0;

void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "PASS  " : "FAIL  ") << what << std::endl;
    if (! ok)
        ++failures;
}
}

int main()
{
    using namespace creation::ai;

    // ---- Messages ----
    {
        auto user = Message::text(Role::user, "hello");
        check(user.role == Role::user && user.plainText() == "hello", "a text message reads back");

        Message assistant;
        assistant.role = Role::assistant;
        assistant.content.push_back(TextBlock { "I will add it. " });
        assistant.content.push_back(ToolCallBlock { "call_1", "run_frust", "{\"code\":\"x\"}" });
        assistant.content.push_back(TextBlock { "Done." });
        check(assistant.plainText() == "I will add it. Done.", "plainText joins only the text blocks");
        check(assistant.toolCalls().size() == 1 && assistant.toolCalls()[0].name == "run_frust", "toolCalls finds the call");

        auto result = Message::toolResult("call_1", "ok", false);
        check(result.role == Role::tool && std::get<ToolResultBlock>(result.content[0]).callId == "call_1",
              "a tool result quotes its call id");
    }

    // ---- Errors ----
    {
        check(ProviderError { ErrorKind::rateLimited }.retryable(), "a rate limit is retryable");
        check(ProviderError { ErrorKind::network }.retryable(), "a network failure is retryable");
        check(ProviderError { ErrorKind::serverError }.retryable(), "a server error is retryable");
        check(! ProviderError { ErrorKind::authentication }.retryable(), "a bad key is not retryable");
        check(! ProviderError { ErrorKind::badRequest }.retryable(), "a bad request is not retryable");
    }

    // ---- Scripted provider ----
    {
        ScriptedProvider provider;
        provider.addToolCallReply("call_1", "run_frust", "{\"code\":\"pub fn run() -> String = { \\\"hi\\\" }\"}", "Let me try.");
        provider.addTextReply("All done.");
        provider.addError(ErrorKind::rateLimited, "slow down", 2.0);

        ChatRequest request;
        request.model = "test";
        request.system = "You are a coder.";
        request.messages.push_back(Message::text(Role::user, "add a track"));
        request.tools.push_back({ "run_frust", "Run FRust", "{\"type\":\"object\"}" });

        CancelToken cancel;
        std::vector<StreamEvent> events;
        auto first = provider.complete(request, cancel, [&](const StreamEvent& e) { events.push_back(e); });
        check(first.ok() && first.stopReason == StopReason::toolUse, "first reply asks for a tool");
        check(first.assistant.toolCalls().size() == 1 && first.assistant.plainText() == "Let me try.", "the reply carries the call and its lead-in text");
        check(events.size() == 4 && events[0].kind == StreamEvent::Kind::textDelta
                  && events[1].kind == StreamEvent::Kind::toolCallStarted
                  && events[3].kind == StreamEvent::Kind::toolCallFinished,
              "the reply is streamed as text then a tool call");

        request.messages.push_back(first.assistant);
        request.messages.push_back(Message::toolResult("call_1", "hi", false));
        auto second = provider.complete(request, cancel, nullptr);
        check(second.ok() && second.stopReason == StopReason::endOfTurn && second.assistant.plainText() == "All done.",
              "second reply ends the turn");

        auto third = provider.complete(request, cancel, nullptr);
        check(! third.ok() && third.error->kind == ErrorKind::rateLimited && third.error->retryable()
                  && third.error->retryAfterSeconds == 2.0,
              "a scripted error comes back as data with its retry delay");

        auto exhausted = provider.complete(request, cancel, nullptr);
        check(! exhausted.ok() && exhausted.error->kind == ErrorKind::badRequest, "running out of replies is reported, not a crash");

        auto received = provider.receivedRequests();
        check(received.size() == 4 && received[0].messages.size() == 1 && received[1].messages.size() == 3,
              "the provider recorded exactly what it was sent");
        check(received[0].tools.size() == 1 && received[0].system == "You are a coder.", "tools and the system prompt were sent");

        cancel.cancel();
        provider.addTextReply("never seen");
        auto cancelled = provider.complete(request, cancel, nullptr);
        check(! cancelled.ok() && cancelled.stopReason == StopReason::cancelled && cancelled.error->kind == ErrorKind::cancelled,
              "a cancelled request returns a cancelled response");
    }

    std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: " + std::to_string(failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
