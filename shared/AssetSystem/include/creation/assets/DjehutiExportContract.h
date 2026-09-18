#pragma once

#include <juce_core/juce_core.h>

namespace creation::services { class SuiteVfsServiceClient; }

namespace creation::assets
{
// What the engine expects from an external export tool (the Djehuti
// Bridge Blender add-on, or any future equivalent) -- published so the
// exporter can try to comply, not used to validate/reject on the
// engine's side except for acceptedFormats. See DjehutiImportWatcher
// (apps/CreationEngine) for the one place acceptedFormats is actually
// enforced; upAxis/unitsPerMeter are informational only, a mismatch is
// a later manual-fix problem, not a rejection.
//
// A default-constructed contract already reflects the engine's real
// current behavior: glTF/GLB only, +Y up (ViewportComponent.cpp's own
// worldUp literal), one glTF unit = one meter (glTF's own spec default,
// which the engine has never had a reason to differ from).
struct DjehutiExportContract
{
    juce::StringArray acceptedFormats { "gltf", "glb" };
    juce::String upAxis { "+Y" };
    double unitsPerMeter = 1.0;
};

// VFS logical paths for the global default and a project-level override.
// Read via VfsService's existing generic entry API (GET /suite/entry,
// GET /project/entry) -- no dedicated endpoint for this.
struct DjehutiExportContractPaths
{
    static constexpr const char* suiteDefault = "djehuti-export-contract.json";
    static constexpr const char* projectOverride = "Metadata/djehuti-export-contract.json";
};

juce::var toVar(const DjehutiExportContract& contract);
bool fromVar(const juce::var& value, DjehutiExportContract& outContract);
juce::String serializeExportContract(const DjehutiExportContract& contract, bool prettyPrint = true);
bool deserializeExportContract(const juce::String& jsonText, DjehutiExportContract& outContract, juce::String& errorMessage);

// Idempotent: a no-op (returns true) if DjehutiExportContractPaths::suiteDefault
// already exists on `client`, whatever it currently contains -- only writes the
// engine's default when nothing is there yet, so an outside tool always has a
// real file to read instead of needing to know a fallback itself. `client`
// must already be discovered (SuiteVfsServiceClient::discover()).
bool EnsureDjehutiExportContractDefaultExists(creation::services::SuiteVfsServiceClient& client, juce::String& errorMessage);
}
