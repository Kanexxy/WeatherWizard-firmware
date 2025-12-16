#include "Node.hpp"

namespace Node {
    
    nodeState_t nodeState;
    
    void addReceivedDataToRxQueue(LoRaHelper::SX1262_helper &radio){
        uint8_t outLength = 0;
        byte* data = radio.popDataFromInstanceBuffer(outLength);
        Packets::Packet* receivedPacket = new Packets::Packet(outLength, data, radio.getRSSI(), radio.getSNR());
        Packets::receiveQueue[receivedPacket->getPacketIdentifier()] = receivedPacket;
        S_DEBUGF("Added received packet (%d Bytes) ID %d from %d to receive queue\n", outLength, receivedPacket->head.packetId, receivedPacket->head.senderId);
        delete[] data;
        data = nullptr;
        return;
    }

    void sendPacket(LoRaHelper::SX1262_helper &radio, const Packets::Packet* packet){
        byte* data = new byte[sizeof(Packets::PacketHead_t) + packet->head.dataLength];
        memcpy(data, &(packet->head), sizeof(Packets::PacketHead_t));
        ((Packets::PacketHead_t*)data)->data = nullptr;
        memcpy(data + sizeof(Packets::PacketHead_t), packet->head.data, packet->head.dataLength);

        S_DEBUGF("Starting polite transmit of packet %.8X\n", packet->getPacketIdentifier());
        
        if (!radio.startPoliteTransmit((const byte*) data, sizeof(Packets::PacketHead_t) + packet->head.dataLength)){
            S_DEBUGF("Failed to start polite transmit of packet ID %d\n", packet->head.packetId);
        } else {
            nodeState = NODE_STATE_SENDING;
        }
        return;
    }

    bool checkAndHandleRadioIRQ(LoRaHelper::SX1262_helper &radio, Packets::Packet* &currentlySendingPacket){
        if (!radio.isInterruptPending()) {
            return false;
        }

        LoRaHelper::progress_t progress = radio.handlePendingIRQ();
        switch (progress){
            case LoRaHelper::PROGRESS_RX_COMPLETE: {
                Node::addReceivedDataToRxQueue(radio);
                nodeState = Node::NODE_STATE_IDLE;
                break;
            }

            case LoRaHelper::PROGRESS_TX: {
                // still transmitting, do nothing
                break;
            }

            case LoRaHelper::ERR_TX_RECEIVED_PACKET: {
                Node::addReceivedDataToRxQueue(radio);
                nodeState = Node::NODE_STATE_IDLE;
                break;
            }
        
            case LoRaHelper::ERR_TX_CAD_DETECTED: {
                S_DEBUGF("Channel activity detected during polite send, aborting transmission.\n");
                nodeState = Node::NODE_STATE_RECEIVING;
                break;
            }
        
            case LoRaHelper::PROGRESS_TX_COMPLETE: {
                S_DEBUGF("Transmission of packet %.4X complete.\n", currentlySendingPacket->getPacketIdentifier());
                if (currentlySendingPacket->head.packetType == Packets::PACKET_TYPE_ACK){
                    Packets::sendQueue.erase(currentlySendingPacket->getPacketIdentifier());
                    delete currentlySendingPacket;
                    currentlySendingPacket = nullptr;
                    nodeState = Node::NODE_STATE_IDLE;
                    break;
                } else {
                    currentlySendingPacket->hasBeenSentInCycle = true;
                    currentlySendingPacket->hasBeenSent = true;
                    currentlySendingPacket->sendAttempts++;
                    currentlySendingPacket->lastSendTime = millis();
                }
                nodeState = Node::NODE_STATE_IDLE;
                break;
            }

            default:{
                S_DEBUGF("Unhandled radio IRQ progress code: %d\n", progress);
                break;
            }
        }
        return true;
    }

    bool checkAndHandleIdleState(LoRaHelper::SX1262_helper &radio, Packets::Packet* &currentlySendingPacket){
        LoRaHelper::radioState_t currentRadioState = radio.getCurrentState();
        if (nodeState != Node::NODE_STATE_IDLE) return false;

        auto nextPacket = Packets::sendQueue.next(); 
        if (nextPacket != nullptr){
            currentlySendingPacket = nextPacket;
            Node::sendPacket(radio, currentlySendingPacket);
            return true;
        }
        if (currentRadioState == LoRaHelper::STATE_IDLE){
            S_DEBUGF("Node is idle, starting receive...\n");
            radio.startIdleReceive();
            return true;
        }
        if (currentRadioState != LoRaHelper::STATE_RX_RECEIVING){
            S_DEBUGF("ERROR: Node is idle but radio is not in Idle or RX. Radio state: %d\n", currentRadioState);
        }
        return false;
    }

    bool checkAndHandleReceiveQueue(){
        if (Packets::receiveQueue.empty()) return false;
        
        Packets::Packet* packetToHandle = Packets::receiveQueue.begin()->second;
        S_DEBUGF("Handling received packet %4X\n", packetToHandle->getPacketIdentifier());
        packetToHandle->handle();
        delete packetToHandle;
        Packets::receiveQueue.erase(packetToHandle->getPacketIdentifier());
        packetToHandle = nullptr;
        return true;
    }
}