#include <Arduino.h>
#include <RadioLib.h>
#include "SX1262_helper.hpp"

LoRaHelper::SX1262_helper radio = LoRaHelper::SX1262_helper(new Module(HELTEC_NSS_PIN, HELTEC_DIO0_PIN, HELTEC_RESET_PIN, HELTEC_BUSY_PIN));

volatile bool packetFlag = false;

#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
void setFlag(void) {
  // we got a packet, set the flag
  packetFlag = true;
}

void setup() {
  Serial.begin(115200);
  // initialize SX1262 with default settings
  radio.initialize();
  Serial.print(F("[SX1262] Initializing ... "));
  radio.setDio1Action(setFlag);

  Serial.print(F("[SX1262] Starting CAD to RX ... "));
  int state = radio.startChannelScan(LoRaHelper::defaultChannelScanConfig);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
  }
}

void loop() {
  // check if the flag is set
  static unsigned long lastSuccessfulRX = 0;
  static bool channelFreeReported = false;
  static bool rxMode = false;
  static unsigned long irqCountSinceLastFlagchange = 0;
  static uint32_t lastirq = 0;

  if(packetFlag) {
    packetFlag = false;
    uint32_t irq = radio.getIrqFlags();
    radio.clearIrqFlags(irq);

    if (irq != lastirq) {
      Serial.printf("[SX1262] IRQ Flags changed after %lu cycles:\n", irqCountSinceLastFlagchange);
    //  Serial.printf("\tIRQ_CAD_DONE:     %s\n", (irq & (1UL << RADIOLIB_IRQ_CAD_DONE)) ? "true" : "false");
    //  Serial.printf("\tIRQ_CAD_DETECTED: %s\n", (irq & (1UL << RADIOLIB_IRQ_CAD_DETECTED)) ? "true" : "false");
    //  Serial.printf("\tIRQ_RX_DONE:      %s\n", (irq & (1UL << RADIOLIB_IRQ_RX_DONE)) ? "true" : "false");
    //  Serial.printf("\tIRQ_TIMEOUT:      %s\n", (irq & RADIOLIB_SX126X_IRQ_TIMEOUT) ? "true" : "false");
      irqCountSinceLastFlagchange = 0;
    } else ++irqCountSinceLastFlagchange;
    lastirq = irq;
    
    switch (irq) {
      case (1UL << RADIOLIB_IRQ_RX_DONE):
        {
          String str;
          int16_t state = radio.readData(str);
          if (state == RADIOLIB_ERR_NONE) {
            Serial.printf("\t\t\t[SX1262] Received after %lu ms: ", millis() - lastSuccessfulRX);
            Serial.println(str);
            Serial.print(F("[SX1262] RSSI: "));
            Serial.print(radio.getRSSI());
            Serial.print(F(" dBm, SNR: "));
            Serial.println(radio.getSNR());
            lastSuccessfulRX = millis();
            channelFreeReported = false;
          } else {
            Serial.print(F("[SX1262] Reception failed, code "));
            Serial.println(state);
          }
          rxMode = false;
          //clear checked flags from irq var
          irq &= ~(1UL << RADIOLIB_IRQ_RX_DONE);
        }
        break;
      
      case (1UL << RADIOLIB_IRQ_CAD_DETECTED) | (1UL << RADIOLIB_IRQ_CAD_DONE):
        if (channelFreeReported == true){
          Serial.print(F("[SX1262] Channel now busy, switching to RX mode\n"));
        }
        rxMode = true;
        irq &= ~((1UL << RADIOLIB_IRQ_CAD_DETECTED) | (1UL << RADIOLIB_IRQ_CAD_DONE));
      break;
      
      case (1UL << RADIOLIB_IRQ_CAD_DONE):
        if (!rxMode & !channelFreeReported) {
          Serial.print(F("[SX1262] Channel is now free!\n"));
          channelFreeReported = true;
        }
        irq &= ~(1UL << RADIOLIB_IRQ_CAD_DONE);
      break;
      
      default:
        Serial.print(F("[SX1262] IRQ: "));
        Serial.println(irq);
      break;
    }
    /*
    if (irq != 0) {
      Serial.print(F("[SX1262] Warning: Unhandled IRQ flags: "));
      //print irq variable in binary like 0b0000000000000000
      Serial.print("0b");
      for (int i = 15; i >= 0; i--) {
        Serial.print((irq & (1UL << i)) ? "1" : "0");
      }
      Serial.println();
    }
    
    if (radio.getIrqFlags() != 0) {
      Serial.print(F("[SX1262] Warning: IRQ flags not cleared after handling! Flags: "));
      Serial.println(radio.getIrqFlags());
    }
    */ 
    if (!rxMode) {
      int state = radio.startChannelScan(LoRaHelper::defaultChannelScanConfig);
    }
  }
}
