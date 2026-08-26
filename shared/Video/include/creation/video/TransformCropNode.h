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
// Reference native node #3: 2D scale/translate/rotate plus edge cropping, done as an inverse-UV
// remap in the pixel shader (there's no real geometry to transform - every node is a full-screen
// post-process pass over its input frame) with explicit bounds checks for crisp, alpha-clean crop
// edges rather than relying solely on sampler address-mode clamping.
class TransformCropNode final : public INativeVideoNode
{
public:
    explicit TransformCropNode(ID3D11Device* deviceToUse);

    const char* GetTypeName() const override { return "TransformCrop"; }
    void Render(VideoFrameContext& context) override;
    bool SetParameter(const std::string& name, float value) override;
    bool GetParameter(const std::string& name, float& outValue) const override;
    std::vector<std::string> ParameterNames() const override;

private:
    // Layout must match TransformCropParams in the node's own HLSL b1 declaration exactly.
    struct Params
    {
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        float translateX = 0.0f;
        float translateY = 0.0f;
        float rotationRadians = 0.0f;
        float cropLeft = 0.0f;
        float cropTop = 0.0f;
        float cropRight = 0.0f;
        float cropBottom = 0.0f;
        float pad0 = 0.0f;
        float pad1 = 0.0f;
        float pad2 = 0.0f;
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
