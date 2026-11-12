#pragma once
#include "stdint.h"

class Packet;

typedef enum {
    COMMAND_ADD_PACKET_TO_SEND_QUEUE,
    COMMAND_DELETE_PACKET_FROM_SEND_QUEUE,
    COMMAND_SEND_RESET_COMMAND_PACKET,
    COMMAND_SEND_UPDATE_RECEIVER_ID_COMMAND_PACKET,
    COMMAND_USE_MASTER_NODE_HANDLER,
    COMMAND_RESET_NODE,
    COMMAND_UPDATE_RECEIVER_ID,
} CommandType_t;

typedef struct {
    uint16_t targetNodeId;
    uint16_t commandData;
} SendCommandContent_t;

class ICommandHandler {
    public:
        virtual void handleCommand(const CommandType_t type, const uint8_t* data, const uint8_t len) = 0;
};