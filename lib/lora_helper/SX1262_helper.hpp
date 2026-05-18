#pragma once
#include <RadioLib.h>
#include <SPI.h>

#ifdef __AVR__
    #include <ArduinoSTL.h>
    #include <vector>
#else
    #include <vector>
#endif

#include "debug_helper.hpp"
#include "Packets.hpp"

#define LORA_FREQUENCY 868.0
#define LORA_SPREADING_FACTOR 8
#define LORA_BANDWIDTH 125
#define LORA_CODING_RATE 5
#define LORA_PREAMBLE_LENGTH 14
#define LORA_OUTPUT_POWER -9

#define LORA_POLITE_SEND_MIN_BACKOFF_MS 50
#define LORA_POLITE_SEND_MAX_BACKOFF_MS 220


#define WW_SX1262_CS 5
#define WW_SX1262_SCK 18
#define WW_SX1262_MOSI 23
#define WW_SX1262_MISO 19

#define WW_SX1262_RESET 12
#define WW_SX1262_BUSY 13
#define WW_SX1262_DIO1 14

#define HELTEC_SX1262_NSS_PIN 8
#define HELTEC_SX1262_RESET_PIN 12
#define HELTEC_SX1262_DIO0_PIN 14
#define HELTEC_SX1262_BUSY_PIN 13

#if defined(HELTEC_V3_USED)
    #define SX1262_NSS_PIN HELTEC_SX1262_NSS_PIN
    #define SX1262_DIO_PIN HELTEC_SX1262_DIO0_PIN
    #define SX1262_RESET_PIN HELTEC_SX1262_RESET_PIN
    #define SX1262_BUSY_PIN HELTEC_SX1262_BUSY_PIN
    //#define SX1262_EXPLICIT_SPI_INIT 0
#else
    #define SX1262_NSS_PIN WW_SX1262_CS
    #define SX1262_DIO_PIN WW_SX1262_DIO1
    #define SX1262_RESET_PIN WW_SX1262_RESET
    #define SX1262_BUSY_PIN WW_SX1262_BUSY
    //#define SX1262_EXPLICIT_SPI_INIT 1
#endif

namespace LoRaHelper {

    // Forward declaration
    class SX1262_helper;

    // Callback type for pre-transmission updates (e.g., timestamps)
    typedef void (*PreTransmitCallback_t)(SX1262_helper* radio, byte* data, uint8_t length);

    typedef enum {
        STATE_UNINITIALIZED,
        STATE_IDLE,
        STATE_RX_RECEIVING,
        STATE_TX_CAD_1,
        STATE_TX_DELAY_RX,
        STATE_TX_CAD_2,
        STATE_TX_TRANSMITTING
    } radioState_t;

    typedef enum {
        PROGRESS_TX,
        PROGRESS_TX_COMPLETE,
        PROGRESS_RX_COMPLETE,

        ERR_UNKNOWN,
        ERR_UNITIALIZED,
        ERR_TX_CAD_DETECTED,
        ERR_TX_RECEIVED_PACKET,
        ERR_RX_TIMEOUT,
        ERR_RX_HEADER_INVALID,
        ERR_INVALID_STATE,
        ERR_UNHANDLED_INTERRUPT,
    } progress_t;


    extern const ChannelScanConfig_t defaultChannelScanConfig;

    class SX1262_helper : public SX1262 {
        public:
            SX1262_helper(Module* mod) : SX1262(mod) {}

            void initialize();
            bool startPoliteTransmit(const byte *data, uint8_t length, PreTransmitCallback_t callback = nullptr);
            bool startIdleReceive();
            progress_t handlePendingIRQ();
            static void haltIfError(int radiolibState);
            static void debugIfError(int radiolibState);
            bool isInterruptPending() const { return interruptIsPending; }
            radioState_t getCurrentState() const { return currentState; }
            byte* popDataFromInstanceBuffer(uint8_t& outLength);
            void printStatusRegister();
            
        private:
            bool interruptIsPending = false;
            radioState_t currentState;
            
            byte* _tx_data = nullptr;
            uint8_t _tx_length = 0;
            PreTransmitCallback_t _preTransmitCallback = nullptr;

            byte* _rx_data = nullptr;
            uint8_t _rx_length = 0;

            void finishPoliteTransmit();
            progress_t handleRXDoneIRQ(bool error);
            progress_t handleTXDoneIRQ();
            progress_t handleCADDoneIRQ(bool channelActivityDetected);
            progress_t handleTimeoutIRQ();
            void addDataToInstanceBuffer(byte* packetData, size_t packetLength, int radiolibState);
            static uint32_t msToSX1262TimeoutValue(uint32_t ms);
            static inline std::vector<SX1262_helper*> s_instances{};
            static void DIO0Trampoline();
            void DIO0Callback();
    };
}