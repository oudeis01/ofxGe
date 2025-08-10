#include "PluginManager.h"
#include <iostream>
#include <algorithm>
#include <string>

bool PluginManager::loadPlugin(const std::string& plugin_path, const std::string& alias) {
    // Load the dynamic library using dlopen. RTLD_LAZY resolves symbols as code is executed.
    void* handle = dlopen(plugin_path.c_str(), RTLD_LAZY);
    if (!handle) {
        std::cerr << "Cannot load plugin: " << dlerror() << std::endl;
        return false;
    }

    // Check for ABI version compatibility between the manager and the plugin.
    typedef int (*get_abi_version_t)();
    get_abi_version_t get_abi_version = (get_abi_version_t) dlsym(handle, "getPluginABIVersion");
    if (get_abi_version) {
        int plugin_abi = get_abi_version();
        if (plugin_abi != PLUGIN_ABI_VERSION) {
            std::cerr << "Plugin ABI version mismatch. Expected: " << PLUGIN_ABI_VERSION 
                      << ", Got: " << plugin_abi << std::endl;
            dlclose(handle);
            return false;
        }
    }
    
    // Look for the required factory and info functions in the library.
    typedef IPluginInterface* (*create_plugin_t)();
    typedef const char* (*get_info_t)();
    
    create_plugin_t create_plugin = (create_plugin_t) dlsym(handle, "createPlugin");
    get_info_t get_info = (get_info_t) dlsym(handle, "getPluginInfo");
    
    if (!create_plugin || !get_info) {
        std::cerr << "Invalid plugin format: missing required symbols (createPlugin or getPluginInfo)" << std::endl;
        dlclose(handle);
        return false;
    }
    
    // Create an instance of the plugin.
    IPluginInterface* plugin = create_plugin();
    if (!plugin) {
        std::cerr << "Failed to create plugin instance" << std::endl;
        dlclose(handle);
        return false;
    }
    
    // Set the plugin's data directory path so it can find its own resources.
    std::string plugin_data_dir = extractPluginDirectory(plugin_path);
    std::cout << "[PluginManager] Setting plugin data path: " << plugin_data_dir << std::endl;
    plugin->setPath(plugin_data_dir);
    
    // Determine the alias for the plugin.
    std::string plugin_alias = alias.empty() ? plugin->getName() : alias;
    
    // Prevent loading a plugin with an alias that is already in use.
    if (loaded_plugins.find(plugin_alias) != loaded_plugins.end()) {
        std::cerr << "Plugin with alias '" << plugin_alias << "' already loaded." << std::endl;
        typedef void (*destroy_plugin_t)(IPluginInterface*);
        destroy_plugin_t destroy_plugin = (destroy_plugin_t) dlsym(handle, "destroyPlugin");
        if (destroy_plugin) {
            destroy_plugin(plugin);
        }
        dlclose(handle);
        return false;
    }
    
    // Store the new plugin in the map.
    loaded_plugins[plugin_alias] = std::make_unique<LoadedPlugin>(handle, plugin, plugin_path);
    
    std::cout << "Loaded plugin: " << plugin->getName() 
              << " v" << plugin->getVersion() 
              << " by " << plugin->getAuthor()
              << " (" << plugin->getFunctionCount() << " functions)" << std::endl;
    
    return true;
}

void PluginManager::unloadPlugin(const std::string& alias) {
    auto it = loaded_plugins.find(alias);
    if (it != loaded_plugins.end()) {
        std::cout << "Unloading plugin: " << alias << std::endl;
        // The unique_ptr's destructor will handle cleanup via ~LoadedPlugin().
        loaded_plugins.erase(it);
    }
}

void PluginManager::unloadAllPlugins() {
    std::cout << "Unloading all plugins..." << std::endl;
    loaded_plugins.clear();
}

bool PluginManager::isPluginLoaded(const std::string& alias) const {
    return loaded_plugins.find(alias) != loaded_plugins.end();
}

const GLSLFunction* PluginManager::findFunction(const std::string& function_name) {
    // Search across all loaded plugins.
    for (const auto& [alias, plugin] : loaded_plugins) {
        if (const GLSLFunction* func = plugin->interface->findFunction(function_name)) {
            return func;
        }
    }
    return nullptr;
}

const GLSLFunction* PluginManager::findFunction(const std::string& plugin_name, const std::string& function_name) {
    auto it = loaded_plugins.find(plugin_name);
    if (it != loaded_plugins.end()) {
        return it->second->interface->findFunction(function_name);
    }
    return nullptr;
}

std::vector<std::string> PluginManager::getLoadedPlugins() const {
    std::vector<std::string> plugins;
    for (const auto& [alias, plugin] : loaded_plugins) {
        plugins.push_back(alias + " (" + plugin->name + " v" + plugin->version + ")");
    }
    return plugins;
}

std::vector<std::string> PluginManager::getAllFunctions() const {
    std::vector<std::string> all_functions;
    for (const auto& [alias, plugin] : loaded_plugins) {
        auto plugin_functions = plugin->interface->getAllFunctionNames();
        for (const auto& func : plugin_functions) {
            all_functions.push_back(alias + "::" + func);
        }
    }
    return all_functions;
}

std::map<std::string, std::vector<std::string>> PluginManager::getFunctionsByPlugin() const {
    std::map<std::string, std::vector<std::string>> result;
    for (const auto& [alias, plugin] : loaded_plugins) {
        result[alias] = plugin->interface->getAllFunctionNames();
    }
    return result;
}

std::map<std::string, PluginInfo> PluginManager::getPluginInfos() const {
    std::map<std::string, PluginInfo> result;
    for (const auto& [alias, plugin] : loaded_plugins) {
        result[alias] = plugin->interface->getPluginInfo();
    }
    return result;
}

std::map<std::string, std::string> PluginManager::getPluginPaths() const {
    std::map<std::string, std::string> result;
    for (const auto& [alias, plugin] : loaded_plugins) {
        result[alias] = plugin->interface->getPath();
    }
    return result;
}

std::vector<const GLSLFunction*> PluginManager::findFunctionsByCategory(const std::string& category) {
    std::vector<const GLSLFunction*> result;
    for (const auto& [alias, plugin] : loaded_plugins) {
        auto category_functions = plugin->interface->getFunctionsByCategory(category);
        for (const auto& func_name : category_functions) {
            if (const GLSLFunction* func = plugin->interface->findFunction(func_name)) {
                result.push_back(func);
            }
        }
    }
    return result;
}

std::vector<const GLSLFunction*> PluginManager::findFunctionsByReturnType(const std::string& returnType) {
    std::vector<const GLSLFunction*> result;
    for (const auto& [alias, plugin] : loaded_plugins) {
        auto typed_functions = plugin->interface->findFunctionsByReturnType(returnType);
        result.insert(result.end(), typed_functions.begin(), typed_functions.end());
    }
    return result;
}

std::map<std::string, size_t> PluginManager::getPluginStatistics() const {
    std::map<std::string, size_t> stats;
    for (const auto& [alias, plugin] : loaded_plugins) {
        stats[alias] = plugin->interface->getFunctionCount();
    }
    return stats;
}

IPluginInterface* PluginManager::getPlugin(const std::string& alias) {
    auto it = loaded_plugins.find(alias);
    return (it != loaded_plugins.end()) ? it->second->interface : nullptr;
}

const IPluginInterface* PluginManager::getPlugin(const std::string& alias) const {
    auto it = loaded_plugins.find(alias);
    return (it != loaded_plugins.end()) ? it->second->interface : nullptr;
}

std::string PluginManager::extractPluginDirectory(const std::string& plugin_lib_path) const {
    // Example plugin_lib_path: "/path/to/bin/data/plugins/lygia-plugin/libLygiaPlugin.so"
    // Returns: "/path/to/bin/data/plugins/lygia-plugin/"
    
    size_t last_slash = plugin_lib_path.find_last_of('/');
    if (last_slash != std::string::npos) {
        return plugin_lib_path.substr(0, last_slash + 1); // Include the trailing '/'
    }
    
    // If no slash is found, return the current directory.
    return "./";
}