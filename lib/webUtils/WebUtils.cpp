#include "WebUtils.hpp"

namespace WebUtils {

    //const char* serverURL;
    //const char* registerEndpoint;
    //const char* readingEndpoint;

    bool Request::send(){
        WiFiClient wifi;
        //client.setInsecure(); // Skip certificate validation for simplicity, consider using proper certificates in production
        HttpClient http(wifi, serverURL, serverPort);
        S_DEBUGF("Sending request to %s:%d with payload: %s\n", serverURL, serverPort, this->jsonPayload.c_str());
        http.post(this->endpoint.c_str(), "application/json", this->jsonPayload.c_str());
        this->handleResponse(http.responseStatusCode(), http.responseBody());
        return true;
    }

    void Request::handleResponse(int statusCode, String responseBody){
        S_DEBUGF("Received response with status code %d and body: %s\n", statusCode, responseBody.c_str());
        //Parse JSON response and check for commands
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, responseBody);
        if (error) {
            S_DEBUGF("Failed to parse JSON response: %s\n", error.c_str());
            return;
        }
        //check if response contains a command for the node, else return
        if (!doc.containsKey("command") || !doc.containsKey("target_node_id")) {
            S_DEBUGF("No command found in response\n");
            return;
        }
        const char* command = doc["command"]; // e.g. "update_next_node"
        uint16_t targetNodeId = doc["target_node_id"];
        
        SendCommandContent_t payload = {
            .targetNodeId = targetNodeId,
            .commandData = 0
        };

        if (strcmp(command, "reset") == 0) {
            // 1. Tell the node handler to fire off a reset command packet to the target node
            commandHandler.handleCommand(COMMAND_SEND_RESET_COMMAND_PACKET, (const uint8_t*)&payload, sizeof(payload));
        } 
        else if (strcmp(command, "update_next_node") == 0) {
            if (!doc.containsKey("next_node_id")) {
                S_DEBUGF("No next_node_id found in update_next_node command\n");
                return;
            }
            // 2. Extract the new next_node_id
            uint16_t nextNodeId = doc["next_node_id"];
            payload.commandData = nextNodeId;
            // 3. Tell the node handler to fire off an update receiver ID command packet
            commandHandler.handleCommand(COMMAND_SEND_UPDATE_RECEIVER_ID_COMMAND_PACKET, (const uint8_t*)&payload, sizeof(payload));
        }
    }

    void Request::queue(){
        WebUtils::requestQueue.push_back(*this);
    }

    Request Request::createRegisterNodeRequest(RegisterNodeInfo_t nodeInfo, ICommandHandler& commandHandler){
        JsonDocument doc;
        doc["node_id"] = nodeInfo.nodeID;
        if (nodeInfo.nextNodeID != 0) doc["next_node_id"] = nodeInfo.nextNodeID;
        else doc["next_node_id"] = nullptr;
        doc["isGateway"] = nodeInfo.isGateway;
        doc["last_seen"] = nodeInfo.registrationTime;
        String jsonPayload;
        serializeJson(doc, jsonPayload);
        return Request(String(registerEndpoint), "POST", jsonPayload, commandHandler);
    }

    Request Request::createSensorDataUploadRequest(SensorReading_t reading, ICommandHandler& commandHandler){
        JsonDocument doc;
        doc["node_id"] = reading.nodeID;
        doc["temperature_c"] = reading.temperatureC;
        doc["humidity_percent"] = reading.humidityPercent;
        doc["pressure_hpa"] = reading.pressurehPa;
        doc["gas_resistance_kohm"] = reading.gasResistanceKOhms;
        doc["battery_level_mv"] = reading.battery_level_mv;
        doc["reading_time"] = reading.readingTime;
        String jsonPayload;
        serializeJson(doc, jsonPayload);
        return Request(String(readingEndpoint), "POST", jsonPayload, commandHandler);
    }

    void processRequestQueue(void* parameters){
        for (;;){
            if (!WebUtils::requestQueue.empty()){
                WebUtils::requestQueue.front().send();
                WebUtils::requestQueue.pop_front();
            }
            vTaskDelay(500 / portTICK_PERIOD_MS); // Check the queue every 500ms
        }
    }

    void startRequestProcessingTask(){
        xTaskCreatePinnedToCore(
            processRequestQueue,   // Function that implements the task.
            "RequestProcessor",   // Text name for the task.
            4096,                // Stack size in words, not bytes.
            nullptr,             // Parameter passed into the task.
            1,                   // Priority at which the task is created.
            &WebUtils::requestProcessingTaskHandle,  // Used to pass out the created task's handle.
            0                    // Core where the task should run
        );
    }
    
}
