#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <node_system/graph.h>
#include <video/INativeVideoNode.h>
#include <video/ComPtr.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
struct ID3D11RenderTargetView;
struct ID3D11Buffer;

namespace creation::video
{
class VideoRenderTargetPool;

// Runs one ce::node_system::Graph of native video nodes for one frame: topologically sorts it
// (via node_system's own TopologicalDataOrder, not a reimplementation), then executes each node
// in order, wiring each node's "input" pin to whatever feeds it.
//
// Scoped assumption: a strict linear chain - every node has at most one incoming and one outgoing
// Data connection on its "input"/"output" pins. This covers all three reference nodes (each is
// single-input/single-output) and is enough to prove the registry/dispatch/parameter wiring end
// to end; a real fan-in/fan-out compositor scheduler is future scope once something actually
// needs non-linear graphs (nothing does yet - there's no video-FX graph editor/player in the app
// this could come from today).
class NativeVideoGraphExecutor
{
public:
    NativeVideoGraphExecutor(ID3D11Device* deviceToUse, ID3D11DeviceContext* contextToUse,
                             VideoRenderTargetPool* poolToUse);

    // Builds (or rebuilds, if the graph's node set changed) the INativeVideoNode instance for
    // every node in graph via CreateNativeVideoNode, keyed by NodeId. Call once after editing the
    // graph, before run() - not on every frame.
    void syncNodes(const ce::node_system::Graph& graph);

    // Runs the whole chain for one frame: inputTexture feeds whichever node has no incoming
    // connection, outputTarget receives whichever node has no outgoing connection. Returns false
    // if the graph has no nodes, more than one node with no incoming connection (ambiguous entry
    // point - not a linear chain), or a cycle (TopologicalDataOrder itself returns nullopt).
    bool run(const ce::node_system::Graph& graph, ID3D11ShaderResourceView* inputTexture,
             ID3D11RenderTargetView* outputTarget, unsigned int width, unsigned int height,
             double timelineSeconds, unsigned long long frameCounter);

    INativeVideoNode* findNode(ce::node_system::NodeId id) const;

private:
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    VideoRenderTargetPool* pool = nullptr;
    std::unordered_map<ce::node_system::NodeId, std::unique_ptr<INativeVideoNode>> nodes;
    ComPtr<ID3D11Buffer> globalConstantBuffer;
};
}
