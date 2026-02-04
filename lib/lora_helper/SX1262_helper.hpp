#pragma once
#include <RadioLib.h>

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
                                         
#define LORA_POLITE_SEND_MIN_BACKOFF_MS 40
#define LORA_POLITE_SEND_BACKOFF_MULTIPLIER 1.5

#define NSS_PIN 5
#define RESET_PIN 14
#define DIO0_PIN 2

#define HELTEC_NSS_PIN 8
#define HELTEC_RESET_PIN 12
#define HELTEC_DIO0_PIN 14
#define HELTEC_BUSY_PIN 13

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
            static inline std::vector<SX1262_helper*> s_instances{};
            static void DIO0Trampoline();
            void DIO0Callback();
    };
}