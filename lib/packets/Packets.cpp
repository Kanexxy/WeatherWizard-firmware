#include "Packets.hpp"

namespace Packets {
    
    PacketQueue sendQueue;
    PacketQueue receiveQueue;

    Packet::Packet(uint16_t senderId, uint16_t receiverId, uint16_t sourceId, PacketType_t packetType, uint16_t dataLength, uint8_t *data)
    {
        this->head.senderId = senderId;
        this->head.receiverId = receiverId;
        this->head.sourceId = sourceId;
        this->head.packetId = Packet::nextPacketId++;
        this->head.packetType = packetType;
        this->head.dataLength = dataLength;
        this->head.data = data;
        this->rssi = 0.0f;
        this->snr = 0.0f;
    }

    Packet::Packet(uint16_t size, uint8_t *data, float rssi, float snr) {
        if (size < sizeof(PacketHead_t)) {
            this->head.packetType = PACKET_TYPE_MALFORMED; //Packet too short to contain header, mark as malformed
            return;
        }
        memcpy(&(this->head), data, sizeof(PacketHead_t));
        if (size < (sizeof(PacketHead_t) + this->head.dataLength)) {
            this->head.packetType = PACKET_TYPE_MALFORMED; //Packet too short to contain full data, mark as malformed
            return;
        }
        this->head.data = new uint8_t[this->head.dataLength];
        memcpy(this->head.data, data + sizeof(PacketHead_t), this->head.dataLength);
        this->rssi = rssi;
        this->snr = snr;
        return;
    }

    Packet::Packet(const Packet& other) {
        memcpy(&(this->head), &(other.head), sizeof(PacketHead_t));
        if (other.head.dataLength > 0 && other.head.data != nullptr) {
            this->head.data = new uint8_t[other.head.dataLength];
            memcpy(this->head.data, other.head.data, other.head.dataLength);
        } else {
            this->head.data = nullptr;
        }
        this->hasBeenSent = other.hasBeenSent;
        this->lastSendTime = other.lastSendTime;
        this->sendAttempts = other.sendAttempts;
        this->rssi = other.rssi;
        this->snr = other.snr;
    }

    Packet::~Packet() {
        if (this->head.data != nullptr) {
            delete[] this->head.data;
            this->head.data = nullptr;
        }
    }

    uint32_t Packet::getPacketIdentifier() const {
        return ((uint32_t)this->head.senderId << 16) | this->head.packetId;
    }

    void Packet::handle() {
        MeshUtils::availableNodes.addOrUpdateNode(this->head.senderId, this->rssi, this->snr);
        if (this->head.receiverId != MeshUtils::nodeID && this->head.receiverId != MeshUtils::broadcastID) {
            //packet not for me, ignore
            return;
        }
        switch (this->head.packetType) {

            case PACKET_TYPE_MALFORMED:{
                S_DEBUGF("Received malformed packet from %d\n", this->head.senderId);
                break;
            }
            case PACKET_TYPE_SENSORDATA:{
                //Sensor packets are only for gateway nodes, forward if not gateway
                Packet *ackPacket = createAckPacket(*this);
                sendQueue[ackPacket->getPacketIdentifier()] = ackPacket;
                if (!MeshUtils::isGateway) {
                    S_DEBUGF("Forwarding sensor data packet ID %d from %d to %d\n", this->head.packetId, this->head.senderId, MeshUtils::nextHopID);
                    this->head.receiverId = MeshUtils::nextHopID;
                    Packet* forwardedPacket = new Packet(*this);
                    sendQueue[forwardedPacket->getPacketIdentifier()] = forwardedPacket;
                    return;
                }
                break;
            }
            case PACKET_TYPE_ACK: {
                S_DEBUGF("Handling ACK packet (ID: %4X) from %4X to %4X\n", this->head.packetId, this->head.senderId, this->head.receiverId);
                Packets::Packet* originalPacket = sendQueue.at(*((uint32_t*)this->head.data));
                sendQueue.erase(originalPacket->getPacketIdentifier());
                delete originalPacket;
                originalPacket = nullptr;
                break;
            }
            case PACKET_TYPE_COMMAND:{
                // Handle command packet
                break;
            }
            case PACKET_TYPE_DISCOVER:{
                // Handle discover packet
                break;
            }
            case PACKET_TYPE_ADVERTISE:{
                // Handle advertise packet
                break;
            }
            case PACKET_TYPE_DEBUG_MESSAGE:{
                Packet *ackPacket = createAckPacket(*this);
                sendQueue[ackPacket->getPacketIdentifier()] = ackPacket;
                S_DEBUGF("Debug message: %.*s\n", this->head.dataLength, this->head.data);
                break;
            }
            default:{
                S_DEBUGF("Unknown packet type (%d)\n", this->head.packetType);
                break;
            }
        }
        return;
    }

    Packet* Packet::createAckPacket(const Packet &original) {
        uint32_t* identifier = new uint32_t(original.getPacketIdentifier());
        return new Packet(
            MeshUtils::nodeID,      //senderId
            original.head.senderId, //receiverId (original sender)
            MeshUtils::nodeID,      //sourceId
            PACKET_TYPE_ACK,
            sizeof(uint32_t),      //dataLength
            (uint8_t*)identifier
        );
    }

    Packet* Packet::createDebugMessagePacket(String message, uint16_t receiverId, DebugMessageSeverity_t severity) {
        uint16_t dataLength = message.length() + 1; // +1 for null terminator
        uint8_t* data = new uint8_t[dataLength];
        message.toCharArray((char*)data, dataLength);
        return new Packet(
            MeshUtils::nodeID,
            receiverId,
            MeshUtils::nodeID,
            PACKET_TYPE_DEBUG_MESSAGE,
            dataLength,
            data
        );
    }

    void Packet::print() {
        S_DEBUGF("Packet ID: %d\n", this->head.packetId);
        S_DEBUGF("  Sender ID: %d\n", this->head.senderId);
        S_DEBUGF("  Receiver ID: %d\n", this->head.receiverId);
        S_DEBUGF("  Source ID: %d\n", this->head.sourceId);
        S_DEBUGF("  Packet Type: %d\n", this->head.packetType);
        S_DEBUGF("  Data Length: %d\n", this->head.dataLength);
        S_DEBUGF("  RSSI: %.2f dBm\n", this->rssi);
        S_DEBUGF("  SNR: %.2f dB\n", this->snr);
    }
}