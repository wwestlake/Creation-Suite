// The path-based half of PluginRuntime: loading and reloading a plugin from a file on a real disk.
// Its own object file: an application that keeps sources in the VFS uses loadSource / loadFromEnvironment
// and never links this. Older hosts that load by path get the disk installed here, once, before the first
// load (the FRust libraries themselves touch no file).

#include "creation/frust/PluginRuntime.h"

#include <DiskLoader.h>

#include <mutex>

namespace creation::frust
{
namespace
{
void useTheDisk()
{
    static std::once_flag once;
    std::call_once(once, [] { ::frust::InstallDiskResolvers(); });
}
}

bool PluginRuntime::load(const std::string& pluginPath, std::string& error)
{
    unload(defaultPluginKey);
    return load(defaultPluginKey, pluginPath, error);
}

bool PluginRuntime::reload(std::string& error)
{
    return reload(defaultPluginKey, error);
}

bool PluginRuntime::load(const std::string& key, const std::string& pluginPath, std::string& error)
{
    useTheDisk();
    if (key.empty())
    {
        error = "A FRust plugin needs a non-empty runtime key.";
        return false;
    }
    if (plugins.contains(key))
    {
        error = "FRust plugin '" + key + "' is already loaded. Reload or unload it explicitly.";
        errors[key] = error;
        return false;
    }

    const auto plugin = frust_plugin_load(pluginPath.c_str());
    if (plugin == nullptr)
    {
        error = frust_plugin_last_error();
        errors[key] = error;
        return false;
    }

    frust_plugin_call_on_init(plugin);
    plugins.emplace(key, plugin);
    errors.erase(key);
    return true;
}

bool PluginRuntime::reload(const std::string& key, std::string& error)
{
    useTheDisk();
    const auto found = plugins.find(key);
    if (found == plugins.end())
    {
        error = "Cannot reload FRust plugin '" + key + "' because it is not loaded.";
        errors[key] = error;
        return false;
    }

    const auto reloadedPlugin = frust_plugin_reload(found->second);
    if (reloadedPlugin == nullptr)
    {
        error = frust_plugin_last_error();
        errors[key] = error;
        plugins.erase(found);
        return false;
    }

    found->second = reloadedPlugin;
    errors.erase(key);
    return true;
}

}
