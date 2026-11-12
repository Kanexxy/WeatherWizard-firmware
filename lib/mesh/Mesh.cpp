#include "Mesh.hpp"
#include "WiFi.h"

namespace MeshUtils{

    uint16_t nodeID = 0;
    uint16_t nextHopID = 0; //ID of the next hop towards the gateway, 0 if unknown / uninitialized
    uint8_t hopCountToGateway = 255;
    bool hasConnection = false; //true if a connection to the gateway is known
    bool isGateway = false;     //true if this node a the gateway

    AvailableNodes availableNodes;
    
    void setNodeID(){
        //Use last 2 bytes of esp32 MAC as node ID (not ideal), see TODO.txt
        uint8_t mac[6];
        //esp_efuse_mac_get_default(mac); tried this first, behaves the same as esp_read_mac
        esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
        if  (err != 0){
            S_DEBUGF("Error getting MAC address for node ID initialization. ESP32 error code: %d\nHalting...", err);
            while (1);
        }
        nodeID = (mac[4] << 8) | mac[5];
        S_DEBUGF("Set mesh node ID to %.4X based on ESP32 MAC address.\n", nodeID);
    }

    void initMeshVars(bool isGatewayNode){
        setNodeID();
        nextHopID = 0;
        if (isGatewayNode){
            isGateway = true;
            hopCountToGateway = 0;
            hasConnection = true;
        }
        else {
            isGateway = false;
            hopCountToGateway = 255;
            hasConnection = false;
        }
    }

    bool isInitialized(){
        return nodeID != 0;
    }

    void setNextHop(uint16_t nextHop, uint8_t remotehopCount){
        nextHopID = nextHop;
        hopCountToGateway = remotehopCount + 1;
        hasConnection = true;
    }

    bool setNextHopFromAvailableNodes(){
        NodeInRange_t cheapestNode = availableNodes.getCheapestRouteNode();
        if (cheapestNode.nodeId != 0){
            setNextHop(cheapestNode.nodeId, cheapestNode.hopCount);
            TimeUtils::wakeCycleConfig = cheapestNode.wakeCycleConfig;
            S_DEBUGF("Set next hop to node %.4X with hop count %d\n", nextHopID, hopCountToGateway);
            return true;
        } else {
            setNextHop(0, 254);
            hasConnection = false;
            S_DEBUGF("No available nodes to set as next hop.\n");
        }
        return false;
    }
}