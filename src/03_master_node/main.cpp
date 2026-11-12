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

LoRaHelper::SX1262_helper radio = LoRaHelper::SX1262_helper(new Module(SX1262_NSS_PIN, SX1262_DIO_PIN, SX1262_RESET_PIN, SX1262_BUSY_PIN));

void masterNodeHandler(Packets::Packet* packet);

Node::Node masterNode(masterNodeHandler);

void masterNodeHandler(Packets::Packet* packet){
    S_DEBUGF("Master node handler called for packet %08X\n", packet->getGlobalPacketIdentifier());
    if (packet->head.packetType == Packets::PACKET_TYPE_REGISTER_NODE){
        Packets::RegisterNodePacketContent_t* content = (Packets::RegisterNodePacketContent_t*)packet->head.data;
        S_DEBUGF("Received register-node-packet from node %d with RSSI %.2f and SNR %.2f\n", packet->head.sourceId, packet->info.rssi, packet->info.snr);
        WebUtils::RegisterNodeInfo_t registerInfo = {
            packet->head.sourceId,
            content->nextNodeId,
            false,
            content->lastSeenTime
        };
        WebUtils::Request::createRegisterNodeRequest(registerInfo, masterNode).queue();
    }

    if (packet->head.packetType == Packets::PACKET_TYPE_SENSORDATA){
        Packets::SensorDataPacketContent_t* content = (Packets::SensorDataPacketContent_t*)packet->head.data;
        S_DEBUGF("[SENSOR DATA %8X, RSSI: %.2f dbm, SNR: %.2f]\n", packet->getGlobalPacketIdentifier(), packet->info.rssi, packet->info.snr);
        S_DEBUGF("\tTemperature: %.2f C\n", content->temperatureC);
        S_DEBUGF("\tHumidity: %.2f %%\n", content->humidityPercent);
        S_DEBUGF("\tPressure: %.2f hPa\n", content->pressurehPa);
        S_DEBUGF("\tGas Resistance: %.2f kOhms\n", content->gasResistanceKOhms);
        //S_DEBUGF("\tBattery Level: %d mV\n", content->battery_level_mv);
        WebUtils::SensorReading_t sensorReading = {
            .nodeID             = packet->head.sourceId,
            .temperatureC       = content->temperatureC,
            .humidityPercent    = content->humidityPercent,
            .pressurehPa        = content->pressurehPa,
            .gasResistanceKOhms = content->gasResistanceKOhms,
            .battery_level_mv   = 0, //Battery level is currently not transmitted in the packet, will be set to 0 for now.
            .readingTime        = content->readingTime
        };
        WebUtils::Request::createSensorDataUploadRequest(sensorReading, masterNode).queue();
    }
}

void sendRegisterNodeRequest(){
    WebUtils::RegisterNodeInfo_t registerInfo = {
        MeshUtils::nodeID,
        0,
        true,
        TimeUtils::getCurrentTime()
    };
    WebUtils::Request::createRegisterNodeRequest(registerInfo, masterNode).queue();
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
    sendRegisterNodeRequest();
    WebUtils::startRequestProcessingTask();
}

void loop() {
    masterNode.sendQueue.newCycle();
    masterNode.idle(radio);
    unsigned long lastRetryTime = millis();
    static unsigned long lastKeepAliveTime = 0;
    if (millis() - lastKeepAliveTime > (TimeUtils::getCycleDurationSeconds() * 2) * 1000){
        S_DEBUGF("Sending keep-alive register node request to check for pending commands from server.\n");
        sendRegisterNodeRequest();
        lastKeepAliveTime = millis();
    }

    while (millis() - lastRetryTime < TimeUtils::wakeCycleConfig.wakeDurationSeconds * 1000) {        
        if (masterNode.tick(radio)) continue;
    }
    S_DEBUGF(masterNode.sendQueue.empty() ? "." : "SendQueue RST. %d packets remaining\n", masterNode.sendQueue.size());
}