#include "geMain.h"

//--------------------------------------------------------------
graphicsEngine::graphicsEngine() {
    // Constructor: Initialization of managers is deferred to the setup() phase
    // to ensure all openFrameworks systems are ready.
}

//--------------------------------------------------------------
graphicsEngine::~graphicsEngine() {
    // Destructor: Cleanup is handled automatically by the unique_ptrs,
    // which will delete the managed objects when graphicsEngine is destroyed.
}

//--------------------------------------------------------------
std::vector<std::string> graphicsEngine::findPluginFiles() {
    std::vector<std::string> plugin_files;
    
    // Get the absolute path to the data directory.
    std::string data_path = ofToDataPath("", true);
    std::string plugins_dir = data_path + "/plugins/";
    
    ofDirectory dir(plugins_dir);
    if (!dir.exists()) {
        ofLogWarning("graphicsEngine") << "Plugins directory not found: " << plugins_dir;
        return plugin_files;
    }

    // Iterate through the subdirectories in the plugins folder.
    for (const auto& sub_dir : dir.getFiles()) {
        if (sub_dir.isDirectory()) {
            ofDirectory sub_directory(sub_dir.getAbsolutePath());
            // Filter for .so files (shared objects/dynamic libraries).
            sub_directory.allowExt("so");
            sub_directory.listDir();
            for (size_t i = 0; i < sub_directory.size(); i++) {
                plugin_files.push_back(sub_directory.getPath(i));
            }
        }
    }

    return plugin_files;
}

//--------------------------------------------------------------
void graphicsEngine::loadAllPlugins() {
    auto plugin_files = findPluginFiles();
    
    if (plugin_files.empty()) {
        ofLogWarning("graphicsEngine") << "No plugin files found";
        return;
    }
    
    for (const auto& plugin_path : plugin_files) {
        // Derive a plugin alias from its filename.
        std::string filename = ofFilePath::getFileName(plugin_path);
        std::string plugin_name = filename;
        
        // Remove common prefixes and suffixes (e.g., "lib" and ".so").
        if (plugin_name.rfind("lib", 0) == 0) { // pos=0 limits the search to the prefix
            plugin_name = plugin_name.substr(3);
        }
        
        size_t pos = plugin_name.rfind(".so");
        if (pos != std::string::npos) {
            plugin_name = plugin_name.substr(0, pos);
        }
        
        if (plugin_manager->loadPlugin(plugin_path, plugin_name)) {
            loaded_plugin_names.push_back(plugin_name);
            
            // Store the functions provided by this plugin.
            auto functions = plugin_manager->getFunctionsByPlugin()[plugin_name];
            plugin_functions[plugin_name] = functions;
            
            ofLogNotice("graphicsEngine") << "Successfully loaded plugin: " << plugin_name 
                                << " with " << functions.size() << " functions";
        } else {
            ofLogError("graphicsEngine") << "Failed to load plugin: " << plugin_path;
        }
    }
}

//--------------------------------------------------------------
void graphicsEngine::displayPluginInfo() {
    ofLogNotice("graphicsEngine") << "=== Loaded Plugins Summary ===";
    ofLogNotice("graphicsEngine") << "Total plugins loaded: " << loaded_plugin_names.size();
    
    std::map<std::string, std::string> plugin_paths = plugin_manager->getPluginPaths();
    for(const auto & [plugin_name, path] : plugin_paths) {
        ofLogNotice("graphicsEngine") << "Plugin: " << plugin_name << " at path: " << path;
    }

    for (const auto& plugin_name : loaded_plugin_names) {
        auto stats = plugin_manager->getPluginStatistics();
        auto it = stats.find(plugin_name);
        if (it != stats.end()) {
            ofLogNotice("graphicsEngine") << "  - " << plugin_name << " provides " << it->second << " functions";
        }
    }
}

//--------------------------------------------------------------
void graphicsEngine::initializeShaderSystem() {
    if (!plugin_manager) {
        ofLogError("graphicsEngine") << "Cannot initialize shader system: PluginManager is null";
        return;
    }
    
    shader_manager = std::make_unique<ShaderManager>(plugin_manager.get());
    ofLogNotice("graphicsEngine") << "Shader system initialized";
}

//--------------------------------------------------------------
void graphicsEngine::testShaderCreation(std::string function_name, std::vector<std::string>& args) {
    if (!shader_manager) {
        ofLogError("graphicsEngine") << "Shader manager not initialized";
        return;
    }
    
    ofLogNotice("graphicsEngine") << "Testing shader creation with 'curl' function...";
    
    // Test with valid arguments.
    current_shader = shader_manager->createShader(function_name, args);
    
    if (current_shader) {
        if (current_shader->isReady()) {
            ofLogNotice("graphicsEngine") << "Shader created and compiled successfully!";
        } else if (current_shader->has_error) {
            ofLogError("graphicsEngine") << "Shader creation failed: " << current_shader->error_message;
        } 
        current_shader->printDebugInfo();
    } else {
        ofLogError("graphicsEngine") << "Failed to create shader node.";
    }
}

//--------------------------------------------------------------
void graphicsEngine::updateShaderUniforms() {
    if (current_shader && current_shader->isReady()) {
        current_shader->updateAutoUniforms();
    }
}
