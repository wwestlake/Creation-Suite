# Node Library Loader

Turns a loaded Frust plugin's reflected node-library manifests
(`creation::frust::PluginRuntime::NodeLibraryManifest` -- raw JSON produced
by Frust's own `compileNodeReflection`, off a pod's `node pure`/`node
callable`/`node loop` declarations) into real entries in a
`ce::node_system::NodeLibraryRegistry`.

This is the mechanism that lets a pod published on the registry show up as
an ordinary, connectable node in any app's schematic editor, without that
app hand-writing a C++ catalog entry for it. It is purely mechanical JSON
parsing -- no app-specific logic at all.

## Why its own library, not inside NodeSystem or FrustPluginRuntime

`shared/NodeSystem` is documented to stay backend-agnostic: no JUCE, no
Frust, nothing linked. This loader needs both (JUCE for JSON parsing,
`PluginRuntime::NodeLibraryManifest` for the input type), so it lives beside
both instead of inside either.

## Using it

An app must `add_subdirectory` `shared/NodeSystem` and
`shared/FrustPluginRuntime` (both guarded with `if(NOT TARGET ...)`, same as
every other shared library in this suite) before adding this one -- its own
`CMakeLists.txt` fails fast with a clear error if either target is missing.

```cpp
#include <creation/frust/NodeLibraryLoader.h>

std::string error;
bool ok = creation::frust::RegisterPluginNodeLibraries(
    runtime.nodeLibraries(key), registry, supportedCapabilities, error);
```

`supportedCapabilities` is the set of capability strings this specific host
actually provides -- a library with any node requiring a capability outside
that set is rejected whole, with a specific error naming which node and
which capability, rather than loading half-working.

## History

Originally written as `apps/CreationEngine/Source/Frust/EngineNodeLibraryLoader`
(`ce::frust::RegisterPluginNodeLibraries`). That code had zero Engine-specific
logic, but was never extracted to `shared/`, so only CreationEngine could
load real pod-reflected nodes -- Station and Texture had no path to it.
Moved here so every app can use the same mechanism.
