#include "creation/engineering/ConnectorSpec.h"

namespace creation::engineering
{
juce::String toStorageToken(ConnectorKind kind)
{
    switch (kind)
    {
        case ConnectorKind::tNut: return "tNut";
        case ConnectorKind::cornerBracket: break;
    }

    return "cornerBracket";
}

ConnectorKind connectorKindFromStorageToken(const juce::String& token)
{
    if (token.trim().equalsIgnoreCase("tNut"))
        return ConnectorKind::tNut;

    return ConnectorKind::cornerBracket;
}

juce::var toVar(const ConnectorSpec& connector)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("id", connector.id);
    object->setProperty("displayName", connector.displayName);
    object->setProperty("kind", toStorageToken(connector.kind));
    object->setProperty("compatibleFamilyName", connector.compatibleFamilyName);
    object->setProperty("sizeMmX", connector.sizeMmX);
    object->setProperty("sizeMmY", connector.sizeMmY);
    object->setProperty("sizeMmZ", connector.sizeMmZ);
    object->setProperty("defaultMaterialId", connector.defaultMaterialId);
    object->setProperty("source", toVar(connector.source));
    return juce::var(object);
}

bool fromVar(const juce::var& value, ConnectorSpec& outConnector)
{
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return false;

    outConnector.id = object->getProperty("id").toString();
    outConnector.displayName = object->getProperty("displayName").toString();
    outConnector.kind = connectorKindFromStorageToken(object->getProperty("kind").toString());
    outConnector.compatibleFamilyName = object->getProperty("compatibleFamilyName").toString();
    outConnector.sizeMmX = static_cast<float>(static_cast<double>(object->getProperty("sizeMmX")));
    outConnector.sizeMmY = static_cast<float>(static_cast<double>(object->getProperty("sizeMmY")));
    outConnector.sizeMmZ = static_cast<float>(static_cast<double>(object->getProperty("sizeMmZ")));
    outConnector.defaultMaterialId = object->getProperty("defaultMaterialId").toString();
    fromVar(object->getProperty("source"), outConnector.source);
    return true;
}
}
