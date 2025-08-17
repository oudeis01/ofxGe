#pragma once

#include "MinimalBuiltinChecker.h"
#include "../pluginSystem/PluginManager.h"
#include <string>
#include <vector>
#include <set>
#include <map>
#include <regex>

/**
 * @enum FunctionClassification
 * @brief Classification of function types for dependency resolution
 */
enum class FunctionClassification {
    GLSL_BUILTIN,      ///< GLSL built-in function (no loading required)
    PLUGIN_FUNCTION,   ///< Plugin-provided function (needs loading)
    UNKNOWN_FUNCTION   ///< Unknown function (error case)
};

/**
 * @struct FunctionCall
 * @brief Represents a single function call found in an expression
 */
struct FunctionCall {
    std::string function_name;              ///< Name of the function
    std::vector<std::string> arguments;     ///< Arguments passed to the function
    std::string full_expression;           ///< Complete function call expression
    size_t start_pos;                      ///< Start position in original string
    size_t end_pos;                        ///< End position in original string
};

/**
 * @struct ClassifiedFunction
 * @brief Result of function classification analysis
 */
struct ClassifiedFunction {
    std::string function_name;
    FunctionClassification classification;
    std::string plugin_name;               ///< Plugin name if PLUGIN_FUNCTION
    std::string error_message;             ///< Error message if UNKNOWN_FUNCTION
};

/**
 * @struct DependencyAnalysisResult
 * @brief Complete result of dependency analysis for a shader creation request
 */
struct DependencyAnalysisResult {
    std::string main_function;                    ///< Main function name
    std::vector<std::string> final_arguments;    ///< Parsed final arguments
    std::set<std::string> required_plugin_functions; ///< Plugin functions that need loading
    std::set<std::string> used_builtin_functions;    ///< GLSL built-ins found (for reference)
    std::map<std::string, FunctionCall> function_calls; ///< All function calls found
    std::map<std::string, ClassifiedFunction> classified_functions; ///< Classification results
    bool is_valid;                               ///< Whether analysis succeeded
    std::string error_message;                   ///< Error message if failed
};

/**
 * @class FunctionDependencyAnalyzer
 * @brief Analyzes expressions to find nested function dependencies
 * @details This class parses complex expressions like "st,int(st.x*10.0)*random(time),time*0.5"
 *          to identify all function calls, classify them as GLSL built-ins or plugin functions,
 *          and determine which plugin functions need to be loaded for shader generation.
 */
class FunctionDependencyAnalyzer {
public:
    /**
     * @brief Constructor
     * @param plugin_manager Pointer to the plugin manager for function lookups
     */
    FunctionDependencyAnalyzer(PluginManager* plugin_manager);
    
    /**
     * @brief Analyzes a shader creation request to find all function dependencies
     * @param main_function The main function name (e.g., "snoise")
     * @param raw_arguments Comma-separated argument string (e.g., "st,int(st.x*10.0)*random(time),time*0.5")
     * @return Complete dependency analysis result
     */
    DependencyAnalysisResult analyzeCreateMessage(
        const std::string& main_function,
        const std::string& raw_arguments
    );
    
    /**
     * @brief Parses a comma-separated argument string into individual arguments
     * @param raw_arguments The raw argument string
     * @return Vector of parsed arguments
     */
    std::vector<std::string> parseArgumentList(const std::string& raw_arguments);
    
    /**
     * @brief Finds all function calls in a given expression
     * @param expression The expression to analyze
     * @return Vector of function calls found
     */
    std::vector<FunctionCall> extractFunctionCalls(const std::string& expression);
    
    /**
     * @brief Classifies a function as built-in, plugin, or unknown
     * @param function_name The name of the function to classify
     * @return Classification result
     */
    ClassifiedFunction classifyFunction(const std::string& function_name);

private:
    PluginManager* plugin_manager;          ///< Plugin manager for function lookups
    
    /**
     * @brief Recursively finds all function dependencies in an expression
     * @param expression The expression to analyze
     * @param found_functions Set to store found function names
     */
    void findAllDependencies(const std::string& expression, std::set<std::string>& found_functions);
    
    /**
     * @brief Extracts function arguments from a parenthesized string
     * @param args_string String containing arguments (without outer parentheses)
     * @return Vector of argument strings
     */
    std::vector<std::string> extractFunctionArguments(const std::string& args_string);
    
    /**
     * @brief Finds matching closing parenthesis for an opening parenthesis
     * @param expression The expression to search in
     * @param start_pos Position of the opening parenthesis
     * @return Position of the matching closing parenthesis, or string::npos if not found
     */
    size_t findMatchingParenthesis(const std::string& expression, size_t start_pos);
    
    /**
     * @brief Validates that parentheses are properly balanced in an expression
     * @param expression The expression to validate
     * @return True if parentheses are balanced, false otherwise
     */
    bool isValidParenthesesStructure(const std::string& expression);
    
    /**
     * @brief Validates that a function name follows GLSL naming conventions
     * @param function_name The function name to validate
     * @return True if valid, false otherwise
     */
    bool isValidFunctionName(const std::string& function_name);
    
    /**
     * @brief Trims whitespace from the beginning and end of a string
     * @param str The string to trim
     * @return Trimmed string
     */
    std::string trim(const std::string& str);
    
    /**
     * @brief Checks if a string is empty or contains only whitespace
     * @param str The string to check
     * @return True if empty or whitespace only
     */
    bool isEmpty(const std::string& str);
};