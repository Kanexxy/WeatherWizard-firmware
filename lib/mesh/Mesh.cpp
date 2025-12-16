#include "Mesh.hpp"

namespace MeshUtils{

    uint16_t nodeID = 0; //written to EEPROM at programming, read at initialization
    uint16_t nextHopID = 0; //ID of the next hop towards the gateway, 0 if unknown / uninitialized
    bool hasConnection = false; //true if a connection to the gateway is known
    bool isGateway = false;

    AvailableNodes availableNodes;

    void initializeMeshConnection(){
        
    }
}