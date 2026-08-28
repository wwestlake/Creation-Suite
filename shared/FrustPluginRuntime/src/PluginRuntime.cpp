#include "creation/frust/PluginRuntime.h"

namespace creation::frust
{
PluginRuntime::PluginRuntime(std::string applicationIdentity)
{
    frust_plugin_host_set_application_identity(applicationIdentity.c_str());
}

PluginRuntime::~PluginRuntime()
{
    unload();
}

void PluginRuntime::registerHostFunction(const char* name, void* function)
{
    frust_plugin_register_host_function(name, function);
}

bool PluginRuntime::load(const std::string& pluginPath, std::string& error)
{
    unload();
    plugin = frust_plugin_load(pluginPath.c_str());
    if (plugin != nullptr)
    {
        frust_plugin_call_on_init(plugin);
        return true;
    }

    error = frust_plugin_last_error();
    return false;
}

void PluginRuntime::unload()
{
    if (plugin == nullptr)
        return;

    frust_plugin_call_on_unload(plugin);
    frust_plugin_unload(plugin);
    plugin = nullptr;
}

void* PluginRuntime::getFunction(const char* name) const
{
    return plugin != nullptr ? frust_plugin_get_fn(plugin, name) : nullptr;
}

bool PluginRuntime::isLoaded() const noexcept
{
    return plugin != nullptr;
}
}
