#pragma once

#include <video/INativeVideoNode.h>
#include <video/ComPtr.h>

struct ID3D11Device;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11SamplerState;
struct ID3D11Buffer;
struct ID3D11ShaderResourceView;
struct ID3D11RenderTargetView;

namespace creation::video
{
// Reference native node #2: two-pass separable Gaussian blur (horizontal then vertical),
// configurable radius in pixels. The intermediate horizontal-pass target comes from
// VideoFrameContext::renderTargetPool, not a permanently-owned texture, per the spec's texture
// resource pooling requirement.
class GaussianBlurNode final : public INativeVideoNode
{
public:
    explicit GaussianBlurNode(ID3D11Device* deviceToUse);

    const char* GetTypeName() const override { return "GaussianBlur"; }
    void Render(VideoFrameContext& context) override;
    bool SetParameter(const std::string& name, float value) override;
    bool GetParameter(const std::string& name, float& outValue) const override;
    std::vector<std::string> ParameterNames() const override;

private:
    // Layout must match GaussianBlurParams in the node's own HLSL b1 declaration exactly.
    struct Params
    {
        float radius = 4.0f;
        float direction = 0.0f; // 0 = horizontal pass, 1 = vertical pass - set per draw, not user-facing
        float pad0 = 0.0f;
        float pad1 = 0.0f;
    };

    void runPass(VideoFrameContext& context, ID3D11ShaderResourceView* source, ID3D11RenderTargetView* destination,
                 unsigned int width, unsigned int height, float direction);

    ID3D11Device* device = nullptr;
    Params params;

    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;
    ComPtr<ID3D11SamplerState> samplerState;
    ComPtr<ID3D11Buffer> paramBuffer;
};
}
