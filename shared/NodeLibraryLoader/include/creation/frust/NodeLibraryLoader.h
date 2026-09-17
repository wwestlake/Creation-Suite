#pragma once

#include <set>
#include <string>
#include <vector>

#include <creation/frust/PluginRuntime.h>
#include "node_system/node_library.h"

namespace creation::frust {

// Bridges a loaded Frust plugin's reflected node-library manifests
// (PluginRuntime::NodeLibraryManifest -- raw JSON, produced by Frust's own
// compileNodeReflection off `node pure`/`node callable`/`node loop`
// declarations) into ce::node_system::NodeLibraryRegistry, so any app's
// node-graph editor can offer those nodes without hand-writing a C++
// catalog for them.
//
// Lives in its own small library, not inside shared/NodeSystem itself:
// NodeSystem is documented to stay backend-agnostic (no JUCE, no Frust,
// links nothing) so it can be reused outside a JUCE/Frust context. This
// loader is inherently Frust- and JUCE-specific (JSON parsing), so it sits
// beside both shared/NodeSystem and shared/FrustPluginRuntime instead of
// inside either.
//
// Extracted from apps/CreationEngine/Source/Frust/EngineNodeLibraryLoader
// (ce::frust::RegisterPluginNodeLibraries) -- that implementation had zero
// Engine-specific logic; only CreationEngine could load real pod-reflected
// nodes because this was never pulled out to shared/. Station and Texture
// can now use the same mechanism.
//
// supportedCapabilities: the real fix for a gap found while wiring up the
// Node/Behavior Graph Foundations plan Phase 8: requiredCapabilities was
// already parsed off every node's manifest entry, but nothing ever checked
// it against anything -- a node library could declare it needed a
// capability the host doesn't actually provide and load anyway. A library
// with ANY node requiring a capability not in this set is rejected with a
// clear error instead of registering unenforced.
bool RegisterPluginNodeLibraries(const std::vector<PluginRuntime::NodeLibraryManifest>& manifests,
                                  ce::node_system::NodeLibraryRegistry& registry,
                                  const std::set<std::string, std::less<>>& supportedCapabilities,
                                  std::string& error);

} // namespace creation::frust
