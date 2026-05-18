#include <Arduino.h>

#include "Node.hpp"
#include "debug_helper.hpp"
#include "SX1262_helper.hpp"
#include "Packets.hpp"
#include "Mesh.hpp"

LoRaHelper::SX1262_helper radio = LoRaHelper::SX1262_helper(new Module(SX1262_NSS_PIN, SX1262_DIO_PIN, SX1262_RESET_PIN, SX1262_BUSY_PIN));

Node::Node sensorNode;
void findAndSetHopToGateway();

void setup() {
    Serial.begin(115200);
    S_DEBUGF("Starting Sensor Node...\n");
    SensorHelper::initialize();
    MeshUtils::initMeshVars(false);
    sensorNode.nodeState = Node::NODE_STATE_INITIALIZING;
    findAndSetHopToGateway();
    
    S_DEBUGF("Initialization complete. Syncing with wake cycle...\n");
    while(!TimeUtils::isInWakeWindow()) delay(5);
    //Adding register Node packet
    sensorNode.sendQueue.add(Packets::Packet::createRegisterNodePacket(MeshUtils::nextHopID));
}

void loop() {
    S_DEBUGF("Waking up sensor node (ID %.4X)...\n", MeshUtils::nodeID);
    radio.initialize();

    sensorNode.nodeState = Node::NODE_STATE_IDLE;
    unsigned long wakeUpTime = millis();
    time_t currentTime = TimeUtils::getCurrentTime();
    Packets::Packet* debugPacket = Packets::Packet::createDebugMessagePacket(
            "Test debug message. Current Time: " + String(ctime(&currentTime)),
            MeshUtils::nextHopID,
            Packets::DEBUG_MESSAGE_TYPE_INFO
    );
    sensorNode.sendQueue.add(debugPacket);
    
    sensorNode.sendQueue.newCycle();
    
    while (TimeUtils::isInWakeWindow()){
        if (sensorNode.tick(radio)) continue;
    }
    //going to sleep
    S_DEBUGF("Going to sleep...\n\n");
    //Sleeping is simulated by just waiting here
    sensorNode.nodeState = Node::NODE_STATE_SLEEPING;
    radio.standby();
    delay(TimeUtils::wakeCycleConfig.sleepDurationSeconds * 1000);
}

void findAndSetHopToGateway(){
    Node::Node tempNode;
    radio.initialize();
    tempNode.nodeState = Node::NODE_STATE_IDLE;
    S_DEBUGF("Finding hop to gateway...\n");
    unsigned long discoverPacketSentTime = millis();
    tempNode.sendQueue.add(Packets::Packet::createDiscoverPacket());
    bool discoverPacketSent = true;
    while (MeshUtils::hasConnection == false){

        if (tempNode.tick(radio)) continue;
        // Send discover packet if recent nodes found and not sent yet, or if timeout of one cycle reached
        if ((MeshUtils::availableNodes.hasRecentNode(1) ||
            millis() - discoverPacketSentTime > TimeUtils::getCycleDurationSeconds() * 1000 * 1)
            && !discoverPacketSent){
            S_DEBUGF("Sending discover packet to find route to gateway...\n");
            tempNode.sendQueue.add(Packets::Packet::createDiscoverPacket());
            discoverPacketSent = true;
            discoverPacketSentTime = millis();
        }
        // Retry sending discover packet every 2 full wake/sleep cycles if no connection yet
        if (discoverPacketSent && (millis() - discoverPacketSentTime) > TimeUtils::getCycleDurationSeconds() * 1000 * 2){
            S_DEBUGF("Trying to set next hop from available nodes...\n");
            discoverPacketSent = MeshUtils::setNextHopFromAvailableNodes(); //returns true if next hop successfully set, false if no suitable node found
        }
    }
}