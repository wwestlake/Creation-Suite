// The whole path, with a scripted model and a fake application: the model writes FRust that does not compile,
// reads the compile error, fixes it, and the fixed script changes the application. Also: stopping a running
// script, and a policy that keeps the model out of the application.

#include <creation/ai/AgentEngine.h>
#include <creation/ai/FrustTools.h>
#include <creation/ai/ScriptedProvider.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>

using creation::frust::ScriptApi;
using creation::frust::ScriptRunner;
using Call = ScriptRunner::Call;

namespace
{
int failures = 0;

void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "PASS  " : "FAIL  ") << what << std::endl;
    if (! ok)
        ++failures;
}

bool contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

// The fake application: a list of tracks.
struct FakeApp
{
    struct Track { std::string name; double volumeDb = 0.0; };
    std::mutex mutex;
    std::vector<Track> tracks;
    std::atomic<int> callsOffMainThread { 0 };
};

std::thread::id mainThreadId;

FakeApp* app() { return static_cast<FakeApp*>(Call::userData()); }

extern "C"
{
long long app_track_add(const char* name)
{
    auto* a = app();
    const std::string trackName = name != nullptr ? name : "";
    long long number = 0;
    Call::onMainThread([&]
    {
        if (std::this_thread::get_id() != mainThreadId)
            ++a->callsOffMainThread;
        std::lock_guard<std::mutex> lock(a->mutex);
        a->tracks.push_back({ trackName, 0.0 });
        number = (long long) a->tracks.size();
    });
    return number;
}

long long app_set_volume_db(long long track, double db)
{
    auto* a = app();
    long long ok = 0;
    Call::onMainThread([&]
    {
        std::lock_guard<std::mutex> lock(a->mutex);
        if (track >= 1 && track <= (long long) a->tracks.size())
        {
            a->tracks[(size_t) track - 1].volumeDb = db;
            ok = 1;
        }
    });
    return ok;
}

long long app_log(const char* text)
{
    Call::log(text != nullptr ? text : "");
    return 1;
}

long long app_spin()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    return Call::shouldStop() ? 0 : 1;
}
}

const char* declarations = R"(
extern fn app_track_add(name: String) -> i64;      // returns the new track's number
extern fn app_set_volume_db(track: i64, db: f64) -> i64;   // 1 if it worked
extern fn app_log(text: String) -> i64;
extern fn app_spin() -> i64;
)";

struct MainQueue
{
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<std::function<void()>> jobs;

    bool run(const std::function<void()>& work)
    {
        std::mutex doneMutex;
        std::condition_variable doneWake;
        bool done = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            jobs.push_back([&]
            {
                work();
                std::lock_guard<std::mutex> doneLock(doneMutex);
                done = true;
                doneWake.notify_all();
            });
        }
        wake.notify_all();
        std::unique_lock<std::mutex> lock(doneMutex);
        doneWake.wait(lock, [&] { return done; });
        return true;
    }

    void pumpUntil(const std::atomic<bool>& finished)
    {
        while (! finished)
        {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait_for(lock, std::chrono::milliseconds(5), [&] { return ! jobs.empty(); });
                if (jobs.empty())
                    continue;
                job = std::move(jobs.front());
                jobs.pop_front();
            }
            job();
        }
    }
};

ScriptApi makeApi(FakeApp& target)
{
    ScriptApi api;
    api.moduleName = "FakeApi";
    api.declarations = declarations;
    api.applicationName = "smoke-app";
    api.userData = &target;
    api.functions = {
        { "app_track_add", reinterpret_cast<void*>(&app_track_add) },
        { "app_set_volume_db", reinterpret_cast<void*>(&app_set_volume_db) },
        { "app_log", reinterpret_cast<void*>(&app_log) },
        { "app_spin", reinterpret_cast<void*>(&app_spin) },
    };
    return api;
}

std::string toolArguments(const std::string& code)
{
    // The model's arguments are JSON: quote and escape the code.
    std::string json = "{\"code\":\"";
    for (const char c : code)
    {
        if (c == '"' || c == '\\') { json += '\\'; json += c; }
        else if (c == '\n') json += "\\n";
        else json += c;
    }
    return json + "\"}";
}

// Runs the agent on its own thread while this thread plays the application's main thread.
creation::ai::AgentResult runAgent(MainQueue& queue, creation::ai::AgentEngine& engine, const creation::ai::AgentRequest& request,
                                   const creation::ai::CancelToken& cancel)
{
    creation::ai::AgentResult result;
    std::atomic<bool> finished { false };
    std::thread worker([&] { result = engine.run(request, cancel); finished = true; });
    queue.pumpUntil(finished);
    worker.join();
    return result;
}
}

int main()
{
    using namespace creation::ai;
    mainThreadId = std::this_thread::get_id();
    MainQueue queue;

    const std::string broken = "pub fn run() -> String = {\n    let t = app_track_add(\"Bass\");\n    let v = ;\n    \"done\"\n}\n";
    const std::string fixed = "pub fn run() -> String = {\n    let t = app_track_add(\"Bass\");\n    app_set_volume_db(t, -6.0);\n"
                              "    app_log(\"track added\");\n    \"Added Bass at -6 dB\"\n}\n";

    // ---- Write, read the compile error, fix, run ----
    {
        FakeApp target;
        auto api = makeApi(target);
        ScriptRunner runner(api, [&](const std::function<void()>& work) { return queue.run(work); });
        ToolRegistry tools;
        tools.add(makeRunFrustTool(runner));
        tools.add(makeCheckFrustTool(runner));
        tools.add(makeFrustApiReferenceTool(api));

        ScriptedProvider provider;
        provider.addToolCallReply("c1", "run_frust", toolArguments(broken), "I will add a Bass track.");
        provider.addToolCallReply("c2", "run_frust", toolArguments(fixed), "That did not compile; fixing it.");
        provider.addTextReply("Added a track called Bass at -6 dB.");

        AgentEngine engine(provider, tools);
        CancelToken cancel;
        AgentRequest request;
        request.model = "test";
        request.system = "You write FRust to control the application.";
        request.userMessage = "Add a track called Bass at -6 dB.";

        std::vector<std::string> events;
        request.events.onToolResult = [&](const ToolCallBlock& call, const ToolResult& result)
        {
            events.push_back(call.name + (result.ok ? ":ok" : ":failed"));
        };

        auto result = runAgent(queue, engine, request, cancel);
        check(result.reason == EndReason::finished && result.toolCalls == 2, "the run finishes after the model corrects its script");
        check(events == std::vector<std::string> { "run_frust:failed", "run_frust:ok" }, "the first script failed to compile and the second ran");

        const auto sent = provider.receivedRequests();
        const auto& firstResult = std::get<ToolResultBlock>(sent[1].messages[2].content[0]);
        check(firstResult.isError && contains(firstResult.content, "script.frust:3:"), "the model was shown the compile error with the script's own line: " + firstResult.content);
        const auto& secondResult = std::get<ToolResultBlock>(sent[2].messages[4].content[0]);
        check(! secondResult.isError && contains(secondResult.content, "Added Bass at -6 dB") && contains(secondResult.content, "track added"),
              "the model was shown what the fixed script logged and returned");

        std::lock_guard<std::mutex> lock(target.mutex);
        check(target.tracks.size() == 1 && target.tracks[0].name == "Bass" && target.tracks[0].volumeDb == -6.0,
              "the fixed script changed the application: a track named Bass at -6 dB");
        check(target.callsOffMainThread == 0, "the application was only touched on its main thread");
        check(sent[0].tools.size() == 3, "the model was offered run_frust, check_frust and the API reference");
    }

    // ---- check_frust and the API reference ----
    {
        FakeApp target;
        auto api = makeApi(target);
        ScriptRunner runner(api, [&](const std::function<void()>& work) { return queue.run(work); });
        ToolRegistry tools;
        tools.add(makeCheckFrustTool(runner));
        tools.add(makeFrustApiReferenceTool(api));
        CancelToken cancel;

        auto good = tools.execute({ "a", "check_frust", toolArguments(fixed) }, cancel);
        check(good.ok && contains(good.content, "compiles"), "check_frust accepts a good script");
        auto bad = tools.execute({ "b", "check_frust", toolArguments(broken) }, cancel);
        check(! bad.ok && contains(bad.content, "script.frust:3:"), "check_frust reports errors with the script's line");
        {
            std::lock_guard<std::mutex> lock(target.mutex);
            check(target.tracks.empty(), "check_frust ran nothing");
        }
        auto reference = tools.execute({ "c", "frust_api_reference", "{}" }, cancel);
        check(reference.ok && contains(reference.content, "app_track_add") && contains(reference.content, "app_set_volume_db"),
              "the API reference lists the application's functions");
        auto noCode = tools.execute({ "d", "check_frust", "{}" }, cancel);
        check(! noCode.ok && contains(noCode.content, "code"), "a missing script is refused by argument validation");
    }

    // ---- A policy that keeps the model out of the application ----
    {
        FakeApp target;
        auto api = makeApi(target);
        ScriptRunner runner(api, [&](const std::function<void()>& work) { return queue.run(work); });
        ToolRegistry tools;
        tools.add(makeRunFrustTool(runner));

        ScriptedProvider provider;
        provider.addToolCallReply("c1", "run_frust", toolArguments(fixed));
        provider.addTextReply("I was not allowed to change the project.");
        AgentEngine engine(provider, tools);
        CancelToken cancel;
        AgentRequest request;
        request.model = "test";
        request.userMessage = "Add a track.";
        request.policy = [](const Tool& tool, const ToolCallBlock&) -> ToolDecision
        {
            if (tool.effect() != Effect::read)
                return { false, "this mode is read-only" };
            return {};
        };

        auto result = runAgent(queue, engine, request, cancel);
        std::lock_guard<std::mutex> lock(target.mutex);
        check(result.reason == EndReason::finished && target.tracks.empty(), "a script the policy refused never touched the application");
    }

    // ---- Stopping the run stops the script ----
    {
        FakeApp target;
        auto api = makeApi(target);
        ScriptRunner runner(api, [&](const std::function<void()>& work) { return queue.run(work); });
        ToolRegistry tools;
        tools.add(makeRunFrustTool(runner));

        const std::string spinning = "pub fn run() -> String = {\n    while (app_spin() == 1) {\n        app_log(\"tick\");\n    };\n    \"ended\"\n}\n";
        ScriptedProvider provider;
        provider.addToolCallReply("c1", "run_frust", toolArguments(spinning));
        provider.addTextReply("never");
        AgentEngine engine(provider, tools);
        CancelToken cancel;
        AgentRequest request;
        request.model = "test";
        request.userMessage = "Spin.";

        std::thread stopper([&] { std::this_thread::sleep_for(std::chrono::milliseconds(300)); cancel.cancel(); });
        const auto started = std::chrono::steady_clock::now();
        auto result = runAgent(queue, engine, request, cancel);
        stopper.join();
        const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        check(result.reason == EndReason::stopped && seconds < 5.0, "stopping the run ends it promptly");

        for (int i = 0; i < 100 && runner.isRunning(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        check(! runner.isRunning(), "the script itself was stopped, not left running");
    }

    std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: " + std::to_string(failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
