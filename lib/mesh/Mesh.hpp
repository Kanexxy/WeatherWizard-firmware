#pragma once
#include <cstdint>

#include "debug_helper.hpp"
#include "Packets.hpp"
#include "AvailableNodes.hpp"

namespace MeshUtils{

    const uint16_t broadcastID = 0xFFFF;

    extern uint16_t nodeID; //written to EEPROM at programming, read at initialization
    extern uint16_t nextHopID; //ID of the next hop towards the gateway, 0 if unknown / uninitialized
    extern bool hasConnection; //true if a connection to the gateway is known
    extern bool isGateway;

    extern AvailableNodes availableNodes;

    void initializeMeshConnection();
}