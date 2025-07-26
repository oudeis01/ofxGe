#pragma once

#include "ofMain.h"
#include <memory>
#include "pluginSystem/PluginManager.h"

// Forward declaration
class PluginManager;

class ofApp : public ofBaseApp{
private:
    std::unique_ptr<PluginManager> plugin_manager;
    std::vector<std::string> loaded_plugin_names;
    std::map<std::string, std::vector<std::string>> plugin_functions;

public:
    ofApp();
    ~ofApp();
    
    void setup() override;
    void update() override;
    void draw() override;
    void exit() override;
    void keyPressed(int key) override;


private:
    void loadAllPlugins();
    void displayPluginInfo();
    std::vector<std::string> findPluginFiles();
};
