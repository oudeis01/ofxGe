#pragma once
#include "../glsl-plugin-interface/include/IPluginInterface.h"
#include <unordered_map>
#include <memory>
#include <dlfcn.h>

/**
 * @brief Manages loading and unloading of GLSL shader library plugins
 */
class PluginManager {
private:
    struct LoadedPlugin {
        void* handle;                    // dlopen handle
        IPluginInterface* interface;     // Plugin instance
        std::string name;
        std::string version;
        std::string author;
        std::string path;
        
        LoadedPlugin() : handle(nullptr), interface(nullptr) {}
        
        LoadedPlugin(void* h, IPluginInterface* iface, const std::string& p)
            : handle(h), interface(iface), path(p) {
            if (interface) {
                name = interface->getName();
                version = interface->getVersion();
                author = interface->getAuthor();
            }
        }
        
        ~LoadedPlugin() {
            if (interface && handle) {
                // Get destroy function
                typedef void (*destroy_plugin_t)(IPluginInterface*);
                destroy_plugin_t destroy_plugin = (destroy_plugin_t) dlsym(handle, "destroyPlugin");
                if (destroy_plugin) {
                    destroy_plugin(interface);
                }
                dlclose(handle);
            }
        }
    };
    
    std::unordered_map<std::string, std::unique_ptr<LoadedPlugin>> loaded_plugins;
    
public:
    PluginManager() = default;
    ~PluginManager() { unloadAllPlugins(); }
    
    // Plugin loading/unloading
    bool loadPlugin(const std::string& plugin_path, const std::string& alias = "");
    void unloadPlugin(const std::string& alias);
    void unloadAllPlugins();
    bool isPluginLoaded(const std::string& alias) const;
    
    // Function search across all plugins
    const GLSLFunction* findFunction(const std::string& function_name);
    const GLSLFunction* findFunction(const std::string& plugin_name, const std::string& function_name);
    
    // Plugin information
    std::vector<std::string> getLoadedPlugins() const;
    std::vector<std::string> getAllFunctions() const;
    std::map<std::string, std::vector<std::string>> getFunctionsByPlugin() const;
    
    // Advanced queries
    std::vector<const GLSLFunction*> findFunctionsByCategory(const std::string& category);
    std::vector<const GLSLFunction*> findFunctionsByReturnType(const std::string& returnType);
    std::map<std::string, size_t> getPluginStatistics() const;
    
    // Plugin access
    IPluginInterface* getPlugin(const std::string& alias);
    const IPluginInterface* getPlugin(const std::string& alias) const;
};
