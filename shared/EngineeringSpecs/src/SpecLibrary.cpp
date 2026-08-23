#include "creation/engineering/SpecLibrary.h"

#include <memory>

namespace creation::engineering
{
const MaterialSpec* SpecLibrary::findMaterial(const juce::String& id) const noexcept
{
    for (const auto& material : materials)
        if (material.id == id)
            return std::addressof(material);

    return nullptr;
}

const ProfileSpec* SpecLibrary::findProfile(const juce::String& id) const noexcept
{
    for (const auto& profile : profiles)
        if (profile.id == id)
            return std::addressof(profile);

    return nullptr;
}

const ConnectorSpec* SpecLibrary::findConnector(const juce::String& id) const noexcept
{
    for (const auto& connector : connectors)
        if (connector.id == id)
            return std::addressof(connector);

    return nullptr;
}

const CustomPartSpec* SpecLibrary::findCustomPart(const juce::String& id) const noexcept
{
    for (const auto& part : customParts)
        if (part.id == id)
            return std::addressof(part);

    return nullptr;
}

void SpecLibrary::merge(const SpecLibrary& overlay)
{
    for (const auto& material : overlay.materials)
        if (findMaterial(material.id) == nullptr)
            materials.push_back(material);

    for (const auto& profile : overlay.profiles)
        if (findProfile(profile.id) == nullptr)
            profiles.push_back(profile);

    for (const auto& connector : overlay.connectors)
        if (findConnector(connector.id) == nullptr)
            connectors.push_back(connector);

    for (const auto& part : overlay.customParts)
        if (findCustomPart(part.id) == nullptr)
            customParts.push_back(part);
}

juce::var toVar(const SpecLibrary& library)
{
    auto* object = new juce::DynamicObject();

    juce::Array<juce::var> materialsVar;
    for (const auto& material : library.materials)
        materialsVar.add(toVar(material));
    object->setProperty("materials", materialsVar);

    juce::Array<juce::var> profilesVar;
    for (const auto& profile : library.profiles)
        profilesVar.add(toVar(profile));
    object->setProperty("profiles", profilesVar);

    juce::Array<juce::var> connectorsVar;
    for (const auto& connector : library.connectors)
        connectorsVar.add(toVar(connector));
    object->setProperty("connectors", connectorsVar);

    juce::Array<juce::var> customPartsVar;
    for (const auto& part : library.customParts)
        customPartsVar.add(toVar(part));
    object->setProperty("customParts", customPartsVar);

    return juce::var(object);
}

bool fromVar(const juce::var& value, SpecLibrary& outLibrary)
{
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return false;

    outLibrary.materials.clear();
    if (const auto* array = object->getProperty("materials").getArray())
        for (const auto& entry : *array)
        {
            MaterialSpec material;
            if (fromVar(entry, material))
                outLibrary.materials.push_back(material);
        }

    outLibrary.profiles.clear();
    if (const auto* array = object->getProperty("profiles").getArray())
        for (const auto& entry : *array)
        {
            ProfileSpec profile;
            if (fromVar(entry, profile))
                outLibrary.profiles.push_back(profile);
        }

    outLibrary.connectors.clear();
    if (const auto* array = object->getProperty("connectors").getArray())
        for (const auto& entry : *array)
        {
            ConnectorSpec connector;
            if (fromVar(entry, connector))
                outLibrary.connectors.push_back(connector);
        }

    outLibrary.customParts.clear();
    if (const auto* array = object->getProperty("customParts").getArray())
        for (const auto& entry : *array)
        {
            CustomPartSpec part;
            if (fromVar(entry, part))
                outLibrary.customParts.push_back(part);
        }

    return true;
}
}
