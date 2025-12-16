#pragma once
#include <cstdint>

#include "debug_helper.hpp"
#include "Mesh.hpp"
#include "PacketQueue.hpp"


namespace Packets {

    class PacketQueue;

    typedef enum {
        PACKET_TYPE_SENSORDATA,
        PACKET_TYPE_ACK,
        PACKET_TYPE_COMMAND,
        PACKET_TYPE_DISCOVER,
        PACKET_TYPE_ADVERTISE,
        PACKET_TYPE_DEBUG_MESSAGE,
        PACKET_TYPE_MALFORMED
    } PacketType_t;

    typedef enum {
        COMMAND_TYPE_FORWARD,
        COMMAND_TYPE_UPDATE_RECEIVER_ID,
    } CommandType_t;

    typedef enum {
        DEBUG_MESSAGE_TYPE_INFO,
        DEBUG_MESSAGE_TYPE_WARNING,
        DEBUG_MESSAGE_TYPE_ERROR,
    } DebugMessageSeverity_t;
    
    typedef struct {
        uint16_t receiverId;
        uint16_t senderId;
        uint16_t sourceId;
        PacketType_t packetType;
        uint16_t packetId;
        uint16_t dataLength;
        uint8_t *data;
    } PacketHead_t;

    typedef struct{
        String message;
        DebugMessageSeverity_t severity;
    } DebugMessage_t;

    extern PacketQueue sendQueue;
    extern PacketQueue receiveQueue;

    class Packet {
        public:
            PacketHead_t head;
            bool hasBeenSent = false;
            bool hasBeenSentInCycle = false;
            unsigned long lastSendTime = 0;
            uint8_t sendAttempts = 0;
            
            float rssi = 0.0f;
            float snr = 0.0f;

            Packet(uint16_t senderId, uint16_t receiverId, uint16_t sourceId, PacketType_t packetType, uint16_t dataLength, uint8_t *data);
            Packet(uint16_t size, uint8_t *data, float rssi, float snr);
            Packet(const Packet& other);
            ~Packet();
            uint32_t getPacketIdentifier() const;
            static Packet* createAckPacket(const Packet &original);
            static Packet* createDebugMessagePacket(String message, uint16_t receiverId, DebugMessageSeverity_t severity);
            void handle();
            void print();
        
        private:
            static inline uint8_t nextPacketId = 0;
            static Packet mergePacket(Packet *packet1, Packet *packet2);
    };
}