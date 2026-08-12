#include <creation/video/VideoRenderTargetPool.h>

#include <d3d11.h>

namespace creation::video
{
PooledRenderTarget VideoRenderTargetPool::acquire(unsigned int width, unsigned int height, DXGI_FORMAT format)
{
    for (auto& entry : entries)
    {
        if (! entry.inUse && entry.width == width && entry.height == height && entry.format == format)
        {
            entry.inUse = true;
            return { entry.texture.get(), entry.renderTargetView.get(), entry.shaderResourceView.get() };
        }
    }

    Entry entry;
    entry.width = width;
    entry.height = height;
    entry.format = format;

    D3D11_TEXTURE2D_DESC textureDesc {};
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = format;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(device->CreateTexture2D(&textureDesc, nullptr, entry.texture.address())))
        return {};

    if (FAILED(device->CreateRenderTargetView(entry.texture.get(), nullptr, entry.renderTargetView.address())))
        return {};

    if (FAILED(device->CreateShaderResourceView(entry.texture.get(), nullptr, entry.shaderResourceView.address())))
        return {};

    entry.inUse = true;
    entries.push_back(std::move(entry));
    auto& stored = entries.back();
    return { stored.texture.get(), stored.renderTargetView.get(), stored.shaderResourceView.get() };
}

void VideoRenderTargetPool::release(const PooledRenderTarget& target)
{
    for (auto& entry : entries)
    {
        if (entry.texture.get() == target.texture)
        {
            entry.inUse = false;
            return;
        }
    }
}
}
