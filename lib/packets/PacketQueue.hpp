#pragma once

#ifdef __AVR__
    #include <ArduinoSTL.h>
    #include <unordered_map>
#else
    #include <unordered_map>
#endif

#include "Packets.hpp"

namespace Packets {

    class Packet;
    class PacketQueue : public std::unordered_map<uint32_t, Packet*> {
        public:
            PacketQueue() = default;
            void newCycle();
            Packet* next();
    };
}