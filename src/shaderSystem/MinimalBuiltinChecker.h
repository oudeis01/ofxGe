#pragma once

#include <string>
#include <unordered_set>
#include <vector>

/**
 * @class MinimalBuiltinChecker
 * @brief Provides a minimal registry of GLSL built-in data types and functions
 * @details This class maintains a curated list of essential GLSL built-in types and functions
 *          to distinguish them from plugin-provided functions. The registry is kept minimal
 *          to reduce maintenance overhead while covering the most commonly used built-ins.
 */
class MinimalBuiltinChecker {
public:
    /**
     * @brief Checks if a given name is a GLSL built-in data type
     * @param type_name The name to check (e.g., "vec3", "float", "mat4")
     * @return True if it's a built-in data type, false otherwise
     */
    static bool isBuiltinDataType(const std::string& type_name);
    
    /**
     * @brief Checks if a given name is a GLSL built-in function
     * @param function_name The function name to check (e.g., "sin", "mix", "normalize")
     * @return True if it's a built-in function, false otherwise
     */
    static bool isBuiltinFunction(const std::string& function_name);
    
    /**
     * @brief Checks if a given name is any kind of GLSL built-in (type or function)
     * @param name The name to check
     * @return True if it's a built-in type or function, false otherwise
     */
    static bool isBuiltin(const std::string& name);
    
    /**
     * @brief Gets all supported built-in data types
     * @return A vector containing all built-in data type names
     */
    static std::vector<std::string> getAllBuiltinDataTypes();
    
    /**
     * @brief Gets all supported built-in functions
     * @return A vector containing all built-in function names
     */
    static std::vector<std::string> getAllBuiltinFunctions();
    
    /**
     * @brief Gets the total count of registered built-ins
     * @return Total number of built-in types and functions
     */
    static size_t getBuiltinCount();

private:
    // ================================================================================
    // GLSL BUILT-IN DATA TYPES REGISTRY
    // ================================================================================
    // Note: Keep this list organized and easy to maintain
    
    /// Boolean types
    static const std::unordered_set<std::string> boolean_types;
    
    /// Integer types
    static const std::unordered_set<std::string> integer_types;
    
    /// Unsigned integer types
    static const std::unordered_set<std::string> unsigned_integer_types;
    
    /// Floating point types
    static const std::unordered_set<std::string> float_types;
    
    /// Double precision types
    static const std::unordered_set<std::string> double_types;
    
    /// Matrix types
    static const std::unordered_set<std::string> matrix_types;
    
    /// Double precision matrix types
    static const std::unordered_set<std::string> double_matrix_types;
    
    // ================================================================================
    // GLSL BUILT-IN FUNCTIONS REGISTRY
    // ================================================================================
    // Note: Functions are organized by category for easy maintenance
    
    /// Angle and trigonometry functions
    static const std::unordered_set<std::string> angle_trigonometry_functions;
    
    /// Exponential functions
    static const std::unordered_set<std::string> exponential_functions;
    
    /// Common functions
    static const std::unordered_set<std::string> common_functions;
    
    /// Geometric functions
    static const std::unordered_set<std::string> geometric_functions;
    
    /// Matrix functions
    static const std::unordered_set<std::string> matrix_functions;
    
    /// Vector relational functions
    static const std::unordered_set<std::string> vector_relational_functions;
    
    // ================================================================================
    // HELPER METHODS
    // ================================================================================
    
    /**
     * @brief Helper to check if a name exists in any of the data type sets
     * @param name The name to check
     * @return True if found in any data type set
     */
    static bool checkInDataTypeSets(const std::string& name);
    
    /**
     * @brief Helper to check if a name exists in any of the function sets
     * @param name The name to check
     * @return True if found in any function set
     */
    static bool checkInFunctionSets(const std::string& name);
};