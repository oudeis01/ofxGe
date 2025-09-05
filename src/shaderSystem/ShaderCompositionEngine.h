#pragma once

#include "ShaderNode.h"
#include "../pluginSystem/PluginManager.h"
#include "FunctionDependencyAnalyzer.h"
#include "ofMain.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

/**
 * @struct CompositionNode
 * @brief Represents a node in the shader composition graph
 * @details Each node stores function metadata and dependency information
 *          without creating actual shader programs until compilation time.
 */
struct CompositionNode {
    std::string function_name;                    ///< The GLSL function name
    std::vector<std::string> arguments;           ///< Raw argument strings
    std::vector<CompositionNode*> input_nodes;    ///< Dependencies on other nodes
    std::string node_id;                         ///< Unique identifier for this node
    
    // Resolved information (populated during dependency analysis)
    std::vector<std::string> resolved_arguments;  ///< Arguments with $shader_XXX resolved
    bool is_external_dependency;                 ///< True if depends on external nodes
    
    CompositionNode(const std::string& func_name, 
                   const std::vector<std::string>& args, 
                   const std::string& id)
        : function_name(func_name)
        , arguments(args)
        , node_id(id)
        , is_external_dependency(false) {}
};

/**
 * @class ShaderCompositionEngine
 * @brief Manages deferred compilation of shader composition graphs
 * @details This class implements the core optimization from the analysis document.
 *          Instead of compiling each OSC /create message immediately, it builds
 *          a dependency graph and compiles the entire chain into a single shader
 *          when /connect is called.
 */
class ShaderCompositionEngine {
public:
    ShaderCompositionEngine(PluginManager* plugin_manager);
    ~ShaderCompositionEngine();
    
    // ================================================================================
    // NODE REGISTRATION (OSC /create handling)
    // ================================================================================
    
    /**
     * @brief Registers a shader node without compiling it
     * @param function_name The GLSL function to use
     * @param arguments Raw argument strings (may contain $shader_XXX references)
     * @return Unique node ID for this shader, empty string on error
     */
    std::string registerNode(const std::string& function_name, 
                            const std::vector<std::string>& arguments);
    
    /**
     * @brief Checks if a node with the given ID exists
     * @param node_id The ID to check
     * @return True if the node exists, false otherwise
     */
    bool hasNode(const std::string& node_id) const;
    
    /**
     * @brief Gets information about a registered node
     * @param node_id The ID of the node
     * @return Pointer to CompositionNode, nullptr if not found
     */
    const CompositionNode* getNode(const std::string& node_id) const;
    
    // ================================================================================
    // GRAPH COMPILATION (OSC /connect handling)
    // ================================================================================
    
    /**
     * @brief Compiles the entire dependency graph into a single shader
     * @param output_node_id The ID of the final output node
     * @return Compiled ShaderNode ready for rendering, nullptr on error
     */
    std::shared_ptr<ShaderNode> compileGraph(const std::string& output_node_id);
    
    /**
     * @brief Analyzes the dependency graph for a given output node
     * @param output_node_id The root node to analyze
     * @return Vector of node IDs in topological order, empty on error
     */
    std::vector<std::string> analyzeDependencies(const std::string& output_node_id);
    
    // ================================================================================
    // CACHE MANAGEMENT
    // ================================================================================
    
    /**
     * @brief Checks if a compiled graph is already cached
     * @param graph_key Unique key representing the graph structure
     * @return Cached shader if available, nullptr otherwise
     */
    std::shared_ptr<ShaderNode> getCachedCompiledGraph(const std::string& graph_key);
    
    /**
     * @brief Stores a compiled graph in the cache
     * @param graph_key Unique key for the graph
     * @param compiled_shader The compiled shader to cache
     */
    void cacheCompiledGraph(const std::string& graph_key, 
                           std::shared_ptr<ShaderNode> compiled_shader);
    
    /**
     * @brief Generates a unique key for a shader graph structure
     * @param dependency_chain Vector of node IDs in topological order
     * @return Unique string key for caching
     */
    std::string generateGraphKey(const std::vector<std::string>& dependency_chain);
    
    // ================================================================================
    // UTILITY METHODS
    // ================================================================================
    
    /**
     * @brief Removes a node and cleans up any dependencies
     * @param node_id The ID of the node to remove
     * @return True if removed successfully, false if not found
     */
    bool removeNode(const std::string& node_id);
    
    /**
     * @brief Clears all registered nodes and cache
     */
    void clearAll();
    
    /**
     * @brief Gets the total number of registered nodes
     * @return Count of nodes in the graph
     */
    size_t getNodeCount() const;
    
    /**
     * @brief Prints debug information about the current graph state
     */
    void printGraphInfo() const;
    
    /**
     * @brief Enables or disables verbose debug logging
     * @param debug True to enable debug mode, false to disable
     */
    void setDebugMode(bool debug);

private:
    // ================================================================================
    // INTERNAL STATE
    // ================================================================================
    
    PluginManager* plugin_manager;                ///< Reference to the plugin system
    bool debug_mode;                             ///< Debug logging flag
    
    // Node storage and management
    std::unordered_map<std::string, std::unique_ptr<CompositionNode>> pending_nodes;
    std::atomic<int> next_node_id{1};            ///< Counter for generating unique IDs
    
    // Caching system for compiled graphs
    std::unordered_map<std::string, std::shared_ptr<ShaderNode>> compiled_cache;
    
    // ================================================================================
    // INTERNAL METHODS
    // ================================================================================
    
    /**
     * @brief Generates a unique node ID
     * @return New unique ID string
     */
    std::string generateUniqueNodeId();
    
    /**
     * @brief Resolves shader references in arguments ($shader_XXX -> actual dependencies)
     * @param node The node to resolve dependencies for
     * @return True if successful, false on circular dependency or missing reference
     */
    bool resolveDependencies(CompositionNode* node);
    
    /**
     * @brief Performs topological sort on the dependency graph
     * @param output_node_id The root node to start from
     * @param sorted_nodes Output vector for the sorted node IDs
     * @return True if successful, false on circular dependency
     */
    bool topologicalSort(const std::string& output_node_id, 
                        std::vector<std::string>& sorted_nodes);
    
    /**
     * @brief Helper for topological sort using DFS
     * @param node_id Current node being processed
     * @param visited Set of visited nodes
     * @param rec_stack Recursion stack for cycle detection
     * @param sorted_nodes Output vector for sorted nodes
     * @return True if successful, false on circular dependency
     */
    bool topologicalSortDFS(const std::string& node_id,
                           std::unordered_map<std::string, bool>& visited,
                           std::unordered_map<std::string, bool>& rec_stack,
                           std::vector<std::string>& sorted_nodes);
    
    /**
     * @brief Generates unified GLSL code from the dependency chain
     * @param dependency_chain Nodes in topological order
     * @return Complete GLSL fragment shader code, empty on error
     */
    std::string generateUnifiedShaderCode(const std::vector<std::string>& dependency_chain);
    
    /**
     * @brief Inlines function calls to eliminate intermediate steps
     * @param shader_code The GLSL code to optimize
     * @return Optimized GLSL code with inlined functions
     */
    std::string optimizeShaderCode(const std::string& shader_code);
    
    /**
     * @brief Validates that all nodes in the dependency chain can be compiled
     * @param dependency_chain The chain to validate
     * @return True if all nodes are valid, false otherwise
     */
    bool validateDependencyChain(const std::vector<std::string>& dependency_chain);
};