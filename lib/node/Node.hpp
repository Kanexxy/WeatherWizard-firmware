#pragma once
#include "debug_helper.hpp"
#include "SX1262_helper.hpp"
#include "Packets.hpp"
#include "Mesh.hpp"

namespace Node {

    typedef enum {
        NODE_STATE_UNINITIALIZED,
        NODE_STATE_IDLE,
        NODE_STATE_SENSING,
        NODE_STATE_SENDING,
        NODE_STATE_RECEIVING,
        NODE_STATE_SLEEPING
    } nodeState_t;

    extern nodeState_t nodeState;

    void addReceivedDataToRxQueue(LoRaHelper::SX1262_helper &radio);
    void sendPacket(LoRaHelper::SX1262_helper &radio, const Packets::Packet* packet);
    
    bool checkAndHandleRadioIRQ(LoRaHelper::SX1262_helper &radio, Packets::Packet* &currentlySendingPacket);
    bool checkAndHandleIdleState(LoRaHelper::SX1262_helper &radio, Packets::Packet* &currentlySendingPacket);
    bool checkAndHandleReceiveQueue();
}