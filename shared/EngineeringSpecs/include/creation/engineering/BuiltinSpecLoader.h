#pragma once

#include <juce_core/juce_core.h>

#include "creation/engineering/SpecLibrary.h"

namespace creation::engineering
{
// Reads the three loose builtin-{materials,profiles,connectors}.json files
// out of `dataDirectory` (each a flat juce::Array<juce::var>, i.e. exactly
// one collection's worth of SpecLibrary::toVar's per-collection output) and
// populates outLibrary. Every record loaded this way should have
// source.kind == SpecSourceKind::builtinGeneric -- that's a data-authoring
// convention the JSON files follow, not something this loader enforces.
bool loadBuiltinSpecLibrary(const juce::File& dataDirectory, SpecLibrary& outLibrary, juce::String& errorMessage);
}
