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
    Node::nodeState = Node::NODE_STATE_IDLE;
}

void loop() {
    S_DEBUGF("Resetting send queue iterator...\n");

    Packets::sendQueue.newCycle();
    Packets::Packet* currentlySendingPacket = Packets::sendQueue.next();

    unsigned long lastRetryTime = millis();
    while (millis() - lastRetryTime < wakeDuration){
        
        if (Node::checkAndHandleRadioIRQ(radio, currentlySendingPacket)) continue;

        if (Node::checkAndHandleIdleState(radio, currentlySendingPacket)) continue;

        if (Node::checkAndHandleReceiveQueue()) continue;

    }
}