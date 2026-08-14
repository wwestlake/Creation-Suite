#include <creation/video/TransformCropNode.h>
#include <creation/video/VideoNodeCommonSource.h>
#include <creation/video/ShaderCompileHelpers.h>

#include <d3d11.h>
#include <string>

namespace creation::video
{
namespace
{
constexpr float kPi = 3.14159265358979323846f;

const std::string kPixelShaderSource = std::string(kVideoNodeCommonHlsl) + R"hlsl(
cbuffer TransformCropParams : register(b1)
{
    float ScaleX;
    float ScaleY;
    float TranslateX;
    float TranslateY;
    float RotationRadians;
    float CropLeft;
    float CropTop;
    float CropRight;
    float CropBottom;
    float _Pad0;
    float _Pad1;
    float _Pad2;
};

Texture2D inputTexture : register(t0);
SamplerState linearSampler : register(s0);

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 PSMain(VSOutput input) : SV_TARGET
{
    float2 centered = input.uv - 0.5;

    float cosR = cos(-RotationRadians);
    float sinR = sin(-RotationRadians);
    float2 rotated = float2(centered.x * cosR - centered.y * sinR, centered.x * sinR + centered.y * cosR);

    float2 scaled = rotated / float2(max(ScaleX, 0.0001), max(ScaleY, 0.0001));
    float2 sourceUv = scaled + 0.5 - float2(TranslateX, TranslateY);

    if (sourceUv.x < CropLeft || sourceUv.x > 1.0 - CropRight
        || sourceUv.y < CropTop || sourceUv.y > 1.0 - CropBottom
        || sourceUv.x < 0.0 || sourceUv.x > 1.0 || sourceUv.y < 0.0 || sourceUv.y > 1.0)
        return float4(0, 0, 0, 0);

    return inputTexture.Sample(linearSampler, sourceUv);
}
)hlsl";

const std::string kVertexShaderSource = kVideoNodeCommonHlsl + std::string(kFullscreenTriangleVertexShaderHlsl);
}

TransformCropNode::TransformCropNode(ID3D11Device* deviceToUse) : device(deviceToUse)
{
    auto vsBlob = CompileHlsl(kVertexShaderSource, "TransformCropNode.vs", "VSMain", "vs_4_0");
    auto psBlob = CompileHlsl(kPixelShaderSource, "TransformCropNode.ps", "PSMain", "ps_4_0");
    if (! vsBlob || ! psBlob)
        return;

    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, vertexShader.address());
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, pixelShader.address());

    D3D11_SAMPLER_DESC samplerDesc {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    device->CreateSamplerState(&samplerDesc, samplerState.address());

    D3D11_BUFFER_DESC bufferDesc {};
    bufferDesc.ByteWidth = sizeof(Params);
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&bufferDesc, nullptr, paramBuffer.address());
}

void TransformCropNode::Render(VideoFrameContext& context)
{
    if (! vertexShader || ! pixelShader || ! samplerState || ! paramBuffer)
        return;

    auto* ctx = context.context;

    if (paramsDirty)
    {
        D3D11_MAPPED_SUBRESOURCE mapped {};
        if (SUCCEEDED(ctx->Map(paramBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            *static_cast<Params*>(mapped.pData) = params;
            ctx->Unmap(paramBuffer.get(), 0);
        }
        paramsDirty = false;
    }

    D3D11_VIEWPORT viewport {};
    viewport.Width = (float) context.width;
    viewport.Height = (float) context.height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    ID3D11RenderTargetView* rtvs[] = { context.outputTarget };
    ctx->OMSetRenderTargets(1, rtvs, nullptr);
    ctx->RSSetViewports(1, &viewport);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetInputLayout(nullptr);
    ctx->VSSetShader(vertexShader.get(), nullptr, 0);
    ctx->PSSetShader(pixelShader.get(), nullptr, 0);

    ID3D11Buffer* globalCbs[] = { context.globalConstants };
    ctx->VSSetConstantBuffers(0, 1, globalCbs);
    ctx->PSSetConstantBuffers(0, 1, globalCbs);
    ID3D11Buffer* paramCbs[] = { paramBuffer.get() };
    ctx->PSSetConstantBuffers(1, 1, paramCbs);

    ID3D11ShaderResourceView* srvs[] = { context.inputTexture };
    ctx->PSSetShaderResources(0, 1, srvs);
    ID3D11SamplerState* samplers[] = { samplerState.get() };
    ctx->PSSetSamplers(0, 1, samplers);

    ctx->Draw(3, 0);

    ID3D11ShaderResourceView* nullSrvs[] = { nullptr };
    ctx->PSSetShaderResources(0, 1, nullSrvs);
}

bool TransformCropNode::SetParameter(const std::string& name, float value)
{
    paramsDirty = true;
    if (name == "scaleX") { params.scaleX = value; return true; }
    if (name == "scaleY") { params.scaleY = value; return true; }
    if (name == "translateX") { params.translateX = value; return true; }
    if (name == "translateY") { params.translateY = value; return true; }
    if (name == "rotationDegrees") { params.rotationRadians = value * (kPi / 180.0f); return true; }
    if (name == "cropLeft") { params.cropLeft = value; return true; }
    if (name == "cropTop") { params.cropTop = value; return true; }
    if (name == "cropRight") { params.cropRight = value; return true; }
    if (name == "cropBottom") { params.cropBottom = value; return true; }
    paramsDirty = false;
    return false;
}

bool TransformCropNode::GetParameter(const std::string& name, float& outValue) const
{
    if (name == "scaleX") { outValue = params.scaleX; return true; }
    if (name == "scaleY") { outValue = params.scaleY; return true; }
    if (name == "translateX") { outValue = params.translateX; return true; }
    if (name == "translateY") { outValue = params.translateY; return true; }
    if (name == "rotationDegrees") { outValue = params.rotationRadians * (180.0f / kPi); return true; }
    if (name == "cropLeft") { outValue = params.cropLeft; return true; }
    if (name == "cropTop") { outValue = params.cropTop; return true; }
    if (name == "cropRight") { outValue = params.cropRight; return true; }
    if (name == "cropBottom") { outValue = params.cropBottom; return true; }
    return false;
}

std::vector<std::string> TransformCropNode::ParameterNames() const
{
    return { "scaleX", "scaleY", "translateX", "translateY", "rotationDegrees",
             "cropLeft", "cropTop", "cropRight", "cropBottom" };
}
}
