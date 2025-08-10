#pragma once

#include "ofMain.h"
#include <memory>
#include "geMain.h"

/**
 * @class ofApp
 * @brief The main openFrameworks application class.
 * @details This class sets up the application window and graphics context.
 *          It owns the `graphicsEngine` instance and handles the main
 *          update and draw loops, delegating core logic to the engine.
 */
class ofApp : public ofBaseApp{

public:
    ofApp();
    ~ofApp();
    
    void setup() override;
    void update() override;
    void draw() override;
    void exit() override;
    void keyPressed(int key) override;

    float width, height; ///< The width and height of the application window.
    ofPlanePrimitive plane; ///< A 3D plane used as a canvas for rendering shaders.
    graphicsEngine ge; ///< The main graphics engine instance.

};