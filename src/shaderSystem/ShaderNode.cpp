#include "ShaderNode.h"
#include "ofLog.h"

//--------------------------------------------------------------
ShaderNode::ShaderNode() 
    : is_compiled(false), has_error(false), auto_update_time(false), auto_update_resolution(false),
      node_state(ShaderNodeState::CREATED), is_connected_to_output(false) {
    creation_timestamp = getCurrentTimestamp();
}

//--------------------------------------------------------------
ShaderNode::ShaderNode(const std::string& func_name, const std::vector<std::string>& args)
    : function_name(func_name), arguments(args), auto_update_time(false), auto_update_resolution(false),
      is_compiled(false), has_error(false), node_state(ShaderNodeState::CREATED), is_connected_to_output(false) {
    shader_key = generateShaderKey();
    creation_timestamp = getCurrentTimestamp();
}

//--------------------------------------------------------------
ShaderNode::~ShaderNode() {
    cleanup();
}

//--------------------------------------------------------------
bool ShaderNode::compile() {
    setState(ShaderNodeState::COMPILING);
    
    if (vertex_shader_code.empty() || fragment_shader_code.empty()) {
        setError("Shader code not set before compilation");
        return false;
    }
    
    try {
        // Clean up any previously loaded shader.
        if (compiled_shader.isLoaded()) {
            compiled_shader.unload();
        }
        
        // Setup the shader from source. Providing the source directory path allows
        // ofShader to correctly handle #include directives with relative paths.
        bool success = compiled_shader.setupShaderFromSource(GL_VERTEX_SHADER, vertex_shader_code, source_directory_path) &&
                       compiled_shader.setupShaderFromSource(GL_FRAGMENT_SHADER, fragment_shader_code, source_directory_path) &&
                       compiled_shader.linkProgram();
        
        if (success) {
            is_compiled = true;
            has_error = false;
            error_message.clear();
            setState(ShaderNodeState::IDLE);  // Set to idle after successful compilation
            ofLogNotice("ShaderNode") << "Successfully compiled shader for function: " << function_name;
            return true;
        } else {
            setError("Failed to compile or link shader program");
            return false;
        }
    } catch (const std::exception& e) {
        setError("Exception during shader compilation: " + std::string(e.what()));
        return false;
    }
}

//--------------------------------------------------------------
void ShaderNode::cleanup() {
    if (compiled_shader.isLoaded()) {
        compiled_shader.unload();
    }
    is_compiled = false;
    has_error = false;
    error_message.clear();
}

//--------------------------------------------------------------
bool ShaderNode::isReady() const {
    return is_compiled && !has_error && compiled_shader.isLoaded();
}

//--------------------------------------------------------------
std::string ShaderNode::generateShaderKey() const {
    std::string key = function_name;
    for (const auto& arg : arguments) {
        key += "_" + arg;
    }
    return key;
}

//--------------------------------------------------------------
void ShaderNode::setShaderCode(const std::string& vertex, const std::string& fragment) {
    vertex_shader_code = vertex;
    fragment_shader_code = fragment;
}

//--------------------------------------------------------------
void ShaderNode::setCustomShaderCode(const std::string& custom_code) {
    // Set a minimal vertex shader for 2D rendering
    vertex_shader_code = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
void main() {
    gl_Position = vec4(aPos, 1.0);
}
)";
    
    // Use the provided custom fragment shader code
    fragment_shader_code = custom_code;
    
    ofLogNotice("ShaderNode") << "Set custom shader code (" << custom_code.length() << " characters)";
}

//--------------------------------------------------------------
void ShaderNode::setError(const std::string& error) {
    has_error = true;
    error_message = error;
    is_compiled = false;
    ofLogError("ShaderNode") << "Error in shader '" << function_name << "': " << error;
}

//--------------------------------------------------------------
void ShaderNode::printDebugInfo() const {
    ofLogNotice("ShaderNode") << "=== Shader Node Debug Info ===";
    ofLogNotice("ShaderNode") << "Function: " << function_name;
    ofLogNotice("ShaderNode") << "Arguments: " << ofJoinString(arguments, ", ");
    ofLogNotice("ShaderNode") << "Shader Key: " << shader_key;
    ofLogNotice("ShaderNode") << "Status: " << getStatusString();
    if (has_error) {
        ofLogNotice("ShaderNode") << "Error: " << error_message;
    }
    ofLogNotice("ShaderNode") << "Vertex Shader Length: " << vertex_shader_code.length();
    ofLogNotice("ShaderNode") << "Fragment Shader Length: " << fragment_shader_code.length();
}

//--------------------------------------------------------------
std::string ShaderNode::getStatusString() const {
    if (has_error) return "ERROR";
    if (is_compiled) return "COMPILED";
    if (!vertex_shader_code.empty() && !fragment_shader_code.empty()) return "READY_TO_COMPILE";
    return "NOT_READY";
}

//--------------------------------------------------------------
void ShaderNode::setFloatUniform(const std::string& name, float value) {
    float_uniforms[name] = value;
    if (is_compiled && compiled_shader.isLoaded()) {
        compiled_shader.setUniform1f(name, value);
    }
}

//--------------------------------------------------------------
void ShaderNode::setVec2Uniform(const std::string& name, const ofVec2f& value) {
    vec2_uniforms[name] = value;
    if (is_compiled && compiled_shader.isLoaded()) {
        compiled_shader.setUniform2f(name, value.x, value.y);
    }
}

//--------------------------------------------------------------
void ShaderNode::setAutoUpdateTime(bool enable) {
    auto_update_time = enable;
}

//--------------------------------------------------------------
void ShaderNode::setAutoUpdateResolution(bool enable) {
    auto_update_resolution = enable;
}

//--------------------------------------------------------------
void ShaderNode::updateUniforms() {
    if (!isReady()) {
        return;
    }
    
    // Update all user-defined float uniforms.
    for (const auto& [name, value] : float_uniforms) {
        compiled_shader.setUniform1f(name, value);
    }
    
    // Update all user-defined vec2 uniforms.
    for (const auto& [name, value] : vec2_uniforms) {
        compiled_shader.setUniform2f(name, value.x, value.y);
    }
    
    // Update any automatic uniforms as well.
    updateAutoUniforms();
}

//--------------------------------------------------------------
void ShaderNode::updateAutoUniforms() {
    if (!isReady()) {
        return;
    }
    
    // Update the time uniform if enabled.
    if (auto_update_time) {
        compiled_shader.setUniform1f("time", ofGetElapsedTimef());
    }
    
    // Update the resolution uniform if enabled.
    if (auto_update_resolution) {
        compiled_shader.setUniform2f("resolution", ofGetWidth(), ofGetHeight());
    }
}

// ================================================================================
// STATE MANAGEMENT METHODS
// ================================================================================

//--------------------------------------------------------------
void ShaderNode::setState(ShaderNodeState state) {
    if (node_state != state) {
        ShaderNodeState old_state = node_state;
        node_state = state;
        
        // Handle state transitions
        if (state == ShaderNodeState::ERROR) {
            has_error = true;
            is_compiled = false;
        } else if (state == ShaderNodeState::IDLE && old_state == ShaderNodeState::COMPILING) {
            // Successfully compiled, ready to be connected
            ofLogNotice("ShaderNode") << "Shader '" << function_name << "' is now IDLE and ready for connection";
        } else if (state == ShaderNodeState::CONNECTED) {
            is_connected_to_output = true;
            ofLogNotice("ShaderNode") << "Shader '" << function_name << "' is now CONNECTED to global output";
        } else if (state == ShaderNodeState::IDLE && old_state == ShaderNodeState::CONNECTED) {
            is_connected_to_output = false;
            ofLogNotice("ShaderNode") << "Shader '" << function_name << "' is now IDLE (disconnected from output)";
        }
    }
}

//--------------------------------------------------------------
ShaderNodeState ShaderNode::getState() const {
    return node_state;
}

//--------------------------------------------------------------
bool ShaderNode::isIdle() const {
    return node_state == ShaderNodeState::IDLE;
}

//--------------------------------------------------------------
bool ShaderNode::isConnected() const {
    return node_state == ShaderNodeState::CONNECTED && is_connected_to_output;
}

//--------------------------------------------------------------
void ShaderNode::setConnectedToOutput(bool connected) {
    is_connected_to_output = connected;
    if (connected) {
        setState(ShaderNodeState::CONNECTED);
    } else {
        setState(ShaderNodeState::IDLE);
    }
}

//--------------------------------------------------------------
std::string ShaderNode::getDetailedStatus() const {
    std::stringstream status;
    
    status << "=== Shader Node Status ===\n";
    status << "Function: " << function_name << "\n";
    status << "Created: " << creation_timestamp << "\n";
    
    // State information
    switch (node_state) {
        case ShaderNodeState::CREATED:
            status << "State: CREATED (not yet compiled)\n";
            break;
        case ShaderNodeState::COMPILING:
            status << "State: COMPILING (in progress)\n";
            break;
        case ShaderNodeState::IDLE:
            status << "State: IDLE (compiled, ready for connection)\n";
            break;
        case ShaderNodeState::CONNECTED:
            status << "State: CONNECTED (active rendering)\n";
            break;
        case ShaderNodeState::ERROR:
            status << "State: ERROR\n";
            status << "Error: " << error_message << "\n";
            break;
    }
    
    // Compilation status
    status << "Compiled: " << (is_compiled ? "Yes" : "No") << "\n";
    status << "Ready: " << (isReady() ? "Yes" : "No") << "\n";
    status << "Connected to Output: " << (is_connected_to_output ? "Yes" : "No") << "\n";
    
    // Arguments
    if (!arguments.empty()) {
        status << "Arguments: ";
        for (size_t i = 0; i < arguments.size(); i++) {
            if (i > 0) status << ", ";
            status << "\"" << arguments[i] << "\"";
        }
        status << "\n";
    }
    
    // Uniforms
    if (!float_uniforms.empty() || !vec2_uniforms.empty()) {
        status << "Uniforms: " << (float_uniforms.size() + vec2_uniforms.size()) << " total\n";
    }
    
    return status.str();
}

//--------------------------------------------------------------
std::string ShaderNode::getCurrentTimestamp() const {
    time_t now = time(0);
    char* dt = ctime(&now);
    std::string timestamp(dt);
    // Remove newline at end
    if (!timestamp.empty() && timestamp.back() == '\n') {
        timestamp.pop_back();
    }
    return timestamp;
}
