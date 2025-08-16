#include "ExpressionParser.h"
#include "BuiltinVariables.h"
#include "ofMain.h"
#include <regex>
#include <algorithm>

//--------------------------------------------------------------
ExpressionInfo ExpressionParser::parseExpression(const std::string& expr) {
    ExpressionInfo info;
    info.original = expr;
    
    try {
        // Check if it's a simple variable first
        if (isSimpleVariable(expr)) {
            info.glsl_code = expr;
            info.dependencies = {expr};
            info.is_simple_var = true;
            info.is_constant = false;
            
            // Get type from BuiltinVariables, considering swizzles
            BuiltinVariables& builtins = BuiltinVariables::getInstance();
            std::string base_var = builtins.extractBaseVariable(expr);
            const BuiltinVariable* builtin_info = builtins.getBuiltinInfo(base_var);
            
            if (builtin_info) {
                // Check if this is a swizzled access
                if (builtins.hasSwizzle(expr)) {
                    std::string swizzle = builtins.extractSwizzle(expr);
                    int swizzle_components = swizzle.length();
                    
                    // Determine type based on swizzle component count
                    switch (swizzle_components) {
                        case 1: info.type = "float"; break;
                        case 2: info.type = "vec2"; break;
                        case 3: info.type = "vec3"; break;
                        case 4: info.type = "vec4"; break;
                        default: info.type = "float"; break;
                    }
                } else {
                    // Use base variable type
                    info.type = builtin_info->glsl_type;
                }
            } else {
                info.type = "float"; // Default for user variables
            }
            
            return info;
        }
        
        // Parse complex expression
        parser.SetExpr(expr);
        info.dependencies = extractDependencies(expr);
        info.glsl_code = convertToGLSL(expr);
        info.type = inferGLSLType(expr, info.dependencies);
        info.is_simple_var = false;
        
        // Check if it's a constant expression
        if (info.dependencies.empty()) {
            info.is_constant = true;
            info.constant_value = parser.Eval();
        } else {
            info.is_constant = false;
            info.constant_value = 0.0;
        }
        
    } catch (mu::Parser::exception_type& e) {
        ofLogError("ExpressionParser") << "Parse error in '" << expr << "': " << e.GetMsg();
        ofLogError("ExpressionParser") << "  Position: " << e.GetPos();
        ofLogError("ExpressionParser") << "  Token: '" << e.GetToken() << "'";
        
        // Return fallback info
        info.glsl_code = expr;
        info.type = "float";
        info.is_simple_var = false;
        info.is_constant = false;
        info.dependencies = {}; // Empty dependencies on error
    }
    
    return info;
}

//--------------------------------------------------------------
bool ExpressionParser::isSimpleVariable(const std::string& expr) {
    // Check if expr contains only alphanumeric chars, underscore, and dots (for swizzling)
    std::regex simple_var_pattern("^[a-zA-Z_][a-zA-Z0-9_]*(\\.?[xyzwrgba]+)?$");
    return std::regex_match(expr, simple_var_pattern);
}

//--------------------------------------------------------------
std::string ExpressionParser::convertToGLSL(const std::string& expr) {
    // For complex expressions, just return as-is since muParser validates the syntax
    // GLSL can handle the same mathematical expressions that muParser accepts
    return expr;
}

//--------------------------------------------------------------
std::string ExpressionParser::inferGLSLType(const std::string& expr, const std::vector<std::string>& dependencies) {
    // For now, most mathematical expressions return float
    // This can be extended later to handle vector operations
    
    // Check if any dependency is a vector type
    BuiltinVariables& builtins = BuiltinVariables::getInstance();
    for (const auto& dep : dependencies) {
        std::string base_var = builtins.extractBaseVariable(dep);
        const BuiltinVariable* builtin_info = builtins.getBuiltinInfo(base_var);
        
        if (builtin_info && builtin_info->glsl_type != "float") {
            // If we have vector dependencies, we might need more complex analysis
            // For now, mathematical operations on vectors typically return float
            // unless it's a direct vector access
            if (dep == base_var) {
                // Direct vector access (e.g., "st") - this would be handled as simple var
                return builtin_info->glsl_type;
            }
        }
    }
    
    // Default to float for mathematical expressions
    return "float";
}

//--------------------------------------------------------------
std::vector<std::string> ExpressionParser::extractDependencies(const std::string& expr) {
    std::vector<std::string> deps;
    
    // For expressions containing GLSL swizzle syntax (e.g., "st.x"), 
    // we need to handle them specially since muParser doesn't understand GLSL syntax
    if (expr.find('.') != std::string::npos) {
        // Parse GLSL-style expressions manually
        deps = extractDependenciesManually(expr);
    } else {
        // Use muParser for pure mathematical expressions
        try {
            parser.SetExpr(expr);
            mu::varmap_type variables = parser.GetUsedVar();
            
            for (const auto& var : variables) {
                deps.push_back(var.first);
            }
            
            // Sort for consistent ordering
            std::sort(deps.begin(), deps.end());
            
        } catch (mu::Parser::exception_type& e) {
            ofLogError("ExpressionParser") << "Error extracting dependencies from '" << expr << "': " << e.GetMsg();
            ofLogError("ExpressionParser") << "  Position: " << e.GetPos();
            ofLogError("ExpressionParser") << "  Token: '" << e.GetToken() << "'";
            
            // Fallback to manual parsing
            deps = extractDependenciesManually(expr);
        }
    }
    
    return deps;
}

//--------------------------------------------------------------
std::vector<std::string> ExpressionParser::extractDependenciesManually(const std::string& expr) {
    std::vector<std::string> deps;
    std::set<std::string> unique_deps; // To avoid duplicates
    
    // Simple regex-based approach to find variable names
    // Look for patterns like: word characters, optionally followed by .word characters
    std::regex var_pattern("\\b([a-zA-Z_][a-zA-Z0-9_]*(?:\\.[a-zA-Z0-9_]+)?)\\b");
    std::sregex_iterator iter(expr.begin(), expr.end(), var_pattern);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        std::string match = iter->str();
        
        // Skip numeric literals
        if (!std::isdigit(match[0])) {
            // Check if this is a function call (followed by parentheses) or a variable
            size_t match_pos = expr.find(match);
            bool is_function_call = false;
            
            // Look for pattern: identifier followed by optional whitespace and '('
            if (match_pos != std::string::npos) {
                size_t check_pos = match_pos + match.length();
                // Skip whitespace
                while (check_pos < expr.length() && std::isspace(expr[check_pos])) {
                    check_pos++;
                }
                // Check if followed by '('
                if (check_pos < expr.length() && expr[check_pos] == '(') {
                    is_function_call = true;
                }
            }
            
            // Only add to dependencies if it's NOT a function call
            if (!is_function_call) {
                unique_deps.insert(match);
            }
        }
    }
    
    // Convert set to vector
    for (const auto& dep : unique_deps) {
        deps.push_back(dep);
    }
    
    std::sort(deps.begin(), deps.end());
    return deps;
}