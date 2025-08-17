#include "FunctionDependencyAnalyzer.h"
#include "ofMain.h"
#include <algorithm>
#include <sstream>

//--------------------------------------------------------------
FunctionDependencyAnalyzer::FunctionDependencyAnalyzer(PluginManager* plugin_manager)
    : plugin_manager(plugin_manager) {
    if (!plugin_manager) {
        ofLogError("FunctionDependencyAnalyzer") << "Plugin manager is null!";
    }
}

//--------------------------------------------------------------
DependencyAnalysisResult FunctionDependencyAnalyzer::analyzeCreateMessage(
    const std::string& main_function,
    const std::string& raw_arguments) {
    
    DependencyAnalysisResult result;
    result.main_function = main_function;
    result.is_valid = false;
    
    ofLogNotice("FunctionDependencyAnalyzer") << "=== Starting Dependency Analysis ===";
    ofLogNotice("FunctionDependencyAnalyzer") << "Main function: " << main_function;
    ofLogNotice("FunctionDependencyAnalyzer") << "Raw arguments: " << raw_arguments;
    
    // 1. Classify main function
    ClassifiedFunction main_func_class = classifyFunction(main_function);
    if (main_func_class.classification == FunctionClassification::UNKNOWN_FUNCTION) {
        result.error_message = main_func_class.error_message;
        return result;
    }
    
    result.classified_functions[main_function] = main_func_class;
    
    // 2. Parse arguments
    result.final_arguments = parseArgumentList(raw_arguments);
    ofLogNotice("FunctionDependencyAnalyzer") << "Parsed " << result.final_arguments.size() << " arguments";
    
    // 3. Find all function dependencies in arguments
    std::set<std::string> all_found_functions;
    for (const std::string& arg : result.final_arguments) {
        findAllDependencies(arg, all_found_functions);
    }
    
    ofLogNotice("FunctionDependencyAnalyzer") << "Found " << all_found_functions.size() << " function dependencies";
    
    // 4. Classify all found functions
    for (const std::string& func_name : all_found_functions) {
        ClassifiedFunction classified = classifyFunction(func_name);
        result.classified_functions[func_name] = classified;
        
        if (classified.classification == FunctionClassification::UNKNOWN_FUNCTION) {
            result.error_message = classified.error_message;
            return result;
        }
        
        // Separate into different sets based on classification
        if (classified.classification == FunctionClassification::PLUGIN_FUNCTION) {
            result.required_plugin_functions.insert(func_name);
        } else if (classified.classification == FunctionClassification::GLSL_BUILTIN) {
            result.used_builtin_functions.insert(func_name);
        }
    }
    
    // 5. Add main function to plugin functions if needed
    if (main_func_class.classification == FunctionClassification::PLUGIN_FUNCTION) {
        result.required_plugin_functions.insert(main_function);
    } else if (main_func_class.classification == FunctionClassification::GLSL_BUILTIN) {
        result.used_builtin_functions.insert(main_function);
    }
    
    // 6. Extract detailed function call information
    for (const std::string& arg : result.final_arguments) {
        std::vector<FunctionCall> calls = extractFunctionCalls(arg);
        for (const FunctionCall& call : calls) {
            result.function_calls[call.function_name] = call;
        }
    }
    
    // 7. Log analysis results
    ofLogNotice("FunctionDependencyAnalyzer") << "=== Analysis Results ===";
    ofLogNotice("FunctionDependencyAnalyzer") << "Plugin functions to load: " << result.required_plugin_functions.size();
    for (const std::string& func : result.required_plugin_functions) {
        ofLogNotice("FunctionDependencyAnalyzer") << "  - " << func << " (Plugin)";
    }
    
    ofLogNotice("FunctionDependencyAnalyzer") << "GLSL built-ins found: " << result.used_builtin_functions.size();
    for (const std::string& func : result.used_builtin_functions) {
        ofLogNotice("FunctionDependencyAnalyzer") << "  - " << func << " (Built-in)";
    }
    
    result.is_valid = true;
    return result;
}

//--------------------------------------------------------------
std::vector<std::string> FunctionDependencyAnalyzer::parseArgumentList(const std::string& raw_arguments) {
    std::vector<std::string> result;
    
    if (raw_arguments.empty()) {
        return result;
    }
    
    // Simple comma splitting with parentheses awareness
    std::string current_arg;
    int paren_depth = 0;
    
    for (size_t i = 0; i < raw_arguments.length(); i++) {
        char c = raw_arguments[i];
        
        if (c == '(') {
            paren_depth++;
            current_arg += c;
        } else if (c == ')') {
            paren_depth--;
            current_arg += c;
        } else if (c == ',' && paren_depth == 0) {
            // Top-level comma, split here
            std::string trimmed = trim(current_arg);
            if (!isEmpty(trimmed)) {
                result.push_back(trimmed);
            }
            current_arg.clear();
        } else {
            current_arg += c;
        }
    }
    
    // Add the last argument
    std::string trimmed = trim(current_arg);
    if (!isEmpty(trimmed)) {
        result.push_back(trimmed);
    }
    
    return result;
}

//--------------------------------------------------------------
std::vector<FunctionCall> FunctionDependencyAnalyzer::extractFunctionCalls(const std::string& expression) {
    std::vector<FunctionCall> function_calls;
    
    // Regular expression to find function patterns: identifier followed by opening parenthesis
    std::regex function_pattern(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\()");
    
    std::sregex_iterator iter(expression.begin(), expression.end(), function_pattern);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        const std::smatch& match = *iter;
        std::string func_name = match[1].str();
        size_t start_pos = match.position();
        
        // Find the opening parenthesis position
        size_t paren_pos = start_pos + func_name.length();
        while (paren_pos < expression.length() && expression[paren_pos] != '(') {
            paren_pos++;
        }
        
        if (paren_pos >= expression.length()) {
            continue; // No opening parenthesis found
        }
        
        // Find matching closing parenthesis
        size_t closing_paren = findMatchingParenthesis(expression, paren_pos);
        if (closing_paren == std::string::npos) {
            ofLogWarning("FunctionDependencyAnalyzer") 
                << "Unmatched parentheses for function: " << func_name;
            continue;
        }
        
        // Extract arguments string (between parentheses)
        std::string args_string = expression.substr(paren_pos + 1, closing_paren - paren_pos - 1);
        
        FunctionCall call;
        call.function_name = func_name;
        call.start_pos = start_pos;
        call.end_pos = closing_paren + 1;
        call.full_expression = expression.substr(start_pos, call.end_pos - start_pos);
        call.arguments = extractFunctionArguments(args_string);
        
        function_calls.push_back(call);
    }
    
    return function_calls;
}

//--------------------------------------------------------------
ClassifiedFunction FunctionDependencyAnalyzer::classifyFunction(const std::string& function_name) {
    ClassifiedFunction result;
    result.function_name = function_name;
    
    // 1. Check if it's a GLSL built-in function
    if (MinimalBuiltinChecker::isBuiltinFunction(function_name)) {
        result.classification = FunctionClassification::GLSL_BUILTIN;
        return result;
    }
    
    // 2. Check if it's a plugin function
    if (plugin_manager) {
        const GLSLFunction* func_metadata = plugin_manager->findFunction(function_name);
        if (func_metadata) {
            result.classification = FunctionClassification::PLUGIN_FUNCTION;
            
            // Find which plugin contains this function
            auto functions_by_plugin = plugin_manager->getFunctionsByPlugin();
            for (const auto& [plugin_name, function_list] : functions_by_plugin) {
                for (const auto& func_name : function_list) {
                    if (func_name == function_name) {
                        result.plugin_name = plugin_name;
                        return result;
                    }
                }
            }
            
            // Fallback if plugin name not found
            result.plugin_name = "unknown_plugin";
            return result;
        }
    }
    
    // 3. Unknown function
    result.classification = FunctionClassification::UNKNOWN_FUNCTION;
    result.error_message = "Function '" + function_name + "' not found in GLSL built-ins or plugins";
    
    return result;
}

//--------------------------------------------------------------
void FunctionDependencyAnalyzer::findAllDependencies(const std::string& expression, std::set<std::string>& found_functions) {
    std::vector<FunctionCall> calls = extractFunctionCalls(expression);
    
    for (const FunctionCall& call : calls) {
        found_functions.insert(call.function_name);
        
        // Recursively analyze function arguments
        for (const std::string& arg : call.arguments) {
            findAllDependencies(arg, found_functions);
        }
    }
}

//--------------------------------------------------------------
std::vector<std::string> FunctionDependencyAnalyzer::extractFunctionArguments(const std::string& args_string) {
    if (args_string.empty()) {
        return std::vector<std::string>();
    }
    
    // Use the same parsing logic as parseArgumentList
    return parseArgumentList(args_string);
}

//--------------------------------------------------------------
size_t FunctionDependencyAnalyzer::findMatchingParenthesis(const std::string& expression, size_t start_pos) {
    if (start_pos >= expression.length() || expression[start_pos] != '(') {
        return std::string::npos;
    }
    
    int depth = 0;
    for (size_t i = start_pos; i < expression.length(); i++) {
        if (expression[i] == '(') {
            depth++;
        } else if (expression[i] == ')') {
            depth--;
            if (depth == 0) {
                return i;
            }
        }
    }
    
    return std::string::npos; // Unmatched parenthesis
}

//--------------------------------------------------------------
bool FunctionDependencyAnalyzer::isValidParenthesesStructure(const std::string& expression) {
    int depth = 0;
    for (char c : expression) {
        if (c == '(') {
            depth++;
        } else if (c == ')') {
            depth--;
            if (depth < 0) {
                return false; // More closing than opening
            }
        }
    }
    return depth == 0; // All parentheses matched
}

//--------------------------------------------------------------
bool FunctionDependencyAnalyzer::isValidFunctionName(const std::string& function_name) {
    if (function_name.empty()) {
        return false;
    }
    
    // Must start with letter or underscore
    if (!std::isalpha(function_name[0]) && function_name[0] != '_') {
        return false;
    }
    
    // Rest must be alphanumeric or underscore
    for (size_t i = 1; i < function_name.length(); i++) {
        if (!std::isalnum(function_name[i]) && function_name[i] != '_') {
            return false;
        }
    }
    
    return true;
}

//--------------------------------------------------------------
std::string FunctionDependencyAnalyzer::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

//--------------------------------------------------------------
bool FunctionDependencyAnalyzer::isEmpty(const std::string& str) {
    return trim(str).empty();
}