#include "aStarRoute.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <thread>
#include <chrono>
#include <fstream>
#include <filesystem>
#include "utils/MTStat.h"
#include "db/routeResult.h"
#include "utils/mkl_utils.h"

/**
 * @brief The entrance of routing process
 * 
 */
void aStarRoute::route() 
{
	utils::timer baseTimer; baseTimer.start();
	routeIndirectConnections();
	std::cout << "Route indirect time: " << std::fixed << std::setprecision(2) << baseTimer.elapsed() << std::endl;
	if (!database.useRW) {
		routeDirectConnections();
		saveAllRoutingSolutions();
		std::cout << "Total route time: " << std::fixed << std::setprecision(2) << baseTimer.elapsed() << std::endl;
		// database.checkRoute();
	}
}

/**
 * @brief Iteratively route indirect connection and fix all congestions. 
 * The distinction between indirect/direct connections follows RWRoute's approach.
 * Indirect connections are regular connections. Their routing path begins at the source CLB, traverses through INT tiles, and ultimately terminates at the target CLB.
 * Direct connections​ are connections that do not require nodes on INT tiles (such as carry chains). As a result, they have a much smaller routing search space and do not constitute a major part of the routing process.
 */
void aStarRoute::routeIndirectConnections()
{
	log() << "Route indirect connections: " << database.numConns << std::endl;

	// Pre-Process
	updateIndirectConnectionBBox(xMargin, yMargin);
	sortConnections();
	updateSinkNodeUsage();

	if (useParallel) {
		// net scheduling
		partition();

		/* build RPTT for post-processing */
		utils::BoxT<int> bbox;
		for (auto& conn: database.indirectConnections) {
			bbox = bbox.UnionWith(conn.getBBox());
		}
		log() << "Connections bbox: " << bbox << std::endl;
		partitionTree = new PartitionTree(database.indirectConnections, sortedConnectionIds, bbox);
		partitionTree->calculateNodeLevel();
		scheduledLevel = partitionTree->schedule(partitionTree->getLeafNodes());
		log() << "Schedule Level: " << scheduledLevel << std::endl;
	}

	log() << "---------------------------------------------------------------------------------------------------------------------------" << std::endl;
	log() << std::setw(10) << "Iteration" << std::setw(15) << "PFactor" << std::setw(10) << "HFactor" << std::setw(20) << "RoutedConnections" << std::setw(15) << "OverlapNodes" << std::setw(15) << "decreaseRatio" << std::setw(13) << "shareRatio" << std::setw(15) << "numBatches" << std::setw(8) << "Times" << std::endl;
	utils::timer timer;
	float congestRatio = 0;
	int decreaseOfCongestedNodes = 0;
	double decreaseRatio = 0;
	int lastOverusedNodeNum = 0;
	float lastShareRatio = -10000;
	float shareRatio = 0;
	for (iter = 1; iter < maxIter; iter ++) {
		timer.start();
		connectionIdBase += routedConnectionNum + 1;
		routedConnectionNum = 0;
		failRouteNum = 0;
		string labelRouteType = " ";
		if (useParallel) {
			float shareIncreaseRatio = (shareRatio - lastShareRatio) * 1.0 / lastShareRatio;
			if (useOverlapRouting && (iter <= 3 || (decreaseRatio > 0.2 && shareIncreaseRatio > 0.15))) {
				if (!isRuntimeFirst) {
					stableFirstParallelRouting();
				} else {
					runtimeFirstParallelRouting();
				}
				
				labelRouteType = "*";
			}
			else {
				/* post-processing */
				routePartitionTree(partitionTree);
				useOverlapRouting = false;
			}
				
		} else {
			// single-threaded mode
			for (int connectionId : sortedConnectionIds) {
				auto& connection = database.indirectConnections[connectionId];
				if (shouldRoute(connection)) {
					ripup(connection, false);
					bool success = routeOneConnection(connectionId, 0, false);
					if (!success) {
						failRouteNum ++;
						auto& connection = database.indirectConnections[connectionId];
						log() << "Routing failure. Connection "<< connection << " Coordinate: [" << connection.getSourceRNode()->getEndTileXCoordinate() << " " << connection.getSinkRNode()->getEndTileXCoordinate() << " " << connection.getSourceRNode()->getEndTileYCoordinate() << " " << connection.getSinkRNode()->getEndTileYCoordinate() << "] " << std::endl;
					}
				}
			}
		}
		if (iter == 1) {
			/* determine the congested design based on the ratio of overused rnode number to the number of connections */ 
			int overUseCnt = 0;
        	for (int i = 0; i < database.numNodes; i ++)
        	    if (database.routingGraph.routeNodes[i].getOccupancy() > 1)
					overUseCnt ++;
			congestRatio = overUseCnt * 1.0 / database.numConns;
			if (congestRatio > 0.45) // 0.5 -> 0.45 for new cost function
				isCongestedDesign = true;
		}

		// update congestion cost
		dynamicCostFactorUpdating(isCongestedDesign);

		// compute statistics
		decreaseOfCongestedNodes = lastOverusedNodeNum - numOverUsedRNodes.load();
		decreaseRatio = decreaseOfCongestedNodes * 1.0 / lastOverusedNodeNum;
		lastShareRatio = shareRatio;
		shareRatio = routedConnectionNum * 1.0 / numOverUsedRNodes.load();
		lastOverusedNodeNum = numOverUsedRNodes.load();
		log() << labelRouteType << std::setw(9) << iter << std::setw(15) << presentCongestionFactor << std::setw(10) << historicalCongestionFactor << std::setw(20) << routedConnectionNum << std::setw(15) << numOverUsedRNodes.load() << std::fixed << std::setw(15) << std::setprecision(2) << decreaseRatio << std::setw(13) << std::setprecision(2) << shareRatio << std::setw(15) << numBatches << std::setw(8) << std::setprecision(2) << timer.elapsed() << std::endl;

		if (numOverUsedRNodes.load() == 0 && failRouteNum == 0)
			break;
	}
	log() << "---------------------------------------------------------------------------------------------------------------------------" << std::endl;
	log() << "Congest ratio: " << congestRatio << " label: " << isCongestedDesign << std::endl;

	if (useOverlapRouting) {
		delete partitionTree;
		if (treeForLabeledNets != NULL) {
			delete treeForLabeledNets;
		}
	}
}

/**
 * @brief Route direct connections. 
 * The distinction between indirect/direct connections follows RWRoute's approach.
 * Indirect connections are regular connections. Their routing path begins at the source CLB, traverses through INT tiles, and ultimately terminates at the target CLB.
 * Direct connections​ are connections that do not require nodes on INT tiles (such as carry chains). As a result, they have a much smaller routing search space and do not constitute a major part of the routing process.
 */
void aStarRoute::routeDirectConnections()
{
	log() << "Route direct connections: " << database.directConnections.size() << std::endl;
	int failCnt = 0;
	for (auto& conn : database.directConnections) {
		auto source = conn.getSourceRNode(); 
		auto sink = conn.getSinkRNode();
		for (auto rnodeId : database.device.get_outgoing_nodes(source->getId())) {
			auto rnode = &database.routingGraph.routeNodes[rnodeId];
			if (rnode == sink) {
				conn.addRNode(sink);
				conn.addRNode(source);
				break;
			}
		}
		if (conn.getRNodeSize() != 0) continue;

		std::queue<RouteNode*> q;
		unordered_map<RouteNode*, RouteNode*> prevs;
		prevs[source] = nullptr;
		q.push(source);
        int watchdog = 10000;
        bool success = false;

        while (!q.empty()) {
            auto curr = q.front(); q.pop();
            if (curr == sink) {
                while (curr != nullptr) {
                    conn.addRNode(curr);
                    curr = prevs[curr];
                }
                success = true;
                break;
            }
            for (auto childId : database.device.get_outgoing_nodes(curr->getId())) {
				auto child = &database.routingGraph.routeNodes[childId];
                prevs[child] = curr;
                q.push(child);
            }
            watchdog --;
            if (watchdog < 0) {
                break;
            }
        }

        if (!success) {
			failCnt ++;
			// assert_t(watchdog = 9999 && q.size() == 0);
            log(LOG_ERROR) << "Failed to find a path for direct connection " << conn.getId() << " watchDog " << watchdog << " qSize " << q.size() 
				<< " #child " << source->getChildrenSize() << std::endl;
        }
	}
	log() << "Direct route [Finish]. Failure: " << failCnt << " / " << database.directConnections.size() << std::endl;
}

/**
 * @brief The entrance of net scheduling.
 * 
 */
void aStarRoute::partition() {
    if (!isRuntimeFirst) {
		labelNets();
        kmeansBasedPartition();
    } else {
        regionBasedPartition();
    }
}

/**
 * @brief Save the routing results and prepare for the following writing to output file
 * 
 */
void aStarRoute::saveAllRoutingSolutions() 
{
	log() << "Save all routing solutions [Start]" << std::endl;
	auto& rnodes = database.routingGraph.routeNodes;
	nodeRoutingResults.resize(rnodes.size());
	int fixedNetNum = 0; // multi-driver
	auto saveOneNet = [&](int netId) {
		const auto& net = database.nets[netId];
		std::set<RouteNode*> netRNodes;
		bool hasMultiDriver = false;
		for (int connId : net.getConnections()) {
			Connection& conn = database.indirectConnections[connId];
			const auto& connRNodes = conn.getRNodes();
			// source sitePin to source int node
			vector<obj_idx> totalPath = conn.getSourceToIntPath();
			assert_t(totalPath[0] == net.getIndirectSourcePin()); // TODO: move the checker to the database checking
			rnodes.back().setNodeType(WIRE); // recover
			rnodes[totalPath[0]].setNodeType(PINFEED_O); 
			// source int node to sink int node
			assert_t(totalPath.back() == connRNodes.back()->getId());
			assert_t(connRNodes.size() >= 2);
			for (int i = connRNodes.size() - 2; i >= 0; i --) 
				totalPath.emplace_back(connRNodes[i]->getId());
			// sink int node to sink sitePin
			vector<obj_idx> partialPath = conn.getIntToSinkPath();
			rnodes[partialPath[0]].setNodeType(WIRE); // recover
			rnodes[partialPath.back()].setNodeType(PINFEED_I);
			assert_t(partialPath[0] == totalPath.back());
			// assert_t(partialPath.size() >= 2);
			totalPath.insert(totalPath.end(), partialPath.begin() + 1, partialPath.end());

			for (int i = 0; i < totalPath.size(); i ++) { 
				auto cur = &rnodes[totalPath[i]];
				assert_t(nodeRoutingResults[cur->getId()].netId == -1 || nodeRoutingResults[cur->getId()].netId == netId);
				if (nodeRoutingResults[cur->getId()].netId != -1)
					hasMultiDriver = true;
				nodeRoutingResults[cur->getId()].netId = netId;
				netRNodes.emplace(cur);
				if (i != totalPath.size() - 1)
					nodeRoutingResults[cur->getId()].branches.emplace(&rnodes[totalPath[i+1]]);
			}
		}

		for (int connId : net.getDirectConnections()) {
			Connection& conn = database.directConnections[connId];
			conn.getSourceRNode()->setNodeType(PINFEED_O);
			conn.getSinkRNode()->setNodeType(PINFEED_I);
			// source int node to sink int node
			for (int i = conn.getRNodeSize() - 1; i >= 0; i --) {
				auto cur = conn.getRNode(i);
				assert_t(nodeRoutingResults[cur->getId()].netId == -1 || nodeRoutingResults[cur->getId()].netId == netId);
				if (nodeRoutingResults[cur->getId()].netId != -1)
					hasMultiDriver = true;
				nodeRoutingResults[cur->getId()].netId = netId;
				netRNodes.emplace(cur);
				if (i != 0)
					nodeRoutingResults[cur->getId()].branches.emplace(conn.getRNode(i - 1));
			}
		}
		if (hasMultiDriver) {
			for (RouteNode* rnode : netRNodes) {
				if (nodeRoutingResults[rnode->getId()].branches.size() == 0)
					assert_t(rnode->getNodeType() == PINFEED_I);
			}

			fixNetRoutes(net, netRNodes);
			fixedNetNum ++;
		}
	};
	runJobsMT(database.numNets, numThread, saveOneNet);
	log() << "FixedNetNum: " << fixedNetNum << " / " << database.nets.size() << std::endl;
	log() << "Save all routing solutions [Finish]" << std::endl;
}

/**
 * @brief Find shortest path and fix multi-driver issues. 
 * This router is a connection-based router. Thus, it's possible that there are multiple paths from node A to node B in the routing result of a net.
 * A way to find these issues is to do a DFS.
 * @param net the net to be fixed
 * @param netRNodes the nodes used in the routing solution of this net
 */
void aStarRoute::fixNetRoutes(const Net& net, std::set<RouteNode*>& netRNodes)
{
	unordered_map<RouteNode*, double> upStreamPathCost;
	unordered_set<RouteNode*> visited;
	unordered_map<RouteNode*, RouteNode*> prevs;
	auto rnodeComp = [&upStreamPathCost](RouteNode* lhs, RouteNode* rhs) {
        	return upStreamPathCost[rhs] < upStreamPathCost[lhs];
    };
    std::priority_queue<RouteNode*, vector<RouteNode*>, decltype(rnodeComp)> rnodeQueue(rnodeComp);
	// initialize 
	for (RouteNode* rnode : netRNodes) {
		visited.emplace(rnode);
		upStreamPathCost[rnode] = INT32_MAX;
		prevs[rnode] = nullptr;
	}
	assert_t(net.getConnectionSize() == 0 || net.getDirectConnectionSize() == 0);
	RouteNode* sourcePin = net.getIndirectSourcePinRNode();
	vector<RouteNode*> sinkPins = net.getIndirectSinkPinRNodes();
	if (netRNodes.find(sourcePin) == netRNodes.end()) {
		// there is no indirect connection
		assert_t(net.getConnectionSize() == 0);
		sourcePin = net.getDirectSourcePinRNode();
		sinkPins = net.getDirectSinkPinRNodes();
		assert_t(netRNodes.find(sourcePin) != netRNodes.end());
	}
	upStreamPathCost[sourcePin] = 0;
	rnodeQueue.push(sourcePin);

	int watchDog = 0;
	while(!rnodeQueue.empty()) {
		RouteNode* cur = rnodeQueue.top(); rnodeQueue.pop();
		// std::set<RouteNode*> children = cur->getBranch();
		if (nodeRoutingResults[cur->getId()].branches.empty())
			continue;
		if (watchDog++ > 1000000)
			assert_t(false);
		for (RouteNode* child : nodeRoutingResults[cur->getId()].branches) {
			double newCost = upStreamPathCost[cur] + child->getBaseCost(); // TODO: verify basecost or wirelength
			if (visited.find(child) == visited.end() || newCost < upStreamPathCost[child]) {
				upStreamPathCost[child] = newCost;
				prevs[child] = cur;
				visited.emplace(child);
				rnodeQueue.push(child);
			}
		}
	}
	// update rnode's branch
	for (RouteNode* rnode : netRNodes)
		nodeRoutingResults[rnode->getId()].branches.clear();
	// mark rnode and remove useless rnodes
	unordered_set<RouteNode*> inRoute;
	for (RouteNode* rnode : sinkPins) {
		assert_t(rnode->getNodeType() == PINFEED_I);
		int watchDog = 0;
		while (rnode != sourcePin) {
			assert_t(prevs[rnode] != nullptr);
			inRoute.emplace(rnode);
			rnode = prevs[rnode];
			if (watchDog++ > 1000000) 
				assert_t(false);
		}
	}
	for (RouteNode* rnode : netRNodes) {
		if (rnode != sourcePin && inRoute.find(rnode) != inRoute.end()) {
			// assert_t(rnode->getPrev() != nullptr);
			if(prevs[rnode] == nullptr) {
				std::cout << netRNodes.size() << " conns: " << net.getConnectionSize() << " " << net.getDirectConnectionSize() << std::endl;
			}
			nodeRoutingResults[prevs[rnode]->getId()].branches.emplace(rnode);
		}
	}
}

/**
 * @brief update the bounding box of an indirect connection
 * 
 * @param x_margin the margin in x-axis
 * @param y_margin the margin in y-axis
 */
void aStarRoute::updateIndirectConnectionBBox(int x_margin, int y_margin) {
	for (int netId = 0; netId < database.nets.size(); netId++) {
		Net& net = database.nets[netId];
		if (net.getConnectionSize() == 0)
			continue;
		
		net.setXMinBB(net.getXCenter());
		net.setXMaxBB(net.getXCenter());
		net.setYMinBB(net.getYCenter());
		net.setYMaxBB(net.getYCenter());

		for (int connId: net.getConnections()) {
			Connection& conn = database.indirectConnections[connId];
			int x_min = conn.getXMin() - x_margin;
			if (x_min < 0) x_min = -1;
			int x_max = conn.getXMax() + x_margin;
			if (x_max > database.layout.hx()) x_max = database.layout.hx();
			int y_min = conn.getYMin() - y_margin;
			if (y_min < 0) y_min = -1;
			int y_max = conn.getYMax() + y_margin;
			if (y_max > database.layout.hy()) y_max = database.layout.hy();
			conn.updateBBox(x_min, y_min, x_max, y_max);
			conn.computeHPWL();
			net.updateXMinBB(x_min);
			net.updateXMaxBB(x_max);
			net.updateYMinBB(y_min);
			net.updateYMaxBB(y_max);
		}
		net.setDoubleHpwl(std::max(0, 2 * (mkl_utils::scalar_abs(net.getYMaxBB() - net.getYMinBB() + 1) + mkl_utils::scalar_abs(net.getXMaxBB() - net.getXMinBB() + 1))));
	}
}

/**
 * @brief Increase the sink node usage for each connection.
 * This is to avoid the sink node of a net is used by another net and this congestion is not found.
 */
void aStarRoute::updateSinkNodeUsage()
{
	
	for (int connectionId : sortedConnectionIds) {
		auto& connection = database.indirectConnections[connectionId];
		auto rnode = connection.getSinkRNode();
		bool newlyAdd = database.nets[connection.getNetId()].incrementUser(rnode);
		if (newlyAdd)
			rnode->incrementOccupancy();
		rnode->updatePresentCongestionCost(presentCongestionFactor);
		if (rnode->getOccupancy() > 1 || database.nets[connection.getNetId()].countConnectionsOfUser(rnode) > 1)
			assert_t(false && "rnode is used by multiple connections");
	}

}

void aStarRoute::sortConnections() 
{
	sortedConnectionIds.clear();
	sortedConnectionIds.resize(database.numConns, 0);
	for (int i = 0; i < database.numConns; i ++)
		sortedConnectionIds[i] = i;

	auto compare = [this](const int& i, const int& j) {
		int si = this->database.nets[this->database.indirectConnections[i].getNetId()].getConnectionSize();
		int sj = this->database.nets[this->database.indirectConnections[j].getNetId()].getConnectionSize();
		if (si == sj) {
			return this->database.indirectConnections[i].getHPWL() < this->database.indirectConnections[j].getHPWL();
		} else {
			return si > sj;
		}
	}; // large-degree net first and then smaller hpwl first
	// std::sort(sortedConnectionIds.begin(), sortedConnectionIds.end(), compare); //TODO: remember to uncomment this when our parser is used
}

/**
 * @brief route a connection
 * 
 * @param connectionId id of the connection to be routed
 * @param tid id of the executing thread
 * @param sync true if synchronization barrier is needed
 * @return true if the connection is successfully routed,
 * @return false otherwise
 */
bool aStarRoute::routeOneConnection(int connectionId, int tid, bool sync) 
{
	// mutex.lock();
	// routedConnectionNum ++;
	// mutex.unlock();
	incrementRoutedConnectionNum();
	auto& connection = database.indirectConnections[connectionId];
	auto& net = database.nets[connection.getNetId()];
	auto& rnodes = database.routingGraph.routeNodes;
	auto& nodeInfos = nodeInfosForThreads[tid];
	connection.setRoutedThisIter(true);

	auto rnodeComp = [&](RouteNode* lhs, RouteNode* rhs) {
		return (nodeInfos[lhs->getId()].cost > nodeInfos[rhs->getId()].cost);
	};

	// Reserve capacity for priority_queue based on connection bbox to reduce reallocations
	int bbox_width = connection.getXMax() - connection.getXMin() + 1;
	int bbox_height = connection.getYMax() - connection.getYMin() + 1;
	int bbox_area = std::max(1, bbox_width * bbox_height);  // Ensure at least 1
	// Estimate nodes to explore: empirical factor of 10x bbox area, capped at 100k
	int estimated_nodes = std::min(bbox_area * 10, 100000);
	std::vector<RouteNode*> pq_container;
	pq_container.reserve(estimated_nodes);

	// std::priority_queue<int, vector<int>, decltype(nodeInfoComp)> nodeInfoQueue(nodeInfoComp);
	std::priority_queue<RouteNode*, vector<RouteNode*>, decltype(rnodeComp)> rnodeQueue(rnodeComp, std::move(pq_container));
	int connectionUniqueId = connectionId + connectionIdBase;

	auto push = [&](RouteNode* rnode, RouteNode* prev, double cost, double partialCost, int isTarget) {
		NodeInfo& ninfo = nodeInfos[rnode->getId()];
		// Use the number-of-connections-routed-so-far as the identifier for whether a rnode
        // has been visited by this connection before
		ninfo.write(prev, cost, partialCost, connectionUniqueId, isTarget);
		rnodeQueue.push(rnode);
	};

	push(connection.getSourceRNode(), nullptr, 0, 0, -1);
	
	NodeInfo& sinkInfo = nodeInfos[connection.getSinkRNode()->getId()];
	sinkInfo.write(nullptr, 0, 0, -1, connectionUniqueId);

	RouteNode* targetRNode = nullptr;

	int nodesPoppedThisConnection = 0;
	while (!rnodeQueue.empty()) {
		nodesPoppedThisConnection ++;
		// int ninfoIdx = nodeInfoQueue.top(); nodeInfoQueue.pop();
		RouteNode* rnode = rnodeQueue.top(); rnodeQueue.pop();
		// THIS MAY BE A DANGLING REFERENCE!!!!!
		NodeInfo& ninfo = nodeInfos[rnode->getId()];
		// RouteNode* rnode = ninfo.cur;
		double ninfo_partialCost = ninfo.partialCost;
		assert_t(rnode != nullptr);

		int childInfoIdx = -1;
		for (auto childRNode : rnode->getChildren()) {
			// childInfoIdx = nodeInfos.getIndex(childRNode);
			NodeInfo& childInfo = nodeInfos[childRNode->getId()];
			bool isVisited = (childInfo.isVisited == connectionUniqueId);
			bool isTarget = (childInfo.isTarget == connectionUniqueId);

			if (isVisited) {
				continue;
			}

			if (isTarget && childRNode == connection.getSinkRNode()) {
				targetRNode = childRNode;
				childInfo.prev = rnode;
				break;
			}

			if (!isAccessible(childRNode, connectionId)) {
				continue; // Note: different from rwroute, the boundary nodes are included
			}
				
			switch (childRNode->getNodeType())
			{
			case WIRE:
				if (!database.routingGraph.isAccessible(childRNode, connection)) {
					continue;
				}
				break; // In rwroute, use UTurn by default
			case PINBOUNCE:	
                assert_t(!isTarget);
                if (!isAccessiblePinbounce(childRNode, connection)) {
                    continue;
                }
				break;
			case PINFEED_I:
                if (!isAccessiblePinfeedI(childRNode, connection, isTarget)) {
                    continue;
                }
				break;
			case LAGUNA_I:
				continue; // never
				break; 
			case SUPER_LONG_LINE:
				exit(0); // never
				break;
			default:
				assert_t(false && "Unexpected rnode type");
				break;
			}
			// evaluate cost and push
			int countSourceUsesOrigin = net.countConnectionsOfUser(childRNode);
			int countSourceUses = countSourceUsesOrigin;
			int occChange = 0;
			if (sync) {
				countSourceUses = countSourceUses - net.getPreDecrementUser(childRNode) + net.getPreIncrementUser(childRNode);
				occChange = childInfo.getOccChange(currentBatchStamp);
			}
			double sharingFactor = 1 + sharingWeight * countSourceUses;
			double nodeCost = getNodeCost(childRNode, connection, occChange, countSourceUses, countSourceUsesOrigin, sharingFactor, isTarget, tid);
			assert_t(nodeCost >= 0);
			// double newPartialPathCost = ninfo.partialCost + rnodeCostWeight * nodeCost + rnodeWLWeight * childRNode->getLength() / sharingFactor;
			double newPartialPathCost = ninfo_partialCost + rnodeCostWeight * nodeCost + rnodeWLWeight * childRNode->getLength() / sharingFactor;

			int childX = childRNode->getEndTileXCoordinate();
	        int childY = childRNode->getEndTileYCoordinate();
	        auto sinkRNode = connection.getSinkRNode();
	        int sinkX = sinkRNode->getBeginTileXCoordinate();
	        int sinkY = sinkRNode->getBeginTileYCoordinate();
	        int deltaX = mkl_utils::scalar_abs(childX - sinkX);
	        int deltaY = mkl_utils::scalar_abs(childY - sinkY);

			double distanceToSink = deltaX + deltaY;
	        double newTotalPathCost = newPartialPathCost + estWLWeight * distanceToSink / sharingFactor;
			push(childRNode, rnode, newTotalPathCost, newPartialPathCost, -1);
		}
		if (targetRNode != nullptr)
			break;
	}

    // nodesPushed += nodesPoppedThisConnection + rnodeQueue.size();
   	// nodesPopped += nodesPoppedThisConnection;
	if (targetRNode == nullptr) {
		assert_t(rnodeQueue.empty());
		return false;
	} 
	
	// update path, rnode occupancy and congestion cost
	bool routed = saveRouting(connection, targetRNode, tid);
	if (sync) {
		if (routed) {
			// auto& net = database.nets[connection.getNetId()];
			for (auto rnode: connection.getRNodes()) {
				net.preIncrementUser(rnode);
			}
			connection.setRouted(true);
			connection.setNumNodesExplored(nodesPoppedThisConnection);
			connection.setLastRoutedIter(iter);
		} else {
			connection.resetRoute();
		}
	} else {
		if (routed) {
			// log() << "Connection " << connectionId << " routed" << endl;
			connection.setRouted(routed);
			updateUsersAndPresentCongestionCost(connection);
		} else {
			connection.resetRoute();
		}
	}
	
	return routed;
}

/**
 * @brief Check if the connection need to be (re-)routed.
 * 
 * @param connection 
 * @return true if the connection is congested or not routed yet,
 * @return false otherwise
 */
bool aStarRoute::shouldRoute(const Connection& connection)
{
	return !connection.getRouted() || connection.isCongested();
}

/**
 * @brief Check if the route node is accessible in the routing process of this connection
 * 
 * @param rnode 
 * @param connectionId 
 * @return true if the ending coordinate of this route node is within the bounding box of this connection,
 * @return false otherwise
 */
bool aStarRoute::isAccessible(const RouteNode* rnode, int connectionId)
{
	auto& conn = database.indirectConnections[connectionId];
	return rnode->getEndTileXCoordinate() > conn.getXMinBB() && rnode->getEndTileXCoordinate() < conn.getXMaxBB() &&
	       rnode->getEndTileYCoordinate() > conn.getYMinBB() && rnode->getEndTileYCoordinate() < conn.getYMaxBB(); 
}

/**
 * @brief Get the cost of the node.
 * 
 * @param rnode The node.
 * @param connection The connection to be routed.
 * @param occChange (In stable-first routing) the uncommitted change in the node's occupancy
 * @param countSourceUses The number of connections from the same net using this node 
 * @param countSourceUsesOrigin (In stable-first routing) The number of connections from the same net using this node at the last synchorization barrier
 * @param sharingFactor The sharing factor.
 * @param isTarget Whether this node is the sink node of this connection
 * @param tid The ID of the executing thread.
 * @return double The cost of this node.
 */
double aStarRoute::getNodeCost(RouteNode* rnode, const Connection& connection, int occChange, int countSourceUses, int countSourceUsesOrigin, double sharingFactor, bool isTarget, int tid)
{
	bool hasSameSourceUsers = (countSourceUses != 0);
	double presentCongestionCost;
	auto& net = database.nets[connection.getNetId()];
	assert_t(countSourceUses >= 0);
	int preDecOcc = ((countSourceUsesOrigin > 0) && (countSourceUses == 0)) ? 1 : 0;
	int preIncOcc = ((countSourceUsesOrigin == 0) && (countSourceUses > 0)) ? 1 : 0;

	if (hasSameSourceUsers) { // the rnode is used by other connection(s) from the same net
		int overOccupancy = rnode->getOccupancy() - preDecOcc + preIncOcc + occChange - rnode->getCapacity();
		// int overOccupancy = rnode->getOccupancy() - preDecOcc + preIncOcc - rnode->getCapacity();
		// make the congestion cost less for the current connection
		presentCongestionCost = 1 + overOccupancy * presentCongestionFactor;
	} else {
		presentCongestionCost = rnode->getPresentCongestionCost();
	}

	double biasCost = 0;
    if (!isTarget) {
        auto& net = database.nets[connection.getNetId()];
        biasCost = rnode->getBaseCost() / net.getConnectionSize() *
                (mkl_utils::scalar_fabs(rnode->getEndTileXCoordinate() - net.getXCenter()) + mkl_utils::scalar_fabs(rnode->getEndTileYCoordinate() - net.getYCenter())) / net.getDoubleHpwl();
    }

	return rnode->getBaseCost() * rnode->getHistoricalCongestionCost() * presentCongestionCost / sharingFactor + biasCost;
}

/**
 * @brief Save routing solution to local routing resource graph during overlap routing process
 * @param connection The connection to be saved
 * @param rnode The last node in the routing result of this connection. This node should be the sink node.
 * @param tid The ID of executing thread. If not using overlap parallel routing, set tid = 0
 */
bool aStarRoute::saveRouting(Connection& connection, RouteNode* rnode, int tid)
{
	// std::cout << "--------------save routing-------------" << endl;
	// mutex.lock();
	assert_t(rnode->getId() == connection.getSink());
	auto& nodeInfos = nodeInfosForThreads[tid];
	int watchDog = 0;
	int ninfoIdx = -1;
	do {
		// ninfoIdx = nodeInfos.getIndex(rnode);
		// assert_t(ninfoIdx >= 0);
		NodeInfo& ninfo = nodeInfos[rnode->getId()];
		if (ninfo.prev == nullptr) {
			assert_t(rnode->getId() == connection.getSource());
		}
		watchDog ++;
		connection.addRNode(rnode);
		if (watchDog == 10000)
			assert_t(false);
		rnode = ninfo.prev;
	} while (rnode != nullptr);

	assert_t(connection.getRNodeSize() > 1);
	// mutex.unlock();
	return true;
}

void aStarRoute::updateUsersAndPresentCongestionCost(Connection& connection)
{
	for (auto rnode : connection.getRNodes()) {
		bool newlyAdd = database.nets[connection.getNetId()].incrementUser(rnode);
		if (newlyAdd)
			rnode->incrementOccupancy();
		rnode->updatePresentCongestionCost(presentCongestionFactor);
	}
}

/**
 * @brief The implementation of dynamic cost factor updating mechanism
 * 
 * @param isCongestedDesign Whether this design is thought to be a congested design after the first routing iteration.
 */
void aStarRoute::dynamicCostFactorUpdating(bool isCongestedDesign)
{
	if (isCongestedDesign) {
		double r = 1.0 / (1 + mkl_utils::scalar_exp((1 - iter) * 0.5));
		historicalCongestionFactor = 2 * r;
		double r2 = 3.0 / (1 + mkl_utils::scalar_exp((iter - 1)));
		presentCongestionMultiplier = 1.1 * (1 + r2);
	}

	presentCongestionFactor *= presentCongestionMultiplier;
	presentCongestionFactor = std::min(presentCongestionFactor, maxPresentCongestionFactor);

	numOverUsedRNodes.store(0);

	std::function<void(int tid)> update = [&](int tid) {
		for (int rnodeId = tid; rnodeId < database.numNodes; rnodeId += numThread) {
			RouteNode& rnode = database.routingGraph.routeNodes[rnodeId];
			int overuse = rnode.getOccupancy() - rnode.getCapacity();
			if (overuse == 0) {
				rnode.setPresentCongestionCost(1 + presentCongestionFactor);
			} else if (overuse > 0) {
				// overUsedRNodeIds.emplace_back(rnode.getId());
				numOverUsedRNodes++;
				rnode.setPresentCongestionCost(1 + (overuse + 1) * presentCongestionFactor);
				rnode.setHistoricalCongestionCost(rnode.getHistoricalCongestionCost() + overuse * historicalCongestionFactor);
			}
		}
	};

	vector<std::thread> threads;
	for (int tid = 0; tid < numThread; tid++) {
		threads.emplace_back(update, tid);
	}
	for (std::thread& thread: threads) {
		thread.join();
	}
}

/**
 * @brief remove the routing result of a connection
 * 
 * @param connection the connection to be ripped up
 * @param sync true if this change waits for a synchronization barrier
 */
void aStarRoute::ripup(Connection& connection, bool sync)
{
	// mutex.lock();
	auto rnodes = connection.getRNodes();
	if (rnodes.size() == 0) {
		assert_t(!connection.getRouted());
		rnodes.emplace_back(connection.getSinkRNode());
	}
	if (!sync) {
		for (auto rnode : rnodes) {
			bool isErased = database.nets[connection.getNetId()].decrementUser(rnode);
			if (isErased)
				rnode->decrementOccupancy();
			rnode->updatePresentCongestionCost(presentCongestionFactor);
		}
	} else {
		auto& net = database.nets[connection.getNetId()];
		for (auto rnode : rnodes) {
			net.preDecrementUser(rnode);
		}
	}
	
	connection.resetRoute();
	connection.setRouted(false);
	// mutex.unlock();
}

bool aStarRoute::isAccessiblePinbounce(const RouteNode* child, const Connection& connection)
{
    return database.routingGraph.isAccessible(child, connection);
}

bool aStarRoute::isAccessiblePinfeedI(RouteNode* child, const Connection& connection, bool isTarget) {
    // When LUT pin swapping is enabled, PINFEED_I are not exclusive anymore
    // return isAccessiblePinfeedI(child, connection, !lutPinSwapping);
    // assert_t(child->getType() == PINFEED_I);
    // assert_t(!assertOnOveruse || !child->isOverUsed());

    if (isTarget) {
        return true;
    }

    if (database.nets[connection.getNetId()].countConnectionsOfUser(child) == 0 || !child->getIsNodePinBounce()) {
        // Inaccessible if child is not a sink pin of another connection on the same
        // net, or it is not a PINBOUNCE node
        return false;
    }

    return true;
}

/* post-process: RPTT routing (CUFR) */
void aStarRoute::routePartitionTree(PartitionTree* tree)
{
	for (auto& treeNodes : tree->scheduledTreeNodes) {
		if (treeNodes.size() == 0) continue;
		auto runRoute = [this, &treeNodes](int i) {
			routePartitionTreeLeafNode(treeNodes[i]);
		};
		runJobsMT(treeNodes.size(), numThread, runRoute);
	}
}

void aStarRoute::routePartitionTreeLeafNode(PartitionTreeNode* node)
{
	for (int connectionId : node->connectionIds) {
		auto& connection = database.indirectConnections[connectionId];
		if (shouldRoute(connection)) {
			ripup(connection, false);
			bool success = routeOneConnection(connectionId, 0, false);
			if (!success) {
				// mutex.lock();
				// failRouteNum ++;
				// mutex.unlock();
				incrementFailRouteNum();
				auto& connection = database.indirectConnections[connectionId];
				log() << "Routing failure. Connection "<< connection << " Coordinate: [" << connection.getSourceRNode()->getEndTileXCoordinate() << " " << connection.getSinkRNode()->getEndTileXCoordinate() << " " << connection.getSourceRNode()->getEndTileYCoordinate() << " " << connection.getSinkRNode()->getEndTileYCoordinate() << "] " << std::endl;
			}
		}
	}
}
