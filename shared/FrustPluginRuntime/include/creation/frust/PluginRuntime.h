#pragma once

#include <frust_plugin_host/FrustPluginHost.h>

#include <cstdint>
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
    bool reload(std::string& error);
    void unload();
    [[nodiscard]] void* getFunction(const char* name) const;
    [[nodiscard]] std::int64_t callEvent(std::int64_t id, std::int64_t argument) const;
    void fireEvent(const char* name, void* payload = nullptr) const;
    [[nodiscard]] bool isLoaded() const noexcept;

private:
    FrustPluginHandle plugin = nullptr;
};
}
