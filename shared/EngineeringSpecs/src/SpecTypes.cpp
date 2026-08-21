#include "creation/engineering/SpecTypes.h"

namespace creation::engineering
{
juce::String toStorageToken(SpecSourceKind kind)
{
    switch (kind)
    {
        case SpecSourceKind::userAuthored: return "userAuthored";
        case SpecSourceKind::builtinGeneric: break;
    }

    return "builtinGeneric";
}

SpecSourceKind specSourceKindFromStorageToken(const juce::String& token)
{
    if (token.trim().equalsIgnoreCase("userAuthored"))
        return SpecSourceKind::userAuthored;

    return SpecSourceKind::builtinGeneric;
}

juce::var toVar(const SpecSource& source)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("kind", toStorageToken(source.kind));
    object->setProperty("manufacturer", source.manufacturer);
    object->setProperty("partNumber", source.partNumber);
    object->setProperty("notes", source.notes);
    return juce::var(object);
}

bool fromVar(const juce::var& value, SpecSource& outSource)
{
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return false;

    outSource.kind = specSourceKindFromStorageToken(object->getProperty("kind").toString());
    outSource.manufacturer = object->getProperty("manufacturer").toString();
    outSource.partNumber = object->getProperty("partNumber").toString();
    outSource.notes = object->getProperty("notes").toString();
    return true;
}
}
