#include "PacketQueue.hpp"

namespace Packets {
    
    void PacketQueue::newCycle() {
        for (auto& pair : *this) {
            pair.second->info.hasBeenSentInCycle = false;
        }
    }

    void PacketQueue::add(Packet* packet) {
        (*this)[packet->getGlobalPacketIdentifier()] = packet;
    }

    Packet* PacketQueue::next() {
        for (auto& pair : *this) {
            if (!pair.second->info.hasBeenSentInCycle) {
                return pair.second;
            }
        }
        return nullptr;
    }

    void PacketQueue::addPacketsFromList(std::list<Packet*>* packets) {
        for (Packet* packet : *packets) {
            this->add(packet);
        }
    }

    void PacketQueue::removePacketsFromIdentifierList(const std::list<uint32_t>& identifiers)
    {
        for (auto it = this->begin(); it != this->end(); ) {
            if (std::find(identifiers.begin(), identifiers.end(), it->first) != identifiers.end()) {
                S_DEBUGF("Removing acknowledged packet %.8X from send queue\n", it->first);
                delete it->second;
                it = this->erase(it);
            } else {
                ++it;
            }
        }
    }
}