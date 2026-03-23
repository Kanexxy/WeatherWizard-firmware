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
        S_DEBUGF("Request sent. Status code: %d\nResponse: %s\n", http.responseStatusCode(), http.responseBody().c_str());
        return true;
    }

    void Request::queue(){
        WebUtils::requestQueue.push_back(*this);
    }

    Request Request::createRegisterNodeRequest(RegisterNodeInfo_t nodeInfo){
        JsonDocument doc;
        doc["node_id"] = nodeInfo.nodeID;
        if (nodeInfo.nextNodeID != 0) doc["next_node_id"] = nodeInfo.nextNodeID;
        else doc["next_node_id"] = nullptr;
        doc["isGateway"] = nodeInfo.isGateway;
        doc["last_seen"] = nodeInfo.registrationTime;
        String jsonPayload;
        serializeJson(doc, jsonPayload);
        return Request(String(registerEndpoint), "POST", jsonPayload);
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
