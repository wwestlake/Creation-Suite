// Checks the generic script runner with a small API of its own (no application): FRust source in, an
// application's native functions called, a result out. Covers compile diagnostics, premade modules, a run's own
// host, running on its own thread, stopping, the time limit, and the plug-in host identity being put back.

#include <creation/frust/ScriptRunner.h>

#include <frust_plugin_host/FrustPluginHost.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>

using creation::frust::ScriptApi;
using creation::frust::ScriptResult;
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

// What the test's "application" is: something the native functions act on.
struct App
{
    std::atomic<long long> additions { 0 };
    std::atomic<long long> lastSum { 0 };
    std::atomic<int> callsOnMainThread { 0 };
    std::atomic<int> calls { 0 };
};

std::thread::id mainThreadId;

App* app() { return static_cast<App*>(Call::userData()); }

extern "C"
{
long long t_add(long long a, long long b)
{
    long long sum = 0;
    // The run's own state is reached through the script's thread, so read it here, before the work moves to
    // the main thread.
    auto* a_ = app();
    if (! Call::onMainThread([&]
        {
            ++a_->calls;
            if (std::this_thread::get_id() == mainThreadId)
                ++a_->callsOnMainThread;
            ++a_->additions;
            sum = a + b;
            a_->lastSum = sum;
        }))
        return -1;
    return sum;
}

double t_scale(double x)
{
    return x * 2.5;
}

const char* t_echo(const char* text)
{
    return Call::handBack(std::string("echo:") + (text != nullptr ? text : ""));
}

long long t_log(const char* text)
{
    Call::log(text != nullptr ? text : "");
    return 1;
}

// 1 while the run may continue; 0 once it has been stopped or has run out of time.
long long t_spin()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    return Call::shouldStop() ? 0 : 1;
}
}

const char* declarations = R"(
extern fn t_add(a: i64, b: i64) -> i64;
extern fn t_scale(x: f64) -> f64;
extern fn t_echo(text: String) -> String;
extern fn t_log(text: String) -> i64;
extern fn t_spin() -> i64;
)";

// The test's stand-in for an application's main thread: work is queued and this thread runs it.
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

    // Runs queued work until `finished` is true.
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

ScriptApi makeApi(App& target)
{
    ScriptApi api;
    api.moduleName = "TestApi";
    api.declarations = declarations;
    api.applicationName = "smoke-app";
    api.userData = &target;
    api.functions = {
        { "t_add", reinterpret_cast<void*>(&t_add) },
        { "t_scale", reinterpret_cast<void*>(&t_scale) },
        { "t_echo", reinterpret_cast<void*>(&t_echo) },
        { "t_log", reinterpret_cast<void*>(&t_log) },
        { "t_spin", reinterpret_cast<void*>(&t_spin) },
    };
    api.modules.push_back({ "helpers", "pub fn add_twice(a: i64) -> i64 = {\n    t_add(a, a)\n}\n" });
    return api;
}
}

int main()
{
    mainThreadId = std::this_thread::get_id();
    MainQueue queue;
    const creation::frust::MainThreadRunner mainThread = [&](const std::function<void()>& work) { return queue.run(work); };

    // ---- A script that works (on this thread, so the "main thread" is this one) ----
    {
        App target;
        ScriptRunner runner(makeApi(target), [](const std::function<void()>& work) { work(); return true; });

        auto result = runner.runBlocking("pub fn run() -> String = {\n    t_add(2, 3);\n    t_log(\"first\");\n    t_echo(\"done\")\n}\n");
        check(result.ok && result.compiled, "a good script compiles and runs: " + result.error);
        check(contains(result.output, "first") && contains(result.output, "echo:done"), "the output has the log lines then the returned text");
        check(target.additions == 1 && target.lastSum == 5, "the script's call reached the application with its arguments");

        auto decimal = runner.runBlocking("pub fn run() -> String = {\n    let y = t_scale(4.0);\n    t_log(\"scaled\");\n    t_echo(\"ok\")\n}\n");
        check(decimal.ok, "a decimal argument and result work: " + decimal.error);
    }

    // ---- Compile errors: structured, with the script's own line numbers ----
    {
        App target;
        ScriptRunner runner(makeApi(target), [](const std::function<void()>& work) { work(); return true; });

        auto result = runner.runBlocking("pub fn run() -> String = {\n    t_log(\"a\");\n    let x = ;\n    t_echo(\"b\")\n}\n");
        check(! result.ok && ! result.compiled && ! result.diagnostics.empty(), "a compile error is reported");
        check(! result.diagnostics.empty() && result.diagnostics[0].line == 3 && result.diagnostics[0].file == "script.frust",
              "the error's line is the script's own line (3)");
        check(target.calls == 0, "a script that does not compile runs nothing");

        auto unknown = runner.runBlocking("pub fn run() -> String = {\n    not_a_function(1);\n    t_echo(\"b\")\n}\n");
        check(! unknown.ok && ! unknown.diagnostics.empty(), "calling something that does not exist is a compile error");

        auto noRun = runner.runBlocking("pub fn other() -> String = {\n    t_echo(\"b\")\n}\n");
        check(! noRun.ok && contains(noRun.error, "pub fn run"), "a script without run says what is missing");

        auto checkOnly = runner.check("pub fn run() -> String = {\n    t_echo(\"b\")\n}\n");
        check(checkOnly.ok && target.calls == 0, "check compiles without running");
    }

    // ---- A premade module ----
    {
        App target;
        ScriptRunner runner(makeApi(target), [](const std::function<void()>& work) { work(); return true; });
        auto result = runner.runBlocking("use self::helpers;\npub fn run() -> String = {\n    add_twice(4);\n    t_echo(\"ok\")\n}\n");
        check(result.ok, "a script can use a premade module: " + result.error + (result.diagnostics.empty() ? "" : " / " + result.diagnostics[0].message));
        check(target.lastSum == 8, "the premade helper called the application");
    }

    // ---- Each run reaches its own application ----
    {
        App first, second;
        ScriptRunner a(makeApi(first), [](const std::function<void()>& work) { work(); return true; });
        ScriptRunner b(makeApi(second), [](const std::function<void()>& work) { work(); return true; });
        ScriptResult ra, rb;
        std::thread ta([&] { ra = a.runBlocking("pub fn run() -> String = {\n    t_add(1, 1);\n    t_add(1, 1);\n    t_echo(\"a\")\n}\n"); });
        std::thread tb([&] { rb = b.runBlocking("pub fn run() -> String = {\n    t_add(5, 5);\n    t_echo(\"b\")\n}\n"); });
        ta.join();
        tb.join();
        check(ra.ok && rb.ok, "two runs at once both finish: " + ra.error + rb.error);
        check(first.additions == 2 && second.additions == 1 && first.lastSum == 2 && second.lastSum == 10,
              "each run reached its own application, not a shared current one");
    }

    // ---- On its own thread, with host calls on the main thread ----
    {
        App target;
        ScriptRunner runner(makeApi(target), mainThread);
        std::atomic<bool> finished { false };
        ScriptResult result;
        const bool started = runner.runAsync("pub fn run() -> String = {\n    t_add(2, 2);\n    t_add(3, 3);\n    t_echo(\"async\")\n}\n",
                                             [&](ScriptResult r) { result = std::move(r); finished = true; });
        check(started, "runAsync starts a run");
        queue.pumpUntil(finished);
        check(result.ok && contains(result.output, "echo:async"), "an async run finishes with its result: " + result.error);
        check(target.calls == 2 && target.callsOnMainThread == 2, "the application's calls ran on the main thread, not the script's");
    }

    // ---- Stop ----
    {
        App target;
        ScriptRunner runner(makeApi(target), mainThread);
        std::atomic<bool> finished { false };
        ScriptResult result;
        runner.runAsync("pub fn run() -> String = {\n    while (t_spin() == 1) {\n        t_log(\"tick\");\n    };\n    t_echo(\"ended\")\n}\n",
                        [&](ScriptResult r) { result = std::move(r); finished = true; });
        std::thread stopper([&] { std::this_thread::sleep_for(std::chrono::milliseconds(150)); runner.requestStop(); });
        queue.pumpUntil(finished);
        stopper.join();
        check(result.ok && result.stopped && contains(result.output, "ended"),
              "a stopped run ends at its next call and says it was stopped [ok=" + std::to_string(result.ok) + " stopped=" + std::to_string(result.stopped)
                  + " error=" + result.error + (result.diagnostics.empty() ? "" : " diag=" + result.diagnostics[0].message) + "]");
    }

    // ---- Time limit ----
    {
        App target;
        ScriptRunner runner(makeApi(target), mainThread);
        runner.setTimeLimitSeconds(0.2);
        std::atomic<bool> finished { false };
        ScriptResult result;
        const auto started = std::chrono::steady_clock::now();
        runner.runAsync("pub fn run() -> String = {\n    while (t_spin() == 1) {\n        t_log(\"tick\");\n    };\n    t_echo(\"ended\")\n}\n",
                        [&](ScriptResult r) { result = std::move(r); finished = true; });
        queue.pumpUntil(finished);
        const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        check(result.stopped && seconds < 3.0,
              "a run past its time limit is ended [ok=" + std::to_string(result.ok) + " stopped=" + std::to_string(result.stopped) + " seconds="
                  + std::to_string(seconds) + " error=" + result.error + "]");
    }

    // ---- Only one async run at a time per runner ----
    {
        App target;
        ScriptRunner runner(makeApi(target), mainThread);
        std::atomic<bool> finished { false };
        runner.runAsync("pub fn run() -> String = {\n    while (t_spin() == 1) {\n        t_log(\"tick\");\n    };\n    t_echo(\"x\")\n}\n", [&](ScriptResult) { finished = true; });
        check(! runner.runAsync("pub fn run() -> String = { \"y\" }", nullptr), "a second run is refused while one is going");
        runner.requestStop();
        queue.pumpUntil(finished);
    }

    // ---- The plug-in host's identity is put back ----
    {
        frust_plugin_host_set_application_identity("identity-before");
        App target;
        ScriptRunner runner(makeApi(target), [](const std::function<void()>& work) { work(); return true; });
        runner.runBlocking("pub fn run() -> String = {\n    t_echo(\"x\")\n}\n");
        const char* after = frust_plugin_host_application_identity();
        check(after != nullptr && std::string(after) == "identity-before", "the plug-in host's application identity is restored after a run");
    }

    std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: " + std::to_string(failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
