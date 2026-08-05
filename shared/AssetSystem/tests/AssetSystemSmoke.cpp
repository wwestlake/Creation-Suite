#include <creation/assets/AssetMaterializer.h>
#include <creation/assets/ProjectAssetService.h>
#include <creation/assets/ProjectContainerService.h>
#include <creation/assets/ProjectSession.h>
#include <creation/assets/ProjectWorkspaceService.h>
#include <creation/assets/AssetResolver.h>
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

        juce::String errorMessage;

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

        juce::File containerPath;
        juce::String projectId;

        // --- create a project, write an asset, commit, close --------------
        {
            creation::assets::ProjectSession session;
            if (! creation::assets::ProjectSession::createNew(settings,
                                                              creation::assets::SuiteAppDomain::station,
                                                              "Smoke Project",
                                                              "0.1.0",
                                                              "0.1.0",
                                                              session,
                                                              errorMessage))
                fail("ProjectSession createNew failed: " + errorMessage);

            if (! session.isValid())
                fail("Newly created project session should be valid immediately.");

            containerPath = session.getContainerFile();
            projectId = session.getManifest().projectId;
            if (! containerPath.existsAsFile())
                fail("createNew should have created a real container file on disk.");

            if (! session.writeEntry(descriptor.logicalPath, juce::MemoryBlock("TEST-DATA", 9)))
                fail("ProjectSession writeEntry failed.");
            session.upsertAssetDescriptor(descriptor);

            // Asset writes are immediately durable on the mounted volume,
            // even before commit -- confirm the write is really there.
            juce::MemoryBlock readBack;
            if (! session.readEntry(descriptor.logicalPath, readBack) || readBack.toString() != "TEST-DATA")
                fail("ProjectSession readEntry did not return the written asset data.");
            if (! session.containsEntry(descriptor.logicalPath))
                fail("ProjectSession containsEntry should report true for a written asset.");

            if (! session.commit(errorMessage))
                fail("ProjectSession commit failed: " + errorMessage);

            session.close();
        }

        // --- reopen a fresh session: proves the manifest and the asset
        //     both really persisted to the container file, not just memory
        {
            creation::assets::ProjectSession reopened;
            if (! creation::assets::ProjectSession::open(containerPath, reopened, errorMessage))
                fail("ProjectSession open (reopen) failed: " + errorMessage);

            if (! reopened.isValid() || reopened.getManifest().projectId != projectId)
                fail("Reopened ProjectSession did not load the expected manifest.");

            juce::MemoryBlock reopenedData;
            if (! reopened.readEntry(descriptor.logicalPath, reopenedData) || reopenedData.toString() != "TEST-DATA")
                fail("Reopened ProjectSession did not read back the expected asset data.");
            if (reopened.getManifest().assetCatalog.assets.size() != 1)
                fail("Reopened manifest asset catalog size mismatch.");

            creation::assets::AssetRef reference;
            reference.id = descriptor.id;
            reference.versionId = descriptor.versionId;
            reference.mode = creation::assets::AssetReferenceMode::exact;
            reference.logicalPath = descriptor.logicalPath;

            const auto* resolved = creation::assets::AssetResolver::resolve(reopened.getManifest().assetCatalog, reference);
            if (resolved == nullptr || resolved->versionId != descriptor.versionId)
                fail("AssetResolver failed to resolve the exact version.");

            if (reopened.removeEntry("Assets/Source/does-not-exist.wav"))
                fail("removeEntry should fail cleanly for a path that was never written.");

            // --- materialize / edit / reconcile round trip ------------------
            creation::assets::MaterializedAssetLease lease;
            if (! reopened.materializeEntry(settings,
                                            descriptor.logicalPath,
                                            creation::assets::MaterializationAccess::writable,
                                            lease,
                                            errorMessage))
                fail("ProjectSession materializeEntry failed: " + errorMessage);

            if (! lease.isValid() || lease.materializedFile.loadFileAsString() != "TEST-DATA")
                fail("Materialized lease content mismatch.");
            if (lease.leaseId.isEmpty() || lease.leaseRoot == juce::File())
                fail("Materialized lease session metadata was not populated.");

            if (! lease.materializedFile.replaceWithText("UPDATED-DATA"))
                fail("Could not edit the materialized lease file.");

            auto updatedDescriptor = descriptor;
            updatedDescriptor.versionId = "asset:smoke:test@2";
            updatedDescriptor.version = "2";
            updatedDescriptor.revision = 1;
            updatedDescriptor.modifiedAt = juce::Time::getCurrentTime();
            reopened.upsertAssetDescriptor(updatedDescriptor);
            if (! reopened.reconcileMaterializedEntry(settings, lease, errorMessage))
                fail("ProjectSession reconcileMaterializedEntry failed: " + errorMessage);

            if (lease.isValid())
                fail("Materialized lease should have been released after reconcile.");

            juce::MemoryBlock updatedData;
            if (! reopened.readEntry(descriptor.logicalPath, updatedData) || updatedData.toString() != "UPDATED-DATA")
                fail("Reconciled asset payload mismatch.");

            creation::assets::MaterializedAssetLease readOnlyLease;
            if (! reopened.materializeEntry(settings,
                                            descriptor.logicalPath,
                                            creation::assets::MaterializationAccess::readOnly,
                                            readOnlyLease,
                                            errorMessage))
                fail("ProjectSession read-only materializeEntry failed: " + errorMessage);

            if (readOnlyLease.access != creation::assets::MaterializationAccess::readOnly)
                fail("Read-only materialized lease did not preserve access mode.");

            if (reopened.reconcileMaterializedEntry(settings, readOnlyLease, errorMessage, false))
                fail("Read-only materialized lease should not reconcile.");
            if (! errorMessage.containsIgnoreCase("read-only"))
                fail("Read-only reconcile did not report the expected error.");
            if (! creation::assets::AssetMaterializer::releaseLease(readOnlyLease, errorMessage))
                fail("Could not release read-only materialized lease.");

            // --- ProjectAssetService import / version / resolve ------------
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
            if (! creation::assets::ProjectAssetService::importFile(reopened,
                                                                    externalImportFile,
                                                                    importOptions,
                                                                    importedDescriptor,
                                                                    errorMessage))
                fail("ProjectAssetService importFile failed: " + errorMessage);
            if (! reopened.commit(errorMessage))
                fail("Could not commit imported asset changes: " + errorMessage);

            creation::assets::AssetRef importedRef;
            importedRef.id = importedDescriptor.id;
            importedRef.versionId = importedDescriptor.versionId;
            importedRef.mode = creation::assets::AssetReferenceMode::exact;
            importedRef.logicalPath = importedDescriptor.logicalPath;

            const auto* resolvedImportedAsset = creation::assets::ProjectAssetService::resolveAsset(reopened, importedRef);
            if (resolvedImportedAsset == nullptr || resolvedImportedAsset->displayName != "Imported Notes")
                fail("ProjectAssetService resolveAsset failed.");

            creation::assets::MaterializedAssetLease importedLease;
            if (! creation::assets::ProjectAssetService::materializeAsset(reopened,
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
            if (! creation::assets::ProjectAssetService::createNewVersion(reopened,
                                                                          importedDescriptor,
                                                                          externalImportFileV2,
                                                                          importOptions,
                                                                          importedDescriptorV2,
                                                                          errorMessage))
                fail("ProjectAssetService createNewVersion failed: " + errorMessage);
            if (! reopened.commit(errorMessage))
                fail("Could not commit versioned imported asset changes: " + errorMessage);
            if (importedDescriptorV2.id != importedDescriptor.id)
                fail("Versioned asset should keep the same asset ID.");
            if (importedDescriptorV2.versionId == importedDescriptor.versionId)
                fail("Versioned asset should get a new version ID.");
            if (importedDescriptorV2.derivedFromVersionId != importedDescriptor.versionId)
                fail("Versioned asset should point back to the previous version.");

            reopened.close();
        }

        // --- ProjectContainerService / ProjectWorkspaceService -------------
        {
            creation::assets::ProjectSession serviceSession;
            if (! creation::assets::ProjectContainerService::createProject(settings,
                                                                           creation::assets::SuiteAppDomain::modeler,
                                                                           "Service Created Project",
                                                                           "0.1.0",
                                                                           "0.1.0",
                                                                           serviceSession,
                                                                           errorMessage))
                fail("ProjectContainerService createProject failed: " + errorMessage);

            const auto serviceProjectId = serviceSession.getManifest().projectId;
            serviceSession.close();

            // listProjects/findProjectById peek each container's manifest via
            // its own short-lived mount -- they can only see containers that
            // are not currently held open elsewhere (VFS-M4's single-owner
            // rule), so the session above must be closed before scanning.
            juce::String listError;
            const auto modelerProjects = creation::assets::ProjectContainerService::listProjects(settings,
                                                                                                 creation::assets::SuiteAppDomain::modeler,
                                                                                                 listError);
            if (modelerProjects.isEmpty())
                fail("ProjectContainerService listProjects did not find the created project.");

            creation::assets::ProjectContainerService::ProjectSummary foundProject;
            if (! creation::assets::ProjectContainerService::findProjectById(settings,
                                                                             serviceProjectId,
                                                                             foundProject,
                                                                             errorMessage))
                fail("ProjectContainerService findProjectById failed: " + errorMessage);

            if (foundProject.manifest.projectName != "Service Created Project")
                fail("ProjectContainerService returned the wrong project summary.");
        }

        creation::assets::ProjectSession workspaceSession;
        if (! creation::assets::ProjectWorkspaceService::openProjectById(settings,
                                                                         projectId,
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

        workspaceSession.close();

        std::cout << "AssetSystemSmoke: all checks passed." << std::endl;

        tempRoot.deleteRecursively();
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "AssetSystemSmoke failure: " << exception.what() << std::endl;
        return 1;
    }
}
