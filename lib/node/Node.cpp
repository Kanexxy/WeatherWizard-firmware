#include "Node.hpp"


namespace Node{    
    //nodeState_t nodeState;
    //Packets::PacketQueue sendQueue;
    //Packets::PacketQueue receiveQueue;

    // Pre-transmit callback to update time-sensitive data right before transmission
    void Node::preTransmitCallback(LoRaHelper::SX1262_helper* callingRadio, byte* data, uint8_t length) {
        Packets::PacketHead_t* head = (Packets::PacketHead_t*)data;
        byte* payloadData = data + sizeof(Packets::PacketHead_t);
        
        time_t currentTime = TimeUtils::getCurrentTime();
        
        // Update timestamp and time-on-air based on packet type
        if (head->packetType == Packets::PACKET_TYPE_ADVERTISE) {
            Packets::AdvertisePacketContent_t* content = (Packets::AdvertisePacketContent_t*)payloadData;
            content->timeSyncInfo.remoteNodeTime = currentTime;
            content->timeSyncInfo.estimatedTimeOnAirUs = callingRadio->getTimeOnAir(length);
            S_DEBUGF("Pre-TX: Updated Advertise packet timestamp to %ld, ToA: %d us\n", currentTime, content->timeSyncInfo.estimatedTimeOnAirUs);
        } 
        else if (head->packetType == Packets::PACKET_TYPE_ACK) {
            Packets::AckPacketContent_t* content = (Packets::AckPacketContent_t*)payloadData;
            content->timeSyncInfo.remoteNodeTime = currentTime;
            content->timeSyncInfo.estimatedTimeOnAirUs = callingRadio->getTimeOnAir(length);
            S_DEBUGF("Pre-TX: Updated Ack packet timestamp to %ld, ToA: %d us\n", currentTime, content->timeSyncInfo.estimatedTimeOnAirUs);
        } else if (head->packetType == Packets::PACKET_TYPE_REGISTER_NODE){
            Packets::RegisterNodePacketContent_t* content = (Packets::RegisterNodePacketContent_t*)payloadData;
            content->lastSeenTime = currentTime;
            S_DEBUGF("Pre-TX: Updated RegisterNode packet timestamp to %ld\n", currentTime);
        }
    }

    void Node::addReceivedDataToRxQueue(LoRaHelper::SX1262_helper &radio){
        uint8_t outLength = 0;
        byte* data = radio.popDataFromInstanceBuffer(outLength);
        Packets::Packet* receivedPacket = new Packets::Packet(outLength, data, radio.getRSSI(), radio.getSNR());
        receiveQueue[receivedPacket->getGlobalPacketIdentifier()] = receivedPacket;
        S_DEBUGF("Added received packet (%d Bytes) ID %04X from %04X to receive queue\n", outLength, receivedPacket->head.packetId, receivedPacket->head.senderId);
        delete[] data;
        data = nullptr;
        return;
    }

    void Node::sendPacket(LoRaHelper::SX1262_helper &radio, const Packets::Packet* packet){
        byte* data = new byte[sizeof(Packets::PacketHead_t) + packet->head.dataLength];
        memcpy(data, &(packet->head), sizeof(Packets::PacketHead_t));
        ((Packets::PacketHead_t*)data)->data = nullptr;
        memcpy(data + sizeof(Packets::PacketHead_t), packet->head.data, packet->head.dataLength);
        S_DEBUGF("Starting polite transmit of packet 0x%08X to 0x%04X\n", packet->getGlobalPacketIdentifier(), packet->head.receiverId);
        
        if (!radio.startPoliteTransmit((const byte*) data, sizeof(Packets::PacketHead_t) + packet->head.dataLength, preTransmitCallback)){
            S_DEBUGF("Failed to start polite transmit of packet ID %04X\n", packet->head.packetId);
        } else {
            nodeState = NODE_STATE_SENDING;
        }
        return;
    }

    bool Node::handleRadioIRQ(LoRaHelper::SX1262_helper &radio){
        if (!radio.isInterruptPending()) {
            return false;
        }

        LoRaHelper::progress_t progress = radio.handlePendingIRQ();
        switch (progress){
            case LoRaHelper::PROGRESS_RX_COMPLETE: {
                Node::addReceivedDataToRxQueue(radio);
                nodeState = NODE_STATE_IDLE;
                break;
            }

            case LoRaHelper::PROGRESS_TX: break; // still transmitting, do nothing
            
            case LoRaHelper::ERR_TX_RECEIVED_PACKET: {
                Node::addReceivedDataToRxQueue(radio);
                nodeState = NODE_STATE_IDLE;
                break;
            }
        
            case LoRaHelper::ERR_TX_CAD_DETECTED: {
                S_DEBUGF("Channel activity detected during polite send, aborting transmission.\n");
                TimeUtils::startCooldown(TimeUtils::collisionCooldown);
                nodeState = NODE_STATE_RECEIVING_COLLISION;
                break;
            }
        
            case LoRaHelper::PROGRESS_TX_COMPLETE: {
                S_DEBUGF("Transmission of packet 0x%08X complete.\n", currentlySendingPacket->getGlobalPacketIdentifier());
                TimeUtils::startCooldown(TimeUtils::sendCooldown);
                if (!currentlySendingPacket->isAckExpected())
                {
                    sendQueue.erase(currentlySendingPacket->getGlobalPacketIdentifier());
                    delete currentlySendingPacket;
                    currentlySendingPacket = nullptr;
                } else {
                    currentlySendingPacket->markAsSent();
                }
                nodeState = NODE_STATE_IDLE;
                break;
            }

            default:{
                S_DEBUGF("Unhandled radio IRQ progress code: %d\n", progress);
                break;
            }
        }
        return true;
    }

    bool Node::startReceiveIfNeeded(LoRaHelper::SX1262_helper &radio){ //if node is in idle, try to send next packet in queue or start receiving if queue is empty
        if (nodeState != NODE_STATE_IDLE) return false;

        LoRaHelper::radioState_t currentRadioState = radio.getCurrentState();
        if (currentRadioState == LoRaHelper::STATE_IDLE){
            S_DEBUGF("Node and radio are idle, starting receive...\n");
            radio.startIdleReceive();
            return true;
        }
        if (currentRadioState != LoRaHelper::STATE_RX_RECEIVING){
            S_DEBUGF("ERROR: Radio in unexpected state %d while node idle, resetting...\n", currentRadioState);
            radio.initialize();
            return true;
        }
        return false;
    }

    bool Node::sendNextPacketIfNeeded(LoRaHelper::SX1262_helper &radio){
        
        if (nodeState == NODE_STATE_RECEIVING_COLLISION && !TimeUtils::isInCooldown(TimeUtils::collisionCooldown)){
            S_DEBUGF("Collision cooldown expired, moving from RECEIVING_COLLISION to IDLE state.\n");
            nodeState = NODE_STATE_IDLE;
        }

        if (nodeState != NODE_STATE_IDLE) return false;

        auto nextPacket = sendQueue.next();
        if (nextPacket != nullptr && (!TimeUtils::isInCooldown(TimeUtils::sendCooldown) || MeshUtils::isGateway)){ //send packet if one is available, unless its a non-gateway node in send cooldown
            currentlySendingPacket = nextPacket;
            Node::sendPacket(radio, currentlySendingPacket);
            return true;
        }
        return false;
    }

    bool Node::handleReceivedPackets(){
        if (receiveQueue.empty()) return false;
        
        Packets::Packet* packetToHandle = receiveQueue.begin()->second;
        S_DEBUGF("Handling received packet %04X\n", packetToHandle->getGlobalPacketIdentifier());

        std::list<uint32_t> ackedPacketIdentifiers;
        auto responsePackets = packetToHandle->handle(ackedPacketIdentifiers, masterNodeHandler);
        
        sendQueue.removePacketsFromIdentifierList(ackedPacketIdentifiers);
        sendQueue.addPacketsFromList(responsePackets);
        
        receiveQueue.erase(packetToHandle->getGlobalPacketIdentifier());
        delete responsePackets; //only delete the list, contained ponters still valid and added to sendQueue
        responsePackets = nullptr;
        delete packetToHandle;
        packetToHandle = nullptr;
        return true;
    }

    bool Node::tick(LoRaHelper::SX1262_helper &radio){
        bool handled = false;
        handled |= handleRadioIRQ(radio);
        handled |= startReceiveIfNeeded(radio);
        handled |= handleReceivedPackets();
        handled |= sendNextPacketIfNeeded(radio);
        return handled;
    }
};