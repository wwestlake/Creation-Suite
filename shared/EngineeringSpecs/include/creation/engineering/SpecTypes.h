#pragma once

#include <juce_core/juce_core.h>

namespace creation::engineering
{
enum class SpecSourceKind
{
    builtinGeneric,
    userAuthored
};

juce::String toStorageToken(SpecSourceKind kind);
SpecSourceKind specSourceKindFromStorageToken(const juce::String& token);

// Identifies where a library record's data came from. manufacturer/partNumber
// are populated only for userAuthored entries (enforced by the authoring form,
// not by this struct) -- this is the field the library browser surfaces so a
// generic/public-domain spec is never confused with a real vendor part.
struct SpecSource
{
    SpecSourceKind kind = SpecSourceKind::builtinGeneric;
    juce::String manufacturer;
    juce::String partNumber;
    juce::String notes;
};

juce::var toVar(const SpecSource& source);
bool fromVar(const juce::var& value, SpecSource& outSource);
}
