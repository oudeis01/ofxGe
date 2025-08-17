#include "GlobalOutputNode.h"
#include "ofMain.h"

//--------------------------------------------------------------
GlobalOutputNode::GlobalOutputNode() 
    : current_state(GlobalOutputState::IDLE)
    , connected_shader(nullptr)
    , connected_shader_id("")
    , default_background_color(ofColor(20, 20, 20))
    , debug_mode(false)
    , total_connections(0)
    , total_renders(0) {
    
    ofLogNotice("GlobalOutputNode") << "GlobalOutputNode initialized";
}

//--------------------------------------------------------------
GlobalOutputNode::~GlobalOutputNode() {
    if (connected_shader) {
        disconnectShader();
    }
    ofLogNotice("GlobalOutputNode") << "GlobalOutputNode destroyed";
}

// ================================================================================
// CONNECTION MANAGEMENT
// ================================================================================

bool GlobalOutputNode::connectShader(const std::string& shader_id, std::shared_ptr<ShaderNode> shader_node) {
    if (!shader_node) {
        ofLogError("GlobalOutputNode") << "Cannot connect null shader node";
        return false;
    }
    
    if (!shader_node->isReady()) {
        ofLogError("GlobalOutputNode") << "Cannot connect shader '" << shader_id 
                                       << "' - shader is not ready (compilation failed?)";
        return false;
    }
    
    // Disconnect current shader if any
    if (connected_shader) {
        ofLogNotice("GlobalOutputNode") << "Disconnecting current shader '" 
                                        << connected_shader_id << "' to connect '" << shader_id << "'";
        disconnectShader();
    }
    
    // Connect new shader
    connected_shader = shader_node;
    connected_shader_id = shader_id;
    connection_timestamp = getCurrentTimestamp();
    total_connections++;
    
    updateState();
    
    ofLogNotice("GlobalOutputNode") << "Connected shader '" << shader_id 
                                   << "' to global output (connection #" << total_connections << ")";
    
    return true;
}

bool GlobalOutputNode::disconnectShader() {
    if (!connected_shader) {
        ofLogWarning("GlobalOutputNode") << "No shader to disconnect";
        return false;
    }
    
    std::string prev_shader_id = connected_shader_id;
    
    connected_shader.reset();
    connected_shader_id = "";
    connection_timestamp = "";
    
    updateState();
    
    ofLogNotice("GlobalOutputNode") << "Disconnected shader '" << prev_shader_id << "' from global output";
    
    return true;
}

bool GlobalOutputNode::hasConnectedShader() const {
    return connected_shader != nullptr && connected_shader->isReady();
}

std::string GlobalOutputNode::getConnectedShaderId() const {
    return connected_shader_id;
}

std::shared_ptr<ShaderNode> GlobalOutputNode::getConnectedShader() const {
    return connected_shader;
}

// ================================================================================
// RENDERING SYSTEM
// ================================================================================

void GlobalOutputNode::render(ofPlanePrimitive& plane) {
    total_renders++;
    
    if (hasConnectedShader()) {
        // Render connected shader
        connected_shader->compiled_shader.begin();
        connected_shader->updateAutoUniforms();
        plane.draw();
        connected_shader->compiled_shader.end();
        
        if (debug_mode) {
            renderDebugInfo();
        }
    } else {
        // Render default output
        renderDefault(plane);
    }
}

void GlobalOutputNode::updateUniforms() {
    if (hasConnectedShader()) {
        connected_shader->updateAutoUniforms();
    }
}

// ================================================================================
// STATE MANAGEMENT
// ================================================================================

GlobalOutputState GlobalOutputNode::getState() const {
    return current_state;
}

std::string GlobalOutputNode::getStatusString() const {
    switch (current_state) {
        case GlobalOutputState::IDLE:
            return "IDLE (no shader connected)";
        case GlobalOutputState::CONNECTED:
            return "CONNECTED (" + connected_shader_id + ")";
        case GlobalOutputState::TRANSITIONING:
            return "TRANSITIONING";
        default:
            return "UNKNOWN";
    }
}

std::string GlobalOutputNode::getDetailedStatus() const {
    std::stringstream status;
    
    status << "=== Global Output Node Status ===\n";
    status << "State: " << getStatusString() << "\n";
    status << "Total Connections: " << total_connections << "\n";
    status << "Total Renders: " << total_renders << "\n";
    
    if (hasConnectedShader()) {
        status << "Connected Shader: " << connected_shader_id << "\n";
        status << "Connection Time: " << connection_timestamp << "\n";
        status << "Shader Function: " << connected_shader->function_name << "\n";
        status << "Shader Status: " << connected_shader->getStatusString() << "\n";
        
        // Show arguments
        status << "Arguments: ";
        for (size_t i = 0; i < connected_shader->arguments.size(); i++) {
            if (i > 0) status << ", ";
            status << "\"" << connected_shader->arguments[i] << "\"";
        }
        status << "\n";
    } else {
        status << "No shader connected\n";
        status << "Background Color: (" << default_background_color.r << ", " 
               << default_background_color.g << ", " << default_background_color.b << ")\n";
    }
    
    return status.str();
}

// ================================================================================
// FALLBACK RENDERING
// ================================================================================

void GlobalOutputNode::setDefaultBackgroundColor(const ofColor& color) {
    default_background_color = color;
    ofLogNotice("GlobalOutputNode") << "Default background color set to (" 
                                   << color.r << ", " << color.g << ", " << color.b << ")";
}

void GlobalOutputNode::setDebugMode(bool enabled) {
    debug_mode = enabled;
    ofLogNotice("GlobalOutputNode") << "Debug mode " << (enabled ? "enabled" : "disabled");
}

// ================================================================================
// INTERNAL METHODS
// ================================================================================

void GlobalOutputNode::updateState() {
    if (hasConnectedShader()) {
        current_state = GlobalOutputState::CONNECTED;
    } else {
        current_state = GlobalOutputState::IDLE;
    }
}

void GlobalOutputNode::renderDefault(ofPlanePrimitive& plane) {
    // Render a simple background color or pattern when no shader is connected
    ofPushStyle();
    
    ofSetColor(default_background_color);
    ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());
    
    // Optional: Draw a subtle pattern or message
    ofSetColor(default_background_color.r + 40, default_background_color.g + 40, default_background_color.b + 40);
    
    // Draw a simple grid pattern
    int grid_size = 50;
    for (int x = 0; x < ofGetWidth(); x += grid_size) {
        ofDrawLine(x, 0, x, ofGetHeight());
    }
    for (int y = 0; y < ofGetHeight(); y += grid_size) {
        ofDrawLine(0, y, ofGetWidth(), y);
    }
    
    // Draw status text
    ofSetColor(100, 100, 100);
    std::string message = "Global Output Node - IDLE\nNo shader connected\nUse /connect to connect a shader";
    ofDrawBitmapString(message, 20, ofGetHeight() - 80);
    
    ofPopStyle();
}

void GlobalOutputNode::renderDebugInfo() {
    ofPushStyle();
    
    // Semi-transparent background for debug info
    ofSetColor(0, 0, 0, 180);
    ofDrawRectangle(ofGetWidth() - 300, 10, 290, 120);
    
    // Debug text
    ofSetColor(255, 255, 255);
    std::stringstream debug_info;
    debug_info << "=== Global Output Debug ===\n";
    debug_info << "State: " << getStatusString() << "\n";
    debug_info << "Shader: " << connected_shader_id << "\n";
    debug_info << "Function: " << connected_shader->function_name << "\n";
    debug_info << "Renders: " << total_renders << "\n";
    debug_info << "Connections: " << total_connections;
    
    ofDrawBitmapString(debug_info.str(), ofGetWidth() - 290, 30);
    
    ofPopStyle();
}

std::string GlobalOutputNode::getCurrentTimestamp() const {
    time_t now = time(0);
    char* dt = ctime(&now);
    std::string timestamp(dt);
    // Remove newline at end
    if (!timestamp.empty() && timestamp.back() == '\n') {
        timestamp.pop_back();
    }
    return timestamp;
}