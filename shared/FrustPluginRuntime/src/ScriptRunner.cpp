#include "creation/frust/ScriptRunner.h"

#include "creation/frust/PluginRuntime.h"

#include <chrono>
#include <memory>

namespace creation::frust
{
namespace
{
// State of one run. A script runs on one thread, and host functions reach its state through that thread.
struct Active
{
    ScriptRunner* runner = nullptr;
    const std::atomic<bool>* stopRequested = nullptr;
    std::chrono::steady_clock::time_point deadline;
    std::vector<std::string> log;
    std::string lastError;
    const MainThreadRunner* mainThread = nullptr;
    void* userData = nullptr;
};

thread_local Active* active = nullptr;

// Loading a script changes process-wide plug-in host state (its application identity, the shared host
// function table), so loading is done one at a time. Running is not.
std::mutex loadMutex;

// The first line of the script file: the manifest every plug-in declares, and the `use` that brings in the
// application's API. The script proper starts on line 2, so a compile error's line is reduced by one.
constexpr int headerLines = 1;
constexpr const char* scriptFileName = "script.frust";

std::string frustStringLiteral(const std::string& text)
{
    std::string literal = "\"";
    for (const char c : text)
    {
        if (c == '"' || c == '\\')
            literal += '\\';
        literal += c;
    }
    return literal + "\"";
}

std::string makeHeader(const ScriptApi& api)
{
    std::string name = api.applicationName.empty() ? std::string("app") : api.applicationName;
    for (auto& c : name)
        if (! ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'))
            c = '_';

    const std::string manifest = "{\"name\":\"" + name + "_script\",\"version\":\"0.1.0\","
                                 "\"description\":\"A script written by an assistant.\"}";
    return "manifest " + frustStringLiteral(manifest) + "; use self::" + api.moduleName + ";\n";
}
}

ScriptRunner::ScriptRunner(ScriptApi api, MainThreadRunner mainThreadRunner)
    : scriptApi(std::move(api)), mainThread(std::move(mainThreadRunner))
{
}

ScriptRunner::~ScriptRunner()
{
    stopRequested = true;
    std::lock_guard<std::mutex> lock(workerMutex);
    if (worker.joinable())
    {
        if (worker.get_id() == std::this_thread::get_id())
            worker.detach();
        else
            worker.join();
    }
}

::frust::CompileRequest ScriptRunner::makeRequest(const std::string& script) const
{
    ::frust::CompileRequest request;
    request.sources.push_back({ scriptFileName, makeHeader(scriptApi) + script });

    // The files `use self::<name>;` can name: the API module, and each premade helper module.
    std::vector<ScriptApi::Module> available = scriptApi.modules;
    available.push_back({ scriptApi.moduleName, scriptApi.declarations });
    request.siblingFiles = [available](const std::string& name, std::string& text)
    {
        for (const auto& module : available)
        {
            const std::string file = module.name + ".frust";
            if (name.size() >= file.size() && name.compare(name.size() - file.size(), file.size(), file) == 0)
            {
                text = module.source;
                return true;
            }
        }
        return false;
    };
    return request;
}

ScriptResult ScriptRunner::check(const std::string& script) const
{
    ScriptResult result;
    auto request = makeRequest(script);
    request.emitObject = false;

    auto compiled = ::frust::Compile(request);
    for (auto& diagnostic : compiled.diagnostics)
    {
        if (diagnostic.file == scriptFileName && diagnostic.line > headerLines)
            diagnostic.line -= headerLines;
        result.diagnostics.push_back(diagnostic);
    }
    result.compiled = compiled.ok;
    result.ok = compiled.ok;
    return result;
}

ScriptResult ScriptRunner::runBlocking(const std::string& script)
{
    auto result = check(script);
    if (! result.ok)
    {
        result.compiled = false;
        return result;
    }
    return execute(script);
}

ScriptResult ScriptRunner::execute(const std::string& script)
{
    ScriptResult result;

    stopRequested = false;
    running = true;

    Active run;
    run.userData = scriptApi.userData;
    run.runner = this;
    run.stopRequested = &stopRequested;
    run.mainThread = &mainThread;
    run.deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds((long long) (timeLimitSeconds * 1000.0));
    active = &run;

    {
        std::string error;
        const std::string key = "script";
        bool loaded = false;
        std::unique_ptr<PluginRuntime> runtimeHolder;
        {
            // The plug-in host's application identity is process-wide and a runtime sets it when it is made.
            // Load under a lock and put back whatever it was, so another runtime in the same app (Signal
            // Lab's) still loads as itself.
            std::lock_guard<std::mutex> loading(loadMutex);
            const char* previousIdentity = frust_plugin_host_application_identity();
            const std::string previous = previousIdentity != nullptr ? previousIdentity : "";

            runtimeHolder = std::make_unique<PluginRuntime>(
                scriptApi.applicationName.empty() ? std::string("script-runner") : scriptApi.applicationName + "-script");
            // The host function table is process-wide: register each function once, not on every run.
            for (const auto& function : scriptApi.functions)
                if (! frust_plugin_is_host_function_available(function.name))
                    runtimeHolder->registerHostFunction(function.name, function.function);
            loaded = runtimeHolder->loadSource(key, makeRequest(script), error);

            if (! previous.empty())
                frust_plugin_host_set_application_identity(previous.c_str());
        }
        auto& runtime = *runtimeHolder;

        if (! loaded)
        {
            result.error = error;
        }
        else
        {
            using RunFn = const char* (*)();
            auto* entry = reinterpret_cast<RunFn>(runtime.getFunction(key, "run"));
            if (entry == nullptr)
            {
                result.error = "The script has no `pub fn run() -> String`. Define it; what it returns is reported back.";
            }
            else
            {
                const char* returned = entry();
                for (const auto& line : run.log)
                    result.output += line + "\n";
                if (returned != nullptr)
                    result.output += returned;
                result.compiled = true;
                result.ok = true;
            }
            runtime.unload(key);
        }
    }

    result.stopped = Call::shouldStop();
    active = nullptr;
    running = false;
    return result;
}

bool ScriptRunner::runAsync(const std::string& script, std::function<void(ScriptResult)> done)
{
    std::lock_guard<std::mutex> lock(workerMutex);
    if (running)
        return false;

    if (worker.joinable())
    {
        if (worker.get_id() == std::this_thread::get_id())
            worker.detach();
        else
            worker.join();
    }

    running = true;
    worker = std::thread([this, script, done = std::move(done)]
    {
        auto result = runBlocking(script);
        running = false;
        if (done)
            done(std::move(result));
    });
    return true;
}

void* ScriptRunner::Call::userData()
{
    return active != nullptr ? active->userData : nullptr;
}

bool ScriptRunner::Call::shouldStop()
{
    return active != nullptr
           && ((active->stopRequested != nullptr && active->stopRequested->load())
               || std::chrono::steady_clock::now() > active->deadline);
}

void ScriptRunner::Call::log(const std::string& line)
{
    if (active != nullptr)
        active->log.push_back(line);
}

void ScriptRunner::Call::setLastError(const std::string& why)
{
    if (active != nullptr)
        active->lastError = why;
}

std::string ScriptRunner::Call::lastError()
{
    return active != nullptr ? active->lastError : std::string();
}

const char* ScriptRunner::Call::handBack(const std::string& text)
{
    // A ring of buffers lets a script hold a few returned strings at once (name(1) beside name(2)).
    constexpr int slots = 32;
    thread_local std::string ring[slots];
    thread_local int next = 0;
    auto& slot = ring[next];
    next = (next + 1) % slots;
    slot = text;
    return slot.c_str();
}

bool ScriptRunner::Call::onMainThread(const std::function<void()>& work)
{
    if (active == nullptr)
        return false;

    if (shouldStop())
    {
        active->lastError = "The request was stopped, or the script ran past its time limit.";
        return false;
    }

    if (active->mainThread == nullptr || ! *active->mainThread)
    {
        active->lastError = "The application is not available to the script.";
        return false;
    }

    if (! (*active->mainThread)(work))
    {
        active->lastError = "The application could not run that call.";
        return false;
    }
    return true;
}
}
