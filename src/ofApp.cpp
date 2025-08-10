#include "ofApp.h"
#include "pluginSystem/PluginManager.h"
#include <iostream>

//--------------------------------------------------------------
ofApp::ofApp() {

}

//--------------------------------------------------------------
ofApp::~ofApp() {

}

//--------------------------------------------------------------
void ofApp::setup(){
    ofDisableArbTex(); // Use modern, non-arbitrary texture dimensions.
    ofSetFrameRate(60);
    ofSetVerticalSync(false);
    ofBackground(0,0,0);

    // --- Initialize Core Systems ---
    ge.plugin_manager = std::make_unique<PluginManager>();
    ge.loadAllPlugins();
    ge.displayPluginInfo();
    ge.initializeShaderSystem();

    // --- Setup Rendering Canvas ---
    width = ofGetWidth();
    height = ofGetHeight();
    plane.set(width, height, 4, 4);
    plane.setPosition(width/2, height/2, 0);
    plane.mapTexCoords(0,0,1,1);
}

//--------------------------------------------------------------
void ofApp::update(){
    ge.updateShaderUniforms();
}

//--------------------------------------------------------------
void ofApp::draw(){
    // --- Draw UI and Help Text ---
    ofDrawBitmapString("GLSL Plugin System Demo", 20, 30);
    ofDrawBitmapString("Press keys:", 20, 60);
    ofDrawBitmapString("r - Unload and reload all plugins", 20, 80);
    ofDrawBitmapString("l - Display plugin info", 20, 100);
    ofDrawBitmapString("t - Test shader creation (curl)", 20, 120);
    ofDrawBitmapString("c - Clear current shader", 20, 140);

    int y_offset = 180;
    ofDrawBitmapString("Loaded Plugins:", 20, y_offset);
    y_offset += 20;

    for (const auto& plugin_name : ge.loaded_plugin_names) {
        auto it = ge.plugin_functions.find(plugin_name);
        if (it != ge.plugin_functions.end()) {
            std::string info = plugin_name + " (" + ofToString(it->second.size()) + " functions)";
            ofDrawBitmapString("- " + info, 20, y_offset);
            y_offset += 15;
        }
    }

    // --- Draw Shader Status and Output ---
    y_offset += 20;
    if (ge.current_shader) {
        ofDrawBitmapString("Current Shader:", 20, y_offset);
        y_offset += 15;
        std::string status = "Function: " + ge.current_shader->function_name + " | Status: " + ge.current_shader->getStatusString();
        ofDrawBitmapString("  " + status, 20, y_offset);
        
        // If the shader is compiled and ready, use it to draw the plane.
        if (ge.current_shader->isReady()) {
            ge.current_shader->compiled_shader.begin();
            ge.current_shader->updateAutoUniforms();
            plane.draw();
            ge.current_shader->compiled_shader.end();
        }
    } else {
        ofDrawBitmapString("No shader loaded", 20, y_offset);
    }
}

//--------------------------------------------------------------
void ofApp::exit(){
    // This is called when the app is about to close.
    // Resources are released automatically by destructors.
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if (!ge.plugin_manager) return;

    switch(key) {
        case 'r':
            // Reload all plugins
            ge.plugin_manager->unloadAllPlugins();
            ge.loaded_plugin_names.clear();
            ge.plugin_functions.clear();
            ge.loadAllPlugins();
            ge.displayPluginInfo();
            break;
        case 'l':
            // Display loaded plugin info
            ge.displayPluginInfo();
            break;
        case 't':
            // Test shader creation
            ge.shader_manager->setDebugMode(true);
            ge.testShaderCreation();
            break;
        case 'c':
            // Clear the current shader
            ge.current_shader.reset();
            ofLogNotice("ofApp") << "Current shader cleared";
            break;
    }
}