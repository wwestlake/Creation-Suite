// VideoNodeCommon.hlsl
//
// Shared constant-buffer conventions for every native video effect node's HLSL shader.
//
//   register(b0): global per-frame context, IDENTICAL layout for every node - the graph
//   executor fills this in once per frame and every node reads it. Declared here.
//
//   register(b1): each node's own custom parameters - deliberately NOT declared here, since the
//   layout is different per node. Declare it in the node's own shader source, immediately after
//   the content of this file.
//
// This file is also mirrored byte-for-byte as a C++ string constant
// (creation::video::kVideoNodeCommonHlsl in VideoNodeCommonSource.h) that each node prepends to
// its own shader source before compiling - keep the two in sync if this file changes.

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
