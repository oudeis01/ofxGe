#pragma once
#include "ofMain.h"
#include "ofxOsc.h"
#include <queue>

/**
 * @struct OscCreateMessage
 * @brief  Holds the parsed data from a "/create" OSC message.
 */
struct OscCreateMessage {
    std::string function_name;      ///< The name of the GLSL function to create a shader from.
    std::string raw_arguments;      ///< The raw, comma-separated string of arguments for the function.
    bool is_valid_format;           ///< True if the OSC message was parsed successfully.
    std::string format_error;       ///< An error message if parsing failed.
};

/**
 * @struct OscConnectMessage
 * @brief  Holds the parsed data from a "/connect" OSC message.
 */
struct OscConnectMessage {
    std::string shader_id;          ///< The unique ID of the shader to connect.
    bool is_valid_format;           ///< True if the OSC message was parsed successfully.
    std::string format_error;       ///< An error message if parsing failed.
};

/**
 * @struct OscFreeMessage
 * @brief  Holds the parsed data from a "/free" OSC message.
 */
struct OscFreeMessage {
    std::string shader_id;          ///< The unique ID of the shader to free.
    bool is_valid_format;           ///< True if the OSC message was parsed successfully.
    std::string format_error;       ///< An error message if parsing failed.
};

/**
 * @class OscHandler
 * @brief Manages receiving, parsing, and sending OSC messages.
 * @details This class listens for incoming OSC messages on a specified port,
 *          parses them into structured data, and places them into message queues.
 *          It also provides methods to send OSC responses.
 */
class OscHandler {
public:
    OscHandler();
    ~OscHandler();

    /**
     * @brief Sets up the OSC receiver and sender.
     * @param receive_port The port number to listen for incoming OSC messages.
     */
    void setup(int receive_port = 12345);

    /**
     * @brief Checks for and processes any waiting OSC messages.
     * @details This should be called once per frame in the main update loop.
     */
    void update();
    
    // --- Message Queue Management ---
    /**
     * @brief Checks if there is a new "/create" message in the queue.
     * @return True if a message is available, false otherwise.
     */
    bool hasCreateMessage();

    /**
     * @brief Checks if there is a new "/connect" message in the queue.
     * @return True if a message is available, false otherwise.
     */
    bool hasConnectMessage();

    /**
     * @brief Checks if there is a new "/free" message in the queue.
     * @return True if a message is available, false otherwise.
     */
    bool hasFreeMessage();
    
    /**
     * @brief Retrieves the next "/create" message from the queue.
     * @return The parsed OscCreateMessage. Check is_valid_format before use.
     */
    OscCreateMessage getNextCreateMessage();

    /**
     * @brief Retrieves the next "/connect" message from the queue.
     * @return The parsed OscConnectMessage. Check is_valid_format before use.
     */
    OscConnectMessage getNextConnectMessage();

    /**
     * @brief Retrieves the next "/free" message from the queue.
     * @return The parsed OscFreeMessage. Check is_valid_format before use.
     */
    OscFreeMessage getNextFreeMessage();
    
    // --- Response Sending ---
    /**
     * @brief Sends a response to a "/create" message.
     * @param success True if the operation was successful, false otherwise.
     * @param message A descriptive message about the result.
     * @param shader_id The unique ID of the created shader, if successful.
     */
    void sendCreateResponse(bool success, const std::string& message, const std::string& shader_id = "");

    /**
     * @brief Sends a response to a "/connect" message.
     * @param success True if the operation was successful, false otherwise.
     * @param message A descriptive message about the result.
     */
    void sendConnectResponse(bool success, const std::string& message);

    /**
     * @brief Sends a response to a "/free" message.
     * @param success True if the operation was successful, false otherwise.
     * @param message A descriptive message about the result.
     */
    void sendFreeResponse(bool success, const std::string& message);
    
private:
    ofxOscReceiver receiver; ///< The object that receives OSC messages.
    ofxOscSender sender;     ///< The object that sends OSC messages.
    
    // --- Message Queues ---
    std::queue<OscCreateMessage> create_message_queue;   ///< Queue for parsed "/create" messages.
    std::queue<OscConnectMessage> connect_message_queue; ///< Queue for parsed "/connect" messages.
    std::queue<OscFreeMessage> free_message_queue;       ///< Queue for parsed "/free" messages.
    
    // --- Parsing Functions ---
    /**
     * @brief Parses an incoming OSC message with the address "/create".
     * @param osc_message The raw OSC message.
     * @return An OscCreateMessage struct with the parsed data.
     */
    OscCreateMessage parseCreateMessage(const ofxOscMessage& osc_message);

    /**
     * @brief Parses an incoming OSC message with the address "/connect".
     * @param osc_message The raw OSC message.
     * @return An OscConnectMessage struct with the parsed data.
     */
    OscConnectMessage parseConnectMessage(const ofxOscMessage& osc_message);

    /**
     * @brief Parses an incoming OSC message with the address "/free".
     * @param osc_message The raw OSC message.
     * @return An OscFreeMessage struct with the parsed data.
     */
    OscFreeMessage parseFreeMessage(const ofxOscMessage& osc_message);
};