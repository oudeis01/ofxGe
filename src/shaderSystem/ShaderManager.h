#pragma once
#include "ShaderNode.h"
#include "BuiltinVariables.h"
#include "../pluginSystem/PluginManager.h"
#include "ofMain.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <atomic>

/**
 * @class ShaderManager
 * @brief Manages the creation, caching, and lifecycle of dynamic GLSL shaders.
 * @details This class works with the PluginManager to find GLSL functions and
 *          transform them into fully compiled, render-ready ofShader objects.
 *          It handles dynamic code generation, uniform management, and caching
 *          to optimize performance.
 */
class ShaderManager {
private:
    // --- Dependencies ---
    PluginManager* plugin_manager; ///< A pointer to the plugin manager for accessing GLSL functions.
    
    // --- Internal State ---
    bool debug_mode; ///< Flag to control verbose debug logging
    
    // --- Caching System ---
    /// A cache for shader nodes, using a key generated from the function name and arguments.
    std::unordered_map<std::string, std::shared_ptr<ShaderNode>> shader_cache;
    
    // --- ID-based Management System ---
    /// A map of currently active shaders, managed by a unique ID.
    std::unordered_map<std::string, std::shared_ptr<ShaderNode>> active_shaders;
    /// An atomic counter to generate unique IDs for shaders.
    std::atomic<int> next_shader_id{0};
    
    // --- Shader Templates ---
    std::string default_vertex_shader; ///< The template for the default vertex shader.
    std::string default_fragment_shader_template; ///< The template for the fragment shader, with placeholders.
    
public:
    /**
     * @brief Constructs the ShaderManager.
     * @param pm A pointer to an initialized PluginManager instance.
     */
    ShaderManager(PluginManager* pm);

    /**
     * @brief Destructor.
     */
    ~ShaderManager();
    
    // --- Core Functionality ---
    /**
     * @brief Creates a shader node from a GLSL function name and arguments.
     * @details This is the main factory method. It finds the function, generates
     *          the shader code, compiles it, and returns a manageable ShaderNode.
     * @param function_name The name of the GLSL function to use.
     * @param arguments A vector of strings representing the arguments to the function.
     * @return A shared_ptr to the created ShaderNode. Returns an error node on failure.
     */
    std::shared_ptr<ShaderNode> createShader(
        const std::string& function_name, 
        const std::vector<std::string>& arguments
    );
    
    // --- ID-based Management ---
    /**
     * @brief Creates a shader, assigns it a unique ID, and registers it.
     * @param function_name The name of the GLSL function.
     * @param arguments The arguments for the function.
     * @return A unique shader ID string on success, or an empty string on failure.
     */
    std::string createShaderWithId(const std::string& function_name, 
                                  const std::vector<std::string>& arguments);
    
    /**
     * @brief Retrieves a shader by its unique ID.
     * @param shader_id The ID of the shader to find.
     * @return A shared_ptr to the ShaderNode if found, otherwise nullptr.
     */
    std::shared_ptr<ShaderNode> getShaderById(const std::string& shader_id);
    
    /**
     * @brief Removes a shader from management by its ID.
     * @param shader_id The ID of the shader to remove.
     * @return True if the shader was found and removed, false otherwise.
     */
    bool removeShaderById(const std::string& shader_id);
    
    /**
     * @brief Gets a list of all currently active shader IDs.
     * @return A vector of strings containing all active IDs.
     */
    std::vector<std::string> getAllActiveShaderIds();
    
    // --- GLSL File Handling ---
    /**
     * @brief Loads the content of a GLSL file from a plugin's data directory.
     * @param function_metadata Pointer to the function's metadata, which contains the file path.
     * @param plugin_name The name of the plugin the function belongs to.
     * @return The content of the GLSL file as a string.
     */
    std::string loadGLSLFunction(const GLSLFunction* function_metadata, const std::string& plugin_name);
    
    // --- Shader Code Generation ---
    /**
     * @brief Generates the standard vertex shader code.
     * @return A string containing the vertex shader code.
     */
    std::string generateVertexShader();
    
    /**
     * @brief Generates the final fragment shader code by combining templates, uniforms, and function code.
     * @param glsl_function_code The source code of the GLSL function and its dependencies.
     * @param function_name The name of the root function being called.
     * @param arguments The arguments being passed to the function.
     * @return A string containing the complete fragment shader code.
     */
    std::string generateFragmentShader(
        const std::string& glsl_function_code,
        const std::string& function_name,
        const std::vector<std::string>& arguments
    );
    
    // --- Wrapper Function System ---
    /**
     * @brief Finds the best matching function overload for a given set of user arguments.
     * @param function_metadata Pointer to the function's metadata.
     * @param user_arguments The arguments provided by the user.
     * @return A pointer to the most suitable FunctionOverload, or nullptr if none is found.
     */
    const FunctionOverload* findBestOverload(
        const GLSLFunction* function_metadata,
        const std::vector<std::string>& user_arguments
    );
    
    /**
     * @brief Generates a wrapper function to adapt user arguments to a specific function overload.
     * @param function_name The name of the function.
     * @param user_arguments The arguments provided by the user.
     * @param target_overload The target overload to adapt to.
     * @return A string containing the generated GLSL wrapper function.
     */
    std::string generateWrapperFunction(
        const std::string& function_name,
        const std::vector<std::string>& user_arguments,
        const FunctionOverload* target_overload
    );
    
    // --- Cache Management ---
    /**
     * @brief Retrieves a shader from the cache.
     * @param shader_key The unique key for the shader.
     * @return A shared_ptr to the cached ShaderNode, or nullptr if not found.
     */
    std::shared_ptr<ShaderNode> getCachedShader(const std::string& shader_key);
    
    /**
     * @brief Stores a shader in the cache.
     * @param shader_key The unique key for the shader.
     * @param shader_node A shared_ptr to the ShaderNode to cache.
     */
    void cacheShader(const std::string& shader_key, std::shared_ptr<ShaderNode> shader_node);
    
    /**
     * @brief Clears the entire shader cache.
     */
    void clearCache();
    
    // --- Utilities ---
    /**
     * @brief Resolves the full, absolute path to a GLSL file within a plugin.
     * @param plugin_name The alias of the plugin.
     * @param function_file_path The relative path of the file within the plugin's data directory.
     * @return The absolute path to the GLSL file.
     */
    std::string resolveGLSLFilePath(const std::string& plugin_name, const std::string& function_file_path);
    
    /**
     * @brief Generates a unique cache key from a function name and its arguments.
     * @param function_name The name of the function.
     * @param arguments The vector of arguments.
     * @return A unique string key.
     */
    std::string generateCacheKey(const std::string& function_name, const std::vector<std::string>& arguments);
    
    /**
     * @brief Determines if a string is a floating-point literal.
     * @param str The string to check.
     * @return True if the string is a float literal, false otherwise.
     */
    bool isFloatLiteral(const std::string& str);
    
    /**
     * @brief Checks if a set of arguments can be combined to form a target vector type.
     * @param arguments The user-provided arguments.
     * @param target_type The target GLSL vector type (e.g., "vec2").
     * @return True if the components can be legally combined, false otherwise.
     */
    bool canCombineToVector(const std::vector<std::string>& arguments, const std::string& target_type);
    
    // --- Debugging and Info ---
    /**
     * @brief Prints the current state of the shader cache to the log.
     */
    void printCacheInfo() const;
    
    /**
     * @brief Enables or disables verbose debug logging.
     * @param debug True to enable debug mode, false to disable.
     */
    void setDebugMode(bool debug);
    
private:
    // --- Internal Helper Methods ---
    /**
     * @brief Generates a new unique ID for a shader.
     * @return A unique ID string.
     */
    std::string generateUniqueId();
    
    /**
     * @brief Initializes the default shader templates.
     */
    void initializeShaderTemplates();
    
    /**
     * @brief Generates the uniform declarations for the shader based on arguments.
     * @param arguments The vector of user-provided arguments.
     * @return A string containing the GLSL uniform declarations.
     */
    std::string generateUniforms(const std::vector<std::string>& arguments);
    
    /**
     * @brief Generates the content of the main() function for the fragment shader.
     * @param function_name The name of the function to call.
     * @param arguments The arguments to pass to the function.
     * @return A string containing the GLSL code for the main() function.
     */
    std::string generateMainFunction(const std::string& function_name, const std::vector<std::string>& arguments);
    
    /**
     * @brief A helper to read the entire content of a file into a string.
     * @param file_path The path to the file.
     * @return The content of the file.
     */
    std::string readFileContent(const std::string& file_path);
    
    /**
     * @brief Creates a ShaderNode that is in an error state.
     * @param function_name The name of the function that failed.
     * @param arguments The arguments that were used.
     * @param error_message The error message to record.
     * @return A shared_ptr to the new error-state ShaderNode.
     */
    std::shared_ptr<ShaderNode> createErrorShader(
        const std::string& function_name, 
        const std::vector<std::string>& arguments,
        const std::string& error_message
    );

    /**
     * @brief Checks if a generated wrapper function would have a signature identical to an existing overload.
     * @param function_metadata Pointer to the function's metadata.
     * @param user_arguments The arguments provided by the user.
     * @return True if a duplicate signature is detected, false otherwise.
     */
    bool isSignatureDuplicate(
        const GLSLFunction* function_metadata, 
        const std::vector<std::string>& user_arguments
    );

    /**
     * @brief Calculates a signature string based on user argument types.
     * @param user_arguments The vector of user arguments.
     * @return A comma-separated string of GLSL types.
     */
    std::string calculateUserArgumentSignature(const std::vector<std::string>& user_arguments);

    /**
     * @brief Calculates a signature string from a function overload definition.
     * @param overload The FunctionOverload struct.
     * @return A comma-separated string of GLSL types.
     */
    std::string calculateOverloadSignature(const FunctionOverload& overload);

    /**
     * @brief Determines the GLSL type of a single user-provided argument.
     * @param argument The argument string (e.g., "st.x", "time", "1.0").
     * @return The inferred GLSL type string (e.g., "float", "vec2").
     */
    std::string getArgumentGLSLType(const std::string& argument);

    /**
     * @brief Determines the return type of a function based on the best matching overload for given arguments.
     * @param function_name The name of the function.
     * @param arguments The user-provided arguments.
     * @return The GLSL return type string.
     */
    std::string getFunctionReturnType(
        const std::string& function_name, 
        const std::vector<std::string>& arguments
    );
};
