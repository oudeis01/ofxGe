#pragma once

#include "ofMain.h"
#include <memory>
#include "pluginSystem/PluginManager.h"
#include "shaderSystem/ShaderManager.h"
#include "shaderSystem/ShaderNode.h"

// Forward declarations
class PluginManager;
class ShaderManager;

class ofApp : public ofBaseApp{
private:
    // Plugin system
    std::unique_ptr<PluginManager> plugin_manager;
    std::vector<std::string> loaded_plugin_names;
    std::map<std::string, std::vector<std::string>> plugin_functions;
    
    // Shader system
    std::unique_ptr<ShaderManager> shader_manager;
    std::shared_ptr<ShaderNode> current_shader;
    
    // Rendering objects
    ofPlanePrimitive plane;

public:
    ofApp();
    ~ofApp();
    
    void setup() override;
    void update() override;
    void draw() override;
    void exit() override;
    void keyPressed(int key) override;

    float width, height;



private:
    // Plugin system methods
    void loadAllPlugins();
    void displayPluginInfo();
    std::vector<std::string> findPluginFiles();
    
    // Shader system methods
    void initializeShaderSystem();
    void testShaderCreation();
    void updateShaderUniforms();
};
