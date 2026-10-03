#include <creation/services/SuiteVfsJsonStore.h>
#include <creation/services/SuiteVfsServiceClient.h>

namespace creation::services
{
namespace
{
juce::String& testScope()
{
    static juce::String scope;
    return scope;
}

juce::String scoped(const juce::String& logicalPath)
{
    return testScope().isEmpty() ? logicalPath : "tests/" + testScope() + "/" + logicalPath;
}
}

void SuiteVfsJsonStore::setScopeForTesting(const juce::String& scope)
{
    testScope() = scope;
}

bool SuiteVfsJsonStore::removeScopeForTesting(juce::String& errorMessage)
{
    if (testScope().isEmpty())
        return true;
    SuiteVfsServiceClient client;
    juce::StringArray paths;
    if (! client.discover() || ! client.listEntries(paths))
    {
        errorMessage = "Could not reach the suite VFS service.";
        return false;
    }
    const auto prefix = "tests/" + testScope() + "/";
    for (const auto& path : paths)
    {
        // Entries are listed as the service keeps them, under "suite/".
        const auto entry = path.startsWith("suite/") ? path.substring(6) : path;
        if (entry.startsWith(prefix) && ! client.removeEntry(entry))
        {
            errorMessage = "Could not remove the test entry \"" + entry + "\".";
            return false;
        }
    }
    return true;
}

juce::var SuiteVfsJsonStore::loadJson(const juce::String& entryName, juce::String& errorMessage)
{
    const auto logicalPath = scoped(entryName);
    SuiteVfsServiceClient client;
    if (! client.discover())
    {
        errorMessage = "Could not reach the suite VFS service.";
        return {};
    }

    juce::MemoryBlock data;
    if (! client.readEntry(logicalPath, data))
        return {}; // no entry yet -- "nothing saved," not an error.

    const auto parsed = juce::JSON::parse(juce::String::createStringFromData(data.getData(), static_cast<int>(data.getSize())));
    if (parsed.isVoid())
        errorMessage = "Could not parse the \"" + logicalPath + "\" suite settings entry.";

    return parsed;
}

bool SuiteVfsJsonStore::saveJson(const juce::String& entryName, const juce::var& value, juce::String& errorMessage)
{
    const auto logicalPath = scoped(entryName);
    SuiteVfsServiceClient client;
    if (! client.discover())
    {
        errorMessage = "Could not reach the suite VFS service.";
        return false;
    }

    const auto json = juce::JSON::toString(value, true);
    const juce::MemoryBlock data(json.toRawUTF8(), json.getNumBytesAsUTF8());
    if (! client.writeEntry(logicalPath, data))
    {
        errorMessage = "Could not save the \"" + logicalPath + "\" suite settings entry.";
        return false;
    }

    return true;
}
}
