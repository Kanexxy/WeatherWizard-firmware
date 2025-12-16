#include "AvailableNodes.hpp"

namespace MeshUtils{

    void AvailableNodes::addOrUpdateNode(uint16_t nodeId, float rssi, float snr) {
        unsigned long currentTime = millis();
        auto it = nodes.find(nodeId);
        if (it != nodes.end()) {
            // Update existing node
            it->second.rssi = rssi;
            it->second.snr = snr;
            it->second.lastSeen = currentTime;
        } else {
            // Add new node
            NodeInRange_t newNode = {nodeId, rssi, snr, currentTime};
            nodes[nodeId] = newNode;
        }
    }

    void AvailableNodes::removeOldNodes(uint16_t maxAgeS) {
        unsigned long currentTime = millis();
        for (auto it = nodes.begin(); it != nodes.end(); ) {
            if (currentTime - it->second.lastSeen > maxAgeS * 1000) {
                S_DEBUGF("Removing node %d due to inactivity\n", it->first);
                nodes.erase(it++);
            } else {
                ++it;
            }
        }
    }

    NodeInRange_t* AvailableNodes::getNode(uint16_t nodeId) {
                auto it = this->nodes.find(nodeId);
                if (it != this->nodes.end()) {
                    return &it->second;
                }
                return nullptr;
            }

    void AvailableNodes::printNodes() {
        S_DEBUGF("Nodes in range:\n");
        for (const auto& pair : nodes) {
            const NodeInRange_t& node = pair.second;
            S_DEBUGF("Node ID: %d, RSSI: %d dBm, SNR: %d, Last Seen: %lu seconds ago\n",
                     node.nodeId, node.rssi, node.snr, (millis() - node.lastSeen)/1000);
        }
    }

}