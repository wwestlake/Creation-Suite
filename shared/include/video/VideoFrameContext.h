#pragma once

// Forward-declared rather than #include <d3d11.h> - most code that just passes a
// VideoFrameContext around (graph wiring, parameter inspection) doesn't need the full D3D11 SDK
// header, only translation units that actually issue draw calls do.
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
struct ID3D11RenderTargetView;
struct ID3D11Buffer;

namespace creation::video
{
class VideoRenderTargetPool;

// Non-owning render context handed to a native node's Render() call for exactly one frame -
// device/context/inputTexture/outputTarget/globalConstants/renderTargetPool are all borrowed
// from the executor that's driving this frame; a node must not retain any of these pointers past
// its own Render() call.
struct VideoFrameContext
{
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    ID3D11ShaderResourceView* inputTexture = nullptr;
    ID3D11RenderTargetView* outputTarget = nullptr;
    // register(b0) contents (VideoFrameGlobalsCB - see VideoNodeCommon.hlsl / VideoNodeCommonSource.h),
    // filled in once per frame by the executor and shared read-only across every node's Render()
    // call this frame. A node's Render() should (re)bind this at the top of its own draw calls
    // rather than assume it's still bound from whatever ran before it.
    ID3D11Buffer* globalConstants = nullptr;
    // Scratch render-target source for nodes that need an intermediate pass (e.g. a two-pass
    // separable blur) - acquire()/release() per VideoRenderTargetPool's own contract, never held
    // past the node's own Render() call.
    VideoRenderTargetPool* renderTargetPool = nullptr;
    unsigned int width = 0;
    unsigned int height = 0;
    double timelineSeconds = 0.0;
    unsigned long long frameCounter = 0;
};
}
