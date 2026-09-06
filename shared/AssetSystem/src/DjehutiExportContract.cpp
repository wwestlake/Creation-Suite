#include "creation/assets/DjehutiExportContract.h"

#include <creation/services/SuiteVfsServiceClient.h>

namespace creation::assets
{
juce::var toVar(const DjehutiExportContract& contract)
{
    auto* object = new juce::DynamicObject();

    juce::Array<juce::var> formats;
    for (const auto& format : contract.acceptedFormats)
        formats.add(format);
    object->setProperty("acceptedFormats", formats);

    object->setProperty("upAxis", contract.upAxis);
    object->setProperty("unitsPerMeter", contract.unitsPerMeter);
    return juce::var(object);
}

bool fromVar(const juce::var& value, DjehutiExportContract& outContract)
{
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return false;

    juce::StringArray formats;
    if (const auto* array = object->getProperty("acceptedFormats").getArray())
        for (const auto& format : *array)
            formats.add(format.toString());
    if (! formats.isEmpty())
        outContract.acceptedFormats = formats;

    const auto upAxis = object->getProperty("upAxis").toString();
    if (upAxis.isNotEmpty())
        outContract.upAxis = upAxis;

    if (object->hasProperty("unitsPerMeter"))
        outContract.unitsPerMeter = static_cast<double>(object->getProperty("unitsPerMeter"));

    return true;
}

juce::String serializeExportContract(const DjehutiExportContract& contract, bool prettyPrint)
{
    return juce::JSON::toString(toVar(contract), prettyPrint);
}

bool deserializeExportContract(const juce::String& jsonText, DjehutiExportContract& outContract, juce::String& errorMessage)
{
    const auto parsed = juce::JSON::parse(jsonText);
    if (parsed.isVoid())
    {
        errorMessage = "Could not parse the Djehuti export contract JSON.";
        return false;
    }

    if (! fromVar(parsed, outContract))
    {
        errorMessage = "Djehuti export contract JSON did not match the expected schema.";
        return false;
    }

    return true;
}

bool EnsureDjehutiExportContractDefaultExists(creation::services::SuiteVfsServiceClient& client, juce::String& errorMessage)
{
    juce::MemoryBlock existing;
    if (client.readEntry(DjehutiExportContractPaths::suiteDefault, existing))
        return true; // Already present -- whatever it contains, leave it alone.

    const DjehutiExportContract defaults;
    const auto json = serializeExportContract(defaults);
    juce::MemoryBlock data(json.toRawUTF8(), static_cast<size_t>(json.getNumBytesAsUTF8()));
    if (! client.writeEntry(DjehutiExportContractPaths::suiteDefault, data))
    {
        errorMessage = "Could not write the default Djehuti export contract to the suite VFS.";
        return false;
    }
    return true;
}
}
