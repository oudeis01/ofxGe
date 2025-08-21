#include "geMain.h"
#include <sstream>

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

//--------------------------------------------------------------
// OSC System Implementation
//--------------------------------------------------------------

void graphicsEngine::initializeOSC(int receive_port) {
    osc_handler = std::make_unique<OscHandler>();
    osc_handler->setup(receive_port);
    
    ofLogNotice("graphicsEngine") << "OSC system initialized on port: " << receive_port;
}

//--------------------------------------------------------------
void graphicsEngine::updateOSC() {
    if (!osc_handler) {
        return;
    }
    
    osc_handler->update();
    
    processCreateMessages();
    processConnectMessages();
    processFreeMessages();
}

//--------------------------------------------------------------
void graphicsEngine::shutdownOSC() {
    if (osc_handler) {
        osc_handler.reset();
        ofLogNotice("graphicsEngine") << "OSC system shut down";
    }
}

//--------------------------------------------------------------
std::string graphicsEngine::createShaderWithId(const std::string& function_name, 
                                              const std::vector<std::string>& arguments) {
    if (!shader_manager) {
        ofLogError("graphicsEngine") << "Shader manager not initialized";
        return "";
    }
    
    std::string shader_id = shader_manager->createShaderWithId(function_name, arguments);
    
    if (!shader_id.empty()) {
        auto shader = shader_manager->getShaderById(shader_id);
        if (shader) {
            active_shaders[shader_id] = shader;
            ofLogNotice("graphicsEngine") << "Created shader with ID: " << shader_id 
                                         << " for function: " << function_name;
        }
    }
    
    return shader_id;
}

//--------------------------------------------------------------
bool graphicsEngine::connectShaderToOutput(const std::string& shader_id) {
    auto it = active_shaders.find(shader_id);
    if (it == active_shaders.end()) {
        ofLogError("graphicsEngine") << "Shader not found with ID: " << shader_id;
        return false;
    }
    
    auto shader = it->second;
    if (!shader || !shader->isReady()) {
        ofLogError("graphicsEngine") << "Shader not ready for connection: " << shader_id;
        return false;
    }
    
    // Connect to output (set as current shader)
    current_shader = shader;
    shader->setConnectedToOutput(true);
    
    ofLogNotice("graphicsEngine") << "Connected shader to output: " << shader_id;
    return true;
}

//--------------------------------------------------------------
bool graphicsEngine::freeShader(const std::string& shader_id) {
    auto it = active_shaders.find(shader_id);
    if (it == active_shaders.end()) {
        ofLogError("graphicsEngine") << "Shader not found with ID: " << shader_id;
        return false;
    }
    
    // Disconnect from output if it's the current shader
    if (current_shader && current_shader == it->second) {
        current_shader.reset();
    }
    
    // Remove from active shaders
    active_shaders.erase(it);
    
    // Remove from shader manager
    bool removed = shader_manager->removeShaderById(shader_id);
    
    ofLogNotice("graphicsEngine") << "Freed shader: " << shader_id 
                                 << " (removed: " << (removed ? "yes" : "no") << ")";
    
    return removed;
}

//--------------------------------------------------------------
void graphicsEngine::processCreateMessages() {
    while (osc_handler->hasCreateMessage()) {
        auto msg = osc_handler->getNextCreateMessage();
        
        if (!msg.is_valid_format) {
            ofLogError("graphicsEngine") << "Invalid create message format: " << msg.format_error;
            osc_handler->sendCreateResponse(false, msg.format_error);
            continue;
        }
        
        ofLogNotice("graphicsEngine") << "Processing OSC /create: " << msg.function_name 
                                     << " with args: " << msg.raw_arguments;
        
        // Parse arguments
        std::vector<std::string> args = parseArguments(msg.raw_arguments);
        
        // Create shader with ID
        std::string shader_id = createShaderWithId(msg.function_name, args);
        
        if (!shader_id.empty()) {
            osc_handler->sendCreateResponse(true, "Shader created successfully", shader_id);
            ofLogNotice("graphicsEngine") << "OSC /create success: shader ID = " << shader_id;
        } else {
            osc_handler->sendCreateResponse(false, "Failed to create shader");
            ofLogError("graphicsEngine") << "OSC /create failed for function: " << msg.function_name;
        }
    }
}

//--------------------------------------------------------------
void graphicsEngine::processConnectMessages() {
    while (osc_handler->hasConnectMessage()) {
        auto msg = osc_handler->getNextConnectMessage();
        
        if (!msg.is_valid_format) {
            ofLogError("graphicsEngine") << "Invalid connect message format: " << msg.format_error;
            osc_handler->sendConnectResponse(false, msg.format_error);
            continue;
        }
        
        ofLogNotice("graphicsEngine") << "Processing OSC /connect: " << msg.shader_id;
        
        bool success = connectShaderToOutput(msg.shader_id);
        
        if (success) {
            osc_handler->sendConnectResponse(true, "Shader connected to output");
            ofLogNotice("graphicsEngine") << "OSC /connect success: " << msg.shader_id;
        } else {
            osc_handler->sendConnectResponse(false, "Failed to connect shader");
            ofLogError("graphicsEngine") << "OSC /connect failed for ID: " << msg.shader_id;
        }
    }
}

//--------------------------------------------------------------
void graphicsEngine::processFreeMessages() {
    while (osc_handler->hasFreeMessage()) {
        auto msg = osc_handler->getNextFreeMessage();
        
        if (!msg.is_valid_format) {
            ofLogError("graphicsEngine") << "Invalid free message format: " << msg.format_error;
            osc_handler->sendFreeResponse(false, msg.format_error);
            continue;
        }
        
        ofLogNotice("graphicsEngine") << "Processing OSC /free: " << msg.shader_id;
        
        bool success = freeShader(msg.shader_id);
        
        if (success) {
            osc_handler->sendFreeResponse(true, "Shader freed successfully");
            ofLogNotice("graphicsEngine") << "OSC /free success: " << msg.shader_id;
        } else {
            osc_handler->sendFreeResponse(false, "Failed to free shader");
            ofLogError("graphicsEngine") << "OSC /free failed for ID: " << msg.shader_id;
        }
    }
}

//--------------------------------------------------------------
std::vector<std::string> graphicsEngine::parseArguments(const std::string& raw_args) {
    std::vector<std::string> args;
    
    if (raw_args.empty()) {
        return args;
    }
    
    std::stringstream ss(raw_args);
    std::string item;
    
    while (std::getline(ss, item, ',')) {
        // Trim whitespace
        size_t start = item.find_first_not_of(" \t");
        size_t end = item.find_last_not_of(" \t");
        
        if (start != std::string::npos) {
            item = item.substr(start, end - start + 1);
            args.push_back(item);
        }
    }
    
    return args;
}
