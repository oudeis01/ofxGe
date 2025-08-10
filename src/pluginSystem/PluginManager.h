#pragma once
#include "../glsl-plugin-interface/include/IPluginInterface.h"
#include <unordered_map>
#include <memory>
#include <dlfcn.h>

/**
 * @brief Manages the lifecycle of GLSL shader library plugins.
 * @details This class handles loading plugins from dynamic libraries (.so),
 *          unloading them, and providing access to their functions and metadata.
 *          It uses unique aliases to manage loaded plugins.
 */
class PluginManager {
private:
    /**
     * @struct LoadedPlugin
     * @brief  A helper struct to hold all resources related to a loaded plugin.
     * @details This struct encapsulates the dynamic library handle, the plugin
     *          interface instance, and basic metadata. Its destructor ensures
     *          that the plugin instance is properly destroyed and the library
     *          handle is closed.
     */
    struct LoadedPlugin {
        void* handle;               ///< Opaque handle for the dynamic library, obtained from dlopen().
        IPluginInterface* interface;///< Pointer to the actual plugin instance created by the factory function.
        std::string name;           ///< The internal name of the plugin.
        std::string version;        ///< The version string of the plugin.
        std::string author;         ///< The author of the plugin.
        std::string path;           ///< The full path to the loaded plugin .so file.
        
        /**
         * @brief Default constructor.
         */
        LoadedPlugin() : handle(nullptr), interface(nullptr) {}
        
        /**
         * @brief Constructs a LoadedPlugin instance.
         * @param h The dynamic library handle.
         * @param iface A pointer to the plugin interface instance.
         * @param p The file path to the plugin library.
         */
        LoadedPlugin(void* h, IPluginInterface* iface, const std::string& p)
            : handle(h), interface(iface), path(p) {
            if (interface) {
                name = interface->getName();
                version = interface->getVersion();
                author = interface->getAuthor();
            }
        }
        
        /**
         * @brief Destructor that cleans up plugin resources.
         * @details Calls the plugin's destroyPlugin() function and then
         *          closes the dynamic library handle using dlclose().
         */
        ~LoadedPlugin() {
            if (interface && handle) {
                // Get the destroy function symbol from the library
                typedef void (*destroy_plugin_t)(IPluginInterface*);
                destroy_plugin_t destroy_plugin = (destroy_plugin_t) dlsym(handle, "destroyPlugin");
                if (destroy_plugin) {
                    destroy_plugin(interface);
                }
                dlclose(handle);
            }
        }
    };
    
    ///< Map storing the loaded plugins, with their alias as the key.
    std::unordered_map<std::string, std::unique_ptr<LoadedPlugin>> loaded_plugins;
    
public:
    /**
     * @brief Default constructor.
     */
    PluginManager() = default;

    /**
     * @brief Destructor that unloads all plugins.
     */
    ~PluginManager() { unloadAllPlugins(); }
    
    // Plugin loading/unloading
    
    /**
     * @brief Loads a plugin from a dynamic library file (.so).
     * @param plugin_path The absolute path to the plugin library file.
     * @param alias A unique alias for the plugin. If empty, a name is derived from the plugin itself.
     * @return True if the plugin was loaded successfully, false otherwise.
     */
    bool loadPlugin(const std::string& plugin_path, const std::string& alias = "");

    /**
     * @brief Unloads a specific plugin by its alias.
     * @param alias The alias of the plugin to unload.
     */
    void unloadPlugin(const std::string& alias);

    /**
     * @brief Unloads all currently loaded plugins.
     */
    void unloadAllPlugins();

    /**
     * @brief Checks if a plugin is currently loaded.
     * @param alias The alias of the plugin to check.
     * @return True if the plugin is loaded, false otherwise.
     */
    bool isPluginLoaded(const std::string& alias) const;
    
    // Function search across all plugins

    /**
     * @brief Finds a function by name across all loaded plugins.
     * @details This performs a search through all plugins. If multiple plugins
     *          contain a function with the same name, the first one found is returned.
     * @param function_name The name of the function to find.
     * @return A pointer to the GLSLFunction metadata if found, otherwise nullptr.
     */
    const GLSLFunction* findFunction(const std::string& function_name);

    /**
     * @brief Finds a function by name within a specific plugin.
     * @param plugin_name The alias of the plugin to search in.
     * @param function_name The name of the function to find.
     * @return A pointer to the GLSLFunction metadata if found, otherwise nullptr.
     */
    const GLSLFunction* findFunction(const std::string& plugin_name, const std::string& function_name);
    
    // Plugin information

    /**
     * @brief Gets a list of human-readable strings for all loaded plugins.
     * @return A vector of strings, each describing a loaded plugin.
     */
    std::vector<std::string> getLoadedPlugins() const;

    /**
     * @brief Gets a list of all available functions from all loaded plugins.
     * @return A vector of strings, with each function prefixed by its plugin alias (e.g., "myplugin::myfunction").
     */
    std::vector<std::string> getAllFunctions() const;

    /**
     * @brief Gets a map of all functions, organized by plugin alias.
     * @return A map where the key is the plugin alias and the value is a vector of function names.
     */
    std::map<std::string, std::vector<std::string>> getFunctionsByPlugin() const;

    /**
     * @brief Gets detailed information for all loaded plugins.
     * @return A map where the key is the plugin alias and the value is a PluginInfo struct.
     */
    std::map<std::string, PluginInfo> getPluginInfos() const;

    /**
     * @brief Gets the file paths for all loaded plugins.
     * @return A map where the key is the plugin alias and the value is the path to the .so file.
     */
    std::map<std::string, std::string> getPluginPaths() const;
    
    // Advanced queries

    /**
     * @brief Finds all functions that belong to a specific category.
     * @param category The category name to search for.
     * @return A vector of pointers to GLSLFunction metadata for all matching functions.
     */
    std::vector<const GLSLFunction*> findFunctionsByCategory(const std::string& category);

    /**
     * @brief Finds all functions that have a specific return type.
     * @param returnType The GLSL return type to search for (e.g., "vec3", "float").
     * @return A vector of pointers to GLSLFunction metadata for all matching functions.
     */
    std::vector<const GLSLFunction*> findFunctionsByReturnType(const std::string& returnType);

    /**
     * @brief Gets statistics about the number of functions in each plugin.
     * @return A map where the key is the plugin alias and the value is the number of functions.
     */
    std::map<std::string, size_t> getPluginStatistics() const;
    
    // Plugin access

    /**
     * @brief Gets a pointer to the interface of a loaded plugin.
     * @param alias The alias of the plugin to retrieve.
     * @return A non-const pointer to the plugin's IPluginInterface, or nullptr if not found.
     */
    IPluginInterface* getPlugin(const std::string& alias);

    /**
     * @brief Gets a const pointer to the interface of a loaded plugin.
     * @param alias The alias of the plugin to retrieve.
     * @return A const pointer to the plugin's IPluginInterface, or nullptr if not found.
     */
    const IPluginInterface* getPlugin(const std::string& alias) const;
    
private:
    /**
     * @brief Extracts the directory path from a full path to a plugin library file.
     * @param plugin_lib_path The full path to the .so file.
     * @return The path to the containing directory, including the trailing slash.
     */
    std::string extractPluginDirectory(const std::string& plugin_lib_path) const;
};