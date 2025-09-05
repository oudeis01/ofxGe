#include "ShaderCompositionEngine.h"
#include "ShaderManager.h"
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
        
        // Simple regex to find $shader_XXX patterns
        std::regex shader_ref_regex(R"(\$shader_(\w+))");
        std::smatch match;
        
        if (std::regex_search(arg, match, shader_ref_regex)) {
            std::string referenced_id = "shader_" + match[1].str();
            
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
    
    // Add function declarations for each node in the chain
    for (const std::string& node_id : dependency_chain) {
        const CompositionNode* node = getNode(node_id);
        if (!node) continue;
        
        unified_code << "// Node: " << node_id << " (" << node->function_name << ")\n";
        
        // This is where we'd load the actual GLSL function code
        // For now, we'll create a placeholder
        unified_code << "float " << node_id << "_func() {\n";
        unified_code << "    // TODO: Inline " << node->function_name << " function\n";
        unified_code << "    return 0.5;\n";
        unified_code << "}\n\n";
    }
    
    // Generate main function that calls the final node
    if (!dependency_chain.empty()) {
        std::string final_node = dependency_chain.back();
        unified_code << "void main() {\n";
        unified_code << "    vec2 st = gl_FragCoord.xy / resolution.xy;\n";
        unified_code << "    float result = " << final_node << "_func();\n";
        unified_code << "    fragColor = vec4(vec3(result), 1.0);\n";
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