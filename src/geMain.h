#pragma once

#include "ofMain.h"
#include <memory>
#include "pluginSystem/PluginManager.h"
#include "shaderSystem/ShaderManager.h"
#include "shaderSystem/ShaderNode.h"
#include "shaderSystem/ShaderCompositionEngine.h"
#include "oscHandler/oscHandler.h"
#include "platformUtils/PlatformUtils.h"

// Forward declarations to avoid circular dependencies
class PluginManager;
class ShaderManager;
class ShaderCompositionEngine;
class OscHandler;

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
     * @brief Scans the plugin directory for all valid plugin files (.so on Linux, .dylib on macOS).
     * @return A vector of strings containing the absolute paths to plugin files.
     */
    std::vector<std::string> findPluginFiles();
    
    // --- Shader System Methods ---
    /**
     * @brief Initializes the shader system, creating the ShaderManager and CompositionEngine.
     */
    void initializeShaderSystem();

    /**
     * @brief Enables or disables deferred compilation mode.
     * @param enabled True to enable deferred compilation, false for immediate compilation.
     */
    void setDeferredCompilationMode(bool enabled);

    /**
     * @brief Runs a test to create a shader using a specific function (e.g., "curl").
     */
    void testShaderCreation(std::string function_name, std::vector<std::string>& args);

    /**
     * @brief Updates the automatic uniforms (e.g., time, resolution) for the current shader.
     */
    void updateShaderUniforms();
    
    // --- OSC System Methods ---
    /**
     * @brief Initializes the OSC system for receiving and sending messages.
     * @param receive_port The port number to listen for incoming OSC messages.
     */
    void initializeOSC(int receive_port = 12345);

    /**
     * @brief Updates the OSC system, processing incoming messages.
     * @details This should be called once per frame in the main update loop.
     */
    void updateOSC();

    /**
     * @brief Shuts down the OSC system and cleans up resources.
     */
    void shutdownOSC();
    
    // --- Shader Management with ID ---
    /**
     * @brief Creates a shader with a unique ID for OSC communication.
     * @param function_name The name of the GLSL function.
     * @param arguments The arguments for the function.
     * @return A unique shader ID string on success, or empty string on failure.
     */
    std::string createShaderWithId(const std::string& function_name, 
                                  const std::vector<std::string>& arguments);

    /**
     * @brief Connects a shader to the global output for rendering.
     * @param shader_id The unique ID of the shader to connect.
     * @return True if connection was successful, false otherwise.
     */
    bool connectShaderToOutput(const std::string& shader_id);

    /**
     * @brief Frees a shader and removes it from management.
     * @param shader_id The unique ID of the shader to free.
     * @return True if the shader was found and freed, false otherwise.
     */
    bool freeShader(const std::string& shader_id);

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
    
    // --- Composition Engine ---
    /// @brief Manages deferred compilation of shader composition graphs.
    std::unique_ptr<ShaderCompositionEngine> composition_engine;
    /// @brief Flag to enable/disable deferred compilation mode.
    bool deferred_compilation_mode;
    
    // --- OSC System ---
    /// @brief Manages OSC message receiving and sending.
    std::unique_ptr<OscHandler> osc_handler;
    /// @brief Map of active shaders managed by ID for OSC communication.
    std::map<std::string, std::shared_ptr<ShaderNode>> active_shaders;
    
private:
    // --- OSC Message Processing Helpers ---
    /**
     * @brief Processes incoming /create messages from OSC.
     */
    void processCreateMessages();

    /**
     * @brief Processes incoming /connect messages from OSC.
     */
    void processConnectMessages();

    /**
     * @brief Processes incoming /free messages from OSC.
     */
    void processFreeMessages();

    /**
     * @brief Parses comma-separated argument string into vector.
     * @param raw_args String like "st,time,1.0"
     * @return Vector of individual arguments ["st", "time", "1.0"]
     */
    std::vector<std::string> parseArguments(const std::string& raw_args);

};
