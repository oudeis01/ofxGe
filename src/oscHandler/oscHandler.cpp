#include "oscHandler.h"

//--------------------------------------------------------------
OscHandler::OscHandler() {
}

//--------------------------------------------------------------
OscHandler::~OscHandler() {
}

//--------------------------------------------------------------
void OscHandler::setup(int receive_port) {
    receiver.setup(receive_port);
    // The sender port is for sending responses back to the client.
    sender.setup("localhost", 54321);  // Changed to match oscdump port
    
    ofLogNotice("OscHandler") << "OSC receiver setup on port: " << receive_port;
    ofLogNotice("OscHandler") << "OSC sender setup to localhost:54321";
}

//--------------------------------------------------------------
void OscHandler::update() {
    while (receiver.hasWaitingMessages()) {
        ofxOscMessage osc_message;
        receiver.getNextMessage(osc_message);
        
        const std::string& address = osc_message.getAddress();

        if (address == "/create") {
            create_message_queue.push(parseCreateMessage(osc_message));
        }
        else if (address == "/connect") {
            connect_message_queue.push(parseConnectMessage(osc_message));
        }
        else if (address == "/free") {
            free_message_queue.push(parseFreeMessage(osc_message));
        }
        else {
            ofLogWarning("OscHandler") << "Unknown OSC address: " << address;
        }
    }
}

//--------------------------------------------------------------
bool OscHandler::hasCreateMessage() {
    return !create_message_queue.empty();
}

//--------------------------------------------------------------
bool OscHandler::hasConnectMessage() {
    return !connect_message_queue.empty();
}

//--------------------------------------------------------------
bool OscHandler::hasFreeMessage() {
    return !free_message_queue.empty();
}

//--------------------------------------------------------------
OscCreateMessage OscHandler::getNextCreateMessage() {
    if (create_message_queue.empty()) {
        OscCreateMessage empty_msg;
        empty_msg.is_valid_format = false;
        empty_msg.format_error = "No messages in queue";
        return empty_msg;
    }
    
    OscCreateMessage message = create_message_queue.front();
    create_message_queue.pop();
    return message;
}

//--------------------------------------------------------------
OscConnectMessage OscHandler::getNextConnectMessage() {
    if (connect_message_queue.empty()) {
        OscConnectMessage empty_msg;
        empty_msg.is_valid_format = false;
        empty_msg.format_error = "No messages in queue";
        return empty_msg;
    }
    
    OscConnectMessage message = connect_message_queue.front();
    connect_message_queue.pop();
    return message;
}

//--------------------------------------------------------------
OscFreeMessage OscHandler::getNextFreeMessage() {
    if (free_message_queue.empty()) {
        OscFreeMessage empty_msg;
        empty_msg.is_valid_format = false;
        empty_msg.format_error = "No messages in queue";
        return empty_msg;
    }
    
    OscFreeMessage message = free_message_queue.front();
    free_message_queue.pop();
    return message;
}

//--------------------------------------------------------------
void OscHandler::sendCreateResponse(bool success, const std::string& message, const std::string& shader_id) {
    ofxOscMessage response;
    response.setAddress("/create/response");
    response.addStringArg(success ? "success" : "error");
    response.addStringArg(message);
    if (!shader_id.empty()) {
        response.addStringArg(shader_id);
    }
    sender.sendMessage(response);
    
    ofLogNotice("OscHandler") << "Sent create response: " << (success ? "success" : "error") 
                              << " - " << message 
                              << (shader_id.empty() ? "" : " [ID: " + shader_id + "]");
}

//--------------------------------------------------------------
void OscHandler::sendConnectResponse(bool success, const std::string& message) {
    ofxOscMessage response;
    response.setAddress("/connect/response");
    response.addStringArg(success ? "success" : "error");
    response.addStringArg(message);
    sender.sendMessage(response);
    
    ofLogNotice("OscHandler") << "Sent connect response: " << (success ? "success" : "error") 
                              << " - " << message;
}

//--------------------------------------------------------------
void OscHandler::sendFreeResponse(bool success, const std::string& message) {
    ofxOscMessage response;
    response.setAddress("/free/response");
    response.addStringArg(success ? "success" : "error");
    response.addStringArg(message);
    sender.sendMessage(response);
    
    ofLogNotice("OscHandler") << "Sent free response: " << (success ? "success" : "error") 
                              << " - " << message;
}

//--------------------------------------------------------------
OscCreateMessage OscHandler::parseCreateMessage(const ofxOscMessage& osc_message) {
    OscCreateMessage result;
    result.is_valid_format = false;
    
    // Expected format: /create [string:function_name] [string:arguments]
    if (osc_message.getNumArgs() != 2) {
        result.format_error = "Expected 2 arguments (function_name, arguments)";
        return result;
    }
    
    if (osc_message.getArgType(0) != OFXOSC_TYPE_STRING ||
        osc_message.getArgType(1) != OFXOSC_TYPE_STRING) {
        result.format_error = "All arguments must be strings";
        return result;
    }
    
    result.function_name = osc_message.getArgAsString(0);
    result.raw_arguments = osc_message.getArgAsString(1);
    result.is_valid_format = true;
    
    ofLogNotice("OscHandler") << "Parsed /create message: " << result.function_name 
                              << " with args: " << result.raw_arguments;
    
    return result;
}

//--------------------------------------------------------------
OscConnectMessage OscHandler::parseConnectMessage(const ofxOscMessage& osc_message) {
    OscConnectMessage result;
    result.is_valid_format = false;
    
    // Expected format: /connect [string:shader_id]
    if (osc_message.getNumArgs() != 1) {
        result.format_error = "Expected 1 argument (shader_id)";
        return result;
    }
    
    if (osc_message.getArgType(0) != OFXOSC_TYPE_STRING) {
        result.format_error = "Shader ID must be a string";
        return result;
    }
    
    result.shader_id = osc_message.getArgAsString(0);
    result.is_valid_format = true;
    
    ofLogNotice("OscHandler") << "Parsed /connect message: shader_id = " << result.shader_id;
    
    return result;
}

//--------------------------------------------------------------
OscFreeMessage OscHandler::parseFreeMessage(const ofxOscMessage& osc_message) {
    OscFreeMessage result;
    result.is_valid_format = false;
    
    // Expected format: /free [string:shader_id]
    if (osc_message.getNumArgs() != 1) {
        result.format_error = "Expected 1 argument (shader_id)";
        return result;
    }
    
    if (osc_message.getArgType(0) != OFXOSC_TYPE_STRING) {
        result.format_error = "Shader ID must be a string";
        return result;
    }
    
    result.shader_id = osc_message.getArgAsString(0);
    result.is_valid_format = true;
    
    ofLogNotice("OscHandler") << "Parsed /free message: shader_id = " << result.shader_id;
    
    return result;
}