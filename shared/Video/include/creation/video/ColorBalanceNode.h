#pragma once

#include <video/INativeVideoNode.h>
#include <video/ComPtr.h>

struct ID3D11Device;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11SamplerState;
struct ID3D11Buffer;

namespace creation::video
{
// Reference native node #1: exposure/contrast/saturation/RGB tint, graded in linear space
// (gamma-decoded on read, re-encoded on write - see the .cpp's shader source) per the spec.
class ColorBalanceNode final : public INativeVideoNode
{
public:
    explicit ColorBalanceNode(ID3D11Device* deviceToUse);

    const char* GetTypeName() const override { return "ColorBalance"; }
    void Render(VideoFrameContext& context) override;
    bool SetParameter(const std::string& name, float value) override;
    bool GetParameter(const std::string& name, float& outValue) const override;
    std::vector<std::string> ParameterNames() const override;

private:
    // Layout must match ColorBalanceParams in the node's own HLSL b1 declaration exactly.
    struct Params
    {
        float exposure = 0.0f;
        float contrast = 1.0f;
        float saturation = 1.0f;
        float pad0 = 0.0f;
        float tintR = 1.0f;
        float tintG = 1.0f;
        float tintB = 1.0f;
        float pad1 = 0.0f;
    };

    ID3D11Device* device = nullptr;
    Params params;
    bool paramsDirty = true;

    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;
    ComPtr<ID3D11SamplerState> samplerState;
    ComPtr<ID3D11Buffer> paramBuffer;
};
}
