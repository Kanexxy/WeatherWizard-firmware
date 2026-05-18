#pragma once
#include <Arduino.h>

#include <WiFiClient.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>

#include <list>

#include "debug_helper.hpp"


namespace WebUtils {

    class Request;

    inline const char* serverURL = "10.0.0.123";
    inline const int serverPort = 3000;
    inline const char* registerEndpoint = "/api/nodes/register";
    inline const char* readingEndpoint = "/api/readings";

    typedef struct {
        uint16_t nodeID;
        uint16_t nextNodeID;
        bool isGateway;
        time_t registrationTime;
    } RegisterNodeInfo_t;

    inline std::list<Request> requestQueue;
    inline TaskHandle_t requestProcessingTaskHandle = nullptr;
    
    void startRequestProcessingTask();
    void processRequestQueue(void* parameters);

    class Request{

        public:
            Request(String endpoint, String method, String jsonPayload) : endpoint(endpoint), method(method), jsonPayload(jsonPayload) {}
            static Request createRegisterNodeRequest(RegisterNodeInfo_t nodeInfo);
            void queue();
            bool send();

        private:
            String endpoint;
            String method;
            String jsonPayload;
    };

}