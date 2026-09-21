#include <creation/services/SuiteProcessRegistry.h>
#include <creation/suite/SuiteSettings.h>
#include <creation/suite/SuiteStoragePaths.h>

#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace
{
// The registry lives inside the VFS root, never in the OS user-data folder (only the root pointer is allowed there).
// Empty File when no root is chosen yet.
juce::File getRegistryRootDirectory()
{
    juce::String error;
    const auto settings = creation::suite::SuiteSettingsStore().load(error);
    if (! creation::suite::hasStorageRoot(settings))
        return {};

    return creation::suite::getSuiteRootDirectory(settings);
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
    object->setProperty("httpPort", record.httpPort);
    object->setProperty("serviceProtocol", record.serviceProtocol);
    object->setProperty("serviceVersion", record.serviceVersion);
    object->setProperty("startedAtMs", record.startedAt.toMilliseconds());
    object->setProperty("lastHeartbeatMs", record.lastHeartbeat.toMilliseconds());
    object->setProperty("openProjectContainerPath", record.openProjectContainerPath);
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
    record.httpPort = static_cast<int>(object->getProperty("httpPort"));
    record.serviceProtocol = static_cast<int>(object->getProperty("serviceProtocol"));
    record.serviceVersion = object->getProperty("serviceVersion").toString();
    record.startedAt = juce::Time(static_cast<juce::int64>(object->getProperty("startedAtMs")));
    record.lastHeartbeat = juce::Time(static_cast<juce::int64>(object->getProperty("lastHeartbeatMs")));
    record.openProjectContainerPath = object->getProperty("openProjectContainerPath").toString();
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
        if (const auto file = RegistrationFile(); file != juce::File())
            file.deleteFile();
}

void SuiteProcessRegistration::RegisterSelf(const juce::String& appId, int oscPort, const juce::String& pipeName, int httpPort)
{
    // Only the VFS service keeps a heartbeat file (VfsHeartbeat.json, next to vfs.bin): it is the one thing apps need to
    // find. Every other program writes nothing to the disk, so registering as an app is deliberately a no-op.
    if (appId != kVfsServiceAppId)
        return;

    appId_ = appId;
    oscPort_ = oscPort;
    httpPort_ = httpPort;
    processId_ = currentProcessId();
    startedAt_ = juce::Time::getCurrentTime();

    // A real, unique pipe name by default -- every process is reachable
    // for control-channel requests (SuiteProjectHandoff) without every
    // caller needing to invent its own naming scheme.
    pipeName_ = pipeName.isNotEmpty() ? pipeName : ("CreationSuiteControl-" + appId + "-" + juce::String(static_cast<int>(processId_)));

    // Written immediately, not left to wait for the first heartbeat
    // interval, so a just-started process is discoverable right away.
    WriteHeartbeatFile();
    startThread(juce::Thread::Priority::low);
}

void SuiteProcessRegistration::SetOpenProject(const juce::File& containerFile)
{
    openProjectContainerPath_ = containerFile.getFullPathName();
    WriteHeartbeatFile();
}

void SuiteProcessRegistration::ClearOpenProject()
{
    openProjectContainerPath_.clear();
    WriteHeartbeatFile();
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
    record.httpPort = httpPort_;
    record.serviceProtocol = serviceProtocol_;
    record.serviceVersion = serviceVersion_;
    record.startedAt = startedAt_;
    record.lastHeartbeat = juce::Time::getCurrentTime();
    record.openProjectContainerPath = openProjectContainerPath_;

    auto directory = SuiteProcessRegistry::RegistryDirectory();
    if (directory == juce::File())
        return;

    if (! directory.exists())
        directory.createDirectory();

    if (const auto file = RegistrationFile(); file != juce::File())
    {
        // Overwritten in place (not the write-a-temp-file-then-swap way): no second file ever appears in the VFS root.
        // A reader that catches it half-written just fails to parse it and tries again a moment later.
        juce::FileOutputStream out(file);
        if (out.openedOk())
        {
            out.setPosition(0);
            out.truncate();
            out.writeText(juce::JSON::toString(toVar(record), true), false, false, nullptr);
        }
    }
}

juce::File SuiteProcessRegistration::RegistrationFile() const
{
    const auto directory = SuiteProcessRegistry::RegistryDirectory();
    if (directory == juce::File())
        return {};

    return directory.getChildFile(kVfsHeartbeatFileName);
}

// The heartbeat file sits directly in the VFS root, next to vfs.bin - no folder of its own.
juce::File SuiteProcessRegistry::RegistryDirectory()
{
    return getRegistryRootDirectory();
}

juce::Array<SuiteProcessRecord> SuiteProcessRegistry::EnumerateLiveProcesses(double staleSeconds)
{
    juce::Array<SuiteProcessRecord> live;
    const auto directory = RegistryDirectory();
    if (directory == juce::File())
        return live;

    const auto file = directory.getChildFile(kVfsHeartbeatFileName);
    if (! file.existsAsFile())
        return live;

    const auto parsed = juce::JSON::parse(file);
    if (parsed.isVoid())
        return live;

    auto record = fromVar(parsed);
    const double ageSeconds = (juce::Time::getCurrentTime() - record.lastHeartbeat).inSeconds();
    if (ageSeconds > staleSeconds)
    {
        // Stale: almost certainly a service that died without running its own clean-up. Remove it so the next start
        // begins clean.
        file.deleteFile();
        return live;
    }

    live.add(record);
    return live;
}
}
