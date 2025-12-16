#include <Arduino.h>
#include <RadioLib.h>

#include <SX1262_helper.hpp>
#define S_DEBUGF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)


SX1262 radio = new Module(HELTEC_NSS_PIN, HELTEC_DIO0_PIN, HELTEC_RESET_PIN, HELTEC_BUSY_PIN);
unsigned long lastSendTime = 0;
unsigned long lastRxTime = 0;

void setup(){
    Serial.begin(115200);
    delay(500); // Give serial time to initialize
    
    S_DEBUGF("Initializing SX1262...\n");
    radio.initialize();
    delay(500);
    S_DEBUGF("Start receive...\n");
    int16_t status = radio.startReceive(0);
    delay(500);
}

void loop(){
    if (millis() - lastSendTime > 5000) {
        unsigned long startTime = millis();
        S_DEBUGF("Sending Hello World after %d ms...\n", millis() - lastSendTime);
        lastSendTime = millis();
        bool messageAvailableBackup = radio.messageAvailable;
        int16_t state = radio.transmit("Hello World!");
        radio.messageAvailable = messageAvailableBackup;
        if (state == RADIOLIB_ERR_NONE) {
            S_DEBUGF("Success!\n");
        } else {
            S_DEBUGF("Transmission failed, code: %d\n", state);
        }
        S_DEBUGF("This Transmission took %d ms\n\n", millis() - startTime);
        radio.startReceive(0);
    }
    if(radio.messageAvailable) {
        unsigned long startTime = millis();
        radio.messageAvailable = false;
        String str;
        int16_t state = radio.readData(str);
        if (state == RADIOLIB_ERR_NONE) {
            // Check if packet has valid content (not empty/self-transmission)
            if (str.length() > 0) {
                S_DEBUGF("Received: %s\n", str.c_str());
                S_DEBUGF("This message was %d bytes long.\n", str.length());
                S_DEBUGF("RSSI: %.2f dbm, SNR: %.2f, Last RX was %dms ago.\n", radio.getRSSI(), radio.getSNR(), millis() - lastRxTime);
                lastRxTime = millis();
            } else {
                S_DEBUGF("Ignoring empty packet (likely self-TX artifact)\n");
            }
        } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
            S_DEBUGF("Reception timed out!\n");
        } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
            S_DEBUGF("CRC error!\n");
        } else {
            S_DEBUGF("Reception failed, code: %d\n", state);
        }
        S_DEBUGF("This Reception took %d ms\n\n", millis() - startTime);
        radio.startReceive(0);
    }
}