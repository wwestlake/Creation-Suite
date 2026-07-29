#include <creation/assets/AssetMaterializer.h>
#include <creation/assets/ProjectContainerIO.h>
#include <creation/assets/AssetResolver.h>
#include <creation/assets/VirtualFileSystem.h>
#include <creation/suite/SuiteStoragePaths.h>

#include <juce_core/juce_core.h>

#include <iostream>

namespace
{
void fail(const juce::String& message)
{
    std::cerr << message << std::endl;
    throw std::runtime_error(message.toStdString());
}
}

int main()
{
    try
    {
        creation::suite::SuiteSettings settings;
        auto tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getChildFile("creation-suite-asset-system-smoke");
        tempRoot.deleteRecursively();
        tempRoot.createDirectory();

        settings.suiteVfsRoot = tempRoot.getChildFile("SuiteRoot").getFullPathName();
        settings.projectContainersRoot = tempRoot.getChildFile("Containers").getFullPathName();
        settings.materializedFilesRoot = tempRoot.getChildFile("Materialized").getFullPathName();
        settings.exportsRoot = tempRoot.getChildFile("Exports").getFullPathName();

        auto manifest = creation::suite::createDefaultManifest("Smoke Project",
                                                               creation::assets::SuiteAppDomain::station,
                                                               "0.1.0",
                                                               "0.1.0");

        creation::assets::AssetDescriptor descriptor;
        descriptor.id = "asset:smoke:test";
        descriptor.versionId = "asset:smoke:test@1";
        descriptor.originalAssetId = "asset:smoke:original";
        descriptor.derivedFromVersionId = "asset:smoke:original@1";
        descriptor.displayName = "Smoke Tone";
        descriptor.kind = creation::assets::AssetKind::audio;
        descriptor.logicalPath = "Assets/Source/smoke-tone.wav";
        descriptor.sourceApp = "Creation Station";
        descriptor.sourceTool = "SmokeTest";
        descriptor.createdAt = juce::Time::getCurrentTime();
        descriptor.modifiedAt = descriptor.createdAt;
        manifest.assetCatalog.assets.add(descriptor);

        creation::assets::ProjectContainerEntry entry;
        entry.logicalPath = "Assets/Source/smoke-tone.wav";
        entry.data.append("TEST-DATA", 9);
        entry.modifiedAt = juce::Time::getCurrentTime();

        const auto containerPath = creation::suite::getProjectContainerPath(settings,
                                                                            creation::assets::SuiteAppDomain::station,
                                                                            manifest.projectName);

        juce::String errorMessage;
        if (! creation::assets::ProjectContainerIO::writeContainer(containerPath, manifest, { entry }, errorMessage))
            fail("writeContainer failed: " + errorMessage);

        creation::assets::ProjectManifest loadedManifest;
        if (! creation::assets::ProjectContainerIO::readManifest(containerPath, loadedManifest, errorMessage))
            fail("readManifest failed: " + errorMessage);

        if (loadedManifest.projectName != manifest.projectName)
            fail("Loaded manifest project name mismatch.");

        if (loadedManifest.assetCatalog.assets.size() != 1)
            fail("Loaded manifest asset catalog size mismatch.");

        creation::assets::AssetRef reference;
        reference.id = descriptor.id;
        reference.versionId = descriptor.versionId;
        reference.mode = creation::assets::AssetReferenceMode::exact;
        reference.logicalPath = descriptor.logicalPath;

        const auto* resolved = creation::assets::AssetResolver::resolve(loadedManifest.assetCatalog, reference);
        if (resolved == nullptr || resolved->versionId != descriptor.versionId)
            fail("AssetResolver failed to resolve the exact version.");

        creation::assets::VirtualFileSystem vfs;
        if (! vfs.mount(containerPath))
            fail("VFS mount failed.");

        juce::MemoryBlock loadedData;
        if (! vfs.readFile("Assets/Source/smoke-tone.wav", loadedData))
            fail("VFS readFile failed.");

        juce::MemoryBlock loadedDataViaContainer;
        if (! creation::assets::ProjectContainerIO::readEntry(containerPath,
                                                              "Assets/Source/smoke-tone.wav",
                                                              loadedDataViaContainer,
                                                              errorMessage))
            fail("ProjectContainerIO readEntry failed: " + errorMessage);

        if (loadedData.toString() != "TEST-DATA")
            fail("Loaded asset payload mismatch.");
        if (loadedDataViaContainer.toString() != "TEST-DATA")
            fail("Container-level loaded asset payload mismatch.");

        const auto entries = vfs.listEntries();
        if (! entries.contains(creation::assets::ProjectContainerPaths::manifestPath)
            || ! entries.contains("Assets/Source/smoke-tone.wav"))
            fail("VFS listEntries missing expected paths.");

        const auto materializedRoot = creation::suite::getMaterializedFilesDirectory(settings, manifest.projectId);
        if (! vfs.materializeEntry("Assets/Source/smoke-tone.wav", materializedRoot, errorMessage))
            fail("materializeEntry failed: " + errorMessage);

        const auto materializedFile = materializedRoot.getChildFile("Assets").getChildFile("Source").getChildFile("smoke-tone.wav");
        if (! materializedFile.existsAsFile())
            fail("Materialized file missing.");

        if (materializedFile.loadFileAsString() != "TEST-DATA")
            fail("Materialized file content mismatch.");

        creation::assets::MaterializedAssetLease lease;
        if (! creation::assets::AssetMaterializer::materializeEntry(settings,
                                                                    containerPath,
                                                                    manifest.projectId,
                                                                    "Assets/Source/smoke-tone.wav",
                                                                    lease,
                                                                    errorMessage))
            fail("AssetMaterializer failed: " + errorMessage);

        if (! lease.isValid() || lease.materializedFile.loadFileAsString() != "TEST-DATA")
            fail("AssetMaterializer lease content mismatch.");

        tempRoot.deleteRecursively();
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "AssetSystemSmoke failure: " << exception.what() << std::endl;
        return 1;
    }
}
