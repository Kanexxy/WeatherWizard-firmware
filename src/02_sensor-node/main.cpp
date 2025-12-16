#include <Arduino.h>

#include "Node.hpp"
#include "debug_helper.hpp"
#include "SX1262_helper.hpp"
#include "Packets.hpp"
#include "Mesh.hpp"

LoRaHelper::SX1262_helper radio = LoRaHelper::SX1262_helper(new Module(HELTEC_NSS_PIN, HELTEC_DIO0_PIN, HELTEC_RESET_PIN, HELTEC_BUSY_PIN));

const unsigned long wakeDuration = 5000;

void setup() {
    Serial.begin(115200);
    S_DEBUGF("Starting Master Node...\n");

    radio.initialize();
    //MeshUtils::initializeMeshConnection();
    MeshUtils::nodeID = radio.random(0xFFFF);
    Node::nodeState = Node::NODE_STATE_IDLE;
}

void loop() {
    S_DEBUGF("Waking up sensor node (ID %.4X)...\n", MeshUtils::nodeID);
    Node::nodeState = Node::NODE_STATE_IDLE;
    unsigned long wakeUpTime = millis();
    
    Packets::Packet* debugPacket = Packets::Packet::createDebugMessagePacket(
            "Test debug message.",
            MeshUtils::broadcastID,
            Packets::DEBUG_MESSAGE_TYPE_INFO
    );
    Packets::sendQueue[debugPacket->getPacketIdentifier()] = debugPacket;

    Packets::sendQueue.newCycle();
    Packets::Packet* currentlySendingPacket = Packets::sendQueue.next();
    
    while (millis() - wakeUpTime < wakeDuration){
        if (Node::checkAndHandleRadioIRQ(radio, currentlySendingPacket)) continue;

        if (Node::checkAndHandleIdleState(radio, currentlySendingPacket)) continue;

        if (Node::checkAndHandleReceiveQueue()) continue;

    }
    //going to sleep
    S_DEBUGF("Going to sleep...\n\n");
    //Sleeping is simulated by just waiting here
    Node::nodeState = Node::NODE_STATE_SLEEPING;
    radio.standby();
    delay(2000);
    radio.initialize(); //reset radio state
}