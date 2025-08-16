#pragma once
#include "ExpressionParser.h"
#include "../pluginSystem/PluginManager.h"
#include "ofMain.h"
#include <string>
#include <vector>
#include <memory>

/**
 * @class ShaderCodeGenerator
 * @brief Generates GLSL shader code from templates and function metadata
 * @details Handles template-based code generation, uniform declarations,
 *          wrapper functions, and expression parsing integration
 */
class ShaderCodeGenerator {
private:
    // --- Dependencies ---
    PluginManager* plugin_manager; ///< Pointer to plugin manager for function metadata
    std::unique_ptr<ExpressionParser> expression_parser; ///< Expression parser for complex arguments
    
    // --- Shader Templates ---
    std::string default_vertex_shader; ///< Default vertex shader template
    std::string default_fragment_shader_template; ///< Fragment shader template with placeholders
    
public:
    /**
     * @brief Constructs the ShaderCodeGenerator
     * @param pm Pointer to initialized PluginManager
     */
    ShaderCodeGenerator(PluginManager* pm);
    
    /**
     * @brief Destructor
     */
    ~ShaderCodeGenerator();
    
    // --- Main Generation Methods ---
    /**
     * @brief Generates complete vertex shader code
     * @return Complete vertex shader source code
     */
    std::string generateVertexShader();
    
    /**
     * @brief Generates complete fragment shader code
     * @param glsl_function_code The GLSL function source code
     * @param function_name Name of the main function to call
     * @param arguments User-provided arguments (may include expressions)
     * @return Complete fragment shader source code
     */
    std::string generateFragmentShader(
        const std::string& glsl_function_code,
        const std::string& function_name,
        const std::vector<std::string>& arguments
    );
    
    // --- Component Generation Methods ---
    /**
     * @brief Generates uniform declarations based on arguments
     * @param arguments User-provided arguments (supports expressions)
     * @return GLSL uniform declaration code
     */
    std::string generateUniforms(const std::vector<std::string>& arguments);
    
    /**
     * @brief Generates main() function content
     * @param function_name Name of function to call
     * @param arguments User arguments (supports expressions)
     * @return GLSL main() function content
     */
    std::string generateMainFunction(
        const std::string& function_name, 
        const std::vector<std::string>& arguments
    );
    
    /**
     * @brief Generates wrapper function to adapt arguments to function signature
     * @param function_name Name of the function
     * @param user_arguments User-provided arguments
     * @param target_overload Target function overload to match
     * @return GLSL wrapper function code
     */
    std::string generateWrapperFunction(
        const std::string& function_name,
        const std::vector<std::string>& user_arguments,
        const FunctionOverload* target_overload
    );
    
    // --- Expression Support ---
    /**
     * @brief Parses an argument that may be an expression
     * @param argument The argument string (variable or expression)
     * @return ExpressionInfo with parsed details
     */
    ExpressionInfo parseArgument(const std::string& argument);
    
    // --- Template Management ---
    /**
     * @brief Initializes default shader templates
     */
    void initializeShaderTemplates();
    
private:
    // --- Internal Helper Methods ---
    /**
     * @brief Generates temporary variable declarations for complex expressions
     * @param arguments User arguments that may contain expressions
     * @return GLSL code declaring temporary variables
     */
    std::string generateTempVariables(const std::vector<std::string>& arguments);
    
    /**
     * @brief Converts user arguments to function call arguments
     * @param arguments User arguments (may include expressions)
     * @return Vector of argument names/expressions for function call
     */
    std::vector<std::string> convertArgumentsForCall(const std::vector<std::string>& arguments);
    
    /**
     * @brief Checks if argument is a floating-point literal
     * @param str String to check
     * @return True if str is a float literal
     */
    bool isFloatLiteral(const std::string& str);
    
    /**
     * @brief Checks if arguments can be combined to form a vector type
     * @param arguments User arguments
     * @param target_type Target GLSL type (vec2, vec3, vec4)
     * @return True if combination is valid
     */
    bool canCombineToVector(const std::vector<std::string>& arguments, const std::string& target_type);
    
    /**
     * @brief Generates GLSL type constructor from user arguments
     * @param target_type The target GLSL type (float, vec2, vec3, vec4)
     * @param user_arguments User-provided arguments
     * @return GLSL constructor expression
     */
    std::string generateTypeConstructor(const std::string& target_type, const std::vector<std::string>& user_arguments);
    
    /**
     * @brief Finds the best function overload that can accommodate all user arguments
     * @param function_name Name of the function
     * @param user_arguments User-provided arguments
     * @return Pointer to best matching overload, or nullptr if none found
     */
    const FunctionOverload* findBestOverloadForArguments(
        const std::string& function_name, 
        const std::vector<std::string>& user_arguments
    );
};