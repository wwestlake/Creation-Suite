#pragma once

namespace creation::video
{
// Byte-for-byte mirror of shared/include/video/VideoNodeCommon.hlsl - see that file for the
// authoritative, human-editable source and the reason this exists as a string constant too
// (nodes prepend it to their own shader source before calling D3DCompile).
inline constexpr char kVideoNodeCommonHlsl[] = R"hlsl(
cbuffer VideoFrameGlobals : register(b0)
{
    float FrameWidth;
    float FrameHeight;
    float InvFrameWidth;
    float InvFrameHeight;
    float TimelineSeconds;
    float FrameCounter;
    float _GlobalsPad0;
    float _GlobalsPad1;
};
)hlsl";

// Every native node uses the same full-screen-triangle vertex shader (no vertex/index buffer -
// generated purely from SV_VertexID) since every node is a post-process pass over its whole input
// frame, not real 3D geometry. Shared here rather than duplicated three times.
inline constexpr char kFullscreenTriangleVertexShaderHlsl[] = R"hlsl(
struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput VSMain(uint id : SV_VertexID)
{
    VSOutput output;
    float2 uv = float2((id << 1) & 2, id & 2);
    output.uv = uv;
    output.position = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
    return output;
}
)hlsl";

// C++ mirror of the VideoFrameGlobals cbuffer's layout, for filling VideoFrameContext::globalConstants
// from the executor side. Keep field order/count in sync with the HLSL above.
struct VideoFrameGlobalsCB
{
    float frameWidth = 0.0f;
    float frameHeight = 0.0f;
    float invFrameWidth = 0.0f;
    float invFrameHeight = 0.0f;
    float timelineSeconds = 0.0f;
    float frameCounter = 0.0f;
    float pad0 = 0.0f;
    float pad1 = 0.0f;
};
}
