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

    void Node::sendPacket(LoRaHelper::SX1262_helper &radio, Packets::Packet* packet){
        if (packet->head.receiverId == MeshUtils::nodeID){ //packet is actually meant for this node, not to be sent, add to receive queue instead, feature meant for commands
            S_DEBUGF("Packet ID %08X is actually meant for this node, adding to receive queue instead of sending.\n", packet->getGlobalPacketIdentifier());
            this->handleCommand(COMMAND_DELETE_PACKET_FROM_SEND_QUEUE, (const uint8_t*)&packet->head.packetId, sizeof(uint16_t));
            this->receiveQueue.add(packet);
            return;
        }
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

            case LoRaHelper::ERR_UNHANDLED_INTERRUPT: {
                S_DEBUGF("Received unhandled interrupt progress code %d from radio, going to idle state...\n", progress);
                Node::idle(radio);
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

        packetToHandle->handle(*this);
        
        receiveQueue.erase(packetToHandle->getGlobalPacketIdentifier());
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

    void Node::idle(LoRaHelper::SX1262_helper &radio){
        radio.cleanAndIdle();
        nodeState = NODE_STATE_IDLE;
    }

    void Node::handleCommand(const CommandType_t type, const uint8_t* data, const uint8_t len){
        switch (type){
            case COMMAND_ADD_PACKET_TO_SEND_QUEUE:{
                sendQueue.add((Packets::Packet*)data);
                S_DEBUGF("Added packet %04X to send queue via command handler\n", ((Packets::Packet*)data)->getGlobalPacketIdentifier());
                break;
            }

            case COMMAND_DELETE_PACKET_FROM_SEND_QUEUE:{
                uint32_t packetIdToDelete = *(uint32_t*)data;
                sendQueue.erase(packetIdToDelete);
                S_DEBUGF("Deleted packet %04X from send queue via command handler\n", packetIdToDelete);
                break;
            }

            case COMMAND_USE_MASTER_NODE_HANDLER:{
                if (masterNodeHandler != nullptr){
                    masterNodeHandler((Packets::Packet*)data);
                    S_DEBUGF("Handled packet %04X with master node handler via command handler\n", ((Packets::Packet*)data)->getGlobalPacketIdentifier());
                } else {
                    S_DEBUGF("Master node handler not set, cannot handle packet %04X\n", ((Packets::Packet*)data)->getGlobalPacketIdentifier());
                }
                break;
            }

            case COMMAND_RESET_NODE:{
                S_DEBUGF("Reset command received, resetting node...\n");
                ESP.restart();
                break;
            }

            case COMMAND_UPDATE_RECEIVER_ID:{
                uint16_t newReceiverId = *(uint16_t*)data;
                S_DEBUGF("Updating receiver ID to %04X via command handler\n", newReceiverId);
                MeshUtils::nextHopID = newReceiverId;
                sendQueue.add(Packets::Packet::createRegisterNodePacket(MeshUtils::nextHopID));
                break;
            }

            case COMMAND_SEND_RESET_COMMAND_PACKET:{
                uint16_t targetNodeId = ((SendCommandContent_t*)data)->targetNodeId;
                Packets::Packet* resetCommandPacket = Packets::Packet::createCommandPacket(targetNodeId, COMMAND_RESET_NODE, 0);
                sendQueue.add(resetCommandPacket);
                S_DEBUGF("Added reset command packet for node %04X to send queue via command handler\n", targetNodeId);
                break;
            }

            case COMMAND_SEND_UPDATE_RECEIVER_ID_COMMAND_PACKET:{
                uint16_t targetNodeId = ((SendCommandContent_t*)data)->targetNodeId;
                uint16_t newReceiverId = ((SendCommandContent_t*)data)->commandData;
                Packets::Packet* updateReceiverIdPacket = Packets::Packet::createCommandPacket(targetNodeId, COMMAND_UPDATE_RECEIVER_ID, newReceiverId);
                sendQueue.add(updateReceiverIdPacket);
                S_DEBUGF("Added update receiver ID command packet for node %04X to send queue via command handler\n", targetNodeId);
                break;
            }

            default:{
                S_DEBUGF("Node received unknown command type %d\n", type);
                break;
            }
        }
    }
};