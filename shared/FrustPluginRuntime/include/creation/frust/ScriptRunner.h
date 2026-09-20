#pragma once

#include <CompilerApi.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace creation::frust
{
// One native function an application offers to scripts.
struct ScriptHostFunction
{
    const char* name = nullptr;   // the name the script declares with `extern fn`
    void* function = nullptr;     // a C-ABI function: integers, doubles and const char* in and out
};

// The API an application gives the scripts an AI (or a person) writes. A script is FRust source; the only way
// it can affect anything is through the functions listed here. The declarations are the FRust text a script
// links against, and the same text is what a model is shown as the API reference, so they cannot disagree.
struct ScriptApi
{
    // Premade FRust an app ships for scripts to build on, written once and tested: helpers on top of the API
    // ("add a track with this name and volume"). A script pulls one in with `use self::<name>;`.
    struct Module
    {
        std::string name;
        std::string source;
    };

    std::string moduleName;                      // scripts get it with `use self::<moduleName>;`
    std::string declarations;                    // FRust: the `extern fn` lines for the functions below
    std::vector<ScriptHostFunction> functions;
    std::vector<Module> modules;                 // premade helper modules, in addition to the API module
    std::string applicationName;                 // names the script's plug-in manifest, e.g. "creation-station"
    void* userData = nullptr;                    // the app's own object (its host); host functions read it with Call::userData()
};

// Runs `work` on the application's main thread and waits for it, returning false if it could not be run.
// Host functions use it because most app state may only be touched there. The application supplies it (in a
// JUCE app, MessageManager::callSync), which keeps this library free of any GUI code.
using MainThreadRunner = std::function<bool(const std::function<void()>& work)>;

struct ScriptResult
{
    bool ok = false;                                // compiled, ran, and `run` returned
    bool compiled = false;                          // got as far as running
    std::vector<::frust::Diagnostic> diagnostics;   // compile errors, lines counted in the script as written
    std::string output;                             // lines the script logged, then what `run` returned
    std::string error;                              // a failure that is not a compile diagnostic
    bool stopped = false;                           // the run was stopped or hit its time limit
};

// Compiles a script and runs it in this process, against an application's ScriptApi. The compiler is the
// embedded library and the script is loaded from memory; nothing is written to disk and no program is started.
//
// A script must define `pub fn run() -> String`; what it returns is reported. A run may be on the caller's
// thread (runBlocking) or on a thread of its own (runAsync), which is how an assistant runs code without
// holding up the application. Stop and the time limit take effect at a script's next call into the
// application; a script that never calls out cannot be interrupted, only ignored once it returns.
class ScriptRunner
{
public:
    ScriptRunner(ScriptApi api, MainThreadRunner mainThread);
    ~ScriptRunner();

    ScriptRunner(const ScriptRunner&) = delete;
    ScriptRunner& operator=(const ScriptRunner&) = delete;

    // The compile-only check: diagnostics, nothing run.
    ScriptResult check(const std::string& script) const;

    ScriptResult runBlocking(const std::string& script);

    // Starts the script on its own thread. `done` is called on that thread when the run ends; post to your
    // own main thread from it if the result must be handled there. False if a run is already going.
    bool runAsync(const std::string& script, std::function<void(ScriptResult)> done);

    void requestStop() noexcept { stopRequested = true; }
    bool isRunning() const noexcept { return running; }
    void setTimeLimitSeconds(double seconds) noexcept { timeLimitSeconds = seconds; }

    const ScriptApi& api() const noexcept { return scriptApi; }

    // For host functions, while a script is running. Each run has its own state, reached through the thread
    // the script runs on, so runs share nothing and there is no process-wide "current script".
    struct Call
    {
        static void* userData();                           // ScriptApi::userData of the run this call belongs to
        static bool shouldStop();                          // stop requested, or past the time limit
        static void log(const std::string& line);          // a line in the run's output
        static void setLastError(const std::string& why);  // for a script that asks what went wrong
        static std::string lastError();
        static const char* handBack(const std::string& text);   // text that outlives the call, for a return value
        // Runs `work` on the main thread (directly if the script is already there). False, with lastError
        // set, if the run was stopped or the main thread was not available.
        static bool onMainThread(const std::function<void()>& work);
    };

private:
    ::frust::CompileRequest makeRequest(const std::string& script) const;
    ScriptResult execute(const std::string& script);

    ScriptApi scriptApi;
    MainThreadRunner mainThread;
    std::atomic<bool> stopRequested { false };
    std::atomic<bool> running { false };
    double timeLimitSeconds = 30.0;
    std::thread worker;
    std::mutex workerMutex;
};
}
