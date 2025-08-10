#include "ShaderManager.h"
#include "BuiltinVariables.h"
#include "ofLog.h"
#include <algorithm>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>

//--------------------------------------------------------------
ShaderManager::ShaderManager(PluginManager * pm)
	: plugin_manager(pm)
	, debug_mode(true) {

	if (!plugin_manager) {
		ofLogError("ShaderManager") << "PluginManager pointer is null";
		return;
	}

	initializeShaderTemplates();

	ofLogNotice("ShaderManager") << "ShaderManager initialized";
}

//--------------------------------------------------------------
ShaderManager::~ShaderManager() {
	clearCache();
}

//--------------------------------------------------------------
void ShaderManager::initializeShaderTemplates() {
	// Default vertex shader passes through position and texture coordinates.
	default_vertex_shader = R"(
#version 150

uniform mat4 modelViewProjectionMatrix;

in vec4 position;
in vec2 texcoord;

out vec2 vTexCoord;

void main() {
    vTexCoord = texcoord;
    gl_Position = modelViewProjectionMatrix * position;
}
)";

	// Default fragment shader template with placeholders for dynamic code injection.
	default_fragment_shader_template = R"(
#version 150

in vec2 vTexCoord;
out vec4 outputColor;

// Uniforms will be inserted here
{UNIFORMS}

// GLSL function will be inserted here
{GLSL_FUNCTION}

void main() {
    // Main function content will be inserted here
    {MAIN_CONTENT}
}
)";

	if (debug_mode) {
		ofLogNotice("ShaderManager") << "Shader templates initialized";
	}
}

//--------------------------------------------------------------
std::shared_ptr<ShaderNode> ShaderManager::createShader(
	const std::string & function_name,
	const std::vector<std::string> & arguments) {

	if (debug_mode) {
		ofLogNotice("ShaderManager") << "Creating shader for function: " << function_name
									 << " with " << arguments.size() << " arguments";
	}

	// Validate arguments, especially swizzling, before proceeding.
	BuiltinVariables & builtins = BuiltinVariables::getInstance();
	for (const auto & arg : arguments) {
		std::string errorMessage;
		if (!builtins.isValidSwizzle(arg, errorMessage)) {
			return createErrorShader(function_name, arguments, errorMessage);
		}
	}

	// Check cache for an existing, ready-to-use shader.
	std::string cache_key = generateCacheKey(function_name, arguments);
	auto cached_shader = getCachedShader(cache_key);
	if (cached_shader && cached_shader->isReady()) {
		if (debug_mode) {
			ofLogNotice("ShaderManager") << "Returning cached shader: " << cache_key;
		}
		return cached_shader;
	}

	// Create a new shader node to manage the shader's state.
	auto shader_node = std::make_shared<ShaderNode>(function_name, arguments);

	// Find the function's metadata from the plugin manager.
	const GLSLFunction * function_metadata = plugin_manager->findFunction(function_name);
	if (!function_metadata) {
		std::string error = "Function '" + function_name + "' not found in any loaded plugin";
		return createErrorShader(function_name, arguments, error);
	}

	// Determine which plugin the function belongs to.
	std::string plugin_name = "";
	auto functions_by_plugin = plugin_manager->getFunctionsByPlugin();
    for (const auto& [name, function_list] : functions_by_plugin) {
        for (const auto& func_name : function_list) {
            if (func_name == function_name) {
                plugin_name = name;
                break;
            }
        }
        if (!plugin_name.empty()) break;
    }

	// Load the GLSL source code for the function.
	std::string glsl_function_code = loadGLSLFunction(function_metadata, plugin_name);
	if (glsl_function_code.empty()) {
		std::string error = "Failed to load GLSL code for function: " + function_name;
		return createErrorShader(function_name, arguments, error);
	}

	shader_node->glsl_function_code = glsl_function_code;

	// Set the source directory path to allow ofShader to resolve #includes.
	std::string glsl_file_path = resolveGLSLFilePath(plugin_name, function_metadata->filePath);
	size_t last_slash = glsl_file_path.find_last_of('/');
	if (last_slash != std::string::npos) {
		shader_node->source_directory_path = glsl_file_path.substr(0, last_slash);
	}

	// Generate the final shader source code.
	std::string vertex_code = generateVertexShader();
	std::string fragment_code = generateFragmentShader(glsl_function_code, function_name, arguments);

	shader_node->setShaderCode(vertex_code, fragment_code);

	// Compile the shader.
	if (!shader_node->compile()) {
		ofLogError("ShaderManager") << "Failed to compile shader for function: " << function_name;
		return shader_node; // Return the node in its error state.
	}

	// Configure automatic uniforms based on the arguments used.
    bool has_time = false;
    for(const auto& arg : arguments) {
        if(builtins.extractBaseVariable(arg) == "time") {
            has_time = true;
            break;
        }
    }

	bool has_st = false;
	for (const auto & arg : arguments) {
		if (builtins.extractBaseVariable(arg) == "st") {
			has_st = true;
			break;
		}
	}

	if (has_time) {
		shader_node->setAutoUpdateTime(true);
	}
	if (has_st) {
		shader_node->setAutoUpdateResolution(true);
	}

	// Store the successfully compiled shader in the cache.
	cacheShader(cache_key, shader_node);

	if (debug_mode) {
		ofLogNotice("ShaderManager") << "Successfully created shader: " << cache_key;
		std::cout << "=== VERTEX SHADER ===" << std::endl;
		std::cout << shader_node->vertex_shader_code << std::endl;
		std::cout << "=== FRAGMENT SHADER ===" << std::endl;
		std::cout << shader_node->fragment_shader_code << std::endl;
		std::cout << "===================" << std::endl;
	}

	return shader_node;
}

//--------------------------------------------------------------
std::string ShaderManager::loadGLSLFunction(const GLSLFunction * function_metadata, const std::string & plugin_name) {
	if (!function_metadata) {
		ofLogError("ShaderManager") << "Function metadata is null";
		return "";
	}

	std::string file_path = resolveGLSLFilePath(plugin_name, function_metadata->filePath);

	if (debug_mode) {
		ofLogNotice("ShaderManager") << "Loading GLSL file: " << file_path;
	}

    // ofShader handles #include directives internally, so we just need to read the file content.
	return readFileContent(file_path);
}

//--------------------------------------------------------------
std::string ShaderManager::resolveGLSLFilePath(const std::string & plugin_name, const std::string & function_file_path) {
	auto plugin_paths = plugin_manager->getPluginPaths();
	auto it = plugin_paths.find(plugin_name);

	if (it == plugin_paths.end()) {
		ofLogError("ShaderManager") << "Plugin data directory not found for: " << plugin_name;
		return "";
	}

	std::string full_path = it->second + function_file_path;

	if (debug_mode) {
		ofLogNotice("ShaderManager") << "Resolved GLSL path: " << full_path;
	}

	return full_path;
}

//--------------------------------------------------------------
std::string ShaderManager::readFileContent(const std::string & file_path) {
	ofBuffer buffer = ofBufferFromFile(file_path);
    if(buffer.size() == 0){
        ofLogError("ShaderManager") << "Failed to open or read file: " << file_path;
        return "";
    }
    return buffer.getText();
}

//--------------------------------------------------------------
std::string ShaderManager::generateVertexShader() {
	return default_vertex_shader;
}

//--------------------------------------------------------------
std::string ShaderManager::generateFragmentShader(
	const std::string & glsl_function_code,
	const std::string & function_name,
	const std::vector<std::string> & arguments) {

	std::string fragment_code = default_fragment_shader_template;

	std::string uniforms = generateUniforms(arguments);

	std::string wrapper_functions = "";
	const GLSLFunction * function_metadata = plugin_manager->findFunction(function_name);
	if (function_metadata && !function_metadata->overloads.empty()) {
		const FunctionOverload * best_overload = findBestOverload(function_metadata, arguments);
		if (best_overload) {
			if (!isSignatureDuplicate(function_metadata, arguments)) {
				wrapper_functions = generateWrapperFunction(function_name, arguments, best_overload);
			}
		}
	}

	std::string main_content = generateMainFunction(function_name, arguments);

	std::string combined_functions = glsl_function_code;
	if (!wrapper_functions.empty()) {
		combined_functions += "\n\n// Generated wrapper function to adapt arguments\n" + wrapper_functions;
	}

	// Replace placeholders in the template.
	size_t pos = fragment_code.find("{UNIFORMS}");
	if (pos != std::string::npos) {
		fragment_code.replace(pos, 10, uniforms);
	}

	pos = fragment_code.find("{GLSL_FUNCTION}");
	if (pos != std::string::npos) {
		fragment_code.replace(pos, 15, combined_functions);
	}

	pos = fragment_code.find("{MAIN_CONTENT}");
	if (pos != std::string::npos) {
		fragment_code.replace(pos, 14, main_content);
	}

	return fragment_code;
}

//--------------------------------------------------------------
bool ShaderManager::isFloatLiteral(const std::string & str) {
    if (str.empty()) return false;
    bool has_dot = false;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '.') {
            if (has_dot) return false; // Max one dot
            has_dot = true;
        } else if (!std::isdigit(str[i]) && (i > 0 || str[i] != '-')) { // Allow minus sign only at start
            return false;
        }
    }
    return true;
}

//--------------------------------------------------------------
bool ShaderManager::canCombineToVector(const std::vector<std::string> & arguments, const std::string & target_type) {
	int required_components = 0;
	if (target_type == "vec2") required_components = 2;
	else if (target_type == "vec3") required_components = 3;
	else if (target_type == "vec4") required_components = 4;
	else if (target_type == "float") required_components = 1;
	else return false;

	int total_components = 0;
	BuiltinVariables & builtins = BuiltinVariables::getInstance();

	for (const auto & arg : arguments) {
		if (isFloatLiteral(arg)) {
			total_components += 1;
		} else {
            std::string base_var = builtins.extractBaseVariable(arg);
            const auto* info = builtins.getBuiltinInfo(base_var);
            if(info) {
                if(builtins.hasSwizzle(arg)) {
                    total_components += builtins.extractSwizzle(arg).length();
                } else {
                    total_components += info->component_count;
                }
            } else {
                total_components += 1; // Assume unknown variables are floats
            }
		}
	}

	return total_components == required_components;
}

//--------------------------------------------------------------
std::string ShaderManager::generateUniforms(const std::vector<std::string> & arguments) {
	std::stringstream uniforms;
	BuiltinVariables & builtins = BuiltinVariables::getInstance();
	std::set<std::string> needed_uniforms;

	for (const auto & arg : arguments) {
		if (isFloatLiteral(arg)) {
			continue;
		}

		std::string base_var = builtins.extractBaseVariable(arg);
		const BuiltinVariable * builtin_info = builtins.getBuiltinInfo(base_var);

		if (builtin_info) {
			if (builtin_info->needs_uniform) {
                // 'st' requires the 'resolution' uniform.
				if (base_var == "st") {
					needed_uniforms.insert("resolution");
				}
                else {
                    needed_uniforms.insert(base_var);
                }
			}
		} else {
			// Assume any non-builtin is a user-defined float uniform.
			needed_uniforms.insert(arg);
		}
	}

	for (const auto & uniform_name : needed_uniforms) {
		if (uniform_name == "time") {
			uniforms << "uniform float time;\n";
		} else if (uniform_name == "resolution") {
			uniforms << "uniform vec2 resolution;\n";
		} else {
			uniforms << "uniform float " << uniform_name << ";\n";
		}
	}

	return uniforms.str();
}
//--------------------------------------------------------------
std::string ShaderManager::getFunctionReturnType(const std::string& function_name, 
                                                const std::vector<std::string>& arguments) {
    const GLSLFunction* function_metadata = plugin_manager->findFunction(function_name);
    if (!function_metadata) {
        return "float"; // Default fallback
    }
    
    const FunctionOverload* best_overload = findBestOverload(function_metadata, arguments);
    if (!best_overload) {
        return "float"; // Default fallback
    }
    
    return best_overload->returnType;
}
//--------------------------------------------------------------
std::string ShaderManager::generateMainFunction(const std::string & function_name, const std::vector<std::string> & arguments) {
    std::stringstream main_func;
    BuiltinVariables & builtins = BuiltinVariables::getInstance();
    std::set<std::string> needed_declarations;

    // Determine which local variables need to be declared (e.g., 'st').
    for (const auto & arg : arguments) {
        if (isFloatLiteral(arg)) continue;

        std::string base_var = builtins.extractBaseVariable(arg);
        const BuiltinVariable * builtin_info = builtins.getBuiltinInfo(base_var);

        if (builtin_info && builtin_info->needs_declaration) {
            needed_declarations.insert(builtin_info->declaration_code);
        }
    }

    for (const auto & declaration : needed_declarations) {
        main_func << "    " << declaration << "\n";
    }
    if (!needed_declarations.empty()) {
        main_func << "\n";
    }

    std::string return_type = getFunctionReturnType(function_name, arguments);

    main_func << "    // Call the target GLSL function\n";
    main_func << "    " << return_type << " result = " << function_name << "(";

    // Pass the arguments to the function call.
    for (size_t i = 0; i < arguments.size(); ++i) {
        if (i > 0) main_func << ", ";
        main_func << arguments[i];
    }
    main_func << ");\n\n";

    // Convert the result to a vec4 for output.
    if (return_type == "float") {
        main_func << "    outputColor = vec4(vec3(result), 1.0);\n";
    } else if (return_type == "vec2") {
        main_func << "    outputColor = vec4(result, 0.0, 1.0);\n";
    } else if (return_type == "vec3") {
        main_func << "    outputColor = vec4(result, 1.0);\n";
    } else if (return_type == "vec4") {
        main_func << "    outputColor = result;\n";
    } else {
        // Fallback for unknown or unsupported return types.
        main_func << "    outputColor = vec4(0.0, 0.0, 0.0, 1.0); // Unsupported return type\n";
    }

    return main_func.str();
}

//--------------------------------------------------------------
std::string ShaderManager::generateCacheKey(const std::string & function_name, const std::vector<std::string> & arguments) {
	std::string key = function_name;
	for (const auto & arg : arguments) {
		key += "_" + arg;
	}
	return key;
}

//--------------------------------------------------------------
std::shared_ptr<ShaderNode> ShaderManager::getCachedShader(const std::string & shader_key) {
	auto it = shader_cache.find(shader_key);
	return (it != shader_cache.end()) ? it->second : nullptr;
}

//--------------------------------------------------------------
void ShaderManager::cacheShader(const std::string & shader_key, std::shared_ptr<ShaderNode> shader_node) {
	shader_cache[shader_key] = shader_node;

	if (debug_mode) {
		ofLogNotice("ShaderManager") << "Cached shader: " << shader_key;
	}
}

//--------------------------------------------------------------
void ShaderManager::clearCache() {
	shader_cache.clear();

	if (debug_mode) {
		ofLogNotice("ShaderManager") << "Shader cache cleared";
	}
}

//--------------------------------------------------------------
std::shared_ptr<ShaderNode> ShaderManager::createErrorShader(
	const std::string & function_name,
	const std::vector<std::string> & arguments,
	const std::string & error_message) {

	auto error_shader = std::make_shared<ShaderNode>(function_name, arguments);
	error_shader->setError(error_message);

	ofLogError("ShaderManager") << "Created error shader: " << error_message;

	return error_shader;
}

//--------------------------------------------------------------
bool ShaderManager::isSignatureDuplicate(const GLSLFunction * function_metadata,
	const std::vector<std::string> & user_arguments) {
	if (!function_metadata) {
		return false;
	}

	std::string user_signature = calculateUserArgumentSignature(user_arguments);

	for (const auto & overload : function_metadata->overloads) {
		std::string existing_signature = calculateOverloadSignature(overload);
		if (user_signature == existing_signature) {
			return true;
		}
	}

	return false;
}

//--------------------------------------------------------------
std::string ShaderManager::calculateUserArgumentSignature(const std::vector<std::string> & user_arguments) {
	std::string signature;
	for (size_t i = 0; i < user_arguments.size(); ++i) {
		if (i > 0) signature += ",";
		signature += getArgumentGLSLType(user_arguments[i]);
	}
	return signature;
}

//--------------------------------------------------------------
std::string ShaderManager::calculateOverloadSignature(const FunctionOverload & overload) {
	std::string signature;
	for (size_t i = 0; i < overload.paramTypes.size(); ++i) {
		if (i > 0) signature += ",";
		signature += overload.paramTypes[i];
	}
	return signature;
}

//--------------------------------------------------------------
std::string ShaderManager::getArgumentGLSLType(const std::string & argument) {
	if (isFloatLiteral(argument)) {
		return "float";
	}

	BuiltinVariables & builtins = BuiltinVariables::getInstance();
	if (builtins.hasSwizzle(argument)) {
		std::string swizzle = builtins.extractSwizzle(argument);
        switch (swizzle.length()) {
            case 1: return "float";
            case 2: return "vec2";
            case 3: return "vec3";
            case 4: return "vec4";
            default: return "float"; // Should not happen
        }
	}

	const BuiltinVariable * info = builtins.getBuiltinInfo(argument);
	if (info) {
		return info->glsl_type;
	}

	// Assume unknown arguments are user-defined float uniforms.
	return "float";
}
//--------------------------------------------------------------
void ShaderManager::printCacheInfo() const {
	ofLogNotice("ShaderManager") << "=== Shader Cache Info ===";
	ofLogNotice("ShaderManager") << "Total cached shaders: " << shader_cache.size();

	for (const auto & [key, shader] : shader_cache) {
		ofLogNotice("ShaderManager") << "  " << key << " -> " << shader->getStatusString();
	}
}

//--------------------------------------------------------------
void ShaderManager::setDebugMode(bool debug) {
	debug_mode = debug;
	ofLogNotice("ShaderManager") << "Debug mode: " << (debug ? "ON" : "OFF");
}

//--------------------------------------------------------------
const FunctionOverload * ShaderManager::findBestOverload(
	const GLSLFunction * function_metadata,
	const std::vector<std::string> & user_arguments) {

	if (!function_metadata || function_metadata->overloads.empty()) {
		return nullptr;
	}

    // Strategy: Find an overload that can be satisfied by the total number of
    // components provided by the user arguments.
	int total_components = 0;
    BuiltinVariables& builtins = BuiltinVariables::getInstance();
	for (const auto & arg : user_arguments) {
        total_components += getArgumentGLSLType(arg) == "vec2" ? 2 : 
                            getArgumentGLSLType(arg) == "vec3" ? 3 : 
                            getArgumentGLSLType(arg) == "vec4" ? 4 : 1;
	}

	// 1. Look for an overload with a single vector parameter that perfectly matches the component count.
	for (const auto & overload : function_metadata->overloads) {
		if (overload.paramTypes.size() == 1) {
			const std::string & param_type = overload.paramTypes[0];
			int required_components = (param_type == "vec2") ? 2 : (param_type == "vec3") ? 3 : (param_type == "vec4") ? 4 : 1;

			if (required_components == total_components && canCombineToVector(user_arguments, param_type)) {
				return &overload;
			}
		}
	}

	// 2. Look for a multi-parameter overload where the number of arguments matches exactly.
	for (const auto & overload : function_metadata->overloads) {
		if (overload.paramTypes.size() == user_arguments.size()) {
			return &overload;
		}
	}

	// 3. As a fallback, find the single-vector overload with the closest component count.
	const FunctionOverload * best_match = nullptr;
	int min_component_diff = INT_MAX;

	for (const auto & overload : function_metadata->overloads) {
		if (overload.paramTypes.size() == 1) {
			const std::string & param_type = overload.paramTypes[0];
			int required_components = (param_type == "vec2") ? 2 : (param_type == "vec3") ? 3 : (param_type == "vec4") ? 4 : 1;
			int component_diff = abs(required_components - total_components);
			if (component_diff < min_component_diff) {
				min_component_diff = component_diff;
				best_match = &overload;
			}
		}
	}

	return best_match;
}

//--------------------------------------------------------------
std::string ShaderManager::generateWrapperFunction(
	const std::string & function_name,
	const std::vector<std::string> & user_arguments,
	const FunctionOverload * target_overload) {

	if (!target_overload) {
		return "";
	}

	std::stringstream wrapper;

    // The wrapper function will have the same name as the original, but its
    // signature is built from the user-provided arguments.
	wrapper << target_overload->returnType << " " << function_name << "(";
	for (size_t i = 0; i < user_arguments.size(); ++i) {
		if (i > 0) wrapper << ", ";
        wrapper << getArgumentGLSLType(user_arguments[i]) << " arg" << i;
	}
	wrapper << ") {\n";

	// The body of the wrapper calls the actual target overload.
	wrapper << "    return " << function_name << "(";

	if (target_overload->paramTypes.size() == 1) {
		// Combine all arguments into a single vector constructor.
		const std::string & param_type = target_overload->paramTypes[0];
		wrapper << param_type << "(";
        for (size_t i = 0; i < user_arguments.size(); ++i) {
            if (i > 0) wrapper << ", ";
            wrapper << "arg" << i;
        }
        wrapper << ")";
	}
    else {
		// Map arguments directly for multi-parameter overloads.
		for (size_t i = 0; i < user_arguments.size(); ++i) {
			if (i > 0) wrapper << ", ";
			wrapper << "arg" << i;
		}
	}

	wrapper << ");\n";
	wrapper << "}\n";

	return wrapper.str();
}

//--------------------------------------------------------------
std::string ShaderManager::generateUniqueId() {
    return std::to_string(next_shader_id++);
}

//--------------------------------------------------------------
std::string ShaderManager::createShaderWithId(const std::string& function_name, 
                                              const std::vector<std::string>& arguments) {
    auto shader = createShader(function_name, arguments);
    
    if (shader && shader->isReady()) {
        std::string shader_id = generateUniqueId();
        active_shaders[shader_id] = shader;
        
        if (debug_mode) {
            ofLogNotice("ShaderManager") << "Created shader with ID: " << shader_id 
                                         << " for function: " << function_name;
        }
        
        return shader_id;
    }
    
    ofLogError("ShaderManager") << "Failed to create shader with ID for function: " << function_name;
    return "";
}

//--------------------------------------------------------------
std::shared_ptr<ShaderNode> ShaderManager::getShaderById(const std::string& shader_id) {
    auto it = active_shaders.find(shader_id);
    if (it != active_shaders.end()) {
        return it->second;
    }
    ofLogWarning("ShaderManager") << "Shader not found with ID: " << shader_id;
    return nullptr;
}

//--------------------------------------------------------------
bool ShaderManager::removeShaderById(const std::string& shader_id) {
    auto it = active_shaders.find(shader_id);
    if (it != active_shaders.end()) {
        active_shaders.erase(it);
        if (debug_mode) {
            ofLogNotice("ShaderManager") << "Removed shader with ID: " << shader_id;
        }
        return true;
    }
    
    ofLogWarning("ShaderManager") << "Failed to remove shader - ID not found: " << shader_id;
    return false;
}

//--------------------------------------------------------------
std::vector<std::string> ShaderManager::getAllActiveShaderIds() {
    std::vector<std::string> ids;
    for (const auto& pair : active_shaders) {
        ids.push_back(pair.first);
    }
    return ids;
}
