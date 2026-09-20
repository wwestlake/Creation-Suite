#include "creation/frust/VfsEnvironment.h"

namespace creation::frust {
namespace {

const juce::String kSuitePodsVirtualPrefix = "pods/";
const juce::String kSuitePodsEntryPrefix = "frate-pods/";
const juce::String kLogDirectory = "Logs/frust/";

juce::MemoryBlock toBlock(const std::string& bytes) {
    return juce::MemoryBlock(bytes.data(), bytes.size());
}

} // namespace

VfsFileSystem::VfsFileSystem(creation::assets::ProjectSession& project, creation::services::SuiteVfsServiceClient& suite)
    : project_(project), suite_(suite) {}

bool VfsFileSystem::map(const std::string& path, Mapped& mapped) {
    std::string normalized;
    if (!::frust::NormalizePath(path, normalized) || normalized.empty()) return false;

    const juce::String logical(normalized);
    if (logical.startsWith(kSuitePodsVirtualPrefix)) {
        mapped.suiteLevel = true;
        mapped.logicalPath = kSuitePodsEntryPrefix + logical.substring(kSuitePodsVirtualPrefix.length());
    } else {
        mapped.suiteLevel = false;
        mapped.logicalPath = logical;
    }
    return true;
}

bool VfsFileSystem::exists(const std::string& path) {
    Mapped m;
    if (!map(path, m)) return false;
    if (!m.suiteLevel) return project_.isValid() && project_.containsEntry(m.logicalPath);

    juce::MemoryBlock ignored;
    return suite_.readEntry(m.logicalPath, ignored);
}

bool VfsFileSystem::read(const std::string& path, std::string& bytes) {
    Mapped m;
    if (!map(path, m)) return false;

    juce::MemoryBlock block;
    const bool ok = m.suiteLevel ? suite_.readEntry(m.logicalPath, block)
                                 : (project_.isValid() && project_.readEntry(m.logicalPath, block));
    if (!ok) return false;
    bytes.assign(static_cast<const char*>(block.getData()), block.getSize());
    return true;
}

bool VfsFileSystem::write(const std::string& path, const std::string& bytes) {
    Mapped m;
    if (!map(path, m)) return false;
    if (m.suiteLevel) return suite_.writeEntry(m.logicalPath, toBlock(bytes));
    return project_.isValid() && project_.writeEntry(m.logicalPath, toBlock(bytes));
}

bool VfsFileSystem::remove(const std::string& path) {
    Mapped m;
    if (!map(path, m)) return false;
    if (m.suiteLevel) return suite_.removeEntry(m.logicalPath);
    return project_.isValid() && project_.removeEntry(m.logicalPath);
}

bool VfsFileSystem::listFiles(const std::string& directory, std::vector<std::string>& relativePaths) {
    relativePaths.clear();

    std::string normalized;
    if (!::frust::NormalizePath(directory, normalized)) return false;

    const juce::String dir(normalized);
    const bool suiteLevel = dir == "pods" || dir.startsWith(kSuitePodsVirtualPrefix);

    juce::String prefix;
    juce::StringArray all;
    if (suiteLevel) {
        prefix = kSuitePodsEntryPrefix + (dir == "pods" ? juce::String() : dir.substring(kSuitePodsVirtualPrefix.length()));
        if (!suite_.listEntries(all)) return false;
    } else {
        prefix = dir;
        if (!project_.isValid()) return true;
        all = project_.listEntryPaths();
    }
    if (prefix.isNotEmpty() && !prefix.endsWithChar('/')) prefix += "/";

    for (const auto& entry : all)
        if (entry.startsWith(prefix)) relativePaths.push_back(entry.substring(prefix.length()).toStdString());
    return true;
}

// ---- Log ----

VfsLogSink::VfsLogSink(creation::assets::ProjectSession& project, int keepDays)
    : project_(project), keepDays_(keepDays) {}

juce::String VfsLogSink::logEntryFor(const juce::Time& when) {
    return kLogDirectory + when.formatted("%Y-%m-%d") + ".log";
}

void VfsLogSink::log(::frust::LogLevel level, const std::string& source, const std::string& message) {
    const char* tag = level == ::frust::LogLevel::Error ? "ERROR" : level == ::frust::LogLevel::Warning ? "WARN " : "INFO ";
    pending_.push_back(juce::Time::getCurrentTime().formatted("%H:%M:%S") + " " + tag + " [" + juce::String(source) + "] "
                       + juce::String(message));
}

void VfsLogSink::flush() {
    if (pending_.empty() || !project_.isValid()) {
        pending_.clear();
        return;
    }

    const auto entry = logEntryFor(juce::Time::getCurrentTime());
    juce::MemoryBlock existing;
    juce::String text;
    if (project_.containsEntry(entry) && project_.readEntry(entry, existing))
        text = juce::String::fromUTF8(static_cast<const char*>(existing.getData()), static_cast<int>(existing.getSize()));

    for (const auto& line : pending_) text += line + "\n";
    pending_.clear();

    const auto utf8 = text.toStdString();
    project_.writeEntry(entry, juce::MemoryBlock(utf8.data(), utf8.size()));
    pruneOldLogs();
}

void VfsLogSink::pruneOldLogs() {
    juce::StringArray logs;
    for (const auto& entry : project_.listEntryPaths())
        if (entry.startsWith(kLogDirectory) && entry.endsWith(".log")) logs.add(entry);
    logs.sort(false); // names are dates: oldest first
    while (logs.size() > keepDays_) {
        project_.removeEntry(logs[0]);
        logs.remove(0);
    }
}

} // namespace creation::frust
