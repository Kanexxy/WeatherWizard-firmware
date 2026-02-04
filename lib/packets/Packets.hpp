#pragma once
#include <cstdint>
#include <list>

#include "debug_helper.hpp"
#include "TimeUtils.hpp"
#include "Mesh.hpp"

namespace Packets {

    class PacketQueue;

    typedef enum {
        PACKET_TYPE_SENSORDATA,
        PACKET_TYPE_ACK,
        PACKET_TYPE_COMMAND,
        PACKET_TYPE_DISCOVER,
        PACKET_TYPE_ADVERTISE,
        PACKET_TYPE_REGISTER_NODE,
        PACKET_TYPE_DEBUG_MESSAGE,
        PACKET_TYPE_MALFORMED
    } PacketType_t;
    
    typedef struct {
        uint16_t packetType;
        uint16_t receiverId;
        uint16_t senderId;
        uint16_t sourceId;
        uint16_t packetId;
        uint16_t dataLength;
        uint8_t *data;
    } PacketHead_t;

    // Size of packet head: 2 + 2 + 2 + 1 + 2 + 2 + 4 = 15 Bytes
    typedef struct {
        bool hasBeenSent = false;
        bool hasBeenSentInCycle = false;
        unsigned long lastSendTime = 0;
        uint8_t sendAttempts = 0;
        
        float rssi = 0.0f;
        float snr = 0.0f;
    } PacketInfo_t;

    typedef enum {
        COMMAND_TYPE_FORWARD,
        COMMAND_TYPE_UPDATE_RECEIVER_ID,
    } CommandType_t;

    typedef enum {
        DEBUG_MESSAGE_TYPE_INFO,
        DEBUG_MESSAGE_TYPE_WARNING,
        DEBUG_MESSAGE_TYPE_ERROR,
    } DebugMessageSeverity_t;
    
    typedef struct{
        String message;
        DebugMessageSeverity_t severity;
    } DebugMessage_t;

    typedef struct{
       uint8_t hopCountToGateway;
       TimeUtils::SyncInfo_t timeSyncInfo;
       TimeUtils::WakeCycleConfig_t wakeCycleConfig;
    } AdvertisePacketContent_t;
    
    typedef struct{
       uint32_t ackedPacketIdentifier;
       TimeUtils::SyncInfo_t timeSyncInfo;
    } AckPacketContent_t;

    //list with packets where no ack is expected
    const PacketType_t noAckExpectedPacketTypes[] = {
        PACKET_TYPE_ACK,
        PACKET_TYPE_DISCOVER,
        PACKET_TYPE_ADVERTISE,
        PACKET_TYPE_MALFORMED,
    };

    class Packet {
        public:
            PacketHead_t head;
            PacketInfo_t info;

            Packet(uint16_t senderId, uint16_t receiverId, uint16_t sourceId, PacketType_t packetType, uint16_t dataLength, uint8_t *data);
            Packet(uint16_t size, uint8_t *data, float rssi, float snr);
            Packet(const Packet& other);
            ~Packet();
            uint32_t getPacketIdentifier() const;
            bool isAckExpected();
            static Packet* createAckPacket(const Packet &original);
            static Packet* createAdvertisePacket(uint16_t receiverId, uint8_t hopCountToGateway);
            static Packet* createDebugMessagePacket(String message, uint16_t receiverId, DebugMessageSeverity_t severity);
            static Packet* createDiscoverPacket();
            static Packet* createToBeForwardedPacket(const Packet &original);
            std::list<Packet*>* handle(std::list<uint32_t>& ackedPacketIdentifiers);
            void markAsSent();
            void printHead();

        
        private:
            static inline uint8_t nextPacketId = 0;
            static Packet mergePacket(Packet *packet1, Packet *packet2);
    };
}