#include "ofApp.h"
#include "pluginSystem/PluginManager.h"
#include <iostream>

//--------------------------------------------------------------
ofApp::ofApp() {
    // Constructor - managers will be initialized in setup()
}

//--------------------------------------------------------------
ofApp::~ofApp() {
    // Destructor - unique_ptr will automatically clean up
}

//--------------------------------------------------------------
void ofApp::setup(){
    ofDisableArbTex();
    ofSetFrameRate(60);
    ofSetVerticalSync(false);
    ofBackground(0,0,0);

    // Create plugin manager
    plugin_manager = std::make_unique<PluginManager>();
    
    loadAllPlugins();
    displayPluginInfo();
    
    // Initialize shader system
    initializeShaderSystem();

    width = ofGetWidth();
    height = ofGetHeight();
    plane.set(width, height, 4, 4);
    plane.setPosition(width/2, height/2, 0);
    plane.mapTexCoords(0,0,1,1);
}

//--------------------------------------------------------------
void ofApp::update(){
    // Update shader uniforms if we have a current shader
    if (current_shader && current_shader->isReady()) {
        updateShaderUniforms();
    }
}

//--------------------------------------------------------------
void ofApp::draw(){
    ofDrawBitmapString("GLSL Plugin System Demo", 20, 30);
    ofDrawBitmapString("Press keys:", 20, 60);
    ofDrawBitmapString("r - Unload and reload all plugins", 20, 80);
    ofDrawBitmapString("l - display plugin info", 20, 100);
    ofDrawBitmapString("f - list all functions in the first plugin", 20, 120);
    ofDrawBitmapString("t - test shader creation (snoise)", 20, 140);
    ofDrawBitmapString("c - clear current shader", 20, 160);


    int y_offset = 200;
    ofDrawBitmapString("Loaded Plugins:", 20, y_offset);
    y_offset += 20;
    
    for (const auto& plugin_name : loaded_plugin_names) {
        auto it = plugin_functions.find(plugin_name);
        if (it != plugin_functions.end()) {
            std::string info = plugin_name + " (" + ofToString(it->second.size()) + " functions)";
            ofDrawBitmapString("- " + info, 20, y_offset);
            y_offset += 15;
        }
    }

    // Shader status
    y_offset += 40;
    if (current_shader) {
        ofDrawBitmapString("Current Shader:", 20, y_offset);
        y_offset += 15;
        std::string status = "Function: " + current_shader->function_name + " | Status: " + current_shader->getStatusString();
        ofDrawBitmapString("  " + status, 20, y_offset);
        y_offset += 20;
        
        // Render shader if ready
        static bool printed_ready = false;
        if (current_shader->isReady()) {
            if(!printed_ready) {
                std::cout << "Shader is ready to use." << std::endl;
                printed_ready = true;
            }
            current_shader->compiled_shader.begin();
            current_shader->updateAutoUniforms(); // 자동 유니폼 시스템 사용
            plane.draw();
            current_shader->compiled_shader.end();
        }
    } else {
        ofDrawBitmapString("No shader loaded", 20, y_offset);
        y_offset += 20;
    }
    
    ofDrawBitmapString("Press 'r' to reload plugins", 20, y_offset + 20);
    ofDrawBitmapString("Press 'f' to list all functions", 20, y_offset + 35);
}

//--------------------------------------------------------------
void ofApp::exit(){

}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if (!plugin_manager) return;
    
    switch(key) {
        case 'r':
            // 플러그인 새로고침
            plugin_manager->unloadAllPlugins();
            loaded_plugin_names.clear();
            plugin_functions.clear();
            loadAllPlugins();
            displayPluginInfo();
            break;
        case 'l':
            // 로드된 플러그인 목록
            displayPluginInfo();
            break;
        case 'f':
            // 특정 플러그인의 모든 함수 출력
            if (!loaded_plugin_names.empty()) {
                std::string plugin_name = loaded_plugin_names[0]; // 첫 번째 플러그인
                if (plugin_functions.count(plugin_name)) {
                    ofLogNotice("ofApp") << "All functions in " << plugin_name << ":";
                    for (const auto& func : plugin_functions[plugin_name]) {
                        ofLogNotice("ofApp") << "  - " << func;
                    }
                }
            }
            break;
        case 't':
            // 셰이더 생성 테스트
            testShaderCreation();
            break;
        case 'c':
            // 현재 셰이더 클리어
            current_shader.reset();
            ofLogNotice("ofApp") << "Current shader cleared";
            break;
    }

    if(key == 's'){
        const GLSLFunction* function_metadata = plugin_manager->findFunction("LygiaPlugin", "snoise");
        if (function_metadata) {
            ofLogNotice("ofApp") << "Found function: " << function_metadata->name 
                                 << " in plugin: LygiaPlugin"
                                 << " at path: " << function_metadata->filePath;
            for (const auto& overload : function_metadata->overloads) {
                ofLogNotice("ofApp") << "  Overload: " << overload.returnType
                                     << " with params: " << ofJoinString(overload.paramTypes, ", ");
            }
        } else {
            ofLogError("ofApp") << "Function not found in plugins";
        }
    }
}

//--------------------------------------------------------------
std::vector<std::string> ofApp::findPluginFiles() {
    std::vector<std::string> plugin_files;
    
    std::string data_path = ofToDataPath("", true);  // absolute path
    
    std::string plugins_dir = data_path + "/plugins/";
    
    // check directory exists
    ofDirectory dir(plugins_dir);
    if (!dir.exists()) {
        ofLogWarning("ofApp") << "Plugins directory not found: " << plugins_dir;
        return plugin_files;
    }



    // iterate through sub directories
    for (const auto& sub_dir : dir.getFiles()) {
        if (sub_dir.isDirectory()) {
            ofDirectory sub_directory(sub_dir.getAbsolutePath());
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
void ofApp::loadAllPlugins() {
    auto plugin_files = findPluginFiles();
    
    if (plugin_files.empty()) {
        ofLogWarning("ofApp") << "No plugin files found";
        return;
    }
    
    for (const auto& plugin_path : plugin_files) {
        // get only the filename without path
        std::string filename = ofFilePath::getFileName(plugin_path);
        std::string plugin_name = filename;
        
        // remove "lib" prefix
        if (plugin_name.substr(0, 3) == "lib") {
            plugin_name = plugin_name.substr(3);
        }
        
        // remove ".so" suffix
        size_t pos = plugin_name.find(".so");
        if (pos != std::string::npos) {
            plugin_name = plugin_name.substr(0, pos);
        }
        
        // try to load the plugin
        if (plugin_manager->loadPlugin(plugin_path, plugin_name)) {
            loaded_plugin_names.push_back(plugin_name);
            
            // get functions from the plugin
            auto functions = plugin_manager->getAllFunctions();
            plugin_functions[plugin_name] = functions;
            
            ofLogNotice("ofApp") << "Successfully loaded plugin: " << plugin_name 
                                << " with " << functions.size() << " functions";
        } else {
            ofLogError("ofApp") << "Failed to load plugin: " << plugin_path;
        }
    }
}
//--------------------------------------------------------------
void ofApp::displayPluginInfo() {
    ofLogNotice("ofApp") << "=== Loaded Plugins Summary ===";
    ofLogNotice("ofApp") << "Total plugins loaded: " << loaded_plugin_names.size();
    std::map<std::string, std::string> plugin_paths = plugin_manager->getPluginPaths();
    for(auto & [plugin_name, path] : plugin_paths) {
        ofLogNotice("ofApp") << "Plugin: " << plugin_name << " at path: " << path;
    }
    // 기본 정보 출력 (안전한 메서드들만 사용)
    for (const auto& plugin_name : loaded_plugin_names) {
        auto stats = plugin_manager->getPluginStatistics();
        auto it = stats.find(plugin_name);
        if (it != stats.end()) {
            ofLogNotice("ofApp") << "Plugin: " << plugin_name << " - " << it->second << " functions";
        }
        
        // 처음 10개 함수만 예시로 출력
        if (plugin_functions.count(plugin_name)) {
            const auto& functions = plugin_functions[plugin_name];
            ofLogNotice("ofApp") << "Sample functions from " << plugin_name << ":";
            for (int i = 0; i < std::min(10, (int)functions.size()); i++) {
                ofLogNotice("ofApp") << "  - " << functions[i];
            }
            if (functions.size() > 10) {
                ofLogNotice("ofApp") << "  ... and " << (functions.size() - 10) << " more";
            }
        }
    }
}

//--------------------------------------------------------------
void ofApp::initializeShaderSystem() {
    if (!plugin_manager) {
        ofLogError("ofApp") << "Cannot initialize shader system: PluginManager is null";
        return;
    }
    
    // Create shader manager
    shader_manager = std::make_unique<ShaderManager>(plugin_manager.get());
    
    // Setup rendering objects
    plane.set(400, 400);  // 400x400 plane
    plane.setResolution(2, 2);
    
    ofLogNotice("ofApp") << "Shader system initialized";
}

//--------------------------------------------------------------
void ofApp::testShaderCreation() {
    if (!shader_manager) {
        ofLogError("ofApp") << "Shader manager not initialized";
        return;
    }
    
    ofLogNotice("ofApp") << "Testing shader creation with snoise function...";
    
    // Test with snoise function
    std::vector<std::string> arguments = {"st", "time"};
    current_shader = shader_manager->createShader("snoise", arguments);
    
    if (current_shader) {
        if (current_shader->isReady()) {
            ofLogNotice("ofApp") << "Shader created and compiled successfully!";
        } else if (current_shader->has_error) {
            ofLogError("ofApp") << "Shader creation failed: " << current_shader->error_message;
        } else {
            ofLogWarning("ofApp") << "Shader created but not ready yet";
        }
        
        // Print debug info
        current_shader->printDebugInfo();
    } else {
        ofLogError("ofApp") << "Failed to create shader";
    }
}

//--------------------------------------------------------------
void ofApp::updateShaderUniforms() {
    if (!current_shader || !current_shader->isReady()) {
        return;
    }
    
    // Use auto uniform system instead of manual setting
    current_shader->updateAutoUniforms();
}