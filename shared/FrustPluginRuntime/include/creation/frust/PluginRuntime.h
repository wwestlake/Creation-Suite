#pragma once

#include <frust_plugin_host/FrustPluginHost.h>

#include <string>

namespace creation::frust
{
class PluginRuntime final
{
public:
    explicit PluginRuntime(std::string applicationIdentity);
    ~PluginRuntime();

    PluginRuntime(const PluginRuntime&) = delete;
    PluginRuntime& operator=(const PluginRuntime&) = delete;

    void registerHostFunction(const char* name, void* function);
    bool load(const std::string& pluginPath, std::string& error);
    void unload();
    [[nodiscard]] void* getFunction(const char* name) const;
    [[nodiscard]] bool isLoaded() const noexcept;

private:
    FrustPluginHandle plugin = nullptr;
};
}
