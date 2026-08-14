#ifndef _ofxImageEffect_h_
#define _ofxImageEffect_h_

// See ofxCore.h for provenance/scoping notes - same caveat applies here, and doubly so for this
// file: it's the largest of the four and the one carrying the D3D11-interop property names this
// whole host exists to use correctly (OfxImagePropD3D11Texture/OfxImagePropD3D11SharedHandle),
// so treat those two names in particular as needing verification against the canonical spec
// before depending on a real plugin recognizing them.
//
// Scoped to the image-effect surface shared/OfxHost's three suites actually exercise - render
// triggers, clip/frame descriptor access, and D3D11/OpenGL GPU-texture interop properties. The
// full spec additionally covers multi-view stereo, custom interacts, and several other clip/
// param property groups not modeled here.

#include <stddef.h>
#include "ofxCore.h"
#include "ofxParam.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kOfxImageEffectSuite "OfxImageEffectSuite"

#define kOfxImageEffectPropContext "OfxImageEffectPropContext"
#define kOfxImageEffectContextFilter "OfxImageEffectContextFilter"
#define kOfxImageEffectContextGenerator "OfxImageEffectContextGenerator"
#define kOfxImageEffectContextTransition "OfxImageEffectContextTransition"

#define kOfxImageEffectActionDescribe "OfxImageEffectActionDescribe"
#define kOfxImageEffectActionDescribeInContext "OfxImageEffectActionDescribeInContext"
#define kOfxImageEffectActionRender "OfxImageEffectActionRender"
#define kOfxImageEffectActionBeginSequenceRender "OfxImageEffectActionBeginSequenceRender"
#define kOfxImageEffectActionEndSequenceRender "OfxImageEffectActionEndSequenceRender"
#define kOfxImageEffectActionGetRegionOfDefinition "OfxImageEffectActionGetRegionOfDefinition"
#define kOfxImageEffectActionGetClipPreferences "OfxImageEffectActionGetClipPreferences"
#define kOfxImageEffectActionIsIdentity "OfxImageEffectActionIsIdentity"

#define kOfxImageEffectPropPixelDepth "OfxImageEffectPropPixelDepth"
#define kOfxImageEffectPropComponents "OfxImageEffectPropComponents"
#define kOfxImageEffectPropRenderScale "OfxImageEffectPropRenderScale"
#define kOfxImageEffectPropFrameRate "OfxImageEffectPropFrameRate"

// GPU acceleration negotiation - a plugin sets these during describe to declare what it supports;
// the host is only allowed to hand it a CPU (kOfxImageBufferPlane) buffer if the plugin
// explicitly rejects/omits GPU support (spec section on zero-copy requirements).
#define kOfxImageEffectPropOpenGLRenderSupported "OfxImageEffectPropOpenGLRenderSupported"
#define kOfxImageEffectPropOpenGLEnabled "OfxImageEffectPropOpenGLEnabled"

#define kOfxImageEffectPropPixelAspectRatio "OfxImageEffectPropPixelAspectRatio"

#define kOfxImagePixelDepthByte "OfxBitDepthByte"
#define kOfxImagePixelDepthShort "OfxBitDepthShort"
#define kOfxImagePixelDepthFloat "OfxBitDepthFloat"

#define kOfxImageComponentRGBA "OfxImageComponentRGBA"
#define kOfxImageComponentRGB "OfxImageComponentRGB"
#define kOfxImageComponentAlpha "OfxImageComponentAlpha"

// CPU fallback buffer descriptor - forbidden by this host's zero-copy policy unless a plugin has
// explicitly rejected GPU acceleration (kOfxImageEffectPropOpenGLRenderSupported == false and no
// D3D11 support declared).
#define kOfxImageBufferPlane "OfxImageBufferPlane"

// GPU texture handoff properties, set on a clip's image descriptor before
// kOfxImageEffectActionRender - the entire point of this host existing rather than falling back
// to a CPU-copy OFX bridge. A D3D11-native plugin reads these directly; an OpenGL-only plugin
// instead goes through the NV_DX_interop bridge (see this host's own design note on why that's
// deferred - no OpenGL context/interop code exists yet).
#define kOfxImagePropD3D11Texture "OfxImagePropD3D11Texture"
#define kOfxImagePropD3D11SharedHandle "OfxImagePropD3D11SharedHandle"

typedef void* OfxImageEffectHandle;
typedef void* OfxImageClipHandle;

// Context management, clip binding, rendering triggers, and frame descriptor allocation - the
// render-facing half of the host, alongside OfxParameterSuiteV1's parameter-facing half.
typedef struct OfxImageEffectSuiteV1
{
    OfxStatus (*getPropertySet)(OfxImageEffectHandle imageEffect, OfxPropertySetHandle* propHandle);
    OfxStatus (*getParamSet)(OfxImageEffectHandle imageEffect, OfxParamSetHandle* paramSet);

    OfxStatus (*clipDefine)(OfxImageEffectHandle imageEffect, const char* name, OfxPropertySetHandle* propertySet);
    OfxStatus (*clipGetHandle)(OfxImageEffectHandle imageEffect, const char* name, OfxImageClipHandle* clip, OfxPropertySetHandle* propertySet);
    OfxStatus (*clipGetPropertySet)(OfxImageClipHandle clip, OfxPropertySetHandle* propHandle);

    // Handed a frame descriptor (a property set) for the given time - for a GPU-resident clip
    // this descriptor carries kOfxImagePropD3D11Texture/kOfxImagePropD3D11SharedHandle rather
    // than a CPU pointer; see this file's own property-name doc comments above.
    OfxStatus (*clipGetImage)(OfxImageClipHandle clip, double time, const void* region, OfxPropertySetHandle* imageHandle);
    OfxStatus (*clipReleaseImage)(OfxPropertySetHandle imageHandle);
    OfxStatus (*clipGetRegionOfDefinition)(OfxImageClipHandle clip, double time, void* bounds);

    OfxStatus (*abort)(OfxImageEffectHandle imageEffect);

    OfxStatus (*imageMemoryAlloc)(OfxImageEffectHandle instanceHandle, size_t nBytes, void* memoryHandle);
    OfxStatus (*imageMemoryFree)(void* memoryHandle);
    OfxStatus (*imageMemoryLock)(void* memoryHandle, void** returnedPtr);
    OfxStatus (*imageMemoryUnlock)(void* memoryHandle);
} OfxImageEffectSuiteV1;

#ifdef __cplusplus
}
#endif

#endif
