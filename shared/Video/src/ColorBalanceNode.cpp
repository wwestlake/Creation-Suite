#include <creation/video/ColorBalanceNode.h>
#include <creation/video/VideoNodeCommonSource.h>
#include <creation/video/ShaderCompileHelpers.h>

#include <d3d11.h>
#include <string>

namespace creation::video
{
namespace
{
const std::string kPixelShaderSource = std::string(kVideoNodeCommonHlsl) + R"hlsl(
cbuffer ColorBalanceParams : register(b1)
{
    float Exposure;
    float Contrast;
    float Saturation;
    float _Pad0;
    float3 Tint;
    float _Pad1;
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
    float4 srcColor = inputTexture.Sample(linearSampler, input.uv);

    float3 linearColor = pow(max(srcColor.rgb, 0.0), 2.2);
    linearColor *= exp2(Exposure);
    linearColor = (linearColor - 0.5) * Contrast + 0.5;

    float luma = dot(linearColor, float3(0.2126, 0.7152, 0.0722));
    linearColor = lerp(luma.xxx, linearColor, Saturation);
    linearColor *= Tint;

    float3 outColor = pow(saturate(linearColor), 1.0 / 2.2);
    return float4(outColor, srcColor.a);
}
)hlsl";

const std::string kVertexShaderSource = kVideoNodeCommonHlsl + std::string(kFullscreenTriangleVertexShaderHlsl);
}

ColorBalanceNode::ColorBalanceNode(ID3D11Device* deviceToUse) : device(deviceToUse)
{
    auto vsBlob = CompileHlsl(kVertexShaderSource, "ColorBalanceNode.vs", "VSMain", "vs_4_0");
    auto psBlob = CompileHlsl(kPixelShaderSource, "ColorBalanceNode.ps", "PSMain", "ps_4_0");
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

void ColorBalanceNode::Render(VideoFrameContext& context)
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

bool ColorBalanceNode::SetParameter(const std::string& name, float value)
{
    paramsDirty = true;
    if (name == "exposure") { params.exposure = value; return true; }
    if (name == "contrast") { params.contrast = value; return true; }
    if (name == "saturation") { params.saturation = value; return true; }
    if (name == "tintR") { params.tintR = value; return true; }
    if (name == "tintG") { params.tintG = value; return true; }
    if (name == "tintB") { params.tintB = value; return true; }
    paramsDirty = false;
    return false;
}

bool ColorBalanceNode::GetParameter(const std::string& name, float& outValue) const
{
    if (name == "exposure") { outValue = params.exposure; return true; }
    if (name == "contrast") { outValue = params.contrast; return true; }
    if (name == "saturation") { outValue = params.saturation; return true; }
    if (name == "tintR") { outValue = params.tintR; return true; }
    if (name == "tintG") { outValue = params.tintG; return true; }
    if (name == "tintB") { outValue = params.tintB; return true; }
    return false;
}

std::vector<std::string> ColorBalanceNode::ParameterNames() const
{
    return { "exposure", "contrast", "saturation", "tintR", "tintG", "tintB" };
}
}
