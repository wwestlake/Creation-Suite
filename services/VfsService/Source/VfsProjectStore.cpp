#include "VfsProjectStore.h"

#include <creation/suite/SuiteStoragePaths.h>

using namespace creation::assets;
using namespace creation::suite;

namespace
{
constexpr const char* kProjectsFolder = "Project Containers";
constexpr const char* kSuiteFolder = "suite";
constexpr const char* kPartialSuffix = ".upload-part";
}

VfsProjectStore::VfsProjectStore(const SuiteSettings& settings)
{
    const auto containerFile = getVfsContainerFile(settings);
    if (containerFile == juce::File())
    {
        startupError_ = "No VFS root is chosen, so there is nowhere to keep the container.";
        return;
    }

    if (! containerFile.getParentDirectory().exists() && ! containerFile.getParentDirectory().createDirectory())
    {
        startupError_ = "Could not create the VFS root folder.";
        return;
    }

    if (containerFile.existsAsFile())
    {
        if (! volume.open(containerFile, startupError_))
            startupError_ = "Could not open the VFS container: " + startupError_;
        return;
    }

    if (! volume.createAndFormat(containerFile, kContainerSizeBytes, startupError_))
        startupError_ = "Could not create the VFS container: " + startupError_;
}

juce::String VfsProjectStore::normalizeLogicalPath(const juce::String& logicalPath)
{
    auto normalized = logicalPath.replaceCharacter('\\', '/').trim();
    while (normalized.startsWithChar('/'))
        normalized = normalized.substring(1);
    return normalized;
}

bool VfsProjectStore::isSafePath(const juce::String& normalizedPath)
{
    if (normalizedPath.isEmpty())
        return false;

    juce::StringArray segments;
    segments.addTokens(normalizedPath, "/", "");
    for (const auto& segment : segments)
        if (segment == ".." || segment == ".")
            return false;

    return true;
}

juce::String VfsProjectStore::projectFolderPath(const juce::String& projectId)
{
    return juce::String(kProjectsFolder) + "/" + projectId;
}

bool VfsProjectStore::projectExists(const juce::String& projectId) const
{
    // A project id is a folder name inside the volume: it must be one plain name, nothing that could reach elsewhere.
    if (projectId.isEmpty() || projectId.containsAnyOf("/\\:") || projectId == ".." || projectId == ".")
        return false;

    return volume.directoryExists(projectFolderPath(projectId));
}

juce::String VfsProjectStore::partialUploadPath(const juce::String& entryPath)
{
    return entryPath + kPartialSuffix;
}

bool VfsProjectStore::createProject(SuiteAppDomain appDomain, const juce::String& projectName,
                                    const juce::String& suiteVersion, const juce::String& appVersion,
                                    juce::String& outProjectId, ProjectManifest& outManifest,
                                    juce::String& errorMessage)
{
    outManifest = createDefaultManifest(projectName, appDomain, suiteVersion, appVersion);
    outProjectId = outManifest.projectId;

    if (! volume.createDirectory(projectFolderPath(outProjectId), errorMessage))
        return false;

    return writeManifest(outProjectId, outManifest, errorMessage);
}

bool VfsProjectStore::readManifest(const juce::String& projectId, ProjectManifest& outManifest,
                                   juce::String& errorMessage) const
{
    if (! projectExists(projectId))
    {
        errorMessage = "No project with that id was found.";
        return false;
    }

    juce::MemoryBlock data;
    juce::String readError;
    if (! volume.readFile(projectFolderPath(projectId) + "/" + ProjectContainerPaths::manifestPath, data, readError))
    {
        errorMessage = "The project does not contain a project manifest.";
        return false;
    }

    return deserializeManifest(juce::String::fromUTF8(static_cast<const char*>(data.getData()), (int) data.getSize()),
                               outManifest, errorMessage);
}

bool VfsProjectStore::writeManifest(const juce::String& projectId, const ProjectManifest& manifest,
                                    juce::String& errorMessage)
{
    if (! projectExists(projectId))
    {
        errorMessage = "No project with that id was found.";
        return false;
    }

    const auto text = serializeManifest(manifest, true);
    const juce::MemoryBlock data(text.toRawUTF8(), text.getNumBytesAsUTF8());
    return volume.writeFile(projectFolderPath(projectId) + "/" + ProjectContainerPaths::manifestPath, data, errorMessage);
}

bool VfsProjectStore::listProjects(juce::Array<ProjectSummary>& outProjects) const
{
    outProjects.clear();

    for (const auto& projectId : volume.listDirectories(kProjectsFolder))
    {
        ProjectSummary summary;
        juce::String errorMessage;
        if (! readManifest(projectId, summary.manifest, errorMessage))
            continue; // not a real project (or a corrupt one): skip it, don't fail the whole listing

        summary.projectId = projectId;

        for (const auto& relative : volume.listFilesUnder(projectFolderPath(projectId)))
            summary.totalSizeBytes += juce::jmax<juce::int64>(0, volume.fileSize(projectFolderPath(projectId) + "/" + relative));

        outProjects.add(summary);
    }

    return true;
}

bool VfsProjectStore::cloneProject(const juce::String& sourceProjectId, const juce::String& newProjectName,
                                   juce::String& outNewProjectId, juce::String& errorMessage)
{
    ProjectManifest sourceManifest;
    if (! readManifest(sourceProjectId, sourceManifest, errorMessage))
        return false;

    ProjectManifest newManifest = sourceManifest;
    newManifest.projectId = juce::Uuid().toString();
    newManifest.projectName = newProjectName;
    newManifest.createdAt = juce::Time::getCurrentTime();
    newManifest.modifiedAt = newManifest.createdAt;
    outNewProjectId = newManifest.projectId;

    const auto sourceFolder = projectFolderPath(sourceProjectId);
    const auto destinationFolder = projectFolderPath(outNewProjectId);
    if (! volume.createDirectory(destinationFolder, errorMessage))
        return false;

    for (const auto& relative : volume.listFilesUnder(sourceFolder))
    {
        if (relative.endsWith(kPartialSuffix))
            continue; // an upload that never finished is not part of the project

        juce::String copyError;
        if (! volume.copyFile(sourceFolder + "/" + relative, destinationFolder + "/" + relative, copyError))
        {
            errorMessage = "Could not copy \"" + relative + "\" into the cloned project.";
            return false;
        }
    }

    return writeManifest(outNewProjectId, newManifest, errorMessage);
}

bool VfsProjectStore::deleteProject(const juce::String& projectId, juce::String& errorMessage)
{
    if (! projectExists(projectId))
    {
        errorMessage = "No project with that id was found.";
        return false;
    }

    if (! volume.deleteTree(projectFolderPath(projectId), errorMessage))
    {
        errorMessage = "Could not delete the project.";
        return false;
    }

    return true;
}

bool VfsProjectStore::readEntry(const juce::String& projectId, const juce::String& logicalPath,
                                juce::MemoryBlock& outData) const
{
    const auto normalized = normalizeLogicalPath(logicalPath);
    if (! projectExists(projectId) || ! isSafePath(normalized))
        return false;

    juce::String errorMessage;
    outData.reset();
    return volume.readFile(projectFolderPath(projectId) + "/" + normalized, outData, errorMessage);
}

bool VfsProjectStore::writeEntry(const juce::String& projectId, const juce::String& logicalPath,
                                 const juce::MemoryBlock& data, juce::String& errorMessage)
{
    if (! projectExists(projectId))
    {
        errorMessage = "No project with that id was found.";
        return false;
    }

    const auto normalized = normalizeLogicalPath(logicalPath);
    if (! isSafePath(normalized) || normalized == ProjectContainerPaths::manifestPath)
    {
        errorMessage = "Refusing to write to that logical path.";
        return false;
    }

    return volume.writeFile(projectFolderPath(projectId) + "/" + normalized, data, errorMessage);
}

bool VfsProjectStore::removeEntry(const juce::String& projectId, const juce::String& logicalPath)
{
    const auto normalized = normalizeLogicalPath(logicalPath);
    if (! projectExists(projectId) || ! isSafePath(normalized))
        return false;

    const auto path = projectFolderPath(projectId) + "/" + normalized;
    if (volume.fileSize(path) < 0)
        return false;

    juce::String errorMessage;
    return volume.deleteFile(path, errorMessage);
}

bool VfsProjectStore::writeEntryChunk(const juce::String& projectId, const juce::String& logicalPath,
                                      std::int64_t offset, std::int64_t totalSize,
                                      const void* chunk, size_t chunkSize,
                                      bool& outCompleted, juce::String& errorMessage)
{
    outCompleted = false;

    if (! projectExists(projectId))
    {
        errorMessage = "No project with that id was found.";
        return false;
    }

    const auto normalized = normalizeLogicalPath(logicalPath);
    if (! isSafePath(normalized) || normalized == ProjectContainerPaths::manifestPath)
    {
        errorMessage = "Refusing to write to that logical path.";
        return false;
    }

    if (offset < 0 || totalSize < 0 || offset + (std::int64_t) chunkSize > totalSize)
    {
        errorMessage = "The upload piece does not fit inside the stated total size.";
        return false;
    }

    const auto entryPath = projectFolderPath(projectId) + "/" + normalized;
    const auto part = partialUploadPath(entryPath);
    const auto partSize = volume.fileSize(part);

    if (offset != 0 && partSize != offset)
    {
        errorMessage = "The upload piece is out of order (the service has " + juce::String(juce::jmax<juce::int64>(0, partSize))
                     + " bytes, the piece starts at " + juce::String(offset) + ").";
        return false;
    }

    if (! volume.writeAt(part, offset, chunk, chunkSize, offset == 0, errorMessage))
        return false;

    if (offset + (std::int64_t) chunkSize == totalSize)
    {
        if (volume.fileSize(part) != totalSize)
        {
            errorMessage = "The finished upload is not the size that was announced.";
            return false;
        }

        if (! volume.renameFile(part, entryPath, errorMessage))
            return false;

        outCompleted = true;
    }

    return true;
}

bool VfsProjectStore::readEntryRange(const juce::String& projectId, const juce::String& logicalPath,
                                     std::int64_t offset, std::int64_t length,
                                     juce::MemoryBlock& outData, std::int64_t& outTotalSize) const
{
    outData.reset();
    outTotalSize = 0;

    const auto normalized = normalizeLogicalPath(logicalPath);
    if (! projectExists(projectId) || ! isSafePath(normalized))
        return false;

    juce::String errorMessage;
    return volume.readRange(projectFolderPath(projectId) + "/" + normalized, offset, length, outData, outTotalSize, errorMessage);
}

bool VfsProjectStore::discardEntryUpload(const juce::String& projectId, const juce::String& logicalPath)
{
    const auto normalized = normalizeLogicalPath(logicalPath);
    if (! projectExists(projectId) || ! isSafePath(normalized))
        return false;

    const auto part = partialUploadPath(projectFolderPath(projectId) + "/" + normalized);
    if (volume.fileSize(part) < 0)
        return false;

    juce::String errorMessage;
    return volume.deleteFile(part, errorMessage);
}

juce::StringArray VfsProjectStore::listEntryPaths(const juce::String& projectId) const
{
    juce::StringArray paths;
    if (! projectExists(projectId))
        return paths;

    for (const auto& relative : volume.listFilesUnder(projectFolderPath(projectId)))
        if (relative != ProjectContainerPaths::manifestPath && ! relative.endsWith(kPartialSuffix) && !relative.startsWith(".streams/"))
            paths.add(relative);

    return paths;
}

bool VfsProjectStore::readSuiteEntry(const juce::String& logicalPath, juce::MemoryBlock& outData) const
{
    const auto normalized = normalizeLogicalPath(logicalPath);
    if (! isSafePath(normalized) || ! normalized.startsWith(juce::String(kSuiteFolder) + "/"))
        return false;

    juce::String errorMessage;
    outData.reset();
    return volume.readFile(normalized, outData, errorMessage);
}

bool VfsProjectStore::writeSuiteEntry(const juce::String& logicalPath, const juce::MemoryBlock& data)
{
    const auto normalized = normalizeLogicalPath(logicalPath);
    if (! isSafePath(normalized) || ! normalized.startsWith(juce::String(kSuiteFolder) + "/"))
        return false;

    juce::String errorMessage;
    return volume.writeFile(normalized, data, errorMessage);
}

bool VfsProjectStore::removeSuiteEntry(const juce::String& logicalPath)
{
    const auto normalized = normalizeLogicalPath(logicalPath);
    if (! isSafePath(normalized) || ! normalized.startsWith(juce::String(kSuiteFolder) + "/") || volume.fileSize(normalized) < 0)
        return false;

    juce::String errorMessage;
    return volume.deleteFile(normalized, errorMessage);
}

juce::StringArray VfsProjectStore::listSuiteEntryPaths() const
{
    juce::StringArray paths;
    for (const auto& relative : volume.listFilesUnder(kSuiteFolder))
        paths.add(juce::String(kSuiteFolder) + "/" + relative);
    return paths;
}


bool VfsProjectStore::streamEntry(const juce::String& project,const juce::String& operation,const juce::String& action,
                                 const juce::String& requestedPath,juce::int64 offset,juce::int64 revision,
                                 const void* data,size_t size,juce::var& status,juce::String& error)
{
    auto fail=[&](const juce::String& message){error=message;return false;};
    if(!projectExists(project) || operation.length()<16 || operation.length()>64
       || operation.containsOnly("0123456789abcdefABCDEF-")==false)return fail("Invalid project or stream operation identity.");
    if(size>4*1024*1024 || offset<0 || offset>INT64_MAX-(juce::int64)size)return fail("Stream piece exceeds the bounded write limits.");
    const auto root=projectFolderPath(project),journal=root+"/.streams/"+operation+".json";
    juce::MemoryBlock metadata;juce::var state;
    if(volume.readFile(journal,metadata,error))state=juce::JSON::parse(metadata.toString());
    error={};
    if(!state.isObject() && volume.readFile(journal+".next",metadata,error))state=juce::JSON::parse(metadata.toString());
    if(!state.isObject())
    {
        if(action!="begin")return fail("Stream operation not found; no data was discarded.");
        const auto path=normalizeLogicalPath(requestedPath);
        if(!isSafePath(path) || (path==ProjectContainerPaths::manifestPath || path.startsWith(".streams/") || path.endsWith(kPartialSuffix)) || volume.fileExists(root+"/"+path))return fail("Stream destination must be a new safe project entry.");
        auto* record=new juce::DynamicObject;state=juce::var(record);
        record->setProperty("operationId",operation);record->setProperty("path",path);record->setProperty("state","writing");
        record->setProperty("size",(juce::int64)0);record->setProperty("revision",(juce::int64)0);
        const auto part=root+"/"+path+"."+operation+".upload-part";
        if(!volume.writeAt(part,0,nullptr,0,true,error))return false;
    }
    auto* record=state.getDynamicObject();const auto path=state["path"].toString();
    const auto finalPath=root+"/"+path,part=finalPath+"."+operation+".upload-part";
    const auto current=(juce::int64)state["revision"],length=(juce::int64)state["size"];
    if(action=="begin" && normalizeLogicalPath(requestedPath)!=path)return fail("Operation identity already belongs to another destination.");
    if(action=="inspect"){status=state;return true;}
    if(action=="write")
    {
        if(state["state"].toString()!="writing")return fail("Stream is already finalized; writes are rejected.");
        if(revision!=current)
        {
            // Exact retry of the last acknowledged write is safe and does not advance revision.
            juce::MemoryBlock previous;juce::int64 total=0;
            if(revision==current-1 && offset==(juce::int64)state["lastOffset"] && (juce::int64)size==(juce::int64)state["lastSize"]
               && volume.readRange(part,offset,(juce::int64)size,previous,total,error) && previous.getSize()==size
               && (size==0 || std::memcmp(previous.getData(),data,size)==0)){status=state;return true;}
            return fail("Stale/out-of-order stream write; inspect the acknowledged revision before retrying.");
        }
        if(offset>length)return fail("Sparse/out-of-order stream writes are not accepted.");
        if(!volume.writeAt(part,offset,data,size,false,error))return false;
        record->setProperty("size",std::max(length,offset+(juce::int64)size));record->setProperty("revision",current+1);
        record->setProperty("lastOffset",offset);record->setProperty("lastSize",(juce::int64)size);
    }
    else if(action=="finish")
    {
        if(state["state"].toString()=="writing")
        {
            if(revision!=current)return fail("Stale finalization revision; inspect acknowledged stream progress first.");
            if(volume.fileSize(part)>=0)
            {if(volume.fileExists(finalPath))return fail("Refusing to overwrite an existing recording.");if(!volume.renameFile(part,finalPath,error))return false;}
            if(volume.fileSize(finalPath)!=length)return fail("Final media size differs from acknowledged stream size; retained for recovery.");
            record->setProperty("state","completed");
        }
    }
    else if(action!="begin")return fail("Unknown streaming operation. Partial bytes have been retained.");
    const auto json=juce::JSON::toString(state);juce::MemoryBlock bytes(json.toRawUTF8(),json.getNumBytesAsUTF8());
    if(!volume.writeFile(journal+".next",bytes,error) || !volume.renameFile(journal+".next",journal,error))return false;
    status=state;return true;
}
