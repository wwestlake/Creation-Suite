#pragma once

#include "creation/assets/ProjectSession.h"
#include "creation/suite/SuiteSettings.h"

namespace creation::assets
{
// The suite's own reserved project: houses suite-wide data (AI provider
// settings, auth session, etc.) as ordinary container entries, using the
// exact same ProjectSession API every real project uses. Not a special
// storage mechanism -- SuiteAppDomain::suite is what keeps this out of
// every app's own project listing, nothing else about it is different.
//
// Only ever opened by the VFS service (services/VfsService) -- apps never
// open this container directly, the same way they never open any other
// project's container directly once they're clients of the service.
class SuiteRootProject final
{
public:
    static constexpr const char* kProjectName = "Suite";

    static bool openOrCreate(const creation::suite::SuiteSettings& settings,
                             ProjectSession& outSession,
                             juce::String& errorMessage);
};
}
