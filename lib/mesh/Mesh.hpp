#pragma once
#include <cstdint>

#include "TimeUtils.hpp"
#include "debug_helper.hpp"
#include "AvailableNodes.hpp"

namespace MeshUtils{

    const uint16_t broadcastID = 0xFFFF;

    extern uint16_t nodeID; //written to EEPROM at programming, read at initialization
    extern uint16_t nextHopID; //ID of the next hop towards the gateway, 0 if unknown / uninitialized
    extern uint8_t hopCountToGateway; //number of hops to the gateway, 0 if gateway, 255 if unknown / uninitialized
    extern bool hasConnection; //true if a connection to the gateway is known
    extern bool isGateway;

    extern AvailableNodes availableNodes;

    void initMeshVars(bool isGatewayNode);
    void setNodeID();
    void setNextHop(uint16_t nextHop, uint8_t remotehopCount);
    bool setNextHopFromAvailableNodes();
    bool isInitialized();
}