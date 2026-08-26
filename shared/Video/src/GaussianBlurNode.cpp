#include <creation/video/GaussianBlurNode.h>
#include <creation/video/VideoNodeCommonSource.h>
#include <creation/video/ShaderCompileHelpers.h>
#include <creation/video/VideoRenderTargetPool.h>

#include <d3d11.h>
#include <string>

namespace creation::video
{
namespace
{
const std::string kPixelShaderSource = std::string(kVideoNodeCommonHlsl) + R"hlsl(
cbuffer GaussianBlurParams : register(b1)
{
    float Radius;
    float Direction;
    float _Pad0;
    float _Pad1;
};

Texture2D inputTexture : register(t0);
SamplerState linearSampler : register(s0);

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

static const int kMaxTaps = 16;

float4 PSMain(VSOutput input) : SV_TARGET
{
    float sigma = max(0.0001, Radius / 3.0);
    float2 texel = Direction > 0.5 ? float2(0.0, InvFrameHeight) : float2(InvFrameWidth, 0.0);

    float4 sum = float4(0, 0, 0, 0);
    float weightSum = 0.0;
    int taps = (int) clamp(Radius, 0.0, (float) kMaxTaps);

    for (int i = -taps; i <= taps; ++i)
    {
        float w = exp(-0.5 * (float(i) / sigma) * (float(i) / sigma));
        sum += inputTexture.Sample(linearSampler, input.uv + texel * float(i)) * w;
        weightSum += w;
    }

    return sum / max(weightSum, 0.0001);
}
)hlsl";

const std::string kVertexShaderSource = kVideoNodeCommonHlsl + std::string(kFullscreenTriangleVertexShaderHlsl);
}

GaussianBlurNode::GaussianBlurNode(ID3D11Device* deviceToUse) : device(deviceToUse)
{
    auto vsBlob = CompileHlsl(kVertexShaderSource, "GaussianBlurNode.vs", "VSMain", "vs_4_0");
    auto psBlob = CompileHlsl(kPixelShaderSource, "GaussianBlurNode.ps", "PSMain", "ps_4_0");
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

void GaussianBlurNode::runPass(VideoFrameContext& context, ID3D11ShaderResourceView* source, ID3D11RenderTargetView* destination,
                               unsigned int width, unsigned int height, float direction)
{
    auto* ctx = context.context;

    Params passParams = params;
    passParams.direction = direction;

    D3D11_MAPPED_SUBRESOURCE mapped {};
    if (SUCCEEDED(ctx->Map(paramBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        *static_cast<Params*>(mapped.pData) = passParams;
        ctx->Unmap(paramBuffer.get(), 0);
    }

    D3D11_VIEWPORT viewport {};
    viewport.Width = (float) width;
    viewport.Height = (float) height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    ID3D11RenderTargetView* rtvs[] = { destination };
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

    ID3D11ShaderResourceView* srvs[] = { source };
    ctx->PSSetShaderResources(0, 1, srvs);
    ID3D11SamplerState* samplers[] = { samplerState.get() };
    ctx->PSSetSamplers(0, 1, samplers);

    ctx->Draw(3, 0);

    ID3D11ShaderResourceView* nullSrvs[] = { nullptr };
    ctx->PSSetShaderResources(0, 1, nullSrvs);
}

void GaussianBlurNode::Render(VideoFrameContext& context)
{
    if (! vertexShader || ! pixelShader || ! samplerState || ! paramBuffer || context.renderTargetPool == nullptr)
        return;

    auto intermediate = context.renderTargetPool->acquire(context.width, context.height);
    if (! intermediate.isValid())
        return;

    // Pass 1: input -> intermediate, horizontal.
    runPass(context, context.inputTexture, intermediate.renderTargetView, context.width, context.height, 0.0f);
    // Pass 2: intermediate -> real output, vertical.
    runPass(context, intermediate.shaderResourceView, context.outputTarget, context.width, context.height, 1.0f);

    context.renderTargetPool->release(intermediate);
}

bool GaussianBlurNode::SetParameter(const std::string& name, float value)
{
    if (name == "radius") { params.radius = value; return true; }
    return false;
}

bool GaussianBlurNode::GetParameter(const std::string& name, float& outValue) const
{
    if (name == "radius") { outValue = params.radius; return true; }
    return false;
}

std::vector<std::string> GaussianBlurNode::ParameterNames() const
{
    return { "radius" };
}
}
