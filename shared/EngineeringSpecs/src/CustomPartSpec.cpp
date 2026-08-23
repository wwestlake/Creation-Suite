#include "creation/engineering/CustomPartSpec.h"

namespace creation::engineering
{
namespace
{
juce::var point2ToVar(juce::Point<float> p)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("x", p.x);
    object->setProperty("y", p.y);
    return juce::var(object);
}

bool point2FromVar(const juce::var& value, juce::Point<float>& outPoint)
{
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return false;

    outPoint.x = static_cast<float>(static_cast<double>(object->getProperty("x")));
    outPoint.y = static_cast<float>(static_cast<double>(object->getProperty("y")));
    return true;
}

juce::var customPartHoleToVar(const CustomPartHole& hole)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("centerUV", point2ToVar(hole.centerUV));
    object->setProperty("diameterMeters", hole.diameterMeters);
    return juce::var(object);
}

bool customPartHoleFromVar(const juce::var& value, CustomPartHole& outHole)
{
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return false;

    point2FromVar(object->getProperty("centerUV"), outHole.centerUV);
    outHole.diameterMeters = static_cast<float>(static_cast<double>(object->getProperty("diameterMeters")));
    return true;
}
}

juce::String toStorageToken(SketchPlaneToken plane)
{
    switch (plane)
    {
        case SketchPlaneToken::xy: return "xy";
        case SketchPlaneToken::yz: return "yz";
        case SketchPlaneToken::xz: break;
    }

    return "xz";
}

SketchPlaneToken sketchPlaneTokenFromStorageToken(const juce::String& token)
{
    if (token.equalsIgnoreCase("xy"))
        return SketchPlaneToken::xy;
    if (token.equalsIgnoreCase("yz"))
        return SketchPlaneToken::yz;

    return SketchPlaneToken::xz;
}

juce::var toVar(const CustomPartSpec& part)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("id", part.id);
    object->setProperty("displayName", part.displayName);
    object->setProperty("defaultMaterialId", part.defaultMaterialId);
    object->setProperty("plane", toStorageToken(part.plane));
    object->setProperty("thicknessMeters", part.thicknessMeters);
    object->setProperty("source", toVar(part.source));

    juce::Array<juce::var> boundaryVar;
    for (const auto& p : part.boundaryUV)
        boundaryVar.add(point2ToVar(p));
    object->setProperty("boundaryUV", boundaryVar);

    juce::Array<juce::var> holesVar;
    for (const auto& hole : part.holes)
        holesVar.add(customPartHoleToVar(hole));
    object->setProperty("holes", holesVar);

    return juce::var(object);
}

bool fromVar(const juce::var& value, CustomPartSpec& outPart)
{
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return false;

    outPart.id = object->getProperty("id").toString();
    outPart.displayName = object->getProperty("displayName").toString();
    outPart.defaultMaterialId = object->getProperty("defaultMaterialId").toString();
    outPart.plane = sketchPlaneTokenFromStorageToken(object->getProperty("plane").toString());
    outPart.thicknessMeters = static_cast<float>(static_cast<double>(object->getProperty("thicknessMeters")));
    fromVar(object->getProperty("source"), outPart.source);

    outPart.boundaryUV.clear();
    if (const auto* array = object->getProperty("boundaryUV").getArray())
        for (const auto& entry : *array)
        {
            juce::Point<float> p;
            if (point2FromVar(entry, p))
                outPart.boundaryUV.push_back(p);
        }

    outPart.holes.clear();
    if (const auto* array = object->getProperty("holes").getArray())
        for (const auto& entry : *array)
        {
            CustomPartHole hole;
            if (customPartHoleFromVar(entry, hole))
                outPart.holes.push_back(hole);
        }

    return true;
}
}
