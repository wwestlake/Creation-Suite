#include "creation/assets/ProjectAssetService.h"

namespace
{
juce::String slugifyFileName(const juce::String& value)
{
    juce::String slug;
    for (auto ch : value)
    {
        if (juce::CharacterFunctions::isLetterOrDigit(ch))
            slug << juce::CharacterFunctions::toLowerCase(ch);
        else if (ch == ' ' || ch == '-' || ch == '_')
            slug << '-';
    }

    while (slug.contains("--"))
        slug = slug.replace("--", "-");

    return slug.trimCharactersAtStart("-").trimCharactersAtEnd("-");
}

juce::String buildDefaultLogicalPath(const juce::File& sourceFile, const creation::assets::ProjectAssetService::ImportOptions& options)
{
    const auto root = options.derivedFromVersionId.isNotEmpty()
                          ? juce::String(creation::assets::ProjectContainerPaths::derivedAssetRoot)
                          : juce::String(creation::assets::ProjectContainerPaths::sourceAssetRoot);

    auto baseName = slugifyFileName(sourceFile.getFileNameWithoutExtension());
    if (baseName.isEmpty())
        baseName = juce::Uuid().toString().replaceCharacter('-', '_');

    return root + baseName + sourceFile.getFileExtension();
}

juce::String nextVersionToken(const juce::String& version, int fallbackRevision)
{
    const auto parsed = version.getIntValue();
    if (parsed > 0)
        return juce::String(parsed + 1);
    return juce::String(juce::jmax(1, fallbackRevision + 1));
}

juce::String versionedLogicalPath(const juce::String& logicalPath, const juce::String& version)
{
    const auto extension = juce::File(logicalPath).getFileExtension();
    if (extension.isEmpty())
        return logicalPath + "@" + version;

    return logicalPath.dropLastCharacters(extension.length()) + "@" + version + extension;
}
}

namespace creation::assets
{
bool ProjectAssetService::importFile(ProjectSession& session,
                                     const juce::File& sourceFile,
                                     const ImportOptions& options,
                                     AssetDescriptor& outDescriptor,
                                     juce::String& errorMessage)
{
    if (! sourceFile.existsAsFile())
    {
        errorMessage = "The source asset file does not exist.";
        return false;
    }

    auto descriptor = outDescriptor;
    if (descriptor.id.isEmpty())
        descriptor.id = "asset:" + juce::Uuid().toString();
    if (descriptor.version.isEmpty())
        descriptor.version = "1";
    if (descriptor.versionId.isEmpty())
        descriptor.versionId = descriptor.id + "@1";

    descriptor.kind = options.kind;
    descriptor.displayName = options.displayName.isNotEmpty() ? options.displayName : sourceFile.getFileNameWithoutExtension();
    descriptor.logicalPath = options.logicalPath.isNotEmpty() ? options.logicalPath : buildDefaultLogicalPath(sourceFile, options);
    descriptor.category = options.category;
    descriptor.description = options.description;
    descriptor.mediaType = options.mediaType;
    descriptor.sourceApp = options.sourceApp;
    descriptor.sourceTool = options.sourceTool;
    descriptor.sourceAssetId = options.sourceAssetId;
    descriptor.sourceVersionId = options.sourceVersionId;
    descriptor.importerId = options.importerId;
    descriptor.importerVersion = options.importerVersion;
    descriptor.importSettings = options.importSettings;
    descriptor.tags = options.tags;
    descriptor.originalAssetId = options.originalAssetId;
    descriptor.derivedFromVersionId = options.derivedFromVersionId;
    descriptor.fileSizeBytes = sourceFile.getSize();
    descriptor.modifiedAt = sourceFile.getLastModificationTime();
    if (descriptor.createdAt == juce::Time())
        descriptor.createdAt = descriptor.modifiedAt;
    ++descriptor.revision;

    if (! session.writeEntryFromFile(descriptor.logicalPath, sourceFile, errorMessage, options.compressionLevel))
        return false;

    session.upsertAssetDescriptor(descriptor);
    outDescriptor = descriptor;
    return true;
}

bool ProjectAssetService::saveGeneratedAsset(ProjectSession& session,
                                             const juce::MemoryBlock& data,
                                             const ImportOptions& options,
                                             AssetDescriptor& outDescriptor,
                                             juce::String& errorMessage)
{
    if (options.logicalPath.isEmpty())
    {
        errorMessage = "saveGeneratedAsset requires an explicit logicalPath.";
        return false;
    }

    AssetDescriptor descriptor;
    for (const auto& existing : session.getManifest().assetCatalog.query({}))
    {
        if (existing.logicalPath == options.logicalPath)
        {
            descriptor = existing;
            break;
        }
    }

    if (descriptor.id.isEmpty())
    {
        descriptor.id = "asset:" + juce::Uuid().toString();
        descriptor.version = "1";
    }
    descriptor.versionId = descriptor.id + "@" + descriptor.version;

    descriptor.kind = options.kind;
    descriptor.displayName = options.displayName.isNotEmpty() ? options.displayName : descriptor.displayName;
    descriptor.logicalPath = options.logicalPath;
    descriptor.category = options.category;
    descriptor.description = options.description;
    descriptor.mediaType = options.mediaType;
    descriptor.sourceApp = options.sourceApp;
    descriptor.sourceTool = options.sourceTool;
    descriptor.sourceAssetId = options.sourceAssetId;
    descriptor.sourceVersionId = options.sourceVersionId;
    descriptor.importerId = options.importerId;
    descriptor.importerVersion = options.importerVersion;
    descriptor.importSettings = options.importSettings;
    descriptor.tags = options.tags;
    descriptor.originalAssetId = options.originalAssetId;
    descriptor.derivedFromVersionId = options.derivedFromVersionId;
    descriptor.fileSizeBytes = static_cast<int64_t>(data.getSize());
    descriptor.modifiedAt = juce::Time::getCurrentTime();
    if (descriptor.createdAt == juce::Time())
        descriptor.createdAt = descriptor.modifiedAt;
    descriptor.revision = juce::jmax(1, descriptor.revision + 1);

    if (! session.writeEntry(options.logicalPath, data, descriptor.modifiedAt, options.compressionLevel))
    {
        errorMessage = "Could not write the generated asset into the project.";
        return false;
    }

    session.upsertAssetDescriptor(descriptor);
    outDescriptor = descriptor;
    return true;
}

bool ProjectAssetService::createNewVersion(ProjectSession& session,
                                           const AssetDescriptor& existingAsset,
                                           const juce::File& sourceFile,
                                           const ImportOptions& overrides,
                                           AssetDescriptor& outDescriptor,
                                           juce::String& errorMessage)
{
    AssetDescriptor descriptor = existingAsset;
    descriptor.version = nextVersionToken(existingAsset.version, existingAsset.revision);
    descriptor.versionId = existingAsset.id + "@" + descriptor.version;
    descriptor.displayName = overrides.displayName.isNotEmpty() ? overrides.displayName : existingAsset.displayName;
    descriptor.logicalPath = overrides.logicalPath.isNotEmpty()
                                 ? overrides.logicalPath
                                 : versionedLogicalPath(existingAsset.logicalPath, descriptor.version);
    descriptor.category = overrides.category.isNotEmpty() ? overrides.category : existingAsset.category;
    descriptor.description = overrides.description.isNotEmpty() ? overrides.description : existingAsset.description;
    descriptor.mediaType = overrides.mediaType.isNotEmpty() ? overrides.mediaType : existingAsset.mediaType;
    descriptor.sourceApp = overrides.sourceApp.isNotEmpty() ? overrides.sourceApp : existingAsset.sourceApp;
    descriptor.sourceTool = overrides.sourceTool.isNotEmpty() ? overrides.sourceTool : existingAsset.sourceTool;
    descriptor.sourceAssetId = overrides.sourceAssetId.isNotEmpty() ? overrides.sourceAssetId : existingAsset.sourceAssetId;
    descriptor.sourceVersionId = overrides.sourceVersionId.isNotEmpty() ? overrides.sourceVersionId : existingAsset.sourceVersionId;
    descriptor.importerId = overrides.importerId.isNotEmpty() ? overrides.importerId : existingAsset.importerId;
    descriptor.importerVersion = overrides.importerVersion.isNotEmpty() ? overrides.importerVersion : existingAsset.importerVersion;
    descriptor.importSettings = overrides.importSettings.isNotEmpty() ? overrides.importSettings : existingAsset.importSettings;
    descriptor.tags = overrides.tags.isEmpty() ? existingAsset.tags : overrides.tags;
    descriptor.kind = overrides.kind != AssetKind::unknown ? overrides.kind : existingAsset.kind;
    descriptor.originalAssetId = existingAsset.originalAssetId.isNotEmpty() ? existingAsset.originalAssetId : existingAsset.id;
    descriptor.derivedFromVersionId = existingAsset.versionId;
    descriptor.revision = existingAsset.revision + 1;
    descriptor.modifiedAt = sourceFile.getLastModificationTime();
    descriptor.fileSizeBytes = sourceFile.getSize();

    if (! session.writeEntryFromFile(descriptor.logicalPath, sourceFile, errorMessage, overrides.compressionLevel))
        return false;

    session.upsertAssetDescriptor(descriptor);
    outDescriptor = descriptor;
    return true;
}

const AssetDescriptor* ProjectAssetService::resolveAsset(const ProjectSession& session,
                                                         const AssetRef& reference) noexcept
{
    return AssetResolver::resolve(session.getManifest().assetCatalog, reference);
}

bool ProjectAssetService::materializeAsset(const ProjectSession& session,
                                           const creation::suite::SuiteSettings& settings,
                                           const AssetRef& reference,
                                           MaterializationAccess access,
                                           MaterializedAssetLease& outLease,
                                           juce::String& errorMessage)
{
    const auto* descriptor = resolveAsset(session, reference);
    if (descriptor == nullptr)
    {
        errorMessage = "The requested asset reference could not be resolved in this project container.";
        return false;
    }

    return session.materializeEntry(settings, descriptor->logicalPath, access, outLease, errorMessage);
}

bool ProjectAssetService::reconcileAsset(ProjectSession& session,
                                         const creation::suite::SuiteSettings& settings,
                                         MaterializedAssetLease& lease,
                                         juce::String& errorMessage,
                                         bool releaseLeaseAfterCommit)
{
    return session.reconcileMaterializedEntry(settings, lease, errorMessage, releaseLeaseAfterCommit);
}
}
