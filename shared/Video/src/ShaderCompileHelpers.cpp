#include <creation/video/ShaderCompileHelpers.h>

#include <d3dcompiler.h>
#include <windows.h>

#pragma comment(lib, "d3dcompiler.lib")

namespace creation::video
{
ComPtr<ID3DBlob> CompileHlsl(const std::string& source, const char* sourceName, const char* entryPoint, const char* target)
{
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> errors;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
   #ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
   #endif

    auto hr = D3DCompile(source.data(), source.size(), sourceName, nullptr, nullptr,
                         entryPoint, target, flags, 0, blob.address(), errors.address());
    if (FAILED(hr))
    {
        if (errors)
            OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
        blob.reset();
    }

    return blob;
}
}
