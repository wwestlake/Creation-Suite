#include <creation/ofxhost/OfxHostSuites.h>

#include <cstdarg>
#include <string>

namespace creation::ofxhost
{
OfxPropertySetHandle OfxParamSet::define(const std::string& type, const std::string& name)
{
    auto& entry = params[name];
    entry.type = type;
    entry.properties->setString(kOfxParamPropType, 0, type);
    return entry.properties.get();
}

OfxPropertySetHandle OfxParamSet::findHandle(const std::string& name) const
{
    auto it = params.find(name);
    return it != params.end() ? static_cast<OfxPropertySetHandle>(it->second.properties.get()) : nullptr;
}

OfxPropertySet* OfxParamSet::find(const std::string& name) const
{
    auto it = params.find(name);
    return it != params.end() ? it->second.properties.get() : nullptr;
}

namespace
{
// ---- OfxPropertySuiteV1 ----

OfxStatus PropSetPointer(OfxPropertySetHandle h, const char* name, int index, void* value)
{
    if (h == nullptr || name == nullptr) return kOfxStatErrBadHandle;
    static_cast<OfxPropertySet*>(h)->setPointer(name, index, value);
    return kOfxStatOK;
}

OfxStatus PropSetString(OfxPropertySetHandle h, const char* name, int index, const char* value)
{
    if (h == nullptr || name == nullptr) return kOfxStatErrBadHandle;
    static_cast<OfxPropertySet*>(h)->setString(name, index, value != nullptr ? value : "");
    return kOfxStatOK;
}

OfxStatus PropSetDouble(OfxPropertySetHandle h, const char* name, int index, double value)
{
    if (h == nullptr || name == nullptr) return kOfxStatErrBadHandle;
    static_cast<OfxPropertySet*>(h)->setDouble(name, index, value);
    return kOfxStatOK;
}

OfxStatus PropSetInt(OfxPropertySetHandle h, const char* name, int index, int value)
{
    if (h == nullptr || name == nullptr) return kOfxStatErrBadHandle;
    static_cast<OfxPropertySet*>(h)->setInt(name, index, value);
    return kOfxStatOK;
}

OfxStatus PropSetPointerN(OfxPropertySetHandle h, const char* name, int count, void* const* value)
{
    if (h == nullptr || name == nullptr || value == nullptr) return kOfxStatErrBadHandle;
    auto* set = static_cast<OfxPropertySet*>(h);
    for (int i = 0; i < count; ++i)
        set->setPointer(name, i, value[i]);
    return kOfxStatOK;
}

OfxStatus PropSetStringN(OfxPropertySetHandle h, const char* name, int count, const char* const* value)
{
    if (h == nullptr || name == nullptr || value == nullptr) return kOfxStatErrBadHandle;
    auto* set = static_cast<OfxPropertySet*>(h);
    for (int i = 0; i < count; ++i)
        set->setString(name, i, value[i] != nullptr ? value[i] : "");
    return kOfxStatOK;
}

OfxStatus PropSetDoubleN(OfxPropertySetHandle h, const char* name, int count, const double* value)
{
    if (h == nullptr || name == nullptr || value == nullptr) return kOfxStatErrBadHandle;
    auto* set = static_cast<OfxPropertySet*>(h);
    for (int i = 0; i < count; ++i)
        set->setDouble(name, i, value[i]);
    return kOfxStatOK;
}

OfxStatus PropSetIntN(OfxPropertySetHandle h, const char* name, int count, const int* value)
{
    if (h == nullptr || name == nullptr || value == nullptr) return kOfxStatErrBadHandle;
    auto* set = static_cast<OfxPropertySet*>(h);
    for (int i = 0; i < count; ++i)
        set->setInt(name, i, value[i]);
    return kOfxStatOK;
}

OfxStatus PropGetPointer(OfxPropertySetHandle h, const char* name, int index, void** value)
{
    if (h == nullptr || name == nullptr || value == nullptr) return kOfxStatErrBadHandle;
    return static_cast<OfxPropertySet*>(h)->getPointer(name, index, value) ? kOfxStatOK : kOfxStatErrValue;
}

OfxStatus PropGetString(OfxPropertySetHandle h, const char* name, int index, char** value)
{
    if (h == nullptr || name == nullptr || value == nullptr) return kOfxStatErrBadHandle;
    const char* stored = nullptr;
    if (! static_cast<OfxPropertySet*>(h)->getString(name, index, &stored))
        return kOfxStatErrValue;
    // Deliberately hands back the OfxPropertySet's own internal storage (a real host
    // implementation this simple is common for OFX - the string's lifetime is the property's,
    // not the caller's, matching the spec's own note that returned strings are host-owned).
    *value = const_cast<char*>(stored);
    return kOfxStatOK;
}

OfxStatus PropGetDouble(OfxPropertySetHandle h, const char* name, int index, double* value)
{
    if (h == nullptr || name == nullptr || value == nullptr) return kOfxStatErrBadHandle;
    return static_cast<OfxPropertySet*>(h)->getDouble(name, index, value) ? kOfxStatOK : kOfxStatErrValue;
}

OfxStatus PropGetInt(OfxPropertySetHandle h, const char* name, int index, int* value)
{
    if (h == nullptr || name == nullptr || value == nullptr) return kOfxStatErrBadHandle;
    return static_cast<OfxPropertySet*>(h)->getInt(name, index, value) ? kOfxStatOK : kOfxStatErrValue;
}

OfxStatus PropGetPointerN(OfxPropertySetHandle h, const char* name, int count, void** value)
{
    if (h == nullptr || name == nullptr || value == nullptr) return kOfxStatErrBadHandle;
    auto* set = static_cast<OfxPropertySet*>(h);
    for (int i = 0; i < count; ++i)
        if (! set->getPointer(name, i, &value[i]))
            return kOfxStatErrValue;
    return kOfxStatOK;
}

OfxStatus PropGetStringN(OfxPropertySetHandle h, const char* name, int count, char** value)
{
    if (h == nullptr || name == nullptr || value == nullptr) return kOfxStatErrBadHandle;
    auto* set = static_cast<OfxPropertySet*>(h);
    for (int i = 0; i < count; ++i)
    {
        const char* stored = nullptr;
        if (! set->getString(name, i, &stored))
            return kOfxStatErrValue;
        value[i] = const_cast<char*>(stored);
    }
    return kOfxStatOK;
}

OfxStatus PropGetDoubleN(OfxPropertySetHandle h, const char* name, int count, double* value)
{
    if (h == nullptr || name == nullptr || value == nullptr) return kOfxStatErrBadHandle;
    auto* set = static_cast<OfxPropertySet*>(h);
    for (int i = 0; i < count; ++i)
        if (! set->getDouble(name, i, &value[i]))
            return kOfxStatErrValue;
    return kOfxStatOK;
}

OfxStatus PropGetIntN(OfxPropertySetHandle h, const char* name, int count, int* value)
{
    if (h == nullptr || name == nullptr || value == nullptr) return kOfxStatErrBadHandle;
    auto* set = static_cast<OfxPropertySet*>(h);
    for (int i = 0; i < count; ++i)
        if (! set->getInt(name, i, &value[i]))
            return kOfxStatErrValue;
    return kOfxStatOK;
}

OfxStatus PropReset(OfxPropertySetHandle h, const char* name)
{
    if (h == nullptr || name == nullptr) return kOfxStatErrBadHandle;
    static_cast<OfxPropertySet*>(h)->reset(name);
    return kOfxStatOK;
}

OfxStatus PropGetDimension(OfxPropertySetHandle h, const char* name, int* count)
{
    if (h == nullptr || name == nullptr || count == nullptr) return kOfxStatErrBadHandle;
    *count = static_cast<OfxPropertySet*>(h)->getDimension(name);
    return kOfxStatOK;
}

// ---- OfxParameterSuiteV1 ----
// paramGetValue/paramSetValue/*AtTime: double-only first pass - see OfxHostSuites.h's own note.

int ComponentCountForType(const std::string& type)
{
    if (type == kOfxParamTypeRGBA) return 4;
    if (type == kOfxParamTypeRGB) return 3;
    return 1;
}

OfxStatus ParamDefine(OfxParamSetHandle paramSet, const char* paramType, const char* name, OfxPropertySetHandle* propertySet)
{
    if (paramSet == nullptr || paramType == nullptr || name == nullptr) return kOfxStatErrBadHandle;
    auto handle = static_cast<OfxParamSet*>(paramSet)->define(paramType, name);
    if (propertySet != nullptr) *propertySet = handle;
    return kOfxStatOK;
}

OfxStatus ParamGetHandle(OfxParamSetHandle paramSet, const char* name, OfxParamHandle* param, OfxPropertySetHandle* propertySet)
{
    if (paramSet == nullptr || name == nullptr) return kOfxStatErrBadHandle;
    auto handle = static_cast<OfxParamSet*>(paramSet)->findHandle(name);
    if (handle == nullptr) return kOfxStatErrValue;
    if (param != nullptr) *param = handle;
    if (propertySet != nullptr) *propertySet = handle;
    return kOfxStatOK;
}

OfxStatus ParamSetGetPropertySet(OfxParamSetHandle paramSet, OfxPropertySetHandle* propHandle)
{
    // The param SET itself (as opposed to one param) has no property set of its own in this
    // scoped implementation - no code anywhere calls this yet.
    if (paramSet == nullptr || propHandle == nullptr) return kOfxStatErrBadHandle;
    return kOfxStatErrUnsupported;
}

OfxStatus ParamGetPropertySet(OfxParamHandle param, OfxPropertySetHandle* propHandle)
{
    if (param == nullptr || propHandle == nullptr) return kOfxStatErrBadHandle;
    *propHandle = param;
    return kOfxStatOK;
}

OfxStatus ParamGetValue(OfxParamHandle paramHandle, ...)
{
    if (paramHandle == nullptr) return kOfxStatErrBadHandle;
    auto* properties = static_cast<OfxPropertySet*>(paramHandle);
    const char* typeStr = nullptr;
    if (! properties->getString(kOfxParamPropType, 0, &typeStr))
        return kOfxStatErrValue;
    auto count = ComponentCountForType(typeStr);

    va_list args;
    va_start(args, paramHandle);
    for (int i = 0; i < count; ++i)
    {
        auto* out = va_arg(args, double*);
        double stored = 0.0;
        properties->getDouble("_value", i, &stored);
        *out = stored;
    }
    va_end(args);
    return kOfxStatOK;
}

// time is accepted but ignored - no animation curve storage exists yet (see paramGetNumKeys and
// friends below, all explicitly unsupported for the same reason); this returns the param's one
// current value regardless of what time is asked for.
OfxStatus ParamGetValueAtTime(OfxParamHandle paramHandle, double time, ...)
{
    (void) time;
    if (paramHandle == nullptr) return kOfxStatErrBadHandle;
    auto* properties = static_cast<OfxPropertySet*>(paramHandle);
    const char* typeStr = nullptr;
    if (! properties->getString(kOfxParamPropType, 0, &typeStr))
        return kOfxStatErrValue;
    auto count = ComponentCountForType(typeStr);

    va_list args;
    va_start(args, time);
    for (int i = 0; i < count; ++i)
    {
        auto* out = va_arg(args, double*);
        double stored = 0.0;
        properties->getDouble("_value", i, &stored);
        *out = stored;
    }
    va_end(args);
    return kOfxStatOK;
}

OfxStatus ParamGetDerivative(OfxParamHandle, double, ...) { return kOfxStatErrUnsupported; }
OfxStatus ParamGetIntegral(OfxParamHandle, double, double, ...) { return kOfxStatErrUnsupported; }

OfxStatus ParamSetValue(OfxParamHandle paramHandle, ...)
{
    if (paramHandle == nullptr) return kOfxStatErrBadHandle;
    auto* properties = static_cast<OfxPropertySet*>(paramHandle);
    const char* typeStr = nullptr;
    if (! properties->getString(kOfxParamPropType, 0, &typeStr))
        return kOfxStatErrValue;
    auto count = ComponentCountForType(typeStr);

    va_list args;
    va_start(args, paramHandle);
    for (int i = 0; i < count; ++i)
    {
        auto value = va_arg(args, double);
        properties->setDouble("_value", i, value);
    }
    va_end(args);
    return kOfxStatOK;
}

OfxStatus ParamSetValueAtTime(OfxParamHandle paramHandle, double time, ...)
{
    (void) time;
    if (paramHandle == nullptr) return kOfxStatErrBadHandle;
    auto* properties = static_cast<OfxPropertySet*>(paramHandle);
    const char* typeStr = nullptr;
    if (! properties->getString(kOfxParamPropType, 0, &typeStr))
        return kOfxStatErrValue;
    auto count = ComponentCountForType(typeStr);

    va_list args;
    va_start(args, time);
    for (int i = 0; i < count; ++i)
    {
        auto value = va_arg(args, double);
        properties->setDouble("_value", i, value);
    }
    va_end(args);
    return kOfxStatOK;
}

// Keyframe/animation-curve management - no curve storage exists yet (ParamGetValueAtTime above
// ignores `time` entirely), so these are honestly unsupported rather than silently no-op.
OfxStatus ParamGetNumKeys(OfxParamHandle, unsigned int*) { return kOfxStatErrUnsupported; }
OfxStatus ParamGetKeyTime(OfxParamHandle, unsigned int, double*) { return kOfxStatErrUnsupported; }
OfxStatus ParamGetKeyIndex(OfxParamHandle, double, int, int*) { return kOfxStatErrUnsupported; }
OfxStatus ParamDeleteKey(OfxParamHandle, double) { return kOfxStatErrUnsupported; }
OfxStatus ParamDeleteAllKeys(OfxParamHandle) { return kOfxStatErrUnsupported; }
OfxStatus ParamCopy(OfxParamHandle, OfxParamHandle, double, const double*) { return kOfxStatErrUnsupported; }
OfxStatus ParamEditBegin(OfxParamSetHandle, const char*) { return kOfxStatOK; }
OfxStatus ParamEditEnd(OfxParamSetHandle) { return kOfxStatOK; }

// ---- OfxImageEffectSuiteV1 ----

OfxStatus IEGetPropertySet(OfxImageEffectHandle imageEffect, OfxPropertySetHandle* propHandle)
{
    if (imageEffect == nullptr || propHandle == nullptr) return kOfxStatErrBadHandle;
    *propHandle = &static_cast<OfxImageEffectInstance*>(imageEffect)->properties;
    return kOfxStatOK;
}

OfxStatus IEGetParamSet(OfxImageEffectHandle imageEffect, OfxParamSetHandle* paramSet)
{
    if (imageEffect == nullptr || paramSet == nullptr) return kOfxStatErrBadHandle;
    *paramSet = &static_cast<OfxImageEffectInstance*>(imageEffect)->params;
    return kOfxStatOK;
}

OfxStatus IEClipDefine(OfxImageEffectHandle imageEffect, const char* name, OfxPropertySetHandle* propertySet)
{
    if (imageEffect == nullptr || name == nullptr) return kOfxStatErrBadHandle;
    auto* instance = static_cast<OfxImageEffectInstance*>(imageEffect);
    auto& clip = instance->clips[name];
    if (! clip)
        clip = std::make_unique<OfxPropertySet>();
    if (propertySet != nullptr) *propertySet = clip.get();
    return kOfxStatOK;
}

OfxStatus IEClipGetHandle(OfxImageEffectHandle imageEffect, const char* name, OfxImageClipHandle* clip, OfxPropertySetHandle* propertySet)
{
    if (imageEffect == nullptr || name == nullptr) return kOfxStatErrBadHandle;
    auto* instance = static_cast<OfxImageEffectInstance*>(imageEffect);
    auto it = instance->clips.find(name);
    if (it == instance->clips.end())
        return kOfxStatErrValue;
    if (clip != nullptr) *clip = it->second.get();
    if (propertySet != nullptr) *propertySet = it->second.get();
    return kOfxStatOK;
}

OfxStatus IEClipGetPropertySet(OfxImageClipHandle clipHandle, OfxPropertySetHandle* propHandle)
{
    if (clipHandle == nullptr || propHandle == nullptr) return kOfxStatErrBadHandle;
    *propHandle = clipHandle;
    return kOfxStatOK;
}

// Real GPU frame handoff (kOfxImagePropD3D11Texture/kOfxImagePropD3D11SharedHandle) needs the
// zero-copy DXGI shared-handle wrapping this host explicitly defers (see this library's own
// scope note) - honestly unsupported rather than a fake success that would hide the gap.
OfxStatus IEClipGetImage(OfxImageClipHandle, double, const void*, OfxPropertySetHandle*) { return kOfxStatErrUnsupported; }
OfxStatus IEClipReleaseImage(OfxPropertySetHandle) { return kOfxStatErrUnsupported; }
OfxStatus IEClipGetRegionOfDefinition(OfxImageClipHandle, double, void*) { return kOfxStatErrUnsupported; }
// No cancellation mechanism is wired up yet - always reports "don't abort" rather than pretending
// to track it.
OfxStatus IEAbort(OfxImageEffectHandle) { return kOfxStatReplyNo; }
OfxStatus IEImageMemoryAlloc(OfxImageEffectHandle, size_t, void*) { return kOfxStatErrUnsupported; }
OfxStatus IEImageMemoryFree(void*) { return kOfxStatErrUnsupported; }
OfxStatus IEImageMemoryLock(void*, void**) { return kOfxStatErrUnsupported; }
OfxStatus IEImageMemoryUnlock(void*) { return kOfxStatErrUnsupported; }

const void* FetchSuite(OfxPropertySetHandle /*host*/, const char* suiteName, int suiteVersion)
{
    if (suiteName == nullptr || suiteVersion != 1)
        return nullptr;

    const std::string name = suiteName;
    if (name == kOfxPropertySuite) return GetPropertySuiteV1();
    if (name == kOfxParameterSuite) return GetParameterSuiteV1();
    if (name == kOfxImageEffectSuite) return GetImageEffectSuiteV1();
    return nullptr;
}
}

const OfxPropertySuiteV1* GetPropertySuiteV1()
{
    static const OfxPropertySuiteV1 suite {
        PropSetPointer, PropSetString, PropSetDouble, PropSetInt,
        PropSetPointerN, PropSetStringN, PropSetDoubleN, PropSetIntN,
        PropGetPointer, PropGetString, PropGetDouble, PropGetInt,
        PropGetPointerN, PropGetStringN, PropGetDoubleN, PropGetIntN,
        PropReset, PropGetDimension
    };
    return &suite;
}

const OfxParameterSuiteV1* GetParameterSuiteV1()
{
    static const OfxParameterSuiteV1 suite {
        ParamDefine, ParamGetHandle, ParamSetGetPropertySet, ParamGetPropertySet,
        ParamGetValue, ParamGetValueAtTime, ParamGetDerivative, ParamGetIntegral,
        ParamSetValue, ParamSetValueAtTime,
        ParamGetNumKeys, ParamGetKeyTime, ParamGetKeyIndex, ParamDeleteKey, ParamDeleteAllKeys,
        ParamCopy, ParamEditBegin, ParamEditEnd
    };
    return &suite;
}

const OfxImageEffectSuiteV1* GetImageEffectSuiteV1()
{
    static const OfxImageEffectSuiteV1 suite {
        IEGetPropertySet, IEGetParamSet,
        IEClipDefine, IEClipGetHandle, IEClipGetPropertySet,
        IEClipGetImage, IEClipReleaseImage, IEClipGetRegionOfDefinition,
        IEAbort,
        IEImageMemoryAlloc, IEImageMemoryFree, IEImageMemoryLock, IEImageMemoryUnlock
    };
    return &suite;
}

OfxHost MakeHost()
{
    static OfxHost host { nullptr, FetchSuite };
    return host;
}
}
