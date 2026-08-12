#ifndef _ofxCore_h_
#define _ofxCore_h_

// Reconstructed from the public OpenFX Image Effect API specification (v1.4) - the OFX standard
// itself is open/freely implementable, but this file was authored from specification knowledge
// rather than copied from the canonical AcademySoftwareFoundation/openfx repository (no network
// access in this environment to diff against it directly). Cross-check against that repo before
// loading a real third-party plugin against this host - a wrong string constant here would fail
// silently (a plugin just won't find the property/action it's looking for) rather than loudly.
//
// Scoped to what shared/OfxHost's three required suites actually need - the full OFX spec
// defines many more optional actions/properties (interact UI, multi-view, keyer-specific clip
// properties, etc.) that no code in this suite touches yet.

#ifdef __cplusplus
extern "C" {
#endif

typedef int OfxStatus;

#define kOfxStatOK 0
#define kOfxStatFailed 1
#define kOfxStatErrFatal 2
#define kOfxStatErrUnknown 3
#define kOfxStatErrMissingHostFeature 4
#define kOfxStatErrUnsupported 5
#define kOfxStatErrExists 6
#define kOfxStatErrFormat 7
#define kOfxStatErrMemory 8
#define kOfxStatErrBadHandle 9
#define kOfxStatErrBadIndex 10
#define kOfxStatErrValue 11
#define kOfxStatReplyYes 12
#define kOfxStatReplyNo 13
#define kOfxStatReplyDefault 14

typedef void* OfxPropertySetHandle;

// Every OFX object handle (image effect, clip, param, etc.) doubles as a property set - the host
// exposes/reads its attributes exclusively through OfxPropertySuiteV1, never direct struct access.
typedef struct OfxHost
{
    OfxPropertySetHandle host;
    // Returns the suite struct for a given API name/version (e.g. "OfxImageEffectSuite", 1), or
    // null if this host doesn't implement it - a plugin calls this once per suite it needs,
    // during kOfxActionLoad, and holds onto the returned pointer for its whole lifetime.
    const void* (*fetchSuite)(OfxPropertySetHandle host, const char* suiteName, int suiteVersion);
} OfxHost;

// The struct every .ofx binary exports one array of via OfxGetPlugin - the host's entire view of
// a plugin before it's ever instantiated.
typedef struct OfxPlugin
{
    const char* pluginApi;
    int apiVersion;
    const char* pluginIdentifier;
    unsigned int pluginVersionMajor;
    unsigned int pluginVersionMinor;
    void (*setHost)(OfxHost* host);
    OfxStatus (*mainEntry)(const char* action, const void* handle, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs);
} OfxPlugin;

// The two entry points the host resolves by name (GetProcAddress on Windows) after loading a
// .ofx/.dll - per spec section 3.2.
typedef int (*OfxGetNumberOfPluginsFunc)(void);
typedef OfxPlugin* (*OfxGetPluginFunc)(int nth);

#define kOfxActionLoad "OfxActionLoad"
#define kOfxActionUnload "OfxActionUnload"
#define kOfxActionDescribe "OfxActionDescribe"
#define kOfxActionCreateInstance "OfxActionCreateInstance"
#define kOfxActionDestroyInstance "OfxActionDestroyInstance"
#define kOfxActionBeginInstanceChanged "OfxActionBeginInstanceChanged"
#define kOfxActionInstanceChanged "OfxActionInstanceChanged"
#define kOfxActionEndInstanceChanged "OfxActionEndInstanceChanged"
#define kOfxActionPurgeCaches "OfxActionPurgeCaches"
#define kOfxActionSyncPrivateData "OfxActionSyncPrivateData"

#define kOfxPropType "OfxPropType"
#define kOfxPropName "OfxPropName"
#define kOfxPropLabel "OfxPropLabel"
#define kOfxPropTime "OfxPropTime"
#define kOfxPropIsInteractive "OfxPropIsInteractive"

#define kOfxImageEffectPluginApi "OfxImageEffectPluginAPI"

#ifdef __cplusplus
}
#endif

#endif
