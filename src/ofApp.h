#pragma once

#include "ofMain.h"
#include <memory>
#include "plugin-system/PluginManager.h"

// Forward declaration
class PluginManager;

class ofApp : public ofBaseApp{
private:
    std::unique_ptr<PluginManager> plugin_manager;

public:
    ofApp();
    ~ofApp();
    
    void setup() override;
    void update() override;
    void draw() override;
    void exit() override;

    void keyPressed(int key) override;
    void keyReleased(int key) override;
    void mouseMoved(int x, int y) override;
    void mouseDragged(int x, int y, int button) override;
    void mousePressed(int x, int y, int button) override;
    void mouseReleased(int x, int y, int button) override;
    void mouseEntered(int x, int y) override;
    void mouseExited(int x, int y) override;
    void windowResized(int w, int h) override;
    void gotMessage(ofMessage msg) override;
    void dragEvent(ofDragInfo dragInfo) override;
};
