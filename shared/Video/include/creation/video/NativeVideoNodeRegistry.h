#pragma once

#include <memory>
#include <video/INativeVideoNode.h>
#include <node_system/type_registry.h>

struct ID3D11Device;

namespace creation::video
{
// Registers every built-in ("Layer 1") native video node's structural shape - type name, input/
// output Texture pins, and one Float input pin per named parameter (so a parameter is an
// ordinary graph pin, wireable/automatable the same way any other node_system Data pin is) -
// into registry. Mirrors shared/CEL's node_catalog.cpp RegisterXxxNodes shape, one call covering
// all three reference nodes rather than one function per node (there's no separate "domain" split
// to justify that here, unlike node_catalog's Event/ControlFlow/Math/World/Utility groups).
void RegisterNativeVideoNodeTypes(ce::node_system::NodeTypeRegistry& registry);

// The OTHER half a registered type name needs to actually run: constructs the concrete
// INativeVideoNode for a given registered type name (e.g. "ColorBalance"), or nullptr if
// typeName isn't a native video node type. Mirrors node_catalog's registry/codegen split - the
// NodeTypeRegistry entry describes the node's SHAPE, this dispatch table describes what it DOES.
std::unique_ptr<INativeVideoNode> CreateNativeVideoNode(const std::string& typeName, ID3D11Device* device);
}
