#pragma once

#include <string>
#include <video/ComPtr.h>
// ID3DBlob is `typedef ID3D10Blob ID3DBlob;` in the real SDK header, not a class of its own - a
// plain forward declaration would declare a second, incompatible type of the same name, so this
// pulls in the (small, self-contained) real declaration instead.
#include <d3dcommon.h>

namespace creation::video
{
// Compiles HLSL source text (already assembled by the caller - typically
// kVideoNodeCommonHlsl + the node's own b1 cbuffer + its VS/PS) for entryPoint/target
// (e.g. "VSMain"/"vs_4_0"). Returns a null ComPtr on failure; compile errors go to stderr via
// OutputDebugString-equivalent (see the .cpp) so they're visible without needing a caller-supplied
// error sink for what should be a build-time-caught mistake in practice.
ComPtr<ID3DBlob> CompileHlsl(const std::string& source, const char* sourceName, const char* entryPoint, const char* target);
}
