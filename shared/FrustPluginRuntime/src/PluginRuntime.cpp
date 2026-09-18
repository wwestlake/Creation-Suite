#include "creation/frust/PluginRuntime.h"

namespace creation::frust
{
PluginRuntime::PluginRuntime(std::string applicationIdentity)
{
    frust_plugin_host_set_application_identity(applicationIdentity.c_str());
}

PluginRuntime::~PluginRuntime()
{
    unloadAll();
}

void PluginRuntime::registerHostFunction(const char* name, void* function)
{
    frust_plugin_register_host_function(name, function);
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

void PluginRuntime::unload()
{
    unload(defaultPluginKey);
}

void* PluginRuntime::getFunction(const char* name) const
{
    return getFunction(defaultPluginKey, name);
}

std::int64_t PluginRuntime::callEvent(std::int64_t id, std::int64_t argument) const
{
    return callEvent(defaultPluginKey, id, argument);
}

bool PluginRuntime::isLoaded() const noexcept
{
    return isLoaded(defaultPluginKey);
}

bool PluginRuntime::load(const std::string& key, const std::string& pluginPath, std::string& error)
{
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

void PluginRuntime::unload(const std::string& key)
{
    const auto found = plugins.find(key);
    if (found == plugins.end())
        return;

    frust_plugin_call_on_unload(found->second);
    frust_plugin_unload(found->second);
    plugins.erase(found);
    errors.erase(key);
}

void PluginRuntime::unloadAll()
{
    while (!plugins.empty())
    {
        unload(plugins.begin()->first);
    }
    errors.clear();
}

void* PluginRuntime::getFunction(const std::string& key, const char* name) const
{
    const auto found = plugins.find(key);
    return found != plugins.end() ? frust_plugin_get_fn(found->second, name) : nullptr;
}

std::int64_t PluginRuntime::callEvent(const std::string& key, std::int64_t id, std::int64_t argument) const
{
    const auto found = plugins.find(key);
    return found != plugins.end() ? frust_plugin_call_on_event(found->second, id, argument) : 0;
}

std::vector<PluginRuntime::PluginEventResult> PluginRuntime::callEventAll(std::int64_t id, std::int64_t argument) const
{
    std::vector<PluginEventResult> results;
    results.reserve(plugins.size());
    for (const auto& [key, plugin] : plugins)
    {
        results.push_back({ key, frust_plugin_call_on_event(plugin, id, argument) });
    }
    return results;
}

bool PluginRuntime::isLoaded(const std::string& key) const noexcept
{
    return plugins.contains(key);
}

std::vector<std::string> PluginRuntime::loadedPluginKeys() const
{
    std::vector<std::string> keys;
    keys.reserve(plugins.size());
    for (const auto& [key, plugin] : plugins)
    {
        (void) plugin;
        keys.push_back(key);
    }
    return keys;
}

std::string PluginRuntime::lastError(const std::string& key) const
{
    const auto found = errors.find(key);
    return found != errors.end() ? found->second : std::string{};
}

std::vector<PluginRuntime::NodeLibraryManifest> PluginRuntime::nodeLibraries(const std::string& key) const
{
    const auto found = plugins.find(key);
    if (found == plugins.end())
        return {};

    FrustPluginManifestHandle manifest = frust_plugin_get_manifest(found->second);
    if (manifest == nullptr)
        return {};

    std::vector<NodeLibraryManifest> libraries;
    const int32_t count = frust_plugin_manifest_node_library_count(manifest);
    libraries.reserve(static_cast<std::size_t>(count));
    for (int32_t index = 0; index < count; ++index)
    {
        const char* id = frust_plugin_manifest_node_library_id(manifest, index);
        const char* descriptorJson = frust_plugin_manifest_node_library_json(manifest, index);
        if (id != nullptr && descriptorJson != nullptr)
            libraries.push_back({ id, descriptorJson });
    }
    frust_plugin_manifest_free(manifest);
    return libraries;
}

void PluginRuntime::fireEvent(const char* name, void* payload) const
{
    frust_fire_event(name, payload);
}

}
