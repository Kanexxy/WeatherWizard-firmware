#include "Packets.hpp"

namespace Packets {

    Packet::Packet(uint16_t senderId, uint16_t receiverId, uint16_t sourceId, PacketType_t packetType, uint16_t dataLength, uint8_t *data)
    {
        this->head.senderId = senderId;
        this->head.receiverId = receiverId;
        this->head.sourceId = sourceId;
        this->head.packetId = Packet::nextPacketId++;
        this->head.packetType = static_cast<uint16_t>(packetType);
        this->head.dataLength = dataLength;
        this->head.data = data;
        this->info.rssi = 0.0f;
        this->info.snr = 0.0f;
    }

    Packet::Packet(uint16_t size, uint8_t *data, float rssi, float snr) {
        if (size < sizeof(PacketHead_t)) {
            this->head.packetType = PACKET_TYPE_MALFORMED; //Packet too short to contain header, mark as malformed
            return;
        }
        memcpy(&(this->head), data, sizeof(PacketHead_t));
        if (size < (sizeof(PacketHead_t) + this->head.dataLength)) {
            this->head.packetType = PACKET_TYPE_MALFORMED; //Packet too short to contain full data, mark as malformed
            this->head.data = nullptr;
            return;
        }
        this->head.data = new uint8_t[this->head.dataLength];
        memcpy(this->head.data, data + sizeof(PacketHead_t), this->head.dataLength);
        this->info.rssi = rssi;
        this->info.snr = snr;
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
        memcpy(&(this->info), &(other.info), sizeof(PacketInfo_t));
    }

    Packet::~Packet() {
        if (this->head.data != nullptr) {
            delete[] this->head.data;
            this->head.data = nullptr;
        }
    }

    uint32_t Packet::getPacketIdentifier() const {
        return ((uint32_t)this->head.sourceId << 16) | this->head.packetId;
    }
    //let function also take a function pointer to a handler in case it is a masternode, argument is packet
    std::list<Packet*>* Packet::handle(std::list<uint32_t>& ackedPacketIdentifiers, std::function<void(Packet*)> masterNodeHandler) {
        std::list<Packet*>* responsePackets = new std::list<Packet*>();

        MeshUtils::availableNodes.addOrUpdateNode(this->head.senderId, this->info.rssi, this->info.snr);
        if (this->head.receiverId != MeshUtils::nodeID && this->head.receiverId != MeshUtils::broadcastID) {
            //packet not for me, ignore
            return responsePackets;
        }

        if (this->isAckExpected()) responsePackets->push_back(createAckPacket(*this));

        switch (this->head.packetType) {

            case PACKET_TYPE_MALFORMED:{
                S_DEBUGF("[MALFORMED PACKET]\n\tfrom %d\n", this->head.senderId);
                break;
            }
            case PACKET_TYPE_SENSORDATA:{
                //Sensor packets are only for gateway nodes, forward if not gateway
                if (!MeshUtils::isGateway) {
                    S_DEBUGF("[SENSOR DATA PACKET %08X]\n\tfrom %d\n\tforwarding to %d\n", this->getPacketIdentifier(), this->head.senderId, MeshUtils::nextHopID);
                    responsePackets->push_back(createToBeForwardedPacket(*this));
                } else {
                    masterNodeHandler(this);
                }
                break;
            }
            case PACKET_TYPE_ACK: {
                AckPacketContent_t* ackContent = (AckPacketContent_t*)this->head.data;
                S_DEBUGF("[ACK PACKET %08X]\n\tAcknowledging Packet %08X\n", this->getPacketIdentifier(), ackContent->ackedPacketIdentifier);
                TimeUtils::setLocalTimeFromRemote(ackContent->timeSyncInfo);
                ackedPacketIdentifiers.push_back(ackContent->ackedPacketIdentifier);
                break;
            }
            case PACKET_TYPE_COMMAND:{
                // Handle command packet
                break;
            }
            case PACKET_TYPE_DISCOVER:{
                S_DEBUGF("[DISCOVER PACKET %08X]\n", this->getPacketIdentifier());
                if (MeshUtils::hasConnection){
                    responsePackets->push_back(createAdvertisePacket(this->head.senderId, MeshUtils::hopCountToGateway));
                }
                break;
            }
            case PACKET_TYPE_ADVERTISE:{ 
                AdvertisePacketContent_t* data = (AdvertisePacketContent_t*)this->head.data;
                MeshUtils::availableNodes.addOrUpdateNode(this->head.senderId, this->info.rssi, this->info.snr, data->hopCountToGateway, data->wakeCycleConfig);
                TimeUtils::setLocalTimeFromRemoteAccumulated(data->timeSyncInfo);
                S_DEBUGF("[ADVERTISE PACKET %08X]\n", this->getPacketIdentifier());
                S_DEBUGF("\tHops to gateway: %d\n", data->hopCountToGateway);
                S_DEBUGF("\tRemote Node Time: %s", ctime(&(data->timeSyncInfo.remoteNodeTime)));
                S_DEBUGF("\tWake Duration: %d seconds, Sleep Duration: %d seconds\n", data->wakeCycleConfig.wakeDurationSeconds, data->wakeCycleConfig.sleepDurationSeconds);
                break;
            }

            case PACKET_TYPE_REGISTER_NODE:{
                if (!MeshUtils::isGateway){
                    S_DEBUGF("Forwarding register-node-packet %08X from node %d\n", this->getPacketIdentifier(), this->head.sourceId);
                    responsePackets->push_back(createToBeForwardedPacket(*this));
                    break;
                }
                if (masterNodeHandler == nullptr) {
                    S_DEBUGF("Received register-node-packet %08X but no master node handler defined\n", this->getPacketIdentifier());
                    break;
                }
                masterNodeHandler(this);
                break;
            }

            case PACKET_TYPE_DEBUG_MESSAGE:{
                S_DEBUGF("[DEBUG PACKET]\n\tMessage: %.*s\n", this->head.dataLength, this->head.data);
                break;
            }

            default:{
                S_DEBUGF("Unknown packet type (%d)\n", this->head.packetType);
                break;
            }
        }
        return responsePackets;
    }

    Packet* Packet::createAckPacket(const Packet &original) {
        AckPacketContent_t* content = new AckPacketContent_t();
        content->ackedPacketIdentifier = original.getPacketIdentifier();
        content->timeSyncInfo = {0, 0}; // Will be set just before transmission

        Packet* newPacket = new Packet(
            MeshUtils::nodeID,      //senderId
            original.head.senderId, //receiverId (original sender)
            MeshUtils::nodeID,      //sourceId
            PACKET_TYPE_ACK,
            sizeof(AckPacketContent_t), //dataLength
            (uint8_t*)content
        );
        return newPacket;
    }

    bool Packet::isAckExpected() {
        if (this->head.receiverId == MeshUtils::broadcastID) return false;
        for (PacketType_t type : noAckExpectedPacketTypes) {
            if (type == head.packetType) {
                return false;
            }
        }
        return true;
    }

    Packet* Packet::createAdvertisePacket(uint16_t receiverId, uint8_t hopCountToGateway) {
        AdvertisePacketContent_t* content = new AdvertisePacketContent_t();
        content->hopCountToGateway = hopCountToGateway;
        content->timeSyncInfo = {0, 0}; // Will be set just before transmission
        content->wakeCycleConfig = TimeUtils::wakeCycleConfig;

        Packet* newPacket = new Packet(
            MeshUtils::nodeID,
            receiverId,
            MeshUtils::nodeID,
            PACKET_TYPE_ADVERTISE,
            sizeof(AdvertisePacketContent_t),
            (uint8_t*)content
        );
        return newPacket;
    }

    Packet* Packet::createToBeForwardedPacket(const Packet &original) {
        Packet* forwardedPacket = new Packet(original);
        forwardedPacket->head.senderId = MeshUtils::nodeID;
        forwardedPacket->head.receiverId = MeshUtils::nextHopID;
        return forwardedPacket;
    }

    Packet* Packet::createDiscoverPacket() {
        Packet* newPacket = new Packet(
            MeshUtils::nodeID,      // 2 Byte
            MeshUtils::broadcastID, // 2 Byte
            MeshUtils::nodeID,      // 2 Byte
            PACKET_TYPE_DISCOVER,   // 2 Byte
            0,                      // 2 Byte
            nullptr                 // 4 Byte
        );
        return newPacket;
    }

    Packet* Packet::createRegisterNodePacket(uint16_t receiverId) {
        RegisterNodePacketContent_t* content = new RegisterNodePacketContent_t;
        content->lastSeenTime = 0; //Will be set just before transmission
        content->nextNodeId = MeshUtils::nextHopID;
        content->nodeId = MeshUtils::nodeID;
        return new Packet(
            MeshUtils::nodeID,
            receiverId,
            MeshUtils::nodeID,
            PACKET_TYPE_REGISTER_NODE,
            sizeof(RegisterNodePacketContent_t),
            (uint8_t*)content
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

    void Packet::markAsSent() {
        this->info.hasBeenSent = true;
        this->info.hasBeenSentInCycle = true;
        this->info.sendAttempts += 1;
        this->info.lastSendTime = millis();
    }

    void Packet::printHead() {
        S_DEBUGF("[Packet %d]\n", this->head.packetId);
        S_DEBUGF("\tSender ID: %d\n", this->head.senderId);
        S_DEBUGF("\tReceiver ID: %d\n", this->head.receiverId);
        S_DEBUGF("\tSource ID: %d\n", this->head.sourceId);
        S_DEBUGF("\tPacket Type: %d\n", this->head.packetType);
        S_DEBUGF("\tData Length: %d\n", this->head.dataLength);
        S_DEBUGF("\tRSSI: %.2f dBm\n", this->info.rssi);
        S_DEBUGF("\tSNR: %.2f dB\n", this->info.snr);
    }
}