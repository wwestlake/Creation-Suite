#pragma once

#include <frust_plugin_host/FrustPluginHost.h>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace creation::frust
{
class PluginRuntime final
{
public:
    struct PluginEventResult
    {
        std::string key;
        std::int64_t value = 0;
    };

    struct NodeLibraryManifest
    {
        std::string id;
        std::string descriptorJson;
    };

    static constexpr const char* defaultPluginKey = "default";

    explicit PluginRuntime(std::string applicationIdentity);
    ~PluginRuntime();

    PluginRuntime(const PluginRuntime&) = delete;
    PluginRuntime& operator=(const PluginRuntime&) = delete;

    void registerHostFunction(const char* name, void* function);

    // Legacy one-plugin convenience API. It owns only the named default slot.
    bool load(const std::string& pluginPath, std::string& error);
    bool reload(std::string& error);
    void unload();
    [[nodiscard]] void* getFunction(const char* name) const;
    [[nodiscard]] std::int64_t callEvent(std::int64_t id, std::int64_t argument) const;
    [[nodiscard]] bool isLoaded() const noexcept;

    // Keyed API for real application plugin hosting. A keyed load never
    // replaces another plugin; callers must reload or unload that key first.
    bool load(const std::string& key, const std::string& pluginPath, std::string& error);
    bool reload(const std::string& key, std::string& error);
    void unload(const std::string& key);
    void unloadAll();
    [[nodiscard]] void* getFunction(const std::string& key, const char* name) const;
    [[nodiscard]] std::int64_t callEvent(const std::string& key, std::int64_t id, std::int64_t argument) const;
    [[nodiscard]] std::vector<PluginEventResult> callEventAll(std::int64_t id, std::int64_t argument) const;
    [[nodiscard]] bool isLoaded(const std::string& key) const noexcept;
    [[nodiscard]] std::vector<std::string> loadedPluginKeys() const;
    [[nodiscard]] std::string lastError(const std::string& key) const;
    [[nodiscard]] std::vector<NodeLibraryManifest> nodeLibraries(const std::string& key) const;

    // Loading from source text, for hosts that keep sources somewhere other than
    // the disk (Station keeps them in its VFS container). `siblingFiles` is
    // asked for the files a `use self::x;` names ("x.frust", then "x.fr");
    // return false if there is no such file. It may be empty for a single-file
    // plugin. Nothing here opens a file.
    using SourceFiles = std::function<bool(const std::string& fileName, std::string& text)>;
    bool loadSource(const std::string& key, const std::string& name, const std::string& sourceText,
                    const SourceFiles& siblingFiles, std::string& error);
    bool reloadSource(const std::string& key, const std::string& sourceText,
                      const SourceFiles& siblingFiles, std::string& error);

    void fireEvent(const char* name, void* payload = nullptr) const;

private:
    std::map<std::string, FrustPluginHandle, std::less<>> plugins;
    std::map<std::string, std::string, std::less<>> errors;
};
}
