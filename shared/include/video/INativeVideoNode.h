#pragma once

#include <string>
#include <vector>
#include "VideoFrameContext.h"

namespace creation::video
{
// Base interface every built-in (Layer 1, "Option 2") video effect node implements. A node type
// is also registered into ce::node_system::NodeTypeRegistry (see NativeVideoNodeRegistry) so it
// has a place in an ordinary node graph alongside logic/math/parameter nodes - this interface is
// the OTHER half: what actually runs when the graph executor reaches this node.
class INativeVideoNode
{
public:
    virtual ~INativeVideoNode() = default;

    // Matches the type name this node was registered under in NodeTypeRegistry.
    virtual const char* GetTypeName() const = 0;

    // Executes exactly one frame: reads context.inputTexture, writes context.outputTarget.
    // Must not leave D3D11 immediate-context state (bound shaders/resources/viewport) assumed by
    // a LATER node without that node rebinding its own - the executor runs every node back to
    // back on one shared context with no state reset between them.
    virtual void Render(VideoFrameContext& context) = 0;

    // Named parameter surface - the addressing point for FRust scripts and shared/Timeline
    // automation lanes (both bind to a node by this string name, not by C++ member access).
    virtual bool SetParameter(const std::string& name, float value) = 0;
    virtual bool GetParameter(const std::string& name, float& outValue) const = 0;
    virtual std::vector<std::string> ParameterNames() const = 0;
};
}
