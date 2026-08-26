#include <creation/services/SuiteVfsJsonStore.h>
#include <creation/services/SuiteVfsServiceClient.h>

namespace creation::services
{
juce::var SuiteVfsJsonStore::loadJson(const juce::String& logicalPath, juce::String& errorMessage)
{
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

bool SuiteVfsJsonStore::saveJson(const juce::String& logicalPath, const juce::var& value, juce::String& errorMessage)
{
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
