#pragma once

// The Suite's FRust environment: the compiler and Frate see the VFS as their file system
// and the VFS as their log. They know nothing else (no drive, no folder, no console).
//
// Virtual paths:
//   "pods/..."   the Suite-wide pod store (installed registry pods), shared by every project
//                and app; entries "frate-pods/..." on the VFS service.
//   anything else  the open project's entries ("Assets/Source/FRust/...", "Logs/frust/...", ...).

#include <creation/assets/ProjectSession.h>
#include <creation/services/SuiteVfsServiceClient.h>

#include <HostEnvironment.h>

#include <juce_core/juce_core.h>

#include <string>
#include <vector>

namespace creation::frust {

class VfsFileSystem final : public ::frust::FileSystem {
public:
    VfsFileSystem(creation::assets::ProjectSession& project, creation::services::SuiteVfsServiceClient& suite);

    bool exists(const std::string& path) override;
    bool read(const std::string& path, std::string& bytes) override;
    bool write(const std::string& path, const std::string& bytes) override;
    bool remove(const std::string& path) override;
    bool listFiles(const std::string& directory, std::vector<std::string>& relativePaths) override;

private:
    struct Mapped {
        bool suiteLevel = false;
        juce::String logicalPath;
    };
    static bool map(const std::string& path, Mapped& mapped);

    creation::assets::ProjectSession& project_;
    creation::services::SuiteVfsServiceClient& suite_;
};

// Log lines are held until flush(), then appended to Logs/frust/<yyyy-mm-dd>.log in the open
// project's VFS. Old daily logs beyond keepDays are removed. The log lives inside the VFS and
// nowhere else.
class VfsLogSink final : public ::frust::LogSink {
public:
    explicit VfsLogSink(creation::assets::ProjectSession& project, int keepDays = 14);

    void log(::frust::LogLevel level, const std::string& source, const std::string& message) override;

    // Appends what is pending to today's log. Safe to call with nothing pending.
    void flush();

    int pendingLineCount() const noexcept { return static_cast<int>(pending_.size()); }

    static juce::String logEntryFor(const juce::Time& when);

private:
    void pruneOldLogs();

    creation::assets::ProjectSession& project_;
    int keepDays_;
    std::vector<juce::String> pending_;
};

} // namespace creation::frust
