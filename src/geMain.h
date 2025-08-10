#pragma once

#include "ofMain.h"
#include <memory>
#include "pluginSystem/PluginManager.h"
#include "shaderSystem/ShaderManager.h"
#include "shaderSystem/ShaderNode.h"

// Forward declarations to avoid circular dependencies
class PluginManager;
class ShaderManager;

/**
 * @class graphicsEngine
 * @brief A core class that encapsulates the main functionalities of the graphics engine.
 * @details This class brings together the PluginManager and ShaderManager to handle
 *          plugin loading, shader creation, and rendering logic. It acts as a
 *          central hub for the primary engine components.
 */
class graphicsEngine{
public:
    graphicsEngine();
    ~graphicsEngine();

    // --- Plugin System Methods ---
    /**
     * @brief Loads all valid plugins found in the data/plugins directory.
     */
    void loadAllPlugins();

    /**
     * @brief Displays a summary of all loaded plugins and their functions in the console.
     */
    void displayPluginInfo();

    /**
     * @brief Scans the plugin directory for all valid plugin files (.so).
     * @return A vector of strings containing the absolute paths to plugin files.
     */
    std::vector<std::string> findPluginFiles();
    
    // --- Shader System Methods ---
    /**
     * @brief Initializes the shader system, creating the ShaderManager.
     */
    void initializeShaderSystem();

    /**
     * @brief Runs a test to create a shader using a specific function (e.g., "curl").
     */
    void testShaderCreation();

    /**
     * @brief Updates the automatic uniforms (e.g., time, resolution) for the current shader.
     */
    void updateShaderUniforms();

    // --- Members ---
    /// @brief Manages the lifecycle of all plugins.
    std::unique_ptr<PluginManager> plugin_manager;
    /// @brief A list of names for all successfully loaded plugins.
    std::vector<std::string> loaded_plugin_names;
    /// @brief A map from plugin names to a list of their available functions.
    std::map<std::string, std::vector<std::string>> plugin_functions;
    
    /// @brief Manages the creation, caching, and compilation of shaders.
    std::unique_ptr<ShaderManager> shader_manager;
    /// @brief A pointer to the currently active shader being rendered.
    std::shared_ptr<ShaderNode> current_shader;

};
