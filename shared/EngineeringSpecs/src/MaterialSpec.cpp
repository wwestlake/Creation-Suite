#include "creation/engineering/MaterialSpec.h"

namespace creation::engineering
{
juce::var toVar(const MaterialSpec& material)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("id", material.id);
    object->setProperty("displayName", material.displayName);
    object->setProperty("alloyDesignation", material.alloyDesignation);
    object->setProperty("densityKgM3", material.densityKgM3);
    object->setProperty("yieldStrengthMPa", material.yieldStrengthMPa);
    object->setProperty("ultimateTensileStrengthMPa", material.ultimateTensileStrengthMPa);
    object->setProperty("elasticModulusGPa", material.elasticModulusGPa);
    object->setProperty("shearModulusGPa", material.shearModulusGPa);
    object->setProperty("poissonsRatio", material.poissonsRatio);
    object->setProperty("source", toVar(material.source));
    return juce::var(object);
}

bool fromVar(const juce::var& value, MaterialSpec& outMaterial)
{
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return false;

    outMaterial.id = object->getProperty("id").toString();
    outMaterial.displayName = object->getProperty("displayName").toString();
    outMaterial.alloyDesignation = object->getProperty("alloyDesignation").toString();
    outMaterial.densityKgM3 = static_cast<float>(static_cast<double>(object->getProperty("densityKgM3")));
    outMaterial.yieldStrengthMPa = static_cast<float>(static_cast<double>(object->getProperty("yieldStrengthMPa")));
    outMaterial.ultimateTensileStrengthMPa = static_cast<float>(static_cast<double>(object->getProperty("ultimateTensileStrengthMPa")));
    outMaterial.elasticModulusGPa = static_cast<float>(static_cast<double>(object->getProperty("elasticModulusGPa")));
    outMaterial.shearModulusGPa = static_cast<float>(static_cast<double>(object->getProperty("shearModulusGPa")));
    outMaterial.poissonsRatio = static_cast<float>(static_cast<double>(object->getProperty("poissonsRatio")));
    fromVar(object->getProperty("source"), outMaterial.source);
    return true;
}
}
