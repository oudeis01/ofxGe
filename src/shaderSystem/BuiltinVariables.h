#pragma once
#include <string>
#include <unordered_map>
#include <set>

/**
 * @struct BuiltinVariable
 * @brief  Holds metadata for a built-in variable available in the shader system.
 * @details This struct defines the properties of a variable that the ShaderManager
 *          can automatically provide and manage within a generated shader.
 */
struct BuiltinVariable {
    std::string name;             ///< The name of the variable used in shader arguments (e.g., "st", "time").
    std::string glsl_type;        ///< The corresponding GLSL type (e.g., "vec2", "float").
    int component_count;          ///< The number of components in the GLSL type (e.g., float=1, vec2=2).
    bool needs_uniform;           ///< True if this variable requires a uniform declaration in the shader.
    bool needs_declaration;       ///< True if this variable requires a local declaration in the main() function.
    std::string declaration_code; ///< The GLSL code for the local declaration (if needed).

    /**
     * @brief Default constructor.
     */
    BuiltinVariable() = default;
    
    /**
     * @brief Constructs a BuiltinVariable instance.
     * @param n The name of the variable.
     * @param type The GLSL type string.
     * @param comp The number of components.
     * @param uniform True if a uniform is required.
     * @param decl True if a local declaration is required.
     * @param decl_code The code for the local declaration.
     */
    BuiltinVariable(const std::string& n, const std::string& type, int comp, 
                   bool uniform, bool decl, const std::string& decl_code = "")
        : name(n), glsl_type(type), component_count(comp), 
          needs_uniform(uniform), needs_declaration(decl), declaration_code(decl_code) {}
};

/**
 * @class BuiltinVariables
 * @brief A singleton manager for GLSL built-in variables.
 * @details This class provides a centralized repository of information about
 *          "built-in" variables like 'st', 'time', and 'resolution' that can be
 *          used in shader generation. It helps in validating arguments and
 *          generating necessary shader code.
 */
class BuiltinVariables {
public:
    /**
     * @brief Gets the singleton instance of the BuiltinVariables manager.
     * @return A reference to the singleton instance.
     */
    static BuiltinVariables& getInstance();
    
    /**
     * @brief Retrieves the metadata for a specific built-in variable.
     * @param name The name of the built-in variable.
     * @return A const pointer to the BuiltinVariable struct if found, otherwise nullptr.
     */
    const BuiltinVariable* getBuiltinInfo(const std::string& name) const;
    
    /**
     * @brief Checks if a given name corresponds to a known built-in variable.
     * @param name The name to check.
     * @return True if the name is a built-in variable, false otherwise.
     */
    bool isBuiltin(const std::string& name) const;
    
    /**
     * @brief Extracts the base variable name from a swizzled expression.
     * @details For example, "st.xy" would return "st".
     * @param variable The full variable expression (e.g., "st.x").
     * @return The base variable name.
     */
    std::string extractBaseVariable(const std::string& variable) const;
    
    /**
     * @brief Checks if a variable expression contains a swizzle operator ('.').
     * @param variable The variable expression to check.
     * @return True if a '.' is found, false otherwise.
     */
    bool hasSwizzle(const std::string& variable) const;
    
    /**
     * @brief Extracts the swizzle component from a variable expression.
     * @details For example, "st.xy" would return "xy".
     * @param variable The full variable expression.
     * @return The swizzle string, or an empty string if none is present.
     */
    std::string extractSwizzle(const std::string& variable) const;
    
    /**
     * @brief Gets a list of all known built-in variable names.
     * @return A set of strings containing all built-in names.
     */
    std::set<std::string> getAllBuiltinNames() const;
    
    /**
     * @brief Validates a variable expression, including its swizzle part.
     * @param variable The variable expression to validate (e.g., "st.xyz").
     * @param[out] errorMessage A string to receive an error message if validation fails.
     * @return True if the variable and its swizzle are valid, false otherwise.
     */
    bool isValidSwizzle(const std::string& variable, std::string& errorMessage) const;
    
    /**
     * @brief Gets the supported swizzle components for a base variable.
     * @param baseVariable The base variable name (e.g., "st").
     * @return A string containing the valid swizzle characters (e.g., "xy").
     */
    std::string getSupportedSwizzleComponents(const std::string& baseVariable) const;
    
    /**
     * @brief Formats the supported swizzle components into a human-readable string.
     * @param glslType The GLSL type of the variable.
     * @param componentCount The number of components.
     * @return A formatted string (e.g., "x, y, z").
     */
    std::string formatSupportedComponents(const std::string& glslType, int componentCount) const;
    
    /**
     * @brief Checks if a string represents a floating-point literal.
     * @param str The string to check.
     * @return True if the string is a valid float literal, false otherwise.
     */
    bool isFloatLiteral(const std::string& str) const;
    
    /**
     * @brief Checks if a string represents a complex mathematical expression.
     * @param expr The string to check.
     * @return True if the string contains operators or function calls, false otherwise.
     */
    bool isComplexExpression(const std::string& expr) const;

private:
    /**
     * @brief Private constructor to enforce the singleton pattern.
     */
    BuiltinVariables();

    /**
     * @brief Initializes the map of built-in variables.
     */
    void initializeBuiltins();
    
    ///< A map storing the metadata for all known built-in variables.
    std::unordered_map<std::string, BuiltinVariable> builtins;
};
