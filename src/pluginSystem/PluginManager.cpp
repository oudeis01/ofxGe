#include "PluginManager.h"
#include "ofMain.h"
#include <iostream>
#include <algorithm>
#include <string>

bool PluginManager::loadPlugin(const std::string& plugin_path, const std::string& alias) {
    ofLogNotice("PluginManager") << "Loading plugin on " << PlatformUtils::getPlatformName()
                                 << " - Path: " << plugin_path 
                                 << " - Expected extension: " << PlatformUtils::getDynamicLibraryExtension();
    
    // Load the dynamic library using cross-platform loader
    DynamicLoader::LibraryHandle lib_handle = DynamicLoader::loadLibrary(plugin_path);
    if (!lib_handle.is_valid) {
        ofLogError("PluginManager") << "Cannot load plugin: " << DynamicLoader::getLastError();
        return false;
    }

    // Check for ABI version compatibility between the manager and the plugin.
    typedef int (*get_abi_version_t)();
    get_abi_version_t get_abi_version = (get_abi_version_t) DynamicLoader::getSymbol(lib_handle, "getPluginABIVersion");
    if (get_abi_version) {
        int plugin_abi = get_abi_version();
        if (plugin_abi != PLUGIN_ABI_VERSION) {
            ofLogError("PluginManager") << "Plugin ABI version mismatch. Expected: " << PLUGIN_ABI_VERSION 
                      << ", Got: " << plugin_abi;
            DynamicLoader::LibraryHandle temp_handle = lib_handle;
            DynamicLoader::unloadLibrary(temp_handle);
            return false;
        }
    }
    
    // Look for the required factory and info functions in the library.
    typedef IPluginInterface* (*create_plugin_t)();
    typedef const char* (*get_info_t)();
    
    create_plugin_t create_plugin = (create_plugin_t) DynamicLoader::getSymbol(lib_handle, "createPlugin");
    get_info_t get_info = (get_info_t) DynamicLoader::getSymbol(lib_handle, "getPluginInfo");
    
    if (!create_plugin || !get_info) {
        ofLogError("PluginManager") << "Invalid plugin format: missing required symbols (createPlugin or getPluginInfo)";
        DynamicLoader::LibraryHandle temp_handle = lib_handle;
        DynamicLoader::unloadLibrary(temp_handle);
        return false;
    }
    
    // Create an instance of the plugin.
    IPluginInterface* plugin = create_plugin();
    if (!plugin) {
        ofLogError("PluginManager") << "Failed to create plugin instance";
        DynamicLoader::LibraryHandle temp_handle = lib_handle;
        DynamicLoader::unloadLibrary(temp_handle);
        return false;
    }
    
    // Set the plugin's data directory path so it can find its own resources.
    std::string plugin_data_dir = extractPluginDirectory(plugin_path);
    ofLogNotice("PluginManager") << "Setting plugin data path: " << plugin_data_dir;
    plugin->setPath(plugin_data_dir);
    
    // Determine the alias for the plugin.
    std::string plugin_alias = alias.empty() ? plugin->getName() : alias;
    
    // Prevent loading a plugin with an alias that is already in use.
    if (loaded_plugins.find(plugin_alias) != loaded_plugins.end()) {
        ofLogError("PluginManager") << "Plugin with alias '" << plugin_alias << "' already loaded.";
        typedef void (*destroy_plugin_t)(IPluginInterface*);
        destroy_plugin_t destroy_plugin = (destroy_plugin_t) DynamicLoader::getSymbol(lib_handle, "destroyPlugin");
        if (destroy_plugin) {
            destroy_plugin(plugin);
        }
        DynamicLoader::LibraryHandle temp_handle = lib_handle;
        DynamicLoader::unloadLibrary(temp_handle);
        return false;
    }
    
    // Store the new plugin in the map.
    loaded_plugins[plugin_alias] = std::make_unique<LoadedPlugin>(lib_handle, plugin, plugin_path);
    
    ofLogNotice("PluginManager") << "Loaded plugin: " << plugin->getName() 
                                 << " v" << plugin->getVersion() 
                                 << " by " << plugin->getAuthor()
                                 << " (" << plugin->getFunctionCount() << " functions)";
    
    // Detect and log conflicts with GLSL built-ins
    detectAndLogBuiltinConflicts(plugin_alias, plugin);
    
    return true;
}

void PluginManager::unloadPlugin(const std::string& alias) {
    auto it = loaded_plugins.find(alias);
    if (it != loaded_plugins.end()) {
        ofLogNotice("PluginManager") << "Unloading plugin: " << alias;
        // The unique_ptr's destructor will handle cleanup via ~LoadedPlugin().
        loaded_plugins.erase(it);
    }
}

void PluginManager::unloadAllPlugins() {
    ofLogNotice("PluginManager") << "Unloading all plugins...";
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
    // Example plugin_lib_path: 
    //   Linux: "/path/to/bin/data/plugins/lygia-plugin/libLygiaPlugin.so"
    //   macOS: "/path/to/bin/data/plugins/lygia-plugin/libLygiaPlugin.dylib"
    // Returns: "/path/to/bin/data/plugins/lygia-plugin/"
    
    size_t last_slash = plugin_lib_path.find_last_of('/');
    if (last_slash != std::string::npos) {
        return plugin_lib_path.substr(0, last_slash + 1); // Include the trailing '/'
    }
    
    // If no slash is found, return the current directory.
    return "./";
}

// ================================================================================
// BUILTIN CONFLICT DETECTION IMPLEMENTATION
// ================================================================================

void PluginManager::detectAndLogBuiltinConflicts(const std::string& plugin_alias, 
                                                const IPluginInterface* plugin_interface) const {
    if (!plugin_interface) return;
    
    std::vector<std::string> all_function_names = plugin_interface->getAllFunctionNames();
    std::vector<std::string> conflicting_functions;
    
    // Check each plugin function against GLSL built-ins
    for (const std::string& func_name : all_function_names) {
        if (MinimalBuiltinChecker::isBuiltinFunction(func_name)) {
            conflicting_functions.push_back(func_name);
        }
    }
    
    // Log conflicts if any found
    if (!conflicting_functions.empty()) {
        ofLogWarning("PluginManager") 
            << "Plugin '" << plugin_alias << "' contains " << conflicting_functions.size() 
            << " function(s) that conflict with GLSL built-ins (behavior is undetermined):";
        
        for (const std::string& func_name : conflicting_functions) {
            ofLogWarning("PluginManager") << "  - " << func_name << "()";
        }
        
        ofLogWarning("PluginManager") 
            << "These functions may not behave as expected. Use at your own risk.";
    } else {
        ofLogNotice("PluginManager") 
            << "Plugin '" << plugin_alias << "' has no conflicts with GLSL built-ins.";
    }
}

bool PluginManager::hasBuiltinConflict(const std::string& function_name) const {
    return MinimalBuiltinChecker::isBuiltinFunction(function_name);
}

std::map<std::string, std::set<std::string>> PluginManager::getAllBuiltinConflicts() const {
    std::map<std::string, std::set<std::string>> conflicts;
    
    for (const auto& [plugin_alias, plugin] : loaded_plugins) {
        std::vector<std::string> all_function_names = plugin->interface->getAllFunctionNames();
        std::set<std::string> plugin_conflicts;
        
        for (const std::string& func_name : all_function_names) {
            if (MinimalBuiltinChecker::isBuiltinFunction(func_name)) {
                plugin_conflicts.insert(func_name);
            }
        }
        
        if (!plugin_conflicts.empty()) {
            conflicts[plugin_alias] = plugin_conflicts;
        }
    }
    
    return conflicts;
}

void PluginManager::logRuntimeConflictWarning(const std::string& function_name, 
                                             const std::string& plugin_name) const {
    ofLogWarning("PluginManager") 
        << "Using conflicting function '" << function_name << "' from plugin '" 
        << plugin_name << "' - behavior is undetermined (conflicts with GLSL built-in)";
}