#include "SX1262_helper.hpp"

namespace LoRaHelper {
    
    const ChannelScanConfig_t defaultChannelScanConfig = {
        .cad = {
            .symNum   = RADIOLIB_SX126X_CAD_ON_8_SYMB,
            .detPeak  = RADIOLIB_SX126X_CAD_PARAM_DEFAULT,
            .detMin   = RADIOLIB_SX126X_CAD_PARAM_DEFAULT,
            .exitMode = RADIOLIB_SX126X_CAD_GOTO_RX,
            .timeout  = 0,
            .irqFlags = RADIOLIB_IRQ_CAD_DEFAULT_FLAGS | (1UL << RADIOLIB_IRQ_RX_DONE),
            .irqMask  = RADIOLIB_IRQ_CAD_DEFAULT_MASK  | (1UL << RADIOLIB_IRQ_RX_DONE),
        },
    };
    
    void SX1262_helper::haltIfError(int state) {
        if (state != RADIOLIB_ERR_NONE) {
            S_DEBUGF(" failed, code %d\n", state);
            while (true);
        }
    }

    void SX1262_helper::debugIfError(int state) {
        if (state != RADIOLIB_ERR_NONE) {
            S_DEBUGF(" failed, code %d\n", state);
        }
    }

    void SX1262_helper::addDataToInstanceBuffer(byte* packetData, size_t packetSize, int radiolibState) {
        debugIfError(radiolibState);
        if (radiolibState != RADIOLIB_ERR_NONE) return;
        this->_rx_data = packetData;
        this->_rx_length = packetSize;
    }

    byte* SX1262_helper::popDataFromInstanceBuffer(uint8_t& outLength) {
        outLength = this->_rx_length;
        byte* data = this->_rx_data;
        this->_rx_data = nullptr;
        this->_rx_length = 0;
        return data;
    }

    void SX1262_helper::DIO0Trampoline() {
        for(auto* inst : s_instances) {
            if(inst != nullptr) {
                inst->DIO0Callback();
            }
        }
        return;
    }

    void SX1262_helper::DIO0Callback() {
        this->interruptIsPending = true;
    }

    progress_t SX1262_helper::handlePendingIRQ() {
        interruptIsPending = false;
        auto irqFlags = this->getIrqFlags();
        
        progress_t progress = ERR_UNKNOWN;
        switch (irqFlags){
            case RADIOLIB_SX126X_IRQ_CAD_DONE:
                progress = handleCADDoneIRQ(false);
                break;

            case RADIOLIB_SX126X_IRQ_CAD_DONE | RADIOLIB_SX126X_IRQ_CAD_DETECTED:
                progress = handleCADDoneIRQ(true);
                break;

            case RADIOLIB_SX126X_IRQ_TIMEOUT:
                progress = handleTimeoutIRQ();
                break;

            case RADIOLIB_SX126X_IRQ_RX_DONE:
                progress = handleRXDoneIRQ(false);
                break;

            case RADIOLIB_SX126X_IRQ_RX_DONE | RADIOLIB_SX126X_IRQ_HEADER_VALID:
                progress = handleRXDoneIRQ(false);
                break;

            case RADIOLIB_SX126X_IRQ_RX_DONE | RADIOLIB_SX126X_IRQ_HEADER_ERR:
                progress = handleRXDoneIRQ(true);
                break;
                
            case RADIOLIB_SX126X_IRQ_TX_DONE:
                progress = handleTXDoneIRQ();
                break;
            
            default:
                S_DEBUGF("Unhandled IRQ: %lu\n", irqFlags);
                progress = ERR_UNHANDLED_INTERRUPT;
                break;
        }

        if (this->interruptIsPending == false) this->clearIrqFlags(irqFlags);
        return progress;
    }

    progress_t SX1262_helper::handleRXDoneIRQ(bool error){

        if (error){
            S_DEBUGF("RX Done IRQ with other, could only really be LORA_HEADER_INVALID\n");
            this->currentState = STATE_IDLE;
            return ERR_RX_HEADER_INVALID;
        }

        auto packetSize = this->getPacketLength();
        uint8_t *packetData = new uint8_t[packetSize];
        int radiolibState = this->readData(packetData, packetSize);

        for (uint8_t i = 0; i < packetSize; i++){
            S_DEBUGF("%02X ", packetData[i]);
        }

        progress_t progress = ERR_UNKNOWN;
        switch (this->currentState) {

            case STATE_RX_RECEIVING:
                S_DEBUGF("RX Done in RX_RECEIVING state.\n");
                addDataToInstanceBuffer(packetData, packetSize, radiolibState);
                this->currentState = STATE_IDLE;
                progress = PROGRESS_RX_COMPLETE;
                break;
            
            case STATE_TX_DELAY_RX:{
                S_DEBUGF("RX Done in TX_DELAY_RX state.\n");
                addDataToInstanceBuffer(packetData, packetSize, radiolibState);
                finishPoliteTransmit();
                this->currentState = STATE_IDLE;
                progress = ERR_TX_RECEIVED_PACKET;
                break;
            }
            default:
                S_DEBUGF("RX Done in unexpected state %d.\n", this->currentState);
                progress = ERR_INVALID_STATE;
                delete[] packetData;
                packetData = nullptr;
                this->_rx_data = nullptr;
                this->_rx_length = 0;
                break;
        }
        return progress;
    }
    
    progress_t SX1262_helper::handleTXDoneIRQ(){
        progress_t progress;
        switch (currentState) {
            case STATE_TX_TRANSMITTING:{
                S_DEBUGF("TX Done in TX_TRANSMITTING state.\n");
                finishPoliteTransmit();
                currentState = STATE_IDLE;
                progress = PROGRESS_TX_COMPLETE;
                delete [] _tx_data;
                _tx_data = nullptr;
                _tx_length = 0;
                break;
            }
            default:
                S_DEBUGF("TX Done in unexpected state %d.\n", currentState);
                progress = ERR_INVALID_STATE;
                break;
        }
        return progress;
    }

    progress_t SX1262_helper::handleCADDoneIRQ(bool channelActivityDetected){
        
        progress_t progress = ERR_UNKNOWN;

        if (channelActivityDetected){
                    S_DEBUGF("Channel activity detected during CAD. Aborting polite transmit.\n");
                    finishPoliteTransmit();
                    currentState = STATE_RX_RECEIVING;
                    progress = ERR_TX_CAD_DETECTED;
                    //check what radio is doing
                    return progress;
        }

        switch (currentState) {
            case STATE_TX_CAD_1:{
                uint8_t rxDelay = this->random(LORA_POLITE_SEND_MIN_BACKOFF_MS, LORA_POLITE_SEND_MAX_BACKOFF_MS);
                S_DEBUGF("CAD Done in TX_CAD_1 state. Proceeding to RX delay with timeout %d ms\n", rxDelay);
                debugIfError(this->startReceive(msToSX1262TimeoutValue(rxDelay)));
                this->currentState = STATE_TX_DELAY_RX;
                progress = PROGRESS_TX;
                break;
            }
            case STATE_TX_CAD_2: {
                S_DEBUGF("CAD Done in TX_CAD_2 state. Proceeding to transmit (%d Bytes).\n", this->_tx_length);
                // Call pre-transmit callback if set (for updating timestamps, time-on-air, etc.)
                if (this->_preTransmitCallback != nullptr) this->_preTransmitCallback(this, this->_tx_data, this->_tx_length);
                debugIfError(this->startTransmit((const byte*) this->_tx_data, this->_tx_length));
                this->currentState = STATE_TX_TRANSMITTING;
                progress = PROGRESS_TX;
                break;
            }
            
            default:{
                S_DEBUGF("CAD Done in unexpected state %d.\n", currentState);
                progress = ERR_INVALID_STATE;
                break;
            }
        }
        return progress;
    }

    progress_t SX1262_helper::handleTimeoutIRQ(){
        progress_t progress = ERR_UNKNOWN;
        switch (currentState) {
            
            case STATE_RX_RECEIVING:{
                S_DEBUGF("Timeout in RX_RECEIVING state.\n");
                this->currentState = STATE_IDLE;
                progress = ERR_RX_TIMEOUT;
                break;
            }
            
            case STATE_TX_DELAY_RX:{
                S_DEBUGF("Timeout in TX_DELAY_RX state. Starting second CAD before transmit...\n");
                this->standby(); // Go to standby before issuing new commands
                debugIfError(this->startChannelScan(defaultChannelScanConfig));
                S_DEBUGF("Second CAD started successfully.\n");
                this->currentState = STATE_TX_CAD_2;
                progress = PROGRESS_TX;
                break;
            }

            default:{
                S_DEBUGF("Timeout in unexpected state %d.\n", currentState);
                progress = ERR_INVALID_STATE;
                break;
            }
        }
        return progress;
    }

    bool SX1262_helper::startPoliteTransmit(const byte *data, uint8_t length, PreTransmitCallback_t callback) {
        if (currentState != STATE_IDLE && currentState != STATE_RX_RECEIVING) {
            S_DEBUGF("Radio not idle or in RX, cannot send politely.\n");
            return false;
        }

        if (_tx_data != nullptr) {
            S_DEBUGF("It seems like a polite transmit is already in progress.\n");
            return false;
        }

        _tx_data = (byte*)data;
        _tx_length = length;
        _preTransmitCallback = callback;
        
        S_DEBUGF("Starting first CAD before polite send...\n");
        debugIfError(this->startChannelScan(defaultChannelScanConfig));
        currentState = STATE_TX_CAD_1;
        S_DEBUGF("Send started successfully.\n");
        return true;
    }

    void SX1262_helper::finishPoliteTransmit() {
        if (_tx_data == nullptr) return;
        delete[] _tx_data;
        this->_tx_data = nullptr;
        _tx_length = 0;
        _preTransmitCallback = nullptr;
        return;
    }

    void SX1262_helper::initialize(){
        //dont initialize SPI if using Heltec board, because it uses the default SPI pins
        //#if !defined(ARDUINO_HELTEC_WIFI_LORA_32_V3)
        // SPI.begin(WW_SX1262_SCK, WW_SX1262_MISO, WW_SX1262_MOSI, WW_SX1262_CS);
        //#endif

        S_DEBUGF("[LoRaHelper Init]\n\tInitializing SX1262...\n");
        haltIfError(this->begin());

        S_DEBUGF("\tSetting frequency...\n");
        haltIfError(this->setFrequency(LORA_FREQUENCY));

        S_DEBUGF("\tSetting Spreading factor...\n");
        haltIfError(this->setSpreadingFactor(LORA_SPREADING_FACTOR));

        S_DEBUGF("\tSetting Bandwidth...\n");
        haltIfError(this->setBandwidth(LORA_BANDWIDTH));

        S_DEBUGF("\tSetting coding rate...\n");
        haltIfError(this->setCodingRate(LORA_CODING_RATE));

        S_DEBUGF("\tSetting sync word...\n");
        haltIfError(this->setSyncWord(RADIOLIB_SX126X_SYNC_WORD_PRIVATE));

        S_DEBUGF("\tSetting output power...\n");
        haltIfError(this->setOutputPower(LORA_OUTPUT_POWER));

        S_DEBUGF("\tSetting Preamble length...\n");
        haltIfError(this->setPreambleLength(LORA_PREAMBLE_LENGTH));

        S_DEBUGF("\tSetting RX Callback function...\n");
        this->setDio1Action(DIO0Trampoline);
        
        //Add this instance to the static instances vector if not already present
        if(std::find(s_instances.begin(), s_instances.end(), this) == s_instances.end()) {
            s_instances.push_back(this);
        }
        
        this->currentState = STATE_IDLE;
        this->interruptIsPending = false;

        if (this->_tx_data != nullptr && this->_tx_length != 0){
            delete[] this->_tx_data;
            this->_tx_data = nullptr;
            this->_tx_length = 0;
        }

        if (this->_rx_data != nullptr && this->_rx_length != 0){
            delete[] this->_rx_data;
            this->_rx_data = nullptr;
            this->_rx_length = 0;
        }
        S_DEBUGF("\tInitialization completed successfully.\n");
    }

    uint32_t SX1262_helper::msToSX1262TimeoutValue(uint32_t ms) {
        // SX1262 timeout value is in units of 15.625 us
        return (ms * 1000) / 15.625;
    }

    bool SX1262_helper::startIdleReceive() {
        if (currentState != STATE_IDLE) {
            S_DEBUGF("Radio not idle, cannot start receive.\n");
            return false;
        }

        debugIfError(this->startReceive());
        currentState = STATE_RX_RECEIVING;
        S_DEBUGF("Receive started successfully.\n");
        return true;
    }

    void SX1262_helper::printStatusRegister(){
        uint8_t status = this->getStatus();
        //Output with bit masking according to datasheet
        S_DEBUGF("SX1262 Status Register: 0x%02X Command Status: 0x%01X, Chip Mode: 0x%01X\n", status, (status & 0b00001110) >> 1, (status & 0b01110000) >> 4);
    }
}