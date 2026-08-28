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

bool PluginRuntime::reload(std::string& error)
{
    if (plugin == nullptr)
    {
        error = "Cannot reload a FRust plugin before it has been loaded.";
        return false;
    }

    auto* reloadedPlugin = frust_plugin_reload(plugin);
    if (reloadedPlugin == nullptr)
    {
        plugin = nullptr;
        error = frust_plugin_last_error();
        return false;
    }

    plugin = reloadedPlugin;
    return true;
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

std::int64_t PluginRuntime::callEvent(std::int64_t id, std::int64_t argument) const
{
    return plugin != nullptr ? frust_plugin_call_on_event(plugin, id, argument) : 0;
}

void PluginRuntime::fireEvent(const char* name, void* payload) const
{
    frust_fire_event(name, payload);
}

bool PluginRuntime::isLoaded() const noexcept
{
    return plugin != nullptr;
}
}
