#pragma once
#include <cstdint>
#include <list>

#include "debug_helper.hpp"
#include "TimeUtils.hpp"
#include "Mesh.hpp"

#include "ICommandHandler.hpp"

namespace Packets {

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


    typedef struct {
        bool hasBeenSent = false;
        bool hasBeenSentInCycle = false;
        unsigned long lastSendTime = 0;
        uint8_t sendAttempts = 0;
        
        float rssi = 0.0f;
        float snr = 0.0f;
    } PacketInfo_t;

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

    typedef struct{
       uint16_t nextNodeId;
       time_t lastSeenTime;
    } RegisterNodePacketContent_t;

    typedef struct{
        float temperatureC;
        float humidityPercent;
        float pressurehPa;
        float gasResistanceKOhms;
        time_t readingTime;
    } SensorDataPacketContent_t;

    typedef struct{
        CommandType_t commandType;
        uint16_t commandData;
    } CommandPacketContent_t;

    //list with packets where no ack is expected
    const PacketType_t noAckExpectedPacketTypes[] = {
        PACKET_TYPE_ACK,
        PACKET_TYPE_DISCOVER,
        PACKET_TYPE_ADVERTISE,
        PACKET_TYPE_MALFORMED,
        PACKET_TYPE_COMMAND
    };

    class Packet {
        public:
            PacketHead_t head;
            PacketInfo_t info;

            Packet(uint16_t senderId, uint16_t receiverId, uint16_t sourceId, PacketType_t packetType, uint16_t dataLength, uint8_t *data);
            Packet(uint16_t size, uint8_t *data, float rssi, float snr);
            Packet(const Packet& other);
            ~Packet();
            uint32_t getGlobalPacketIdentifier() const;
            bool isAckExpected();
            static Packet* createAckPacket(const Packet &original);
            static Packet* createAdvertisePacket(uint16_t receiverId, uint8_t hopCountToGateway);
            static Packet* createDebugMessagePacket(String message, uint16_t receiverId, DebugMessageSeverity_t severity);
            static Packet* createDiscoverPacket();
            static Packet* createToBeForwardedPacket(const Packet &original);
            static Packet* createRegisterNodePacket(uint16_t receiverId);
            static Packet* createSensorDataPacket(uint16_t receiverId, SensorDataPacketContent_t sensorData);
            static Packet* createCommandPacket(uint16_t receiverId, CommandType_t commandType, uint16_t commandData);
            void handle(ICommandHandler& commandHandler);
            void handleCommandPacket(ICommandHandler& commandHandler);
            void markAsSent();
            void printHead();

        private:
            static inline uint16_t nextPacketId = 0;
            static Packet mergePacket(Packet *packet1, Packet *packet2);
    };
}