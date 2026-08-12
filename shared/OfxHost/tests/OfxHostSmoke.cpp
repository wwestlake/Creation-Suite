// Verifies this host's own three suite implementations are internally consistent (property
// round-trips, param define/get/set, image-effect clip/param-set wiring, fetchSuite resolution).
// Deliberately does NOT load a real .ofx plugin - there isn't one available in this environment
// to test against, so plugin discovery/lifecycle/zero-copy interop are explicitly out of scope
// here (see shared/OfxHost's own README/plan note). This proves the host's plumbing is sound,
// not that a real third-party plugin will actually run against it.

#include <creation/ofxhost/OfxHostSuites.h>
#include <creation/ofxhost/OfxPropertySet.h>

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace
{
void fail(const std::string& message)
{
    std::cerr << message << std::endl;
    throw std::runtime_error(message);
}
}

int main()
{
    try
    {
        // ---- OfxPropertySuiteV1 round-trip ----
        auto* propertySuite = creation::ofxhost::GetPropertySuiteV1();
        creation::ofxhost::OfxPropertySet rawProperties;
        OfxPropertySetHandle propsHandle = &rawProperties;

        if (propertySuite->propSetString(propsHandle, kOfxPropName, 0, "TestEffect") != kOfxStatOK)
            fail("propSetString failed");
        char* nameOut = nullptr;
        if (propertySuite->propGetString(propsHandle, kOfxPropName, 0, &nameOut) != kOfxStatOK
            || std::strcmp(nameOut, "TestEffect") != 0)
            fail("propGetString round-trip failed");

        if (propertySuite->propSetDouble(propsHandle, kOfxPropTime, 0, 12.5) != kOfxStatOK)
            fail("propSetDouble failed");
        double timeOut = 0.0;
        if (propertySuite->propGetDouble(propsHandle, kOfxPropTime, 0, &timeOut) != kOfxStatOK || timeOut != 12.5)
            fail("propGetDouble round-trip failed");

        double renderScale[] = { 0.5, 0.5 };
        if (propertySuite->propSetDoubleN(propsHandle, kOfxImageEffectPropRenderScale, 2, renderScale) != kOfxStatOK)
            fail("propSetDoubleN failed");
        double renderScaleOut[2] = { 0.0, 0.0 };
        if (propertySuite->propGetDoubleN(propsHandle, kOfxImageEffectPropRenderScale, 2, renderScaleOut) != kOfxStatOK
            || renderScaleOut[0] != 0.5 || renderScaleOut[1] != 0.5)
            fail("propGetDoubleN round-trip failed");

        int dimension = 0;
        if (propertySuite->propGetDimension(propsHandle, kOfxImageEffectPropRenderScale, &dimension) != kOfxStatOK || dimension != 2)
            fail("propGetDimension failed");

        // ---- OfxParameterSuiteV1: define, set, get ----
        auto* paramSuite = creation::ofxhost::GetParameterSuiteV1();
        creation::ofxhost::OfxParamSet rawParamSet;
        OfxParamSetHandle paramSetHandle = &rawParamSet;

        OfxPropertySetHandle exposureProps = nullptr;
        if (paramSuite->paramDefine(paramSetHandle, kOfxParamTypeDouble, "exposure", &exposureProps) != kOfxStatOK)
            fail("paramDefine failed");

        OfxParamHandle exposureHandle = nullptr;
        if (paramSuite->paramGetHandle(paramSetHandle, "exposure", &exposureHandle, nullptr) != kOfxStatOK)
            fail("paramGetHandle failed");

        if (paramSuite->paramSetValue(exposureHandle, 0.75) != kOfxStatOK)
            fail("paramSetValue failed");

        double exposureOut = 0.0;
        if (paramSuite->paramGetValue(exposureHandle, &exposureOut) != kOfxStatOK || exposureOut != 0.75)
            fail("paramGetValue round-trip failed");

        // An RGB param carries 3 components - proves the double-only path handles the
        // multi-component case, not just the scalar one.
        OfxPropertySetHandle tintProps = nullptr;
        if (paramSuite->paramDefine(paramSetHandle, kOfxParamTypeRGB, "tint", &tintProps) != kOfxStatOK)
            fail("paramDefine (RGB) failed");
        OfxParamHandle tintHandle = nullptr;
        if (paramSuite->paramGetHandle(paramSetHandle, "tint", &tintHandle, nullptr) != kOfxStatOK)
            fail("paramGetHandle (RGB) failed");
        if (paramSuite->paramSetValue(tintHandle, 1.0, 0.5, 0.25) != kOfxStatOK)
            fail("paramSetValue (RGB) failed");
        double r = 0.0, g = 0.0, b = 0.0;
        if (paramSuite->paramGetValue(tintHandle, &r, &g, &b) != kOfxStatOK || r != 1.0 || g != 0.5 || b != 0.25)
            fail("paramGetValue (RGB) round-trip failed");

        // ---- OfxImageEffectSuiteV1: property set / param set / clip wiring ----
        auto* imageEffectSuite = creation::ofxhost::GetImageEffectSuiteV1();
        creation::ofxhost::OfxImageEffectInstance instance;
        OfxImageEffectHandle instanceHandle = &instance;

        OfxPropertySetHandle instanceProps = nullptr;
        if (imageEffectSuite->getPropertySet(instanceHandle, &instanceProps) != kOfxStatOK || instanceProps != &instance.properties)
            fail("getPropertySet failed");

        OfxParamSetHandle instanceParamSet = nullptr;
        if (imageEffectSuite->getParamSet(instanceHandle, &instanceParamSet) != kOfxStatOK || instanceParamSet != &instance.params)
            fail("getParamSet failed");

        OfxPropertySetHandle sourceClipProps = nullptr;
        if (imageEffectSuite->clipDefine(instanceHandle, "Source", &sourceClipProps) != kOfxStatOK)
            fail("clipDefine failed");

        OfxImageClipHandle sourceClip = nullptr;
        OfxPropertySetHandle sourceClipPropsAgain = nullptr;
        if (imageEffectSuite->clipGetHandle(instanceHandle, "Source", &sourceClip, &sourceClipPropsAgain) != kOfxStatOK
            || sourceClip != sourceClipProps)
            fail("clipGetHandle failed");

        // ---- OfxHost::fetchSuite resolves all three by name/version ----
        auto host = creation::ofxhost::MakeHost();
        if (host.fetchSuite(nullptr, kOfxPropertySuite, 1) != propertySuite)
            fail("fetchSuite(PropertySuite) mismatch");
        if (host.fetchSuite(nullptr, kOfxParameterSuite, 1) != paramSuite)
            fail("fetchSuite(ParameterSuite) mismatch");
        if (host.fetchSuite(nullptr, kOfxImageEffectSuite, 1) != imageEffectSuite)
            fail("fetchSuite(ImageEffectSuite) mismatch");
        if (host.fetchSuite(nullptr, "NotARealSuite", 1) != nullptr)
            fail("fetchSuite should return null for an unknown suite name");
        if (host.fetchSuite(nullptr, kOfxPropertySuite, 99) != nullptr)
            fail("fetchSuite should return null for an unsupported version");

        std::cout << "OfxHostSmoke passed" << std::endl;
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "OfxHostSmoke failed: " << ex.what() << std::endl;
        return 1;
    }
}
