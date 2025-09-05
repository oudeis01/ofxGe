#pragma once
#include "ofMain.h"
#include <vector>
#include <string>
#include <memory>
#include <map>

/**
 * @enum ShaderNodeState
 * @brief Represents the current state of a shader node
 */
enum class ShaderNodeState {
    CREATED,        ///< Just created, not yet compiled
    COMPILING,      ///< Currently being compiled
    IDLE,           ///< Compiled successfully, waiting for connection
    CONNECTED,      ///< Connected to global output and actively rendering
    ERROR           ///< Compilation or runtime error occurred
};

/**
 * @struct ShaderNode
 * @brief  Represents a single, dynamically generated shader instance.
 * @details This struct manages the entire lifecycle of a shader created from a
 *          GLSL function. It holds the source code, the compiled ofShader object,
 *          uniforms, and state information like compilation status and errors.
 */
struct ShaderNode {
    // --- Metadata ---
    std::string function_name;           ///< The name of the root GLSL function used.
    std::vector<std::string> arguments;  ///< The arguments passed to the function.
    std::string shader_key;              ///< A unique key generated for caching purposes.
    
    // --- Shader Source Code ---
    std::string vertex_shader_code;      ///< The generated vertex shader source code.
    std::string fragment_shader_code;    ///< The generated fragment shader source code.
    std::string glsl_function_code;      ///< The original GLSL function code loaded from a plugin.
    std::string source_directory_path;   ///< The directory path of the source GLSL file, for resolving #includes.
    
    // --- Compiled Object ---
    ofShader compiled_shader;            ///< The compiled and linked openFrameworks shader object.
    
    // --- Uniform Management ---
    std::map<std::string, float> float_uniforms;    ///< A map of user-defined float uniforms.
    std::map<std::string, ofVec2f> vec2_uniforms;   ///< A map of user-defined vec2 uniforms.
    bool auto_update_time;               ///< If true, the built-in 'time' uniform will be updated automatically.
    bool auto_update_resolution;         ///< If true, the built-in 'resolution' uniform will be updated automatically.
    
    // --- State Management ---
    bool is_compiled;                    ///< True if the shader has been successfully compiled and linked.
    bool has_error;                      ///< True if an error occurred during generation or compilation.
    std::string error_message;           ///< The error message, if any.
    ShaderNodeState node_state;          ///< Current state of the shader node
    bool is_connected_to_output;         ///< True if connected to global output
    std::string creation_timestamp;      ///< When this node was created
    
    /**
     * @brief Default constructor.
     */
    ShaderNode();

    /**
     * @brief Constructs a ShaderNode with metadata.
     * @param func_name The name of the GLSL function.
     * @param args The arguments for the function.
     */
    ShaderNode(const std::string& func_name, const std::vector<std::string>& args);
    
    /**
     * @brief Destructor.
     */
    ~ShaderNode();
    
    // --- Lifecycle Methods ---
    /**
     * @brief Compiles the vertex and fragment shader code into a usable shader program.
     * @return True on success, false on failure.
     */
    bool compile();

    /**
     * @brief Cleans up resources, unloading the shader from the GPU.
     */
    void cleanup();

    /**
     * @brief Checks if the shader is compiled and ready to be used for rendering.
     * @return True if the shader is ready, false otherwise.
     */
    bool isReady() const;
    
    // --- Utility Methods ---
    /**
     * @brief Generates a unique key for caching, based on function name and arguments.
     * @return A string representing the unique key.
     */
    std::string generateShaderKey() const;

    /**
     * @brief Sets the source code for the shaders.
     * @param vertex The vertex shader code.
     * @param fragment The fragment shader code.
     */
    void setShaderCode(const std::string& vertex, const std::string& fragment);

    /**
     * @brief Sets custom shader code for unified compilation
     * @param custom_code Complete fragment shader code for direct compilation
     */
    void setCustomShaderCode(const std::string& custom_code);

    /**
     * @brief Sets the node to an error state.
     * @param error The error message to store.
     */
    void setError(const std::string& error);
    
    // --- State Management Methods ---
    /**
     * @brief Sets the current state of the shader node
     * @param state The new state to set
     */
    void setState(ShaderNodeState state);
    
    /**
     * @brief Gets the current state of the shader node
     * @return The current ShaderNodeState
     */
    ShaderNodeState getState() const;
    
    /**
     * @brief Checks if the node is in idle state (ready but not connected)
     * @return True if in idle state
     */
    bool isIdle() const;
    
    /**
     * @brief Checks if the node is connected to global output
     * @return True if connected
     */
    bool isConnected() const;
    
    /**
     * @brief Sets the connection status to global output
     * @param connected True if connected, false if disconnected
     */
    void setConnectedToOutput(bool connected);
    
    /**
     * @brief Gets a detailed status string including state information
     * @return Detailed status description
     */
    std::string getDetailedStatus() const;
    
    /**
     * @brief Gets current timestamp as string
     * @return Current timestamp
     */
    std::string getCurrentTimestamp() const;
    
    // --- Uniform Management Methods ---
    /**
     * @brief Sets a float uniform value.
     * @param name The name of the uniform in the shader.
     * @param value The float value to set.
     */
    void setFloatUniform(const std::string& name, float value);

    /**
     * @brief Sets a vec2 uniform value.
     * @param name The name of the uniform in the shader.
     * @param value The ofVec2f value to set.
     */
    void setVec2Uniform(const std::string& name, const ofVec2f& value);

    /**
     * @brief Enables or disables automatic updates of the 'time' uniform.
     * @param enable True to enable, false to disable.
     */
    void setAutoUpdateTime(bool enable);

    /**
     * @brief Enables or disables automatic updates of the 'resolution' uniform.
     * @param enable True to enable, false to disable.
     */
    void setAutoUpdateResolution(bool enable);

    /**
     * @brief Updates all user-defined uniforms on the GPU.
     */
    void updateUniforms();

    /**
     * @brief Updates only the automatic uniforms (time, resolution) on the GPU.
     */
    void updateAutoUniforms();
    
    // --- Debugging Methods ---
    /**
     * @brief Prints a summary of the node's state to the log.
     */
    void printDebugInfo() const;

    /**
     * @brief Gets a string representation of the node's current status.
     * @return A string like "COMPILED", "ERROR", or "NOT_READY".
     */
    std::string getStatusString() const;
};
