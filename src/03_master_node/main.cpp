#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "Node.hpp"
#include "debug_helper.hpp"
#include "SX1262_helper.hpp"
#include "Packets.hpp"
#include "Mesh.hpp"
#include "WebUtils.hpp"
#include "secrets.hpp"

LoRaHelper::SX1262_helper radio = LoRaHelper::SX1262_helper(new Module(HELTEC_NSS_PIN, HELTEC_DIO0_PIN, HELTEC_RESET_PIN, HELTEC_BUSY_PIN));

void masterNodeHandler(Packets::Packet* packet);

Node::Node masterNode(masterNodeHandler);

void masterNodeHandler(Packets::Packet* packet){
    S_DEBUGF("Master node handler called for packet %08X\n", packet->getPacketIdentifier());
    if (packet->head.packetType == Packets::PACKET_TYPE_REGISTER_NODE){
        Packets::RegisterNodePacketContent_t* content = (Packets::RegisterNodePacketContent_t*)packet->head.data;
        S_DEBUGF("Received register-node-packet from node %d with RSSI %.2f and SNR %.2f\n", packet->head.sourceId, packet->info.rssi, packet->info.snr);
        WebUtils::RegisterNodeInfo_t registerInfo = {
            content->nodeId,
            content->nextNodeId,
            false,
            content->lastSeenTime
        };
        WebUtils::Request::createRegisterNodeRequest(registerInfo).queue();
    }
}

void setup() {
    Serial.begin(115200);
    S_DEBUGF("Starting Master Node...\n");
    S_DEBUGF("Connecting to WiFi SSID: %s\n", Secrets::WIFI_SSID);
    WiFi.begin(Secrets::WIFI_SSID, Secrets::WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        S_DEBUGF(".");
    }
    S_DEBUGF("\nWiFi connected. IP address: %s\n", WiFi.localIP().toString().c_str());    
    TimeUtils::initializeTimeFromNTP();
    time_t now = TimeUtils::getCurrentTime();
    S_DEBUGF("\nCurrent time: %s\n", ctime(&now));

    radio.initialize();
    MeshUtils::initMeshVars(true);
    masterNode.nodeState = Node::NODE_STATE_IDLE;
    WebUtils::RegisterNodeInfo_t registerInfo = {
        MeshUtils::nodeID,
        0,
        true,
        now
    };
    WebUtils::Request::createRegisterNodeRequest(registerInfo).queue();
    WebUtils::startRequestProcessingTask();
}

void loop() {
    masterNode.sendQueue.newCycle();
    unsigned long lastRetryTime = millis();
    while (millis() - lastRetryTime < TimeUtils::wakeCycleConfig.wakeDurationSeconds * 1000) {        
        if (masterNode.tick(radio)) continue;
    }
    S_DEBUGF(masterNode.sendQueue.empty() ? "." : "SendQueue RST\n");
}