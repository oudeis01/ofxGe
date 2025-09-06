#include "ShaderCompositionEngine.h"
#include "ShaderManager.h"
#include "BuiltinVariables.h"
#include "ExpressionParser.h"
#include "ofLog.h"
#include <algorithm>
#include <sstream>
#include <regex>

//--------------------------------------------------------------
ShaderCompositionEngine::ShaderCompositionEngine(PluginManager* pm)
    : plugin_manager(pm)
    , debug_mode(true) {
    
    if (!plugin_manager) {
        ofLogError("ShaderCompositionEngine") << "PluginManager pointer is null";
        return;
    }
    
    ofLogNotice("ShaderCompositionEngine") << "ShaderCompositionEngine initialized";
}

//--------------------------------------------------------------
ShaderCompositionEngine::~ShaderCompositionEngine() {
    clearAll();
}

//--------------------------------------------------------------
std::string ShaderCompositionEngine::registerNode(const std::string& function_name, 
                                                  const std::vector<std::string>& arguments) {
    if (debug_mode) {
        ofLogNotice("ShaderCompositionEngine") << "Registering node: " << function_name 
                                               << " with " << arguments.size() << " arguments";
        for (size_t i = 0; i < arguments.size(); i++) {
            ofLogNotice("ShaderCompositionEngine") << "  Arg " << i << ": '" << arguments[i] << "'";
        }
    }
    
    // Validate that the function exists in the plugin system or is a GLSL builtin
    const GLSLFunction* function_metadata = plugin_manager->findFunction(function_name);
    if (!function_metadata) {
        // Check if it's a GLSL builtin function using FunctionDependencyAnalyzer
        FunctionDependencyAnalyzer analyzer(plugin_manager);
        
        ClassifiedFunction classification = analyzer.classifyFunction(function_name);
        if (classification.classification != FunctionClassification::GLSL_BUILTIN) {
            ofLogError("ShaderCompositionEngine") << "Function '" << function_name << "' not found in plugins or GLSL builtins";
            return "";
        }
        
        if (debug_mode) {
            ofLogNotice("ShaderCompositionEngine") << "Function '" << function_name << "' recognized as GLSL builtin";
        }
    }
    
    // Generate unique node ID
    std::string node_id = generateUniqueNodeId();
    
    // Create composition node
    auto node = std::make_unique<CompositionNode>(function_name, arguments, node_id);
    
    // Store the node
    pending_nodes[node_id] = std::move(node);
    
    if (debug_mode) {
        ofLogNotice("ShaderCompositionEngine") << "Registered node with ID: " << node_id;
    }
    
    return node_id;
}

//--------------------------------------------------------------
bool ShaderCompositionEngine::hasNode(const std::string& node_id) const {
    return pending_nodes.find(node_id) != pending_nodes.end();
}

//--------------------------------------------------------------
const CompositionNode* ShaderCompositionEngine::getNode(const std::string& node_id) const {
    auto it = pending_nodes.find(node_id);
    if (it != pending_nodes.end()) {
        return it->second.get();
    }
    return nullptr;
}

//--------------------------------------------------------------
std::shared_ptr<ShaderNode> ShaderCompositionEngine::compileGraph(const std::string& output_node_id) {
    if (debug_mode) {
        ofLogNotice("ShaderCompositionEngine") << "Compiling graph for output node: " << output_node_id;
    }
    
    // Check if the output node exists
    if (!hasNode(output_node_id)) {
        ofLogError("ShaderCompositionEngine") << "Output node not found: " << output_node_id;
        return nullptr;
    }
    
    // Analyze dependencies to get the compilation order
    std::vector<std::string> dependency_chain = analyzeDependencies(output_node_id);
    if (dependency_chain.empty()) {
        ofLogError("ShaderCompositionEngine") << "Failed to analyze dependencies for node: " << output_node_id;
        return nullptr;
    }
    
    if (debug_mode) {
        ofLogNotice("ShaderCompositionEngine") << "Dependency chain has " << dependency_chain.size() << " nodes";
        for (size_t i = 0; i < dependency_chain.size(); i++) {
            ofLogNotice("ShaderCompositionEngine") << "  " << i << ": " << dependency_chain[i];
        }
    }
    
    // Check cache for already compiled graph
    std::string graph_key = generateGraphKey(dependency_chain);
    auto cached_shader = getCachedCompiledGraph(graph_key);
    if (cached_shader && cached_shader->isReady()) {
        if (debug_mode) {
            ofLogNotice("ShaderCompositionEngine") << "Found cached compiled graph: " << graph_key;
        }
        return cached_shader;
    }
    
    // Generate unified shader code
    std::string unified_code = generateUnifiedShaderCode(dependency_chain);
    if (unified_code.empty()) {
        ofLogError("ShaderCompositionEngine") << "Failed to generate unified shader code";
        return nullptr;
    }
    
    if (debug_mode) {
        ofLogNotice("ShaderCompositionEngine") << "Generated unified shader code (" 
                                               << unified_code.length() << " characters)";
    }
    
    // Create a shader node with the unified code
    // We'll use a temporary ShaderManager instance to compile the unified shader
    ShaderManager temp_shader_manager(plugin_manager);
    
    // For now, create a simple shader node manually
    // TODO: This should be integrated better with the existing ShaderManager system
    auto compiled_shader = std::make_shared<ShaderNode>("unified_graph", std::vector<std::string>());
    
    // Set the unified code directly (this is a simplified approach)
    // In a full implementation, we'd need to integrate this with ShaderCodeGenerator
    compiled_shader->setCustomShaderCode(unified_code);
    
    // Configure automatic uniforms based on arguments used in the dependency chain
    // This replicates the logic from ShaderManager::createShader()
    std::vector<std::string> all_arguments;
    for (const auto& node_id : dependency_chain) {
        auto node_it = pending_nodes.find(node_id);
        if (node_it != pending_nodes.end()) {
            const auto& node = node_it->second;
            all_arguments.insert(all_arguments.end(), node->arguments.begin(), node->arguments.end());
        }
    }
    
    BuiltinVariables& builtins = BuiltinVariables::getInstance();
    bool has_time = false;
    bool has_st = false;
    
    for (const auto& arg : all_arguments) {
        // For simple variables, use the old method
        if (builtins.extractBaseVariable(arg) == "time") {
            has_time = true;
        }
        if (builtins.extractBaseVariable(arg) == "st") {
            has_st = true;
        }
        
        // For complex expressions, parse dependencies
        if (builtins.isComplexExpression(arg)) {
            if (debug_mode) {
                ofLogNotice("ShaderCompositionEngine") << "Complex expression detected: '" << arg << "'";
            }
            ExpressionParser temp_parser;
            ExpressionInfo expr_info = temp_parser.parseExpression(arg);
            
            for (const auto& dep : expr_info.dependencies) {
                std::string base_var = builtins.extractBaseVariable(dep);
                if (base_var == "time") {
                    if (debug_mode) {
                        ofLogNotice("ShaderCompositionEngine") << "TIME dependency found in expression: " << arg;
                    }
                    has_time = true;
                }
                if (base_var == "st") {
                    if (debug_mode) {
                        ofLogNotice("ShaderCompositionEngine") << "ST dependency found in expression: " << arg;
                    }
                    has_st = true;
                }
            }
        }
    }
    
    if (debug_mode) {
        ofLogNotice("ShaderCompositionEngine") << "Uniform analysis results - has_time: " << has_time << ", has_st: " << has_st;
    }
    
    if (has_time) {
        if (debug_mode) {
            ofLogNotice("ShaderCompositionEngine") << "Enabling automatic time updates";
        }
        compiled_shader->setAutoUpdateTime(true);
    }
    if (has_st) {
        if (debug_mode) {
            ofLogNotice("ShaderCompositionEngine") << "Enabling automatic resolution updates";
        }
        compiled_shader->setAutoUpdateResolution(true);
    }
    
    if (compiled_shader->compile()) {
        // Cache the successful compilation
        cacheCompiledGraph(graph_key, compiled_shader);
        
        if (debug_mode) {
            ofLogNotice("ShaderCompositionEngine") << "Successfully compiled unified graph";
        }
        
        return compiled_shader;
    } else {
        ofLogError("ShaderCompositionEngine") << "Failed to compile unified shader";
        return nullptr;
    }
}

//--------------------------------------------------------------
std::vector<std::string> ShaderCompositionEngine::analyzeDependencies(const std::string& output_node_id) {
    std::vector<std::string> sorted_nodes;
    
    // First resolve dependencies for all nodes
    for (auto& [node_id, node] : pending_nodes) {
        if (!resolveDependencies(node.get())) {
            ofLogError("ShaderCompositionEngine") << "Failed to resolve dependencies for node: " << node_id;
            return {};
        }
    }
    
    // Perform topological sort
    if (!topologicalSort(output_node_id, sorted_nodes)) {
        ofLogError("ShaderCompositionEngine") << "Topological sort failed - circular dependency detected";
        return {};
    }
    
    return sorted_nodes;
}

//--------------------------------------------------------------
std::shared_ptr<ShaderNode> ShaderCompositionEngine::getCachedCompiledGraph(const std::string& graph_key) {
    auto it = compiled_cache.find(graph_key);
    if (it != compiled_cache.end()) {
        return it->second;
    }
    return nullptr;
}

//--------------------------------------------------------------
void ShaderCompositionEngine::cacheCompiledGraph(const std::string& graph_key, 
                                                 std::shared_ptr<ShaderNode> compiled_shader) {
    compiled_cache[graph_key] = compiled_shader;
    
    if (debug_mode) {
        ofLogNotice("ShaderCompositionEngine") << "Cached compiled graph: " << graph_key;
    }
}

//--------------------------------------------------------------
std::string ShaderCompositionEngine::generateGraphKey(const std::vector<std::string>& dependency_chain) {
    std::stringstream ss;
    ss << "graph_";
    
    for (size_t i = 0; i < dependency_chain.size(); i++) {
        if (i > 0) ss << "_";
        
        const CompositionNode* node = getNode(dependency_chain[i]);
        if (node) {
            ss << node->function_name << "(";
            for (size_t j = 0; j < node->arguments.size(); j++) {
                if (j > 0) ss << ",";
                ss << node->arguments[j];
            }
            ss << ")";
        }
    }
    
    return ss.str();
}

//--------------------------------------------------------------
bool ShaderCompositionEngine::removeNode(const std::string& node_id) {
    auto it = pending_nodes.find(node_id);
    if (it != pending_nodes.end()) {
        pending_nodes.erase(it);
        
        if (debug_mode) {
            ofLogNotice("ShaderCompositionEngine") << "Removed node: " << node_id;
        }
        return true;
    }
    return false;
}

//--------------------------------------------------------------
void ShaderCompositionEngine::clearAll() {
    pending_nodes.clear();
    compiled_cache.clear();
    next_node_id = 1;
    
    if (debug_mode) {
        ofLogNotice("ShaderCompositionEngine") << "Cleared all nodes and cache";
    }
}

//--------------------------------------------------------------
size_t ShaderCompositionEngine::getNodeCount() const {
    return pending_nodes.size();
}

//--------------------------------------------------------------
void ShaderCompositionEngine::printGraphInfo() const {
    ofLogNotice("ShaderCompositionEngine") << "=== Graph Information ===";
    ofLogNotice("ShaderCompositionEngine") << "Total nodes: " << pending_nodes.size();
    ofLogNotice("ShaderCompositionEngine") << "Cached graphs: " << compiled_cache.size();
    
    for (const auto& [node_id, node] : pending_nodes) {
        ofLogNotice("ShaderCompositionEngine") << "Node " << node_id << ": " 
                                               << node->function_name << " (" 
                                               << node->arguments.size() << " args, "
                                               << node->input_nodes.size() << " deps)";
    }
}

//--------------------------------------------------------------
void ShaderCompositionEngine::setDebugMode(bool debug) {
    debug_mode = debug;
    ofLogNotice("ShaderCompositionEngine") << "Debug mode: " << (debug ? "enabled" : "disabled");
}

// ================================================================================
// PRIVATE METHODS
// ================================================================================

//--------------------------------------------------------------
std::string ShaderCompositionEngine::generateUniqueNodeId() {
    return "shader_" + std::to_string(next_node_id++);
}

//--------------------------------------------------------------
bool ShaderCompositionEngine::resolveDependencies(CompositionNode* node) {
    if (!node) return false;
    
    node->input_nodes.clear();
    node->resolved_arguments = node->arguments;
    
    // Look for $shader_XXX references in arguments
    for (size_t i = 0; i < node->arguments.size(); i++) {
        const std::string& arg = node->arguments[i];
        
        if (debug_mode) {
            ofLogNotice("ShaderCompositionEngine") << "Checking argument " << i << ": '" << arg << "'";
        }
        
        // Look for shader_XXX patterns (with or without $)
        // Check if the entire argument is a shader reference or contains one
        std::regex shader_ref_regex(R"((?:^|\$)(shader_\w+))");
        std::smatch match;
        
        if (std::regex_search(arg, match, shader_ref_regex)) {
            std::string referenced_id = match[1].str();
            
            if (debug_mode) {
                ofLogNotice("ShaderCompositionEngine") << "Found shader reference: " << referenced_id << " in argument: " << arg;
            }
            
            // Find the referenced node
            auto it = pending_nodes.find(referenced_id);
            if (it != pending_nodes.end()) {
                node->input_nodes.push_back(it->second.get());
                node->is_external_dependency = true;
                
                if (debug_mode) {
                    ofLogNotice("ShaderCompositionEngine") << "Node " << node->node_id 
                                                           << " depends on " << referenced_id;
                }
            } else {
                ofLogError("ShaderCompositionEngine") << "Referenced node not found: " << referenced_id;
                return false;
            }
        }
    }
    
    return true;
}

//--------------------------------------------------------------
bool ShaderCompositionEngine::topologicalSort(const std::string& output_node_id, 
                                              std::vector<std::string>& sorted_nodes) {
    std::unordered_map<std::string, bool> visited;
    std::unordered_map<std::string, bool> rec_stack;
    
    // Initialize visited map
    for (const auto& [node_id, node] : pending_nodes) {
        visited[node_id] = false;
        rec_stack[node_id] = false;
    }
    
    return topologicalSortDFS(output_node_id, visited, rec_stack, sorted_nodes);
}

//--------------------------------------------------------------
bool ShaderCompositionEngine::topologicalSortDFS(const std::string& node_id,
                                                 std::unordered_map<std::string, bool>& visited,
                                                 std::unordered_map<std::string, bool>& rec_stack,
                                                 std::vector<std::string>& sorted_nodes) {
    
    visited[node_id] = true;
    rec_stack[node_id] = true;
    
    const CompositionNode* current_node = getNode(node_id);
    if (!current_node) {
        ofLogError("ShaderCompositionEngine") << "Node not found during DFS: " << node_id;
        return false;
    }
    
    // Visit all dependencies first
    for (const CompositionNode* dep_node : current_node->input_nodes) {
        if (!visited[dep_node->node_id]) {
            if (!topologicalSortDFS(dep_node->node_id, visited, rec_stack, sorted_nodes)) {
                return false; // Circular dependency detected
            }
        } else if (rec_stack[dep_node->node_id]) {
            ofLogError("ShaderCompositionEngine") << "Circular dependency detected involving: " 
                                                  << node_id << " -> " << dep_node->node_id;
            return false;
        }
    }
    
    rec_stack[node_id] = false;
    sorted_nodes.push_back(node_id);
    
    return true;
}

//--------------------------------------------------------------
std::string ShaderCompositionEngine::generateUnifiedShaderCode(const std::vector<std::string>& dependency_chain) {
    // This is a simplified implementation
    // In a full implementation, this would integrate with ShaderCodeGenerator
    // to produce properly optimized, unified GLSL code
    
    std::stringstream unified_code;
    
    // Add standard shader header
    unified_code << "#version 330 core\n";
    unified_code << "uniform vec2 resolution;\n";
    unified_code << "uniform float time;\n";
    unified_code << "uniform vec2 st;\n";
    unified_code << "out vec4 fragColor;\n";
    unified_code << "\n";
    
    // Add function definitions for each node in the chain
    for (const std::string& node_id : dependency_chain) {
        const CompositionNode* node = getNode(node_id);
        if (!node) continue;
        
        unified_code << "// Node: " << node_id << " (" << node->function_name << ")\n";
        
        // Classify the function to determine if it's a builtin or plugin function
        FunctionDependencyAnalyzer analyzer(plugin_manager);
        ClassifiedFunction classification = analyzer.classifyFunction(node->function_name);
        
        if (classification.classification == FunctionClassification::PLUGIN_FUNCTION) {
            // Include the GLSL file - let OpenFrameworks handle dependencies
            const GLSLFunction* function_metadata = plugin_manager->findFunction(node->function_name);
            if (function_metadata) {
                unified_code << "#include \"plugins/lygia/" << function_metadata->filePath << "\"\n";
                
                // Generate wrapper function using existing ShaderCodeGenerator system
                ShaderCodeGenerator temp_generator(plugin_manager);
                
                // Find best overload first
                const GLSLFunction* func_metadata = plugin_manager->findFunction(node->function_name);
                if (func_metadata && !func_metadata->overloads.empty()) {
                    // Use the first overload for now - in practice we'd select the best one
                    const FunctionOverload* target_overload = &func_metadata->overloads[0];
                    
                    std::string wrapper_code = temp_generator.generateWrapperFunction(
                        node->function_name, 
                        node->arguments, 
                        target_overload
                    );
                    
                    if (!wrapper_code.empty()) {
                        unified_code << wrapper_code << "\n";
                        unified_code << "// Node: " << node_id << " wrapper generated\n\n";
                    } else {
                        unified_code << "// Node: " << node_id << " (" << node->function_name << ") - wrapper generation failed\n\n";
                    }
                } else {
                    unified_code << "// Node: " << node_id << " (" << node->function_name << ") - no function metadata\n\n";
                }
            } else {
                unified_code << "// Error: Function metadata not found for " << node->function_name << "\n\n";
            }
        } else if (classification.classification == FunctionClassification::GLSL_BUILTIN) {
            // For GLSL builtins, we don't need to include anything - they're already available
            unified_code << "// GLSL builtin function: " << node->function_name << " (no include needed)\n\n";
        } else {
            // Unknown function
            unified_code << "// Unknown function: " << node->function_name << "\n\n";
        }
    }
    
    // Generate main function that executes the dependency chain
    if (!dependency_chain.empty()) {
        unified_code << "void main() {\n";
        unified_code << "    vec2 st = gl_FragCoord.xy / resolution.xy;\n";
        
        // Execute each node in the dependency chain
        for (size_t i = 0; i < dependency_chain.size(); i++) {
            const std::string& node_id = dependency_chain[i];
            const CompositionNode* node_data = getNode(node_id);
            
            if (!node_data) {
                if (debug_mode) {
                    ofLogError("ShaderCompositionEngine") << "Node not found in main generation: " << node_id;
                }
                continue;
            }
            
            // Classify function
            FunctionDependencyAnalyzer analyzer(plugin_manager);
            ClassifiedFunction classification = analyzer.classifyFunction(node_data->function_name);
            
            if (debug_mode) {
                ofLogNotice("ShaderCompositionEngine") << "Generating call for: " 
                                                       << node_data->function_name 
                                                       << " -> classification " << (int)classification.classification;
            }
            
            // Generate variable name for this result
            std::string var_name = node_id + "_result";
            
            // Generate argument list - replace $shader_XXX with actual variable names
            std::string arg_list;
            for (size_t j = 0; j < node_data->arguments.size(); j++) {
                if (j > 0) arg_list += ", ";
                
                std::string arg = node_data->arguments[j];
                // Replace $shader_XXX references with variable names
                if (arg.substr(0, 8) == "$shader_") {
                    std::string ref_id = arg.substr(1); // Remove $
                    arg_list += ref_id + "_result";
                } else {
                    arg_list += arg;
                }
            }
            
            // Generate the function call
            if (classification.classification == FunctionClassification::PLUGIN_FUNCTION) {
                // Use wrapper function for plugin functions
                unified_code << "    float " << var_name << " = " 
                           << node_data->function_name << "_wrapper(" << arg_list << ");\n";
            } else if (classification.classification == FunctionClassification::GLSL_BUILTIN) {
                // Direct call for GLSL builtin functions
                unified_code << "    float " << var_name << " = " 
                           << node_data->function_name << "(" << arg_list << ");\n";
            } else {
                // Unknown function - default to 0.0
                unified_code << "    float " << var_name << " = 0.0; // Unknown function\n";
            }
        }
        
        // Use the final result
        if (!dependency_chain.empty()) {
            std::string final_var = dependency_chain.back() + "_result";
            unified_code << "    fragColor = vec4(vec3(" << final_var << "), 1.0);\n";
        } else {
            unified_code << "    fragColor = vec4(0.0, 0.0, 0.0, 1.0);\n";
        }
        
        unified_code << "}\n";
    }
    
    std::string result = unified_code.str();
    
    if (debug_mode) {
        ofLogNotice("ShaderCompositionEngine") << "Generated unified shader:\n" << result;
    }
    
    return result;
}

//--------------------------------------------------------------
std::string ShaderCompositionEngine::optimizeShaderCode(const std::string& shader_code) {
    // TODO: Implement shader code optimization
    // - Function inlining
    // - Dead code elimination
    // - Variable optimization
    return shader_code;
}

//--------------------------------------------------------------
bool ShaderCompositionEngine::validateDependencyChain(const std::vector<std::string>& dependency_chain) {
    for (const std::string& node_id : dependency_chain) {
        if (!hasNode(node_id)) {
            ofLogError("ShaderCompositionEngine") << "Invalid node in dependency chain: " << node_id;
            return false;
        }
    }
    return true;
}

//--------------------------------------------------------------
std::string ShaderCompositionEngine::loadFunctionSource(const std::string& function_name, const std::string& plugin_name) {
    if (!plugin_manager) {
        ofLogError("ShaderCompositionEngine") << "Plugin manager is null";
        return "";
    }
    
    // Find the function metadata from the plugin
    const GLSLFunction* function_metadata = plugin_manager->findFunction(function_name);
    if (!function_metadata) {
        ofLogError("ShaderCompositionEngine") << "Function not found: " << function_name;
        return "";
    }
    
    if (debug_mode) {
        ofLogNotice("ShaderCompositionEngine") << "Loading function source for: " << function_name 
                                               << " from file: " << function_metadata->filePath;
    }
    
    // Use ShaderManager's existing file resolution logic
    // We'll need to create a temporary ShaderManager instance for this
    ShaderManager temp_manager(plugin_manager);
    std::string file_path = temp_manager.resolveGLSLFilePath(plugin_name, function_metadata->filePath);
    
    // Read the file content directly using ofBufferFromFile
    ofBuffer buffer = ofBufferFromFile(file_path);
    if (buffer.size() == 0) {
        ofLogError("ShaderCompositionEngine") << "Failed to open or read file: " << file_path;
        return "";
    }
    std::string glsl_content = buffer.getText();
    
    if (glsl_content.empty()) {
        ofLogError("ShaderCompositionEngine") << "Failed to load GLSL file: " << file_path;
        return "";
    }
    
    if (debug_mode) {
        ofLogNotice("ShaderCompositionEngine") << "Loaded GLSL content (" << glsl_content.length() 
                                               << " characters) from: " << file_path;
    }
    
    return glsl_content;
}

//--------------------------------------------------------------
std::string ShaderCompositionEngine::extractFunctionDefinition(const std::string& glsl_content, const std::string& function_name) {
    if (glsl_content.empty() || function_name.empty()) {
        return "";
    }
    
    // Look for function definitions in the form: returnType functionName(args) { ... }
    // We'll use a simple approach to find the function definition
    std::string result;
    
    // Find the function signature - construct regex pattern
    std::string pattern = R"(\b\w+\s+)" + function_name + R"(\s*\([^)]*\)\s*\{)";
    std::regex function_regex(pattern);
    std::smatch match;
    
    if (std::regex_search(glsl_content, match, function_regex)) {
        size_t start_pos = match.position();
        size_t brace_pos = start_pos + match.length() - 1; // Position of opening brace
        
        // Find the matching closing brace
        int brace_count = 1;
        size_t end_pos = brace_pos + 1;
        
        while (end_pos < glsl_content.length() && brace_count > 0) {
            if (glsl_content[end_pos] == '{') {
                brace_count++;
            } else if (glsl_content[end_pos] == '}') {
                brace_count--;
            }
            end_pos++;
        }
        
        if (brace_count == 0) {
            result = glsl_content.substr(start_pos, end_pos - start_pos);
            
            if (debug_mode) {
                ofLogNotice("ShaderCompositionEngine") << "Extracted function definition for " 
                                                       << function_name << " (" << result.length() << " characters)";
            }
        } else {
            ofLogError("ShaderCompositionEngine") << "Unmatched braces in function: " << function_name;
        }
    } else {
        ofLogError("ShaderCompositionEngine") << "Function definition not found: " << function_name;
    }
    
    return result;
}

//--------------------------------------------------------------
std::string ShaderCompositionEngine::inlineFunctionCode(const std::string& function_code, const std::string& node_id_prefix) {
    if (function_code.empty()) {
        return "";
    }
    
    std::string result = function_code;
    
    // Rename the function itself to have a unique name
    // Find function name pattern: returnType functionName(
    std::regex function_name_regex(R"(\b(\w+)\s+(\w+)\s*\()");
    std::smatch match;
    
    if (std::regex_search(result, match, function_name_regex)) {
        std::string return_type = match[1].str();
        std::string original_name = match[2].str();
        std::string new_function_name = node_id_prefix + "_" + original_name;
        
        // Replace the function name
        std::string pattern = R"(\b)" + original_name + R"(\s*\()";
        std::regex name_regex(pattern);
        result = std::regex_replace(result, name_regex, new_function_name + "(");
        
        if (debug_mode) {
            ofLogNotice("ShaderCompositionEngine") << "Renamed function " << original_name 
                                                   << " to " << new_function_name;
        }
    }
    
    // For now, use a simpler approach: just rename the most common problematic variables
    // This is a temporary solution - in production we'd need a proper GLSL parser
    
    std::vector<std::string> common_vars = {
        "C", "i", "x0", "i1", "x12", "p", "m", "x", "h", "ox", "a0", "g"
    };
    
    for (const std::string& var : common_vars) {
        // Replace all occurrences of these common variable names
        std::string pattern = R"(\b)" + var + R"(\b)";
        std::regex var_regex(pattern);
        std::string replacement = node_id_prefix + "_" + var;
        result = std::regex_replace(result, var_regex, replacement);
    }
    
    // Also handle function calls that might be missing
    // Common GLSL functions that might need to be available
    std::vector<std::string> glsl_functions = {"mod289", "permute", "fract", "floor", "dot", "abs"};
    // Note: These should remain as-is since they're GLSL built-ins or should be from dependencies
    
    if (debug_mode) {
        ofLogNotice("ShaderCompositionEngine") << "Inlined function code with prefix: " << node_id_prefix;
    }
    
    return result;
}