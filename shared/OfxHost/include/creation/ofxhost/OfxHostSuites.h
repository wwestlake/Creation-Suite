#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <ofxCore.h>
#include <ofxProperty.h>
#include <ofxParam.h>
#include <ofxImageEffect.h>
#include <creation/ofxhost/OfxPropertySet.h>

namespace creation::ofxhost
{
// A plugin instance's parameters. Definition/query/property-set access is fully implemented;
// paramGetValue/paramSetValue/paramGetValueAtTime/paramSetValueAtTime (declared variadic in the
// real OFX spec, since a param's value shape depends on its type) are implemented for the double
// case only in this first pass, covering OFX's most common param types (Double/Double2D/
// Double3D) - Int/Boolean/String/Choice value get/set need extending when a real plugin
// integration actually needs them. This is intentionally a scoped-but-genuinely-functional
// first pass, not a total stub: see OfxHostSuites.cpp's own note.
class OfxParamSet
{
public:
    OfxPropertySetHandle define(const std::string& type, const std::string& name);
    OfxPropertySetHandle findHandle(const std::string& name) const;
    OfxPropertySet* find(const std::string& name) const;

private:
    struct ParamEntry
    {
        std::string type;
        std::unique_ptr<OfxPropertySet> properties = std::make_unique<OfxPropertySet>();
    };

    std::unordered_map<std::string, ParamEntry> params;
};

// Minimal backing object for OfxImageEffectHandle - what a plugin instance's suite calls
// (getPropertySet/getParamSet/clipDefine/clipGetHandle) actually operate on. No real plugin
// loading/instantiation is wired up yet (see this library's own CMakeLists/README scope note) -
// tests and future integration code construct one of these directly.
struct OfxImageEffectInstance
{
    OfxPropertySet properties;
    OfxParamSet params;
    std::unordered_map<std::string, std::unique_ptr<OfxPropertySet>> clips;
};

// Singleton suite instances backed by this host's OfxPropertySet/OfxParamSet implementation -
// pass through OfxHost::fetchSuite (see MakeHost) so a plugin's setHost() call gets exactly what
// a real host would hand it.
const OfxPropertySuiteV1* GetPropertySuiteV1();
const OfxParameterSuiteV1* GetParameterSuiteV1();
const OfxImageEffectSuiteV1* GetImageEffectSuiteV1();

// Builds an OfxHost whose fetchSuite resolves "OfxPropertySuite"/"OfxParameterSuite"/
// "OfxImageEffectSuite" (version 1) to the three suites above, and null for anything else -
// matches the real plugin-load handshake shape (a plugin's setHost() receives this, then calls
// fetchSuite once per suite it needs during kOfxActionLoad).
OfxHost MakeHost();
}
