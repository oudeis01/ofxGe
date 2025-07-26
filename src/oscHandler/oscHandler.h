#pragma once
#include "ofMain.h"
#include "ofxOsc.h"

class OscHandler {
public:
    OscHandler();
    ~OscHandler();

    
    ofxOscReceiver receiver;
    ofxOscSender sender;
};
