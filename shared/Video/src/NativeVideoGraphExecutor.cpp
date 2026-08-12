#include <creation/video/NativeVideoGraphExecutor.h>
#include <creation/video/NativeVideoNodeRegistry.h>
#include <creation/video/VideoRenderTargetPool.h>
#include <creation/video/VideoNodeCommonSource.h>
#include <node_system/graph_analysis.h>

#include <d3d11.h>

namespace creation::video
{
NativeVideoGraphExecutor::NativeVideoGraphExecutor(ID3D11Device* deviceToUse, ID3D11DeviceContext* contextToUse,
                                                   VideoRenderTargetPool* poolToUse)
    : device(deviceToUse), context(contextToUse), pool(poolToUse)
{
}

void NativeVideoGraphExecutor::syncNodes(const ce::node_system::Graph& graph)
{
    nodes.clear();
    for (const auto& [id, nodePtr] : graph.Nodes())
    {
        auto created = CreateNativeVideoNode(nodePtr->TypeName(), device);
        if (created)
            nodes.emplace(id, std::move(created));
    }
}

INativeVideoNode* NativeVideoGraphExecutor::findNode(ce::node_system::NodeId id) const
{
    auto it = nodes.find(id);
    return it != nodes.end() ? it->second.get() : nullptr;
}

bool NativeVideoGraphExecutor::run(const ce::node_system::Graph& graph, ID3D11ShaderResourceView* inputTexture,
                                   ID3D11RenderTargetView* outputTarget, unsigned int width, unsigned int height,
                                   double timelineSeconds, unsigned long long frameCounter)
{
    auto order = ce::node_system::TopologicalDataOrder(graph);
    if (! order || order->empty() || device == nullptr || context == nullptr || pool == nullptr)
        return false;

    if (! globalConstantBuffer)
    {
        D3D11_BUFFER_DESC desc {};
        desc.ByteWidth = sizeof(VideoFrameGlobalsCB);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(device->CreateBuffer(&desc, nullptr, globalConstantBuffer.address())))
            return false;
    }

    VideoFrameGlobalsCB globals;
    globals.frameWidth = (float) width;
    globals.frameHeight = (float) height;
    globals.invFrameWidth = width > 0 ? 1.0f / (float) width : 0.0f;
    globals.invFrameHeight = height > 0 ? 1.0f / (float) height : 0.0f;
    globals.timelineSeconds = (float) timelineSeconds;
    globals.frameCounter = (float) frameCounter;

    D3D11_MAPPED_SUBRESOURCE mapped {};
    if (SUCCEEDED(context->Map(globalConstantBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        *static_cast<VideoFrameGlobalsCB*>(mapped.pData) = globals;
        context->Unmap(globalConstantBuffer.get(), 0);
    }

    ID3D11ShaderResourceView* currentSource = inputTexture;
    PooledRenderTarget previousIntermediate;

    for (size_t i = 0; i < order->size(); ++i)
    {
        auto nodeId = (*order)[i];
        auto it = nodes.find(nodeId);
        if (it == nodes.end() || it->second == nullptr)
            continue; // Not a native video node type (e.g. a non-video node mixed into the graph) - pass through untouched.

        bool isLast = (i + 1 == order->size());
        PooledRenderTarget intermediate;
        ID3D11RenderTargetView* destination = outputTarget;

        if (! isLast)
        {
            intermediate = pool->acquire(width, height);
            if (! intermediate.isValid())
                return false;
            destination = intermediate.renderTargetView;
        }

        VideoFrameContext frameContext;
        frameContext.device = device;
        frameContext.context = context;
        frameContext.inputTexture = currentSource;
        frameContext.outputTarget = destination;
        frameContext.globalConstants = globalConstantBuffer.get();
        frameContext.renderTargetPool = pool;
        frameContext.width = width;
        frameContext.height = height;
        frameContext.timelineSeconds = timelineSeconds;
        frameContext.frameCounter = frameCounter;

        it->second->Render(frameContext);

        if (previousIntermediate.isValid())
            pool->release(previousIntermediate);

        if (intermediate.isValid())
            currentSource = intermediate.shaderResourceView;
        previousIntermediate = intermediate;
    }

    if (previousIntermediate.isValid())
        pool->release(previousIntermediate);

    return true;
}
}
