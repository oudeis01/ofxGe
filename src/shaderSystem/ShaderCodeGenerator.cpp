#include "ShaderCodeGenerator.h"
#include "BuiltinVariables.h"
#include <sstream>
#include <set>
#include <algorithm>

//--------------------------------------------------------------
ShaderCodeGenerator::ShaderCodeGenerator(PluginManager* pm) 
    : plugin_manager(pm) {
    
    expression_parser = std::make_unique<ExpressionParser>();
    initializeShaderTemplates();
    
    ofLogNotice("ShaderCodeGenerator") << "ShaderCodeGenerator initialized";
}

//--------------------------------------------------------------
ShaderCodeGenerator::~ShaderCodeGenerator() {
    ofLogNotice("ShaderCodeGenerator") << "ShaderCodeGenerator destroyed";
}

//--------------------------------------------------------------
void ShaderCodeGenerator::initializeShaderTemplates() {
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
}

//--------------------------------------------------------------
std::string ShaderCodeGenerator::generateVertexShader() {
    return default_vertex_shader;
}

//--------------------------------------------------------------
std::string ShaderCodeGenerator::generateFragmentShader(
    const std::string& glsl_function_code,
    const std::string& function_name,
    const std::vector<std::string>& arguments) {
    
    // Debug: Log all arguments
    ofLogNotice("ShaderCodeGenerator") << "generateFragmentShader called with function: " << function_name;
    for (size_t i = 0; i < arguments.size(); i++) {
        ofLogNotice("ShaderCodeGenerator") << "  Argument " << i << ": '" << arguments[i] << "'";
    }
    
    std::string fragment_code = default_fragment_shader_template;
    
    // Generate components
    std::string uniforms = generateUniforms(arguments);
    std::string temp_vars = generateTempVariables(arguments);
    std::string main_content = generateMainFunction(function_name, arguments);
    
    // Generate wrapper functions if needed
    std::string wrapper_functions = "";
    const GLSLFunction* function_metadata = plugin_manager->findFunction(function_name);
    if (function_metadata && !function_metadata->overloads.empty()) {
        // Find best overload (simplified version)
        const FunctionOverload* best_overload = &function_metadata->overloads[0];
        wrapper_functions = generateWrapperFunction(function_name, arguments, best_overload);
    }
    
    // Combine function code
    std::string combined_functions = glsl_function_code;
    if (!wrapper_functions.empty()) {
        combined_functions += "\n\n// Generated wrapper function to adapt arguments\n" + wrapper_functions;
    }
    
    // Note: temp_vars will be inserted into main_content at the right position
    std::string complete_main = main_content;
    
    // Replace placeholders in template
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
        fragment_code.replace(pos, 14, complete_main);
    }
    
    // Debug: Print the complete generated fragment shader
    ofLogNotice("ShaderCodeGenerator") << "=== GENERATED FRAGMENT SHADER ===";
    ofLogNotice("ShaderCodeGenerator") << fragment_code;
    ofLogNotice("ShaderCodeGenerator") << "=== END FRAGMENT SHADER ===";
    
    return fragment_code;
}

//--------------------------------------------------------------
std::string ShaderCodeGenerator::generateUniforms(const std::vector<std::string>& arguments) {
    std::stringstream uniforms;
    std::set<std::string> needed_uniforms;
    
    for (const auto& arg : arguments) {
        ExpressionInfo expr_info = parseArgument(arg);
        
        // Add dependencies from expression to uniforms
        for (const auto& dep : expr_info.dependencies) {
            BuiltinVariables& builtins = BuiltinVariables::getInstance();
            std::string base_var = builtins.extractBaseVariable(dep);
            const BuiltinVariable* builtin_info = builtins.getBuiltinInfo(base_var);
            
            if (builtin_info && builtin_info->needs_uniform) {
                // 'st' requires the 'resolution' uniform
                if (base_var == "st") {
                    needed_uniforms.insert("resolution");
                } else {
                    needed_uniforms.insert(base_var);
                }
            } else if (!expr_info.is_constant) {
                // Assume any non-builtin variable is a user-defined float uniform
                needed_uniforms.insert(dep);
            }
        }
    }
    
    // Generate uniform declarations
    for (const auto& uniform_name : needed_uniforms) {
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
std::string ShaderCodeGenerator::generateTempVariables(const std::vector<std::string>& arguments) {
    std::stringstream temp_vars;
    
    ofLogNotice("ShaderCodeGenerator") << "generateTempVariables called with " << arguments.size() << " arguments";
    
    for (size_t i = 0; i < arguments.size(); i++) {
        ofLogNotice("ShaderCodeGenerator") << "  Processing argument " << i << ": '" << arguments[i] << "'";
        ExpressionInfo expr_info = parseArgument(arguments[i]);
        
        ofLogNotice("ShaderCodeGenerator") << "    Parsed - GLSL: '" << expr_info.glsl_code << "', Simple: " << expr_info.is_simple_var << ", Constant: " << expr_info.is_constant;
        
        // Generate temporary variable for complex expressions
        if (!expr_info.is_simple_var && !expr_info.is_constant) {
            temp_vars << "    " << expr_info.type << " _expr" << i 
                     << " = " << expr_info.glsl_code << ";\n";
        }
    }
    
    return temp_vars.str();
}

//--------------------------------------------------------------
std::string ShaderCodeGenerator::generateMainFunction(
    const std::string& function_name, 
    const std::vector<std::string>& arguments) {
    
    std::stringstream main_func;
    
    // Generate built-in variables
    BuiltinVariables& builtins = BuiltinVariables::getInstance();
    std::set<std::string> needed_builtins;
    
    for (const auto& arg : arguments) {
        ExpressionInfo expr_info = parseArgument(arg);
        for (const auto& dep : expr_info.dependencies) {
            std::string base_var = builtins.extractBaseVariable(dep);
            if (builtins.getBuiltinInfo(base_var)) {
                needed_builtins.insert(base_var);
            }
        }
    }
    
    // Generate builtin variable declarations FIRST (before temp variables that might use them)
    for (const auto& builtin_name : needed_builtins) {
        const BuiltinVariable* builtin_info = builtins.getBuiltinInfo(builtin_name);
        if (builtin_info && builtin_info->needs_declaration) {
            main_func << "    " << builtin_info->declaration_code << ";\n";
        }
    }
    
    if (!needed_builtins.empty()) {
        main_func << "\n";
    }
    
    // Insert temp variables AFTER builtin declarations
    std::string temp_vars = generateTempVariables(arguments);
    main_func << temp_vars;
    if (!temp_vars.empty()) {
        main_func << "\n";
    }
    
    // Convert arguments for function call
    std::vector<std::string> call_args = convertArgumentsForCall(arguments);
    
    // Generate function call (use wrapper if it was generated)
    const GLSLFunction* function_metadata = plugin_manager->findFunction(function_name);
    bool use_wrapper = (function_metadata && 
                       !function_metadata->overloads.empty() && 
                       function_metadata->overloads[0].paramTypes.size() == 1 && 
                       call_args.size() > 1);
    
    std::string func_to_call = use_wrapper ? function_name + "_wrapper" : function_name;
    
    // Get the return type from the best matching overload (not just first overload)
    std::string return_type = "vec3"; // Default
    if (function_metadata && !function_metadata->overloads.empty()) {
        const FunctionOverload* best_overload = findBestOverloadForArguments(function_name, arguments);
        if (best_overload) {
            return_type = best_overload->returnType;
        } else {
            return_type = function_metadata->overloads[0].returnType;
        }
    }
    
    main_func << "    " << return_type << " result = " << func_to_call << "(";
    for (size_t i = 0; i < call_args.size(); i++) {
        if (i > 0) main_func << ", ";
        main_func << call_args[i];
    }
    main_func << ");\n";
    
    // Convert result to vec4 for outputColor based on return type
    if (return_type == "float") {
        main_func << "    outputColor = vec4(vec3(result), 1.0);\n";
    } else if (return_type == "vec2") {
        main_func << "    outputColor = vec4(result.xy, 0.0, 1.0);\n";
    } else if (return_type == "vec3") {
        main_func << "    outputColor = vec4(result, 1.0);\n";
    } else if (return_type == "vec4") {
        main_func << "    outputColor = result;\n";
    } else {
        // Fallback for unknown types
        main_func << "    outputColor = vec4(vec3(result), 1.0);\n";
    }
    
    return main_func.str();
}

//--------------------------------------------------------------
std::vector<std::string> ShaderCodeGenerator::convertArgumentsForCall(const std::vector<std::string>& arguments) {
    std::vector<std::string> call_args;
    
    for (size_t i = 0; i < arguments.size(); i++) {
        ExpressionInfo expr_info = parseArgument(arguments[i]);
        
        if (expr_info.is_constant) {
            // Use the constant value directly
            call_args.push_back(std::to_string(expr_info.constant_value));
        } else if (expr_info.is_simple_var) {
            // Use the variable name directly
            call_args.push_back(expr_info.glsl_code);
        } else {
            // Use the temporary variable
            call_args.push_back("_expr" + std::to_string(i));
        }
    }
    
    return call_args;
}

//--------------------------------------------------------------
std::string ShaderCodeGenerator::generateWrapperFunction(
    const std::string& function_name,
    const std::vector<std::string>& user_arguments,
    const FunctionOverload* target_overload) {
    
    if (!target_overload) {
        return "";
    }
    
    // Find best overload that can accommodate all user arguments
    const FunctionOverload* best_overload = findBestOverloadForArguments(function_name, user_arguments);
    
    if (best_overload && best_overload != target_overload) {
        // Generate wrapper for the best matching overload
        std::stringstream wrapper;
        
        // Generate wrapper function signature
        wrapper << best_overload->returnType << " " << function_name << "_wrapper(";
        for (size_t i = 0; i < user_arguments.size(); i++) {
            if (i > 0) wrapper << ", ";
            
            ExpressionInfo expr_info = parseArgument(user_arguments[i]);
            wrapper << expr_info.type << " arg" << i;
        }
        wrapper << ") {\n";
        
        // Generate function body - handle multi-parameter functions
        wrapper << "    return " << function_name << "(";
        
        if (best_overload->paramTypes.size() == 1) {
            // Single parameter function - use type constructor
            std::string target_type = best_overload->paramTypes[0];
            wrapper << generateTypeConstructor(target_type, user_arguments);
        } else {
            // Multi-parameter function - generate parameter list
            wrapper << generateMultiParameterCall(best_overload, user_arguments);
        }
        
        wrapper << ");\n";
        wrapper << "}\n";
        
        ofLogNotice("ShaderCodeGenerator") << "Generated wrapper function:\n" << wrapper.str();
        return wrapper.str();
    }
    
    // Fallback: generate wrapper for single parameter functions with multiple arguments
    if (target_overload->paramTypes.size() == 1 && user_arguments.size() > 1) {
        std::stringstream wrapper;
        std::string target_type = target_overload->paramTypes[0];
        
        // Generate wrapper function signature
        wrapper << target_overload->returnType << " " << function_name << "_wrapper(";
        for (size_t i = 0; i < user_arguments.size(); i++) {
            if (i > 0) wrapper << ", ";
            
            ExpressionInfo expr_info = parseArgument(user_arguments[i]);
            wrapper << expr_info.type << " arg" << i;
        }
        wrapper << ") {\n";
        
        // Generate function body that combines arguments into target type
        wrapper << "    return " << function_name << "(" << generateTypeConstructor(target_type, user_arguments) << ");\n";
        wrapper << "}\n";
        
        ofLogNotice("ShaderCodeGenerator") << "Generated wrapper function:\n" << wrapper.str();
        return wrapper.str();
    }
    
    return "";
}

//--------------------------------------------------------------
ExpressionInfo ShaderCodeGenerator::parseArgument(const std::string& argument) {
    return expression_parser->parseExpression(argument);
}

//--------------------------------------------------------------
bool ShaderCodeGenerator::isFloatLiteral(const std::string& str) {
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
bool ShaderCodeGenerator::canCombineToVector(const std::vector<std::string>& arguments, const std::string& target_type) {
    int required_components = 0;
    if (target_type == "vec2") required_components = 2;
    else if (target_type == "vec3") required_components = 3;
    else if (target_type == "vec4") required_components = 4;
    else if (target_type == "float") required_components = 1;
    else return false;
    
    int total_components = 0;
    BuiltinVariables& builtins = BuiltinVariables::getInstance();
    
    for (const auto& arg : arguments) {
        ExpressionInfo expr_info = parseArgument(arg);
        
        if (expr_info.is_constant || isFloatLiteral(arg)) {
            total_components += 1;
        } else {
            std::string base_var = builtins.extractBaseVariable(arg);
            const BuiltinVariable* builtin_info = builtins.getBuiltinInfo(base_var);
            
            if (builtin_info) {
                if (builtin_info->glsl_type == "vec2") total_components += 2;
                else if (builtin_info->glsl_type == "vec3") total_components += 3;
                else if (builtin_info->glsl_type == "vec4") total_components += 4;
                else total_components += 1;
            } else {
                total_components += 1; // Assume float
            }
        }
    }
    
    return total_components == required_components;
}

//--------------------------------------------------------------
std::string ShaderCodeGenerator::generateTypeConstructor(const std::string& target_type, const std::vector<std::string>& user_arguments) {
    std::stringstream constructor;
    constructor << target_type << "(";
    
    // Handle different target types
    if (target_type == "float") {
        // For float, just use first argument
        constructor << "arg0";
    }
    else if (target_type == "vec2") {
        if (user_arguments.size() == 2) {
            ExpressionInfo arg0_info = parseArgument(user_arguments[0]);
            ExpressionInfo arg1_info = parseArgument(user_arguments[1]);
            
            if (arg0_info.type == "float" && arg1_info.type == "float") {
                // vec2(float, float)
                constructor << "arg0, arg1";
            } else if (arg0_info.type == "vec2") {
                // Use first argument if it's already vec2
                constructor << "arg0";
            } else {
                // Fallback: try to combine
                constructor << "arg0, arg1";
            }
        } else {
            // Fallback: use first argument
            constructor << "arg0";
        }
    }
    else if (target_type == "vec3") {
        if (user_arguments.size() == 2) {
            ExpressionInfo arg0_info = parseArgument(user_arguments[0]);
            ExpressionInfo arg1_info = parseArgument(user_arguments[1]);
            
            if (arg0_info.type == "vec2" && arg1_info.type == "float") {
                // vec3(vec2.xy, float)
                constructor << "arg0.xy, arg1";
            } else {
                // vec3(float, float, 0.0)
                constructor << "arg0, arg1, 0.0";
            }
        } else if (user_arguments.size() == 3) {
            // vec3(float, float, float)
            constructor << "arg0, arg1, arg2";
        } else {
            // Fallback
            constructor << "arg0, 0.0, 0.0";
        }
    }
    else if (target_type == "vec4") {
        if (user_arguments.size() == 2) {
            ExpressionInfo arg0_info = parseArgument(user_arguments[0]);
            ExpressionInfo arg1_info = parseArgument(user_arguments[1]);
            
            if (arg0_info.type == "vec3" && arg1_info.type == "float") {
                // vec4(vec3.xyz, float)
                constructor << "arg0.xyz, arg1";
            } else if (arg0_info.type == "vec2" && arg1_info.type == "vec2") {
                // vec4(vec2.xy, vec2.xy)
                constructor << "arg0.xy, arg1.xy";
            } else {
                // vec4(float, float, 0.0, 0.0)
                constructor << "arg0, arg1, 0.0, 0.0";
            }
        } else if (user_arguments.size() == 4) {
            // vec4(float, float, float, float)
            constructor << "arg0, arg1, arg2, arg3";
        } else {
            // Fallback
            constructor << "arg0, 0.0, 0.0, 1.0";
        }
    }
    else {
        // For unknown types, just pass first argument
        constructor << "arg0";
    }
    
    constructor << ")";
    return constructor.str();
}

//--------------------------------------------------------------
const FunctionOverload* ShaderCodeGenerator::findBestOverloadForArguments(
    const std::string& function_name, 
    const std::vector<std::string>& user_arguments) {
    
    const GLSLFunction* function_metadata = plugin_manager->findFunction(function_name);
    if (!function_metadata || function_metadata->overloads.empty()) {
        return nullptr;
    }
    
    // Calculate total components from user arguments
    int total_components = 0;
    for (const auto& arg : user_arguments) {
        ExpressionInfo expr_info = parseArgument(arg);
        if (expr_info.type == "float") total_components += 1;
        else if (expr_info.type == "vec2") total_components += 2;
        else if (expr_info.type == "vec3") total_components += 3;
        else if (expr_info.type == "vec4") total_components += 4;
        else total_components += 1; // Default to float
    }
    
    // PRIORITY 1: Look for multi-parameter overloads that match argument pattern
    // Special case: 4 float arguments should prefer (vec3, float) over (vec3)
    if (user_arguments.size() == 4 && total_components == 4) {
        for (const auto& overload : function_metadata->overloads) {
            if (overload.paramTypes.size() == 2 &&
                overload.paramTypes[0] == "vec3" && 
                overload.paramTypes[1] == "float") {
                ofLogNotice("ShaderCodeGenerator") << "Selected 2-parameter overload (vec3, float) for 4 arguments";
                return &overload;
            }
        }
    }
    
    // PRIORITY 2: Look for multi-parameter overloads that can handle the argument count
    for (const auto& overload : function_metadata->overloads) {
        if (overload.paramTypes.size() > 1) {
            // Calculate required components for this overload
            int required_components = 0;
            for (const auto& param_type : overload.paramTypes) {
                if (param_type == "float") required_components += 1;
                else if (param_type == "vec2") required_components += 2;
                else if (param_type == "vec3") required_components += 3;
                else if (param_type == "vec4") required_components += 4;
                else required_components += 1; // Default
            }
            
            // Exact component match for multi-parameter functions
            if (required_components == total_components) {
                ofLogNotice("ShaderCodeGenerator") << "Selected multi-parameter overload with " 
                    << overload.paramTypes.size() << " parameters";
                return &overload;
            }
        }
    }
    
    // PRIORITY 3: Fall back to single-parameter overloads
    const FunctionOverload* best_match = nullptr;
    
    for (const auto& overload : function_metadata->overloads) {
        if (overload.paramTypes.size() == 1) {
            const std::string& param_type = overload.paramTypes[0];
            int required_components = 1;
            
            if (param_type == "vec2") required_components = 2;
            else if (param_type == "vec3") required_components = 3;
            else if (param_type == "vec4") required_components = 4;
            
            // Find exact match or best fit
            if (required_components == total_components) {
                return &overload; // Exact match
            }
            
            // If no exact match found yet, prefer larger types that can accommodate all components
            if (!best_match && required_components >= total_components) {
                best_match = &overload;
            }
        }
    }
    
    return best_match;
}

//--------------------------------------------------------------
std::string ShaderCodeGenerator::generateMultiParameterCall(
    const FunctionOverload* overload,
    const std::vector<std::string>& user_arguments) {
    
    if (!overload || overload->paramTypes.empty()) {
        return "";
    }
    
    std::stringstream call;
    
    // Handle specific case: sphereSDF(vec3, float) with 4 float arguments
    if (overload->paramTypes.size() == 2 && 
        overload->paramTypes[0] == "vec3" && 
        overload->paramTypes[1] == "float" &&
        user_arguments.size() == 4) {
        
        // First 3 arguments -> vec3, last argument -> float
        call << "vec3(arg0, arg1, arg2), arg3";
        return call.str();
    }
    
    // General case: map arguments to parameters
    size_t arg_index = 0;
    
    for (size_t param_index = 0; param_index < overload->paramTypes.size(); param_index++) {
        if (param_index > 0) call << ", ";
        
        const std::string& param_type = overload->paramTypes[param_index];
        
        if (param_type == "float") {
            if (arg_index < user_arguments.size()) {
                call << "arg" << arg_index;
                arg_index++;
            } else {
                call << "0.0";  // Default value
            }
        } else if (param_type == "vec2") {
            if (arg_index + 1 < user_arguments.size()) {
                call << "vec2(arg" << arg_index << ", arg" << (arg_index + 1) << ")";
                arg_index += 2;
            } else if (arg_index < user_arguments.size()) {
                call << "vec2(arg" << arg_index << ", 0.0)";
                arg_index++;
            } else {
                call << "vec2(0.0)";
            }
        } else if (param_type == "vec3") {
            if (arg_index + 2 < user_arguments.size()) {
                call << "vec3(arg" << arg_index << ", arg" << (arg_index + 1) << ", arg" << (arg_index + 2) << ")";
                arg_index += 3;
            } else if (arg_index + 1 < user_arguments.size()) {
                call << "vec3(arg" << arg_index << ", arg" << (arg_index + 1) << ", 0.0)";
                arg_index += 2;
            } else if (arg_index < user_arguments.size()) {
                call << "vec3(arg" << arg_index << ", 0.0, 0.0)";
                arg_index++;
            } else {
                call << "vec3(0.0)";
            }
        } else if (param_type == "vec4") {
            if (arg_index + 3 < user_arguments.size()) {
                call << "vec4(arg" << arg_index << ", arg" << (arg_index + 1) << ", arg" << (arg_index + 2) << ", arg" << (arg_index + 3) << ")";
                arg_index += 4;
            } else {
                // Fill with available arguments and pad with zeros
                call << "vec4(";
                for (int i = 0; i < 4; i++) {
                    if (i > 0) call << ", ";
                    if (arg_index < user_arguments.size()) {
                        call << "arg" << arg_index;
                        arg_index++;
                    } else {
                        call << "0.0";
                    }
                }
                call << ")";
            }
        } else {
            // Unknown type, use first available argument
            if (arg_index < user_arguments.size()) {
                call << "arg" << arg_index;
                arg_index++;
            } else {
                call << "0.0";
            }
        }
    }
    
    return call.str();
}