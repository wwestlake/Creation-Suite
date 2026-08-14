#include <creation/video/NativeVideoNodeRegistry.h>
#include <creation/video/NativeVideoGraphExecutor.h>
#include <creation/video/VideoRenderTargetPool.h>
#include <node_system/graph.h>
#include <node_system/type_registry.h>
#include <video/ComPtr.h>

#include <d3d11.h>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
void fail(const std::string& message)
{
    std::cerr << message << std::endl;
    throw std::runtime_error(message);
}

// WARP (software rasterizer) rather than D3D_DRIVER_TYPE_HARDWARE - this smoke test needs to run
// on any build machine, including ones with no real GPU or a GPU with no D3D11.1+ driver, so it
// deliberately doesn't assume real hardware is present the way the actual app does.
creation::video::ComPtr<ID3D11Device> createWarpDevice(creation::video::ComPtr<ID3D11DeviceContext>& outContext)
{
    creation::video::ComPtr<ID3D11Device> device;
    D3D_FEATURE_LEVEL obtainedLevel {};
    auto hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
                                D3D11_SDK_VERSION, device.address(), &obtainedLevel, outContext.address());
    if (FAILED(hr))
        fail("D3D11CreateDevice (WARP) failed");
    return device;
}
}

int main()
{
    try
    {
        creation::video::ComPtr<ID3D11DeviceContext> context;
        auto device = createWarpDevice(context);

        ce::node_system::NodeTypeRegistry registry;
        creation::video::RegisterNativeVideoNodeTypes(registry);

        ce::node_system::Graph graph("VideoSmokeGraph");
        std::string error;
        auto* colorBalance = ce::node_system::AddRegisteredNode(graph, registry, "ColorBalance", &error);
        if (colorBalance == nullptr)
            fail("Failed to add ColorBalance node: " + error);

        auto* blur = ce::node_system::AddRegisteredNode(graph, registry, "GaussianBlur", &error);
        if (blur == nullptr)
            fail("Failed to add GaussianBlur node: " + error);

        // ColorBalance's "output" (last output pin) -> GaussianBlur's "input" (first input pin).
        const auto colorBalanceOut = colorBalance->Outputs().front().id;
        const auto blurIn = blur->Inputs().front().id;

        ce::node_system::ConnectError connectError {};
        auto connection = graph.Connect(colorBalance->Id(), colorBalanceOut, blur->Id(), blurIn, &connectError);
        if (! connection)
            fail("Failed to connect ColorBalance -> GaussianBlur");

        constexpr UINT kSize = 64;

        D3D11_TEXTURE2D_DESC inputDesc {};
        inputDesc.Width = kSize;
        inputDesc.Height = kSize;
        inputDesc.MipLevels = 1;
        inputDesc.ArraySize = 1;
        inputDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        inputDesc.SampleDesc.Count = 1;
        inputDesc.Usage = D3D11_USAGE_DEFAULT;
        inputDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        creation::video::ComPtr<ID3D11Texture2D> inputTexture;
        if (FAILED(device->CreateTexture2D(&inputDesc, nullptr, inputTexture.address())))
            fail("Failed to create input texture");

        creation::video::ComPtr<ID3D11ShaderResourceView> inputSrv;
        if (FAILED(device->CreateShaderResourceView(inputTexture.get(), nullptr, inputSrv.address())))
            fail("Failed to create input SRV");

        D3D11_TEXTURE2D_DESC outputDesc = inputDesc;
        outputDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

        creation::video::ComPtr<ID3D11Texture2D> outputTexture;
        if (FAILED(device->CreateTexture2D(&outputDesc, nullptr, outputTexture.address())))
            fail("Failed to create output texture");

        creation::video::ComPtr<ID3D11RenderTargetView> outputRtv;
        if (FAILED(device->CreateRenderTargetView(outputTexture.get(), nullptr, outputRtv.address())))
            fail("Failed to create output RTV");

        // Not part of the connected graph above (which only needs to prove the linear-chain
        // executor) - constructed standalone just to prove its shader actually compiles, since
        // nothing else here would otherwise touch TransformCrop at all.
        auto transformCrop = creation::video::CreateNativeVideoNode("TransformCrop", device.get());
        if (! transformCrop)
            fail("Failed to construct TransformCrop node");
        if (! transformCrop->SetParameter("rotationDegrees", 45.0f))
            fail("TransformCrop node rejected a known parameter name");
        float rotationReadBack = 0.0f;
        if (! transformCrop->GetParameter("rotationDegrees", rotationReadBack) || std::abs(rotationReadBack - 45.0f) > 0.01f)
            fail("TransformCrop parameter round-trip failed");

        creation::video::VideoRenderTargetPool pool(device.get());
        creation::video::NativeVideoGraphExecutor executor(device.get(), context.get(), &pool);
        executor.syncNodes(graph);

        if (executor.findNode(colorBalance->Id()) == nullptr)
            fail("ColorBalance node did not resolve to a native video node instance");
        if (executor.findNode(blur->Id()) == nullptr)
            fail("GaussianBlur node did not resolve to a native video node instance");

        auto* colorBalanceNode = executor.findNode(colorBalance->Id());
        if (! colorBalanceNode->SetParameter("exposure", 0.5f))
            fail("ColorBalance node rejected a known parameter name");
        float readBack = 0.0f;
        if (! colorBalanceNode->GetParameter("exposure", readBack) || readBack != 0.5f)
            fail("ColorBalance parameter round-trip failed");

        if (! executor.run(graph, inputSrv.get(), outputRtv.get(), kSize, kSize, 1.5, 42))
            fail("Graph execution failed");

        std::cout << "VideoSmoke passed" << std::endl;
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "VideoSmoke failed: " << ex.what() << std::endl;
        return 1;
    }
}
