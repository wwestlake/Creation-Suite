#ifndef _ofxParam_h_
#define _ofxParam_h_

// See ofxCore.h for provenance/scoping notes - same caveat applies here.

#include "ofxCore.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kOfxParameterSuite "OfxParameterSuite"

typedef void* OfxParamHandle;
typedef void* OfxParamSetHandle;

#define kOfxParamTypeInteger "OfxParamTypeInteger"
#define kOfxParamTypeDouble "OfxParamTypeDouble"
#define kOfxParamTypeBoolean "OfxParamTypeBoolean"
#define kOfxParamTypeChoice "OfxParamTypeChoice"
#define kOfxParamTypeRGBA "OfxParamTypeRGBA"
#define kOfxParamTypeRGB "OfxParamTypeRGB"
#define kOfxParamTypeString "OfxParamTypeString"
#define kOfxParamTypeGroup "OfxParamTypeGroup"
#define kOfxParamTypePage "OfxParamTypePage"
#define kOfxParamTypePushButton "OfxParamTypePushButton"

#define kOfxParamPropType "OfxParamPropType"
#define kOfxParamPropDefault "OfxParamPropDefault"
#define kOfxParamPropMin "OfxParamPropMin"
#define kOfxParamPropMax "OfxParamPropMax"
#define kOfxParamPropDisplayMin "OfxParamPropDisplayMin"
#define kOfxParamPropDisplayMax "OfxParamPropDisplayMax"
#define kOfxParamPropScriptName "OfxParamPropScriptName"
#define kOfxParamPropAnimates "OfxParamPropAnimates"

// Registration (paramDefine, during kOfxActionDescribeInContext - not modeled as its own action
// here, see shared/OfxHost's own scope note), querying (paramGetHandle/paramGetPropertySet), and
// timeline evaluation (paramGetValue/paramGetValueAtTime/paramSetValue/paramSetValueAtTime) of a
// plugin instance's parameters - the automation-lane-facing half of the host, alongside
// OfxImageEffectSuiteV1's clip/render-facing half.
typedef struct OfxParameterSuiteV1
{
    OfxStatus (*paramDefine)(OfxParamSetHandle paramSet, const char* paramType, const char* name, OfxPropertySetHandle* propertySet);
    OfxStatus (*paramGetHandle)(OfxParamSetHandle paramSet, const char* name, OfxParamHandle* param, OfxPropertySetHandle* propertySet);
    OfxStatus (*paramSetGetPropertySet)(OfxParamSetHandle paramSet, OfxPropertySetHandle* propHandle);
    OfxStatus (*paramGetPropertySet)(OfxParamHandle param, OfxPropertySetHandle* propHandle);

    OfxStatus (*paramGetValue)(OfxParamHandle paramHandle, ...);
    OfxStatus (*paramGetValueAtTime)(OfxParamHandle paramHandle, double time, ...);
    OfxStatus (*paramGetDerivative)(OfxParamHandle paramHandle, double time, ...);
    OfxStatus (*paramGetIntegral)(OfxParamHandle paramHandle, double time1, double time2, ...);
    OfxStatus (*paramSetValue)(OfxParamHandle paramHandle, ...);
    OfxStatus (*paramSetValueAtTime)(OfxParamHandle paramHandle, double time, ...);

    OfxStatus (*paramGetNumKeys)(OfxParamHandle paramHandle, unsigned int* numberOfKeys);
    OfxStatus (*paramGetKeyTime)(OfxParamHandle paramHandle, unsigned int nthKey, double* time);
    OfxStatus (*paramGetKeyIndex)(OfxParamHandle paramHandle, double time, int direction, int* index);
    OfxStatus (*paramDeleteKey)(OfxParamHandle paramHandle, double time);
    OfxStatus (*paramDeleteAllKeys)(OfxParamHandle paramHandle);

    OfxStatus (*paramCopy)(OfxParamHandle paramTo, OfxParamHandle paramFrom, double dstOffset, const double* frameRange);

    OfxStatus (*paramEditBegin)(OfxParamSetHandle paramSet, const char* name);
    OfxStatus (*paramEditEnd)(OfxParamSetHandle paramSet);
} OfxParameterSuiteV1;

#ifdef __cplusplus
}
#endif

#endif
