#include "creation/assets/ProjectContainerIO.h"

#include "creation/assets/VirtualFileSystem.h"

namespace
{
juce::String normalizeContainerPath(const juce::String& logicalPath)
{
    auto normalized = logicalPath.replaceCharacter('\\', '/').trim();
    while (normalized.startsWithChar('/'))
        normalized = normalized.substring(1);
    return normalized;
}
}

namespace creation::assets
{
bool ProjectContainerIO::writeContainer(const juce::File& containerFile,
                                        const ProjectManifest& manifest,
                                        const juce::Array<ProjectContainerEntry>& entries,
                                        juce::String& errorMessage)
{
    const auto parentDirectory = containerFile.getParentDirectory();
    if (! parentDirectory.exists() && ! parentDirectory.createDirectory())
    {
        errorMessage = "Could not create the target container directory.";
        return false;
    }

    juce::ZipFile::Builder builder;
    const auto manifestText = serializeManifest(manifest, true);
    builder.addEntry(new juce::MemoryInputStream(manifestText.toRawUTF8(),
                                                 static_cast<size_t>(manifestText.getNumBytesAsUTF8()),
                                                 false),
                     9,
                     ProjectContainerPaths::manifestPath,
                     manifest.modifiedAt);

    for (const auto& entry : entries)
    {
        const auto path = normalizeContainerPath(entry.logicalPath);
        if (path.isEmpty() || path == ProjectContainerPaths::manifestPath)
            continue;

        builder.addEntry(new juce::MemoryInputStream(entry.data, false),
                         juce::jlimit(0, 9, entry.compressionLevel),
                         path,
                         entry.modifiedAt);
    }

    juce::TemporaryFile temporaryTarget(containerFile);
    std::unique_ptr<juce::FileOutputStream> stream(temporaryTarget.getFile().createOutputStream());
    if (stream == nullptr)
    {
        errorMessage = "Could not open the temporary container output stream.";
        return false;
    }

    if (! builder.writeToStream(*stream, nullptr))
    {
        errorMessage = "Could not write the suite project container zip stream.";
        return false;
    }

    stream->flush();
    stream.reset();
    if (! temporaryTarget.overwriteTargetFileWithTemporary())
    {
        errorMessage = "Could not finalize the suite project container write.";
        return false;
    }

    return true;
}

bool ProjectContainerIO::readManifest(const juce::File& containerFile,
                                      ProjectManifest& outManifest,
                                      juce::String& errorMessage)
{
    VirtualFileSystem vfs;
    if (! vfs.mount(containerFile))
    {
        errorMessage = "Could not mount the suite project container.";
        return false;
    }

    juce::MemoryBlock manifestData;
    if (! vfs.readFile(ProjectContainerPaths::manifestPath, manifestData))
    {
        errorMessage = "The suite project container does not contain a project manifest.";
        return false;
    }

    const auto manifestText = juce::String::fromUTF8(static_cast<const char*>(manifestData.getData()),
                                                     static_cast<int>(manifestData.getSize()));
    return deserializeManifest(manifestText, outManifest, errorMessage);
}

bool ProjectContainerIO::readEntry(const juce::File& containerFile,
                                   const juce::String& logicalPath,
                                   juce::MemoryBlock& outData,
                                   juce::String& errorMessage)
{
    VirtualFileSystem vfs;
    if (! vfs.mount(containerFile))
    {
        errorMessage = "Could not mount the suite project container.";
        return false;
    }

    if (! vfs.readFile(logicalPath, outData))
    {
        errorMessage = "The requested project container entry was not found.";
        return false;
    }

    return true;
}

juce::StringArray ProjectContainerIO::listEntries(const juce::File& containerFile)
{
    juce::StringArray results;
    if (! containerFile.existsAsFile())
        return results;

    juce::ZipFile zip(containerFile);
    const auto numEntries = zip.getNumEntries();
    for (int index = 0; index < numEntries; ++index)
        if (const auto* entry = zip.getEntry(index))
            results.add(entry->filename);

    return results;
}
}
