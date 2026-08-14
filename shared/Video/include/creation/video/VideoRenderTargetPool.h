#pragma once

#include <vector>
#include <dxgiformat.h>
#include <video/ComPtr.h>

struct ID3D11Device;
struct ID3D11Texture2D;
struct ID3D11RenderTargetView;
struct ID3D11ShaderResourceView;

namespace creation::video
{
struct PooledRenderTarget
{
    ID3D11Texture2D* texture = nullptr;
    ID3D11RenderTargetView* renderTargetView = nullptr;
    ID3D11ShaderResourceView* shaderResourceView = nullptr;

    bool isValid() const noexcept { return texture != nullptr; }
};

// Reusable pool of intermediate D3D11 render targets, so a multi-node/multi-pass effect chain
// (e.g. Gaussian Blur's two separable passes) never allocates VRAM mid-playback or mid-scrub:
// acquire() hands back an existing free entry matching (width, height, format) if one exists, or
// creates one on first use; release() returns it to the free list rather than destroying it.
//
// Not thread-safe - callers must only use this from the single graphics-context thread that owns
// the D3D11 immediate context, matching this whole system's "video processing never touches the
// audio thread, always the graphics context thread" rule.
class VideoRenderTargetPool
{
public:
    explicit VideoRenderTargetPool(ID3D11Device* deviceToUse) : device(deviceToUse) {}

    PooledRenderTarget acquire(unsigned int width, unsigned int height, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);
    void release(const PooledRenderTarget& target);

private:
    struct Entry
    {
        unsigned int width = 0;
        unsigned int height = 0;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        bool inUse = false;
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11RenderTargetView> renderTargetView;
        ComPtr<ID3D11ShaderResourceView> shaderResourceView;
    };

    ID3D11Device* device = nullptr;
    std::vector<Entry> entries;
};
}
