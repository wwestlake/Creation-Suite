#pragma once

#include <juce_core/juce_core.h>

#include "creation/assets/ProjectManifest.h"

namespace creation::assets
{
struct ProjectContainerEntry
{
    juce::String logicalPath;
    juce::MemoryBlock data;
    juce::Time modifiedAt;
    int compressionLevel = 9;
};

class ProjectContainerIO final
{
public:
    static bool writeContainer(const juce::File& containerFile,
                               const ProjectManifest& manifest,
                               const juce::Array<ProjectContainerEntry>& entries,
                               juce::String& errorMessage);

    static bool readManifest(const juce::File& containerFile,
                             ProjectManifest& outManifest,
                             juce::String& errorMessage);

    static bool readEntry(const juce::File& containerFile,
                          const juce::String& logicalPath,
                          juce::MemoryBlock& outData,
                          juce::String& errorMessage);

    static juce::StringArray listEntries(const juce::File& containerFile);
};
}
