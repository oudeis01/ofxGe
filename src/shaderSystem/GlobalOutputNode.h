#pragma once

#include "ShaderNode.h"
#include "ofMain.h"
#include <memory>
#include <string>

/**
 * @enum GlobalOutputState
 * @brief Represents the current state of the global output node
 */
enum class GlobalOutputState {
    IDLE,           ///< No shader connected, showing default output
    CONNECTED,      ///< A shader is connected and being rendered
    TRANSITIONING   ///< In the process of switching between shaders
};

/**
 * @class GlobalOutputNode
 * @brief Manages the final rendering output for the application
 * @details This singleton-like class handles which shader node is currently
 *          connected to the final output. All created shader nodes start in
 *          an idle state, and only one can be connected to the global output
 *          at a time for final rendering to the screen.
 */
class GlobalOutputNode {
public:
    GlobalOutputNode();
    ~GlobalOutputNode();
    
    // ================================================================================
    // CONNECTION MANAGEMENT
    // ================================================================================
    
    /**
     * @brief Connects a shader node to the global output for rendering
     * @param shader_id The unique ID of the shader to connect
     * @param shader_node The shader node to connect
     * @return True if connection was successful, false otherwise
     */
    bool connectShader(const std::string& shader_id, std::shared_ptr<ShaderNode> shader_node);
    
    /**
     * @brief Disconnects the currently connected shader
     * @return True if disconnection was successful, false if no shader was connected
     */
    bool disconnectShader();
    
    /**
     * @brief Checks if a shader is currently connected
     * @return True if a shader is connected, false otherwise
     */
    bool hasConnectedShader() const;
    
    /**
     * @brief Gets the ID of the currently connected shader
     * @return The shader ID if connected, empty string otherwise
     */
    std::string getConnectedShaderId() const;
    
    /**
     * @brief Gets the currently connected shader node
     * @return Shared pointer to the connected shader node, nullptr if none connected
     */
    std::shared_ptr<ShaderNode> getConnectedShader() const;
    
    // ================================================================================
    // RENDERING SYSTEM
    // ================================================================================
    
    /**
     * @brief Renders the currently connected shader or default output
     * @param plane The plane primitive to render the shader onto
     */
    void render(ofPlanePrimitive& plane);
    
    /**
     * @brief Updates automatic uniforms for the connected shader
     * @details This should be called every frame before rendering
     */
    void updateUniforms();
    
    // ================================================================================
    // STATE MANAGEMENT
    // ================================================================================
    
    /**
     * @brief Gets the current state of the global output
     * @return The current GlobalOutputState
     */
    GlobalOutputState getState() const;
    
    /**
     * @brief Gets a human-readable status string
     * @return Status description for debugging/UI display
     */
    std::string getStatusString() const;
    
    /**
     * @brief Gets detailed information about the current state
     * @return Detailed status information including connected shader details
     */
    std::string getDetailedStatus() const;
    
    // ================================================================================
    // FALLBACK RENDERING
    // ================================================================================
    
    /**
     * @brief Sets the default background color when no shader is connected
     * @param color The background color to use
     */
    void setDefaultBackgroundColor(const ofColor& color);
    
    /**
     * @brief Enables or disables debug visualization
     * @param enabled True to show debug info, false to hide
     */
    void setDebugMode(bool enabled);

private:
    // ================================================================================
    // INTERNAL STATE
    // ================================================================================
    
    GlobalOutputState current_state;           ///< Current state of the output node
    std::shared_ptr<ShaderNode> connected_shader; ///< Currently connected shader
    std::string connected_shader_id;           ///< ID of the connected shader
    
    // Rendering settings
    ofColor default_background_color;          ///< Background color when idle
    bool debug_mode;                          ///< Whether to show debug information
    
    // Statistics
    std::string connection_timestamp;          ///< When the current shader was connected
    size_t total_connections;                 ///< Total number of connections made
    size_t total_renders;                     ///< Total number of render calls
    
    // ================================================================================
    // INTERNAL METHODS
    // ================================================================================
    
    /**
     * @brief Updates the current state based on connection status
     */
    void updateState();
    
    /**
     * @brief Renders the default output when no shader is connected
     * @param plane The plane to render onto
     */
    void renderDefault(ofPlanePrimitive& plane);
    
    /**
     * @brief Renders debug information overlay
     */
    void renderDebugInfo();
    
    /**
     * @brief Generates a timestamp string for the current time
     * @return Current timestamp as string
     */
    std::string getCurrentTimestamp() const;
};