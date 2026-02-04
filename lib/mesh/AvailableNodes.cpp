#include "AvailableNodes.hpp"

namespace MeshUtils{

    void AvailableNodes::addOrUpdateNode(uint16_t nodeId, float rssi, float snr) {
        time_t currentTime = TimeUtils::getCurrentTime();
        try{
            NodeInRange_t* node = this->nodes.at(nodeId);
            (node->avgRssi) = ((node->avgRssi * node->receivedPackets) + rssi) / (node->receivedPackets + 1);
            (node->avgSnr) = ((node->avgSnr * node->receivedPackets) + snr) / (node->receivedPackets + 1);
            node->receivedPackets++;
            node->lastSeen = currentTime;
        }
        catch (const std::out_of_range& _){
            // Node not found, will add new
            NodeInRange_t* newNode = new NodeInRange_t{nodeId, rssi, snr, 1, 255, currentTime, {0,0}};
            nodes[nodeId] = newNode;
        }
    }

    void AvailableNodes::addOrUpdateNode(uint16_t nodeId, float rssi, float snr, uint8_t hopCount, TimeUtils::WakeCycleConfig_t wakeCycleConfig) {
        this->addOrUpdateNode(nodeId, rssi, snr);
        this->nodes[nodeId]->hopCount = hopCount;
        this->nodes[nodeId]->wakeCycleConfig = wakeCycleConfig;
    }

    void AvailableNodes::removeOldNodes(uint32_t maxAgeS) {
        time_t currentTime = TimeUtils::getCurrentTime();
        for (auto it = nodes.begin(); it != nodes.end(); ) {
            NodeInRange_t* node = it->second;
            if ((currentTime - node->lastSeen) > maxAgeS) {
                delete node;
                it = nodes.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool AvailableNodes::hasRecentNode(uint32_t maxAgeS) {
        time_t currentTime = TimeUtils::getCurrentTime();
        for (const auto& pair : nodes) {
            NodeInRange_t* node = pair.second;
            if ((currentTime - node->lastSeen) <= maxAgeS) {
                return true;
            }
        }
        return false;
    }

    void AvailableNodes::printNodes() {
        S_DEBUGF("Nodes in range:\n");
        for (const auto& pair : nodes) {
            const NodeInRange_t *node = pair.second;
            S_DEBUGF("Node ID: %d, RSSI: %d dBm, SNR: %d, Last Seen: %lu seconds ago\n",
                     node->nodeId, node->avgRssi, node->avgSnr, (millis() - node->lastSeen)/1000);
        }
    }

    NodeInRange_t AvailableNodes::getCheapestRouteNode() {
        NodeInRange_t bestNode;
        memset(&bestNode, 0, sizeof(NodeInRange_t));
        int bestScore = INT32_MAX;

        for (const auto& pair : nodes) {
            NodeInRange_t* node = pair.second;
            if (node->hopCount >= 254) continue; // Skip full or uninitialized nodes
            int score = calculateRouteCost(node);
            if (score < bestScore) {
                bestScore = score;
                bestNode = *node;
            }
        }
        return bestNode;
    }

    int AvailableNodes::calculateRouteCost(NodeInRange_t* node) {
        if (node->hopCount >= 254) return INT32_MAX; //Node full or uninitialized
        int score = 0;

        // 1. BASE COST: Hop Count
        // We multiply by a weight so we can add finer-grained penalties later.
        score += (node->hopCount * WEIGHT_HOP);


        // 2. SIGNAL QUALITY PENALTIES (The "Cliff" Logic)
        
        // --- SNR CHECK (Primary Indicator) ---
        if (node->avgSnr >= SNR_PERFECT) {
            // Excellent link (e.g., > -5dB). No penalty.
            score += 0; 
        } 
        else if (node->avgSnr >= SNR_LIMIT) {
            // Usable but degrading (e.g., -5dB to -12dB).
            // We add a small linear penalty to prefer the stronger of two medium links.
            score += (int)((SNR_PERFECT - node->avgSnr) * 2); 
        } 
        else {
            // DANGER ZONE (e.g., < -12dB).
            // This link is near the limit. Only use it if it saves us A LOT of hops.
            score += PENALTY_BAD_SNR;
            
            // Add exponential penalty for every dB below the limit
            score += (int)((SNR_LIMIT - node->avgSnr) * 10); 
        }


        // --- RSSI CHECK (Secondary Indicator) ---
        // Only matters if we are very weak.
        if (node->avgRssi < RSSI_LIMIT) {
            // We are hitting the sensitivity floor (e.g., -120dBm).
            // Add a penalty even if SNR looks okay (ghost signals).
            score += PENALTY_WEAK_SNR;
        }

        return score;
    }
}