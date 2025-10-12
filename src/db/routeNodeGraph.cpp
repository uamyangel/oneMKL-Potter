#include "routeNodeGraph.h"
#include <math.h>

bool RouteNodeGraph::isAccessible(const RouteNode* childRNode, const Connection& connection) 
{
	// only used for INT's node (TODO: add assertion)
    // if (accessibleWireOnlyIfAboveBelowTarget.find(childRNode->getWireId()) == accessibleWireOnlyIfAboveBelowTarget.end()) {
	if (!childRNode->getIsAccesibleWire()) {
        return true;
    }

    int childX = childRNode->getBeginTileXCoordinate();
    int childY = childRNode->getBeginTileYCoordinate();
    // if (connection.isCrossSLR() && nextLagunaColumn[childX] == childX) {
    //     // Connection crosses SLR and this is a Laguna column
    //     return true;
    // }

	int sinkX = connection.getSinkRNode()->getBeginTileXCoordinate();
	int sinkY = connection.getSinkRNode()->getBeginTileYCoordinate();
    if (childX != sinkX) {
        return false;
    }
    return std::abs(childY - sinkY) <= 1;
}