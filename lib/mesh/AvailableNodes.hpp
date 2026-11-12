#pragma once
#include <map>

#include "TimeUtils.hpp"
#include "debug_helper.hpp"

namespace MeshUtils{

    const float SNR_PERFECT = -5.0;   // Above this, the link is effectively perfect
    const float SNR_LIMIT   = -12.0;  // Below this, packet loss starts rising fast (for SF9)
    const float RSSI_LIMIT  = -115.0; // Below this, we are near the sensitivity floor

    // Weights
    const int WEIGHT_HOP = 50;        // Cost of 1 hop (lower = hops are cheaper)
    const int PENALTY_WEAK_SNR = 40;  // Penalty for risky SNR
    const int PENALTY_BAD_SNR  = 100; // Penalty for critical SNR

    typedef struct {
        uint16_t nodeId;
        float avgRssi;
        float avgSnr;
        uint8_t receivedPackets;
        uint8_t hopCount;
        time_t lastSeen;
        TimeUtils::WakeCycleConfig_t wakeCycleConfig;
    } NodeInRange_t;


    class AvailableNodes
    {
        public:
            AvailableNodes() {}
            void addOrUpdateNode(uint16_t nodeId, float rssi, float snr);
            void addOrUpdateNode(uint16_t nodeId, float rssi, float snr, uint8_t hopCount, TimeUtils::WakeCycleConfig_t wakeCycleConfig);


            void removeOldNodes(uint32_t maxAgeS);
            void printNodes();
            int calculateRouteCost(NodeInRange_t* node);
            NodeInRange_t getCheapestRouteNode();
            bool hasRecentNode(uint32_t maxAgeS);
        private:
            std::map<uint16_t, NodeInRange_t*> nodes;
    };
}