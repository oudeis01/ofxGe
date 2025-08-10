#include "ofMain.h"
#include "ofApp.h"

/**
 * @brief The main entry point of the application.
 * @return The exit code of the application.
 */
int main( ){

	// Setup the OpenGL window and context
	ofGLFWWindowSettings settings;
	settings.glVersionMajor = 4;
	settings.glVersionMinor = 3;
	settings.setSize(1024, 768);
	settings.windowMode = OF_WINDOW;

	auto window = ofCreateWindow(settings);

	// Create an instance of the ofApp class and run it.
	ofRunApp(window, std::make_shared<ofApp>());
	ofRunMainLoop();

    return 0; // Ensure a return value.
}