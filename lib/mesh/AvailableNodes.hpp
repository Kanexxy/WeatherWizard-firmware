#pragma once
#include "debug_helper.hpp"
#include <map>

namespace MeshUtils{

    typedef struct {
        uint16_t nodeId;
        float rssi;
        float snr;
        unsigned long lastSeen;
    } NodeInRange_t;


    class AvailableNodes
    {
        public:
            AvailableNodes() {}
            void addOrUpdateNode(uint16_t nodeId, float rssi, float snr);
            NodeInRange_t* getNode(uint16_t nodeId);
            void removeOldNodes(uint16_t maxAgeS = 360);
            void printNodes();
        private:
            std::map<uint16_t, NodeInRange_t> nodes;
    };

}