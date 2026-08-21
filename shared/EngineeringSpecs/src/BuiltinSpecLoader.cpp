#include "creation/engineering/BuiltinSpecLoader.h"

namespace
{
template <typename RecordT>
bool loadFlatArrayFile(const juce::File& jsonFile, std::vector<RecordT>& outRecords, juce::String& errorMessage)
{
    if (!jsonFile.existsAsFile())
    {
        errorMessage = "Missing builtin spec file: " + jsonFile.getFullPathName();
        return false;
    }

    const auto parsed = juce::JSON::parse(jsonFile);
    const auto* array = parsed.getArray();
    if (array == nullptr)
    {
        errorMessage = "Builtin spec file is not a JSON array: " + jsonFile.getFullPathName();
        return false;
    }

    for (const auto& entry : *array)
    {
        RecordT record;
        if (creation::engineering::fromVar(entry, record))
            outRecords.push_back(record);
    }

    return true;
}
}

namespace creation::engineering
{
bool loadBuiltinSpecLibrary(const juce::File& dataDirectory, SpecLibrary& outLibrary, juce::String& errorMessage)
{
    outLibrary = SpecLibrary{};

    if (!loadFlatArrayFile(dataDirectory.getChildFile("builtin-materials.json"), outLibrary.materials, errorMessage))
        return false;

    if (!loadFlatArrayFile(dataDirectory.getChildFile("builtin-profiles.json"), outLibrary.profiles, errorMessage))
        return false;

    if (!loadFlatArrayFile(dataDirectory.getChildFile("builtin-connectors.json"), outLibrary.connectors, errorMessage))
        return false;

    return true;
}
}
