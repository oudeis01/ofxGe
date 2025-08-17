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

	// Initialize code generator (replaces initializeShaderTemplates)
	code_generator = std::make_unique<ShaderCodeGenerator>(plugin_manager);

	ofLogNotice("ShaderManager") << "ShaderManager initialized";
}

//--------------------------------------------------------------
ShaderManager::~ShaderManager() {
	clearCache();
}


//--------------------------------------------------------------
std::shared_ptr<ShaderNode> ShaderManager::createShader(
	const std::string & function_name,
	const std::vector<std::string> & arguments) {

	if (debug_mode) {
		ofLogNotice("ShaderManager") << "Creating shader for function: " << function_name
									 << " with " << arguments.size() << " arguments";
		for (size_t i = 0; i < arguments.size(); i++) {
			ofLogNotice("ShaderManager") << "  Arg " << i << ": '" << arguments[i] << "'";
		}
	}

	// Validate arguments, especially swizzling, before proceeding.
	BuiltinVariables & builtins = BuiltinVariables::getInstance();
	for (size_t i = 0; i < arguments.size(); i++) {
		const auto & arg = arguments[i];
		std::string errorMessage;
		ofLogNotice("ShaderManager") << "Validating argument " << i << ": '" << arg << "'";
		if (!builtins.isValidSwizzle(arg, errorMessage)) {
			ofLogError("ShaderManager") << "Validation failed for '" << arg << "': " << errorMessage;
			return createErrorShader(function_name, arguments, errorMessage);
		}
		ofLogNotice("ShaderManager") << "Argument " << i << " validation passed";
	}

	// Check cache for an existing, ready-to-use shader.
	std::string cache_key = generateCacheKey(function_name, arguments);
	ofLogNotice("ShaderManager") << "Cache key: '" << cache_key << "'";
	auto cached_shader = getCachedShader(cache_key);
	if (cached_shader) {
		ofLogNotice("ShaderManager") << "Found cached shader, isReady: " << cached_shader->isReady();
		if (cached_shader->isReady()) {
			if (debug_mode) {
				ofLogNotice("ShaderManager") << "Returning cached shader: " << cache_key;
			}
			return cached_shader;
		} else {
			ofLogNotice("ShaderManager") << "Cached shader is not ready, proceeding with new creation";
		}
	} else {
		ofLogNotice("ShaderManager") << "No cached shader found";
	}

	// Create a new shader node to manage the shader's state.
	auto shader_node = std::make_shared<ShaderNode>(function_name, arguments);
	ofLogNotice("ShaderManager") << "Step 1: Created ShaderNode";

	// Find the function's metadata from the plugin manager.
	const GLSLFunction * function_metadata = plugin_manager->findFunction(function_name);
	if (!function_metadata) {
		std::string error = "Function '" + function_name + "' not found in any loaded plugin";
		ofLogError("ShaderManager") << "Step 2: Function not found!";
		return createErrorShader(function_name, arguments, error);
	}
	ofLogNotice("ShaderManager") << "Step 2: Found function metadata";
	
	// Check for built-in conflicts and log warning if necessary
	if (plugin_manager->hasBuiltinConflict(function_name)) {
		// We'll determine the plugin name later, so log a general warning for now
		ofLogWarning("ShaderManager") 
			<< "Using function '" << function_name << "' which conflicts with GLSL built-in - behavior is undetermined";
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
    
    // Now that we have the plugin name, log detailed conflict warning if needed
    if (plugin_manager->hasBuiltinConflict(function_name) && !plugin_name.empty()) {
        plugin_manager->logRuntimeConflictWarning(function_name, plugin_name);
    }

	// Load the GLSL source code for the function.
	ofLogNotice("ShaderManager") << "Step 3: Loading GLSL function code";
	std::string glsl_function_code = loadGLSLFunction(function_metadata, plugin_name);
	if (glsl_function_code.empty()) {
		std::string error = "Failed to load GLSL code for function: " + function_name;
		ofLogError("ShaderManager") << "Step 3: Failed to load GLSL code!";
		return createErrorShader(function_name, arguments, error);
	}
	ofLogNotice("ShaderManager") << "Step 3: Loaded GLSL function code (length: " << glsl_function_code.length() << ")";

	shader_node->glsl_function_code = glsl_function_code;

	// Set the source directory path to allow ofShader to resolve #includes.
	std::string glsl_file_path = resolveGLSLFilePath(plugin_name, function_metadata->filePath);
	size_t last_slash = glsl_file_path.find_last_of('/');
	if (last_slash != std::string::npos) {
		shader_node->source_directory_path = glsl_file_path.substr(0, last_slash);
	}

	// Generate the final shader source code using the new code generator.
	if (!code_generator) {
		ofLogError("ShaderManager") << "ERROR: code_generator is null!";
		return createErrorShader(function_name, arguments, "code_generator is null");
	}
	
	ofLogNotice("ShaderManager") << "About to call code_generator->generateFragmentShader()";
	std::string vertex_code = code_generator->generateVertexShader();
	std::string fragment_code = code_generator->generateFragmentShader(glsl_function_code, function_name, arguments);

	shader_node->setShaderCode(vertex_code, fragment_code);

	// Compile the shader.
	if (!shader_node->compile()) {
		ofLogError("ShaderManager") << "Failed to compile shader for function: " << function_name;
		return shader_node; // Return the node in its error state.
	}

	// Configure automatic uniforms based on the arguments used.
    // Use ExpressionParser to properly detect dependencies in complex expressions
    bool has_time = false;
    bool has_st = false;
    
    for(const auto& arg : arguments) {
        // For simple variables, use the old method
        if (builtins.extractBaseVariable(arg) == "time") {
            has_time = true;
        }
        if (builtins.extractBaseVariable(arg) == "st") {
            has_st = true;
        }
        
        // For complex expressions, parse dependencies
        if (builtins.isComplexExpression(arg)) {
            ofLogNotice("ShaderManager") << "Complex expression detected: '" << arg << "'";
            // Use the same ExpressionParser logic as ShaderCodeGenerator
            ExpressionParser temp_parser;
            ExpressionInfo expr_info = temp_parser.parseExpression(arg);
            
            ofLogNotice("ShaderManager") << "Dependencies found: " << expr_info.dependencies.size();
            for (const auto& dep : expr_info.dependencies) {
                ofLogNotice("ShaderManager") << "  Dependency: '" << dep << "'";
                std::string base_var = builtins.extractBaseVariable(dep);
                ofLogNotice("ShaderManager") << "  Base variable: '" << base_var << "'";
                if (base_var == "time") {
                    ofLogNotice("ShaderManager") << "  -> TIME DEPENDENCY FOUND!";
                    has_time = true;
                }
                if (base_var == "st") {
                    ofLogNotice("ShaderManager") << "  -> ST DEPENDENCY FOUND!";
                    has_st = true;
                }
            }
        }
    }

	ofLogNotice("ShaderManager") << "Uniform analysis results - has_time: " << has_time << ", has_st: " << has_st;
	
	if (has_time) {
		ofLogNotice("ShaderManager") << "Enabling automatic time updates";
		shader_node->setAutoUpdateTime(true);
	}
	if (has_st) {
		ofLogNotice("ShaderManager") << "Enabling automatic resolution updates";
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
