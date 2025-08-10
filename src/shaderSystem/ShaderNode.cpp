#include "ShaderNode.h"
#include "ofLog.h"

//--------------------------------------------------------------
ShaderNode::ShaderNode() 
    : is_compiled(false), has_error(false), auto_update_time(false), auto_update_resolution(false) {
}

//--------------------------------------------------------------
ShaderNode::ShaderNode(const std::string& func_name, const std::vector<std::string>& args)
    : function_name(func_name), arguments(args), is_compiled(false), has_error(false), 
      auto_update_time(false), auto_update_resolution(false) {
    shader_key = generateShaderKey();
}

//--------------------------------------------------------------
ShaderNode::~ShaderNode() {
    cleanup();
}

//--------------------------------------------------------------
bool ShaderNode::compile() {
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
