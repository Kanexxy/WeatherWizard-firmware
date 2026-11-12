#pragma once
#include <list>
#include <time.h>

#include "debug_helper.hpp"
#include "SX1262_helper.hpp"
#include "Packets.hpp"
#include "PacketQueue.hpp"
#include "Mesh.hpp"


namespace Node{

    typedef enum {
        NODE_STATE_INITIALIZING,
        NODE_STATE_IDLE,
        NODE_STATE_SENSING,
        NODE_STATE_SENDING,
        NODE_STATE_RECEIVING,
        NODE_STATE_RECEIVING_COLLISION,
        NODE_STATE_SLEEPING
    } nodeState_t;

    class Node : public ICommandHandler {
        public:
            nodeState_t nodeState;
            Packets::PacketQueue sendQueue;
            Packets::PacketQueue receiveQueue;

            Packets::Packet* currentlySendingPacket = nullptr;

            std::function<void(Packets::Packet*)> masterNodeHandler;

            Node(std::function<void(Packets::Packet*)> handler = nullptr) : masterNodeHandler(handler), nodeState(NODE_STATE_INITIALIZING) {}

            void addReceivedDataToRxQueue(LoRaHelper::SX1262_helper &radio);
            void sendPacket(LoRaHelper::SX1262_helper &radio, Packets::Packet* packet);
            void idle(LoRaHelper::SX1262_helper &radio);
            static void preTransmitCallback(LoRaHelper::SX1262_helper* callingRadio, byte* data, uint8_t length);


            bool handleRadioIRQ(LoRaHelper::SX1262_helper &radio);
            bool sendNextPacketIfNeeded(LoRaHelper::SX1262_helper &radio);
            bool startReceiveIfNeeded(LoRaHelper::SX1262_helper &radio);
            bool handleReceivedPackets();
            void handleCommand(const CommandType_t type, const uint8_t* data, const uint8_t len) override;

            bool tick(LoRaHelper::SX1262_helper &radio);
    };
};