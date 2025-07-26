#include "ofApp.h"
#include "pluginSystem/PluginManager.h"
#include <iostream>

//--------------------------------------------------------------
ofApp::ofApp() {
    // Constructor - plugin_manager will be initialized in setup()
}

//--------------------------------------------------------------
ofApp::~ofApp() {
    // Destructor - unique_ptr will automatically clean up
}

//--------------------------------------------------------------
void ofApp::setup(){
    // Create plugin manager
    plugin_manager = std::make_unique<PluginManager>();
    
    loadAllPlugins();

    displayPluginInfo();
}

//--------------------------------------------------------------
void ofApp::update(){
    ofShader s;
    s.load("shaders/noise");
}

//--------------------------------------------------------------
void ofApp::draw(){
    ofDrawBitmapString("GLSL Plugin System Demo", 20, 30);
    ofDrawBitmapString("Press keys:", 20, 60);
    ofDrawBitmapString("r - Unload and reload all plugins", 20, 80);
    ofDrawBitmapString("l - display plugin info", 20, 100);
    ofDrawBitmapString("f - list all functions in the first plugin", 20, 120);


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
            for (int i = 0; i < sub_directory.size(); i++) {
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