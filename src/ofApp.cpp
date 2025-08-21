#include "ofApp.h"
#include "pluginSystem/PluginManager.h"
#include "shaderSystem/ExpressionParser.h"
#include <iostream>
#include <muParser.h>

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
    
    // --- Initialize OSC System ---
    ge.initializeOSC(12345);  // Listen on port 12345

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
    ge.updateOSC();  // Process OSC messages
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
    ofDrawBitmapString("m - Test muParser expressions", 20, 160);
    ofDrawBitmapString("e - Test ExpressionParser", 20, 180);
    ofDrawBitmapString("", 20, 200);
    ofDrawBitmapString("OSC Commands (port 12345):", 20, 220);
    ofDrawBitmapString("/create [function] [args] - Create shader with ID", 20, 240);
    ofDrawBitmapString("/connect [shader_id] - Connect shader to output", 20, 260);
    ofDrawBitmapString("/free [shader_id] - Free shader memory", 20, 280);

    int y_offset = 320;
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
    ge.shutdownOSC();  // Clean shutdown of OSC system
    // Other resources are released automatically by destructors.
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if (!ge.plugin_manager) return;

    switch(key) {
        case 'r':{
            // Reload all plugins
            ge.plugin_manager->unloadAllPlugins();
            ge.loaded_plugin_names.clear();
            ge.plugin_functions.clear();
            ge.loadAllPlugins();
            ge.displayPluginInfo();
            break;
        }
        case 'l':{
            // Display loaded plugin info
            ge.displayPluginInfo();
            break;
        }
        case 't':{
            // Test shader creation
            ge.shader_manager->setDebugMode(true);
            std::vector<std::string> args = {
                "st.x*mix(0.1,10.0,(sin(time*0.4)+1.0)*0.5)",
                "st.y*10.0*sin(time*.5+1000)",
                "cos(time*0.5)"
            };
            ge.testShaderCreation("rgb2srgb", args);
            break;
        }
        case 'c':{
            // Clear the current shader
            ge.current_shader.reset();
            ofLogNotice("ofApp") << "Current shader cleared";
            break;
        }
        case 'm':{
            // Test muParser
            testMuParser();
            break;
        }
        case 'e':{
            // Test ExpressionParser
            testExpressionParser();
            break;
        }
    }
}

//--------------------------------------------------------------
void ofApp::testMuParser() {
    ofLogNotice("ofApp") << "=== muParser Test ===";
    
    try {
        mu::Parser parser;
        
        // Test 1: Simple arithmetic
        parser.SetExpr("2 + 3 * 4");
        double result1 = parser.Eval();
        ofLogNotice("ofApp") << "Test 1: 2 + 3 * 4 = " << result1;
        
        // Test 2: Math functions
        parser.SetExpr("sin(3.14159/2)");
        double result2 = parser.Eval();
        ofLogNotice("ofApp") << "Test 2: sin(π/2) = " << result2;
        
        // Test 3: Variables
        double time_val = 1.0;
        parser.DefineVar("time", &time_val);
        parser.SetExpr("sin(time * 10.0)");
        double result3 = parser.Eval();
        ofLogNotice("ofApp") << "Test 3: sin(time * 10) with time=1.0 = " << result3;
        
        // Test 4: Complex expression with float literal
        parser.SetExpr("sin(time * 10.0) + cos(time * 5.0)");
        double result4 = parser.Eval();
        ofLogNotice("ofApp") << "Test 4: sin(time*10) + cos(time*5) = " << result4;
        
        // Test 5: Extract variables from expression
        std::string expr = "sin(time * 10.0 + phase) + amplitude";
        parser.SetExpr(expr);
        
        mu::varmap_type variables = parser.GetUsedVar();
        ofLogNotice("ofApp") << "Test 5: Variables in '" << expr << "':";
        for (auto& var : variables) {
            ofLogNotice("ofApp") << "  - " << var.first;
        }
        
    } catch (mu::Parser::exception_type &e) {
        ofLogError("ofApp") << "muParser Error: " << e.GetMsg();
    }
    
    ofLogNotice("ofApp") << "=== muParser Test Complete ===";
}

//--------------------------------------------------------------
void ofApp::testExpressionParser() {
    ofLogNotice("ofApp") << "=== ExpressionParser Test ===";
    
    ExpressionParser parser;
    
    // Test cases for the new system - focusing on the problematic case
    std::vector<std::string> test_expressions = {
        "time",
        "0.1",
        "time*0.1",
        "sin(time*0.1)",
        "sin(time*10.0)"
    };
    
    for (const auto& expr : test_expressions) {
        ofLogNotice("ofApp") << "Testing: " << expr;
        
        ExpressionInfo info = parser.parseExpression(expr);
        
        ofLogNotice("ofApp") << "  Original: '" << info.original << "'";
        ofLogNotice("ofApp") << "  GLSL: '" << info.glsl_code << "'";
        ofLogNotice("ofApp") << "  Type: " << info.type;
        ofLogNotice("ofApp") << "  Is simple var: " << (info.is_simple_var ? "true" : "false");
        ofLogNotice("ofApp") << "  Is constant: " << (info.is_constant ? "true" : "false");
        
        if (info.is_constant) {
            ofLogNotice("ofApp") << "  Constant value: " << info.constant_value;
        }
        
        if (!info.dependencies.empty()) {
            std::string deps = "";
            for (size_t i = 0; i < info.dependencies.size(); i++) {
                if (i > 0) deps += ", ";
                deps += info.dependencies[i];
            }
            ofLogNotice("ofApp") << "  Dependencies: " << deps;
        }
        
        ofLogNotice("ofApp") << "---";
    }
    
    ofLogNotice("ofApp") << "=== ExpressionParser Test Complete ===";
}