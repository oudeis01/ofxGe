#pragma once
#include <string>
#include <vector>
#include <set>
#include <muParser.h>

/**
 * @struct ExpressionInfo
 * @brief Contains parsed information about a GLSL expression
 */
struct ExpressionInfo {
    std::string original;           ///< Original expression string
    std::string glsl_code;         ///< GLSL-compatible code
    std::string type;              ///< Inferred GLSL type (float, vec2, etc.)
    std::vector<std::string> dependencies; ///< Variables used in expression
    bool is_simple_var;            ///< True if expression is just a variable name
    bool is_constant;              ///< True if expression has no variables
    double constant_value;         ///< Value if is_constant is true
};

/**
 * @class ExpressionParser
 * @brief Parses mathematical expressions using muParser and converts them to GLSL
 */
class ExpressionParser {
private:
    mu::Parser parser;
    
public:
    /**
     * @brief Parses a mathematical expression into GLSL-usable format
     * @param expr The expression string (e.g., "sin(time*0.1)")
     * @return ExpressionInfo containing parsed details
     */
    ExpressionInfo parseExpression(const std::string& expr);
    
    /**
     * @brief Checks if an expression is just a simple variable name
     * @param expr The expression to check
     * @return True if expr is a simple variable (e.g., "time", "st")
     */
    bool isSimpleVariable(const std::string& expr);
    
private:
    /**
     * @brief Converts muParser expression to GLSL-compatible syntax
     * @param expr The original expression
     * @return GLSL-compatible expression string
     */
    std::string convertToGLSL(const std::string& expr);
    
    /**
     * @brief Infers the GLSL type of an expression
     * @param expr The expression string
     * @param dependencies Variables used in the expression
     * @return GLSL type string (float, vec2, vec3, vec4)
     */
    std::string inferGLSLType(const std::string& expr, const std::vector<std::string>& dependencies);
    
    /**
     * @brief Extracts variable names from a muParser expression
     * @param expr The expression string
     * @return Vector of variable names used
     */
    std::vector<std::string> extractDependencies(const std::string& expr);
    
    /**
     * @brief Manually extracts dependencies from GLSL-style expressions
     * @param expr The expression containing GLSL syntax (e.g., swizzles)
     * @return Vector of variable names used in the expression
     */
    std::vector<std::string> extractDependenciesManually(const std::string& expr);
};