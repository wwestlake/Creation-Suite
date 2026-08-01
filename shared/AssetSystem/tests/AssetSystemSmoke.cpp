#include <creation/assets/AssetMaterializer.h>
#include <creation/assets/ProjectAssetService.h>
#include <creation/assets/ProjectContainerIO.h>
#include <creation/assets/ProjectContainerService.h>
#include <creation/assets/ProjectSession.h>
#include <creation/assets/ProjectWorkspaceService.h>
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

        creation::assets::ProjectSession session;
        if (! creation::assets::ProjectSession::open(containerPath, session, errorMessage))
            fail("ProjectSession open failed: " + errorMessage);

        if (! session.isValid() || session.getManifest().projectId != manifest.projectId)
            fail("ProjectSession did not load the expected manifest.");

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
        if (! session.materializeEntry(settings,
                                       "Assets/Source/smoke-tone.wav",
                                       creation::assets::MaterializationAccess::writable,
                                       lease,
                                       errorMessage))
            fail("ProjectSession materializeEntry failed: " + errorMessage);

        if (! lease.isValid() || lease.materializedFile.loadFileAsString() != "TEST-DATA")
            fail("AssetMaterializer lease content mismatch.");
        if (lease.leaseId.isEmpty() || lease.leaseRoot == juce::File())
            fail("Materialized lease session metadata was not populated.");

        if (! lease.materializedFile.replaceWithText("UPDATED-DATA"))
            fail("Could not edit the materialized lease file.");

        auto updatedDescriptor = descriptor;
        updatedDescriptor.versionId = "asset:smoke:test@2";
        updatedDescriptor.version = "2";
        updatedDescriptor.revision = 1;
        updatedDescriptor.modifiedAt = juce::Time::getCurrentTime();
        session.upsertAssetDescriptor(updatedDescriptor);
        if (! session.reconcileMaterializedEntry(settings, lease, errorMessage))
            fail("ProjectSession reconcileMaterializedEntry failed: " + errorMessage);

        if (lease.isValid())
            fail("Materialized lease should have been released after reconcile.");

        juce::MemoryBlock updatedData;
        if (! creation::assets::ProjectContainerIO::readEntry(containerPath,
                                                              "Assets/Source/smoke-tone.wav",
                                                              updatedData,
                                                              errorMessage))
            fail("Container readEntry after reconcile failed: " + errorMessage);

        if (updatedData.toString() != "UPDATED-DATA")
            fail("Reconciled asset payload mismatch.");

        creation::assets::MaterializedAssetLease readOnlyLease;
        if (! session.materializeEntry(settings,
                                       "Assets/Source/smoke-tone.wav",
                                       creation::assets::MaterializationAccess::readOnly,
                                       readOnlyLease,
                                       errorMessage))
            fail("ProjectSession read-only materializeEntry failed: " + errorMessage);

        if (readOnlyLease.access != creation::assets::MaterializationAccess::readOnly)
            fail("Read-only materialized lease did not preserve access mode.");

        if (session.reconcileMaterializedEntry(settings, readOnlyLease, errorMessage, false))
            fail("Read-only materialized lease should not reconcile.");

        if (! errorMessage.containsIgnoreCase("read-only"))
            fail("Read-only reconcile did not report the expected error.");

        if (! creation::assets::AssetMaterializer::releaseLease(readOnlyLease, errorMessage))
            fail("Could not release read-only materialized lease.");

        creation::assets::ProjectSession newSession;
        if (! creation::assets::ProjectSession::createNew(settings,
                                                          creation::assets::SuiteAppDomain::texture,
                                                          "Session Created Project",
                                                          "0.1.0",
                                                          "0.1.0",
                                                          newSession,
                                                          errorMessage))
            fail("ProjectSession createNew failed: " + errorMessage);

        if (! newSession.writeEntry("Assets/Derived/generated.txt", juce::MemoryBlock("DERIVED", 7)))
            fail("ProjectSession writeEntry failed for new session.");

        if (! newSession.commit(errorMessage))
            fail("ProjectSession commit failed for new session: " + errorMessage);

        creation::assets::ProjectManifest createdManifest;
        if (! creation::assets::ProjectContainerIO::readManifest(newSession.getContainerFile(), createdManifest, errorMessage))
            fail("Created session container manifest read failed: " + errorMessage);

        if (createdManifest.appDomain != creation::assets::SuiteAppDomain::texture)
            fail("Created session container domain mismatch.");

        creation::assets::ProjectSession serviceSession;
        if (! creation::assets::ProjectContainerService::createProject(settings,
                                                                       creation::assets::SuiteAppDomain::modeler,
                                                                       "Service Created Project",
                                                                       "0.1.0",
                                                                       "0.1.0",
                                                                       serviceSession,
                                                                       errorMessage))
            fail("ProjectContainerService createProject failed: " + errorMessage);

        juce::String listError;
        const auto modelerProjects = creation::assets::ProjectContainerService::listProjects(settings,
                                                                                             creation::assets::SuiteAppDomain::modeler,
                                                                                             listError);
        if (modelerProjects.isEmpty())
            fail("ProjectContainerService listProjects did not find the created project.");

        creation::assets::ProjectContainerService::ProjectSummary foundProject;
        if (! creation::assets::ProjectContainerService::findProjectById(settings,
                                                                         serviceSession.getManifest().projectId,
                                                                         foundProject,
                                                                         errorMessage))
            fail("ProjectContainerService findProjectById failed: " + errorMessage);

        if (foundProject.manifest.projectName != "Service Created Project")
            fail("ProjectContainerService returned the wrong project summary.");

        creation::assets::ProjectSession staleSessionA;
        creation::assets::ProjectSession staleSessionB;
        if (! creation::assets::ProjectSession::open(containerPath, staleSessionA, errorMessage))
            fail("Could not open staleSessionA: " + errorMessage);
        if (! creation::assets::ProjectSession::open(containerPath, staleSessionB, errorMessage))
            fail("Could not open staleSessionB: " + errorMessage);

        if (! staleSessionA.writeEntry("Metadata/notes-a.txt", juce::MemoryBlock("A", 1)))
            fail("Could not stage staleSessionA changes.");
        if (! staleSessionA.commit(errorMessage))
            fail("Could not commit staleSessionA changes: " + errorMessage);

        if (! staleSessionB.writeEntry("Metadata/notes-b.txt", juce::MemoryBlock("B", 1)))
            fail("Could not stage staleSessionB changes.");
        if (staleSessionB.commit(errorMessage))
            fail("staleSessionB should have detected an external container change.");
        if (! errorMessage.containsIgnoreCase("changed on disk"))
            fail("staleSessionB did not report the expected stale-session error.");

        if (! staleSessionB.reloadFromDisk(errorMessage))
            fail("Could not reload staleSessionB from disk: " + errorMessage);
        if (! staleSessionB.writeEntry("Metadata/notes-b.txt", juce::MemoryBlock("B", 1)))
            fail("Could not restage staleSessionB changes after reload.");
        if (! staleSessionB.commit(errorMessage))
            fail("Could not commit staleSessionB after reload: " + errorMessage);

        const auto externalImportFile = tempRoot.getChildFile("external-import.txt");
        if (! externalImportFile.replaceWithText("IMPORTED-ASSET"))
            fail("Could not create the external import file.");

        creation::assets::ProjectAssetService::ImportOptions importOptions;
        importOptions.kind = creation::assets::AssetKind::metadata;
        importOptions.displayName = "Imported Notes";
        importOptions.sourceApp = "Creation Suite Smoke";
        importOptions.sourceTool = "AssetSystemSmoke";
        importOptions.tags.add("smoke");

        creation::assets::AssetDescriptor importedDescriptor;
        if (! creation::assets::ProjectAssetService::importFile(staleSessionB,
                                                                externalImportFile,
                                                                importOptions,
                                                                importedDescriptor,
                                                                errorMessage))
            fail("ProjectAssetService importFile failed: " + errorMessage);
        if (! staleSessionB.commit(errorMessage))
            fail("Could not commit imported asset changes: " + errorMessage);

        creation::assets::AssetRef importedRef;
        importedRef.id = importedDescriptor.id;
        importedRef.versionId = importedDescriptor.versionId;
        importedRef.mode = creation::assets::AssetReferenceMode::exact;
        importedRef.logicalPath = importedDescriptor.logicalPath;

        auto importedSession = staleSessionB;
        const auto* resolvedImportedAsset = creation::assets::ProjectAssetService::resolveAsset(importedSession, importedRef);
        if (resolvedImportedAsset == nullptr || resolvedImportedAsset->displayName != "Imported Notes")
            fail("ProjectAssetService resolveAsset failed.");

        creation::assets::MaterializedAssetLease importedLease;
        if (! creation::assets::ProjectAssetService::materializeAsset(importedSession,
                                                                      settings,
                                                                      importedRef,
                                                                      creation::assets::MaterializationAccess::readOnly,
                                                                      importedLease,
                                                                      errorMessage))
            fail("ProjectAssetService materializeAsset failed: " + errorMessage);
        if (importedLease.materializedFile.loadFileAsString() != "IMPORTED-ASSET")
            fail("ProjectAssetService materialized asset content mismatch.");
        if (! creation::assets::AssetMaterializer::releaseLease(importedLease, errorMessage))
            fail("Could not release ProjectAssetService materialized asset lease.");

        const auto externalImportFileV2 = tempRoot.getChildFile("external-import-v2.txt");
        if (! externalImportFileV2.replaceWithText("IMPORTED-ASSET-V2"))
            fail("Could not create the v2 external import file.");

        creation::assets::AssetDescriptor importedDescriptorV2;
        if (! creation::assets::ProjectAssetService::createNewVersion(staleSessionB,
                                                                      importedDescriptor,
                                                                      externalImportFileV2,
                                                                      importOptions,
                                                                      importedDescriptorV2,
                                                                      errorMessage))
            fail("ProjectAssetService createNewVersion failed: " + errorMessage);
        if (! staleSessionB.commit(errorMessage))
            fail("Could not commit versioned imported asset changes: " + errorMessage);
        if (importedDescriptorV2.id != importedDescriptor.id)
            fail("Versioned asset should keep the same asset ID.");
        if (importedDescriptorV2.versionId == importedDescriptor.versionId)
            fail("Versioned asset should get a new version ID.");
        if (importedDescriptorV2.derivedFromVersionId != importedDescriptor.versionId)
            fail("Versioned asset should point back to the previous version.");

        creation::assets::ProjectSession workspaceSession;
        if (! creation::assets::ProjectWorkspaceService::openProjectById(settings,
                                                                         staleSessionB.getManifest().projectId,
                                                                         workspaceSession,
                                                                         errorMessage))
            fail("ProjectWorkspaceService openProjectById failed: " + errorMessage);

        const auto workspaceImportFile = tempRoot.getChildFile("workspace-import.txt");
        if (! workspaceImportFile.replaceWithText("WORKSPACE-ASSET"))
            fail("Could not create the workspace import file.");

        creation::assets::ProjectAssetService::ImportOptions workspaceImportOptions;
        workspaceImportOptions.kind = creation::assets::AssetKind::metadata;
        workspaceImportOptions.displayName = "Workspace Imported Asset";
        workspaceImportOptions.sourceApp = "Creation Workspace";
        workspaceImportOptions.sourceTool = "WorkspaceService";

        creation::assets::AssetDescriptor workspaceDescriptor;
        if (! creation::assets::ProjectWorkspaceService::importFile(workspaceSession,
                                                                    workspaceImportFile,
                                                                    workspaceImportOptions,
                                                                    workspaceDescriptor,
                                                                    errorMessage))
            fail("ProjectWorkspaceService importFile failed: " + errorMessage);

        creation::assets::AssetRef workspaceRef;
        workspaceRef.id = workspaceDescriptor.id;
        workspaceRef.versionId = workspaceDescriptor.versionId;
        workspaceRef.mode = creation::assets::AssetReferenceMode::exact;

        creation::assets::MaterializedAssetLease workspaceLease;
        if (! creation::assets::ProjectWorkspaceService::materializeAsset(workspaceSession,
                                                                          settings,
                                                                          workspaceRef,
                                                                          creation::assets::MaterializationAccess::readOnly,
                                                                          workspaceLease,
                                                                          errorMessage))
            fail("ProjectWorkspaceService materializeAsset failed: " + errorMessage);
        if (workspaceLease.materializedFile.loadFileAsString() != "WORKSPACE-ASSET")
            fail("ProjectWorkspaceService materialized asset content mismatch.");
        if (! creation::assets::AssetMaterializer::releaseLease(workspaceLease, errorMessage))
            fail("Could not release ProjectWorkspaceService asset lease.");

        tempRoot.deleteRecursively();
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "AssetSystemSmoke failure: " << exception.what() << std::endl;
        return 1;
    }
}
