#include "ofApp.h"
#include "plugin-system/PluginManager.h"
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
    
    // Load plugins
    if (plugin_manager->loadPlugin("./plugins/libTestShadersPlugin.so", "lygia")) {
        std::cout << "Successfully loaded LYGIA plugin!" << std::endl;
        
        // Print loaded functions
        auto functions = plugin_manager->getAllFunctions();
        std::cout << "Total functions available: " << functions.size() << std::endl;
        
        // Show some examples
        std::cout << "\nExample functions:" << std::endl;
        for (int i = 0; i < std::min(10, (int)functions.size()); i++) {
            std::cout << "- " << functions[i] << std::endl;
        }
        
        // Test function search
        if (const GLSLFunction* func = plugin_manager->findFunction("gaussianBlur")) {
            std::cout << "\nFound gaussianBlur function in: " << func->filePath << std::endl;
            std::cout << "Overloads: " << func->overloads.size() << std::endl;
        }
    } else {
        std::cout << "Failed to load LYGIA plugin" << std::endl;
    }
}

//--------------------------------------------------------------
void ofApp::update(){

}

//--------------------------------------------------------------
void ofApp::draw(){
    // Display some information on screen
    ofDrawBitmapString("GLSL Plugin System Demo", 20, 30);
    ofDrawBitmapString("Press keys:", 20, 60);
    ofDrawBitmapString("1 - Load additional plugin", 20, 80);
    ofDrawBitmapString("2 - Unload LYGIA plugin", 20, 100);
    ofDrawBitmapString("l - List loaded plugins", 20, 120);
    ofDrawBitmapString("f - Find lighting functions", 20, 140);
    ofDrawBitmapString("s - Show statistics", 20, 160);
}

//--------------------------------------------------------------
void ofApp::exit(){

}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if (!plugin_manager) return;
    
    switch(key) {
        case '1':
            // Load another plugin (this would fail since we don't have it)
            plugin_manager->loadPlugin("./plugins/libCustomPlugin.so", "custom");
            break;
        case '2':
            // Unload plugin
            plugin_manager->unloadPlugin("lygia");
            std::cout << "Unloaded LYGIA plugin" << std::endl;
            break;
        case 'l':
            // List loaded plugins
            {
                auto loaded = plugin_manager->getLoadedPlugins();
                std::cout << "\nLoaded plugins:" << std::endl;
                for (const auto& plugin : loaded) {
                    std::cout << "- " << plugin << std::endl;
                }
            }
            break;
        case 'f':
            // Find functions by category
            {
                auto lighting_funcs = plugin_manager->findFunctionsByCategory("lighting");
                std::cout << "\nLighting functions: " << lighting_funcs.size() << std::endl;
                for (int i = 0; i < std::min(5, (int)lighting_funcs.size()); i++) {
                    std::cout << "- " << lighting_funcs[i]->name << std::endl;
                }
            }
            break;
        case 's':
            // Show statistics
            {
                auto stats = plugin_manager->getPluginStatistics();
                std::cout << "\nPlugin statistics:" << std::endl;
                for (const auto& [name, count] : stats) {
                    std::cout << "- " << name << ": " << count << " functions" << std::endl;
                }
            }
            break;
    }
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
