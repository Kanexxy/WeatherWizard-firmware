#include "PacketQueue.hpp"

namespace Packets {
    
    void PacketQueue::newCycle() {
        for (auto& pair : *this) {
            pair.second->hasBeenSentInCycle = false;
        }
    }

    Packet* PacketQueue::next() {
        for (auto& pair : *this) {
            if (!pair.second->hasBeenSentInCycle) {
                return pair.second;
            }
        }
        return nullptr;
    }
}