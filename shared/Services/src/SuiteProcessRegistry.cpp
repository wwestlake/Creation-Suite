#include <creation/services/SuiteProcessRegistry.h>
#include <creation/suite/SuiteSettings.h>

#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace
{
juce::File getSuiteConfigDirectory()
{
    return creation::suite::SuiteSettingsStore().getSuiteConfigDirectory();
}

// This suite builds Windows-only today (every app's own build docs
// assume MSVC/VS2022) -- confined to this .cpp, never the header, same
// header-firewall spirit as this codebase's LLVM includes (Language/src/
// jit/*.cpp only) so windows.h's macros never leak into anything else
// that includes SuiteProcessRegistry.h.
juce::uint32 currentProcessId()
{
#if JUCE_WINDOWS
    return static_cast<juce::uint32>(::GetCurrentProcessId());
#else
    return 0;
#endif
}

juce::var toVar(const creation::services::SuiteProcessRecord& record)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("appId", record.appId);
    object->setProperty("processId", static_cast<int>(record.processId));
    object->setProperty("oscPort", record.oscPort);
    object->setProperty("pipeName", record.pipeName);
    object->setProperty("startedAtMs", record.startedAt.toMilliseconds());
    object->setProperty("lastHeartbeatMs", record.lastHeartbeat.toMilliseconds());
    return juce::var(object);
}

creation::services::SuiteProcessRecord fromVar(const juce::var& value)
{
    creation::services::SuiteProcessRecord record;
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return record;

    record.appId = object->getProperty("appId").toString();
    record.processId = static_cast<juce::uint32>(static_cast<int>(object->getProperty("processId")));
    record.oscPort = static_cast<int>(object->getProperty("oscPort"));
    record.pipeName = object->getProperty("pipeName").toString();
    record.startedAt = juce::Time(static_cast<juce::int64>(object->getProperty("startedAtMs")));
    record.lastHeartbeat = juce::Time(static_cast<juce::int64>(object->getProperty("lastHeartbeatMs")));
    return record;
}
}

namespace creation::services
{
SuiteProcessRegistration::SuiteProcessRegistration() : juce::Thread("SuiteProcessHeartbeat")
{
}

SuiteProcessRegistration::~SuiteProcessRegistration()
{
    signalThreadShouldExit();
    notify(); // wakes run()'s wait() early so shutdown doesn't block up to a full heartbeat interval.
    stopThread(2000);

    // Graceful-shutdown-only cleanup -- see this class's own header
    // comment on why a crash deliberately does NOT get this treatment
    // (EnumerateLiveProcesses' staleness filter is what handles that).
    if (appId_.isNotEmpty())
        RegistrationFile().deleteFile();
}

void SuiteProcessRegistration::RegisterSelf(const juce::String& appId, int oscPort, const juce::String& pipeName)
{
    appId_ = appId;
    oscPort_ = oscPort;
    pipeName_ = pipeName;
    processId_ = currentProcessId();
    startedAt_ = juce::Time::getCurrentTime();

    // Written immediately, not left to wait for the first heartbeat
    // interval, so a just-started process is discoverable right away.
    WriteHeartbeatFile();
    startThread(juce::Thread::Priority::low);
}

void SuiteProcessRegistration::run()
{
    while (! threadShouldExit())
    {
        wait(kHeartbeatIntervalMs);
        if (threadShouldExit())
            break;
        WriteHeartbeatFile();
    }
}

void SuiteProcessRegistration::WriteHeartbeatFile()
{
    SuiteProcessRecord record;
    record.appId = appId_;
    record.processId = processId_;
    record.oscPort = oscPort_;
    record.pipeName = pipeName_;
    record.startedAt = startedAt_;
    record.lastHeartbeat = juce::Time::getCurrentTime();

    auto directory = SuiteProcessRegistry::RegistryDirectory();
    if (! directory.exists())
        directory.createDirectory();

    RegistrationFile().replaceWithText(juce::JSON::toString(toVar(record), true));
}

juce::File SuiteProcessRegistration::RegistrationFile() const
{
    return SuiteProcessRegistry::RegistryDirectory().getChildFile(appId_ + "-" + juce::String(static_cast<int>(processId_)) + ".json");
}

juce::File SuiteProcessRegistry::RegistryDirectory()
{
    return getSuiteConfigDirectory().getChildFile("processes");
}

juce::Array<SuiteProcessRecord> SuiteProcessRegistry::EnumerateLiveProcesses(double staleSeconds)
{
    juce::Array<SuiteProcessRecord> live;
    auto directory = RegistryDirectory();
    if (! directory.exists())
        return live;

    juce::Array<juce::File> files;
    directory.findChildFiles(files, juce::File::findFiles, false, "*.json");

    const auto now = juce::Time::getCurrentTime();
    for (const auto& file : files)
    {
        const auto parsed = juce::JSON::parse(file);
        if (parsed.isVoid())
            continue;

        auto record = fromVar(parsed);
        const double ageSeconds = (now - record.lastHeartbeat).inSeconds();
        if (ageSeconds > staleSeconds)
        {
            // Stale -- almost certainly a crashed/killed process that
            // never got to run its own destructor's cleanup. Delete
            // opportunistically so the registry directory doesn't
            // accumulate dead entries forever.
            file.deleteFile();
            continue;
        }

        live.add(record);
    }

    return live;
}
}
