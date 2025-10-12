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
 * @brief The main process of stable-first parallel routing process
 * 
 */
void aStarRoute::stableFirstParallelRouting() {
	vector<std::thread> threads;
	// route batches
	for (int batchId = 0; batchId < numBatches; batchId ++) {
		// route
		threads.clear();
		currentBatchStamp = iter * numBatches + batchId;
		for (int tid = 0; tid < numThread; tid++) {
			threads.emplace_back(&aStarRoute::routeWorker, this, std::ref(netIdBatchesForThreads[batchId][tid]), tid);
		}
		for (int tid = 0; tid < numThread; tid++) {
			threads[tid].join();
		}
		// update nets
		threads.clear();
		for (int tid = 0; tid < numThread; tid++) {
			threads.emplace_back(&aStarRoute::updateNetsWorker, this, std::ref(netIdBatchesForThreads[batchId][tid]), tid);
		}
		for (int tid = 0; tid < numThread; tid++) {
			threads[tid].join();
		}
		// update rnodes' present congestion cost
		threads.clear();
		for (int tid = 0; tid < numThread; tid++) {
			threads.emplace_back(&aStarRoute::updatePresentCongCostWorker, this, tid);
		}
		for (int tid = 0; tid < numThread; tid++) {
			threads[tid].join();
		}
	}

	/* Route labeled high-fanout nets */
	if (treeForLabeledNets)
		routePartitionTree(treeForLabeledNets);
}

/**
 * @brief Worker thread
 * 
 * @param netIds A batch of net IDs.
 * @param tid ID of this worker thread
 */
void aStarRoute::routeWorker(vector<int>& netIds, int tid) {
	for (int netId: netIds) {
		assert_t(netId >= 0 && netId < database.nets.size());
		auto& net = database.nets[netId];
		// initialize
		net.clearPreDecrement();
		net.clearPreIncrement();
		// store used rnodes before this iteration
		unordered_set<RouteNode*> usedRNodesBefore;
		vector<int> connectionIds = net.getConnections();
		for (int connectionId: connectionIds) {
			auto& connection = database.indirectConnections[connectionId];
			for (RouteNode* rnode: connection.getRNodes()) {
				usedRNodesBefore.insert(rnode);
			}
		}
		// routing
		for (int connectionId: connectionIds) {
			auto& connection = database.indirectConnections[connectionId];
			connection.setRoutedThisIter(false);
			if (shouldRoute(connection)) {
				// Only pre-decrement the number of users. Do not modify global data in routeNetsOverlap
				ripup(connection, true);
				// Only pre-increment the number of users but not save the routing results.
				bool success = routeOneConnection(connectionId, tid, true);
				if (!success) {
					log() << "Routing failure. Connection "<< connection << " Coordinate: [" << connection.getSourceRNode()->getEndTileXCoordinate() << " " << connection.getSinkRNode()->getEndTileXCoordinate() << " " << connection.getSourceRNode()->getEndTileYCoordinate() << " " << connection.getSinkRNode()->getEndTileYCoordinate() << "] " << std::endl;
				}
			}
		}

		/* prepare for SwiftSync */

		// store used rnodes before this iteration and check whether they were used before
		unordered_set<RouteNode*> usedRNodesAfter;
		for (int connectionId: connectionIds) {
			auto& connection = database.indirectConnections[connectionId];
			for (RouteNode* rnode: connection.getRNodes()) {
				usedRNodesAfter.insert(rnode);
				if (usedRNodesBefore.find(rnode) == usedRNodesBefore.end()) {
					// newly added
					nodeInfosForThreads[tid][rnode->getId()].incOccChange(currentBatchStamp);
				}
			}
		}
		// find rnodes that are used before but not used now
		for (RouteNode* rnode: usedRNodesBefore) {
			if (usedRNodesAfter.find(rnode) == usedRNodesAfter.end()) {
				// newly ripup
				nodeInfosForThreads[tid][rnode->getId()].decOccChange(currentBatchStamp);
			}
		}
	}
}

/**
 * @brief A worker thread in the synchronization process (SwiftSync)
 * 
 * @param netIds the nets to be updated
 * @param tid ID of the worker thread
 */
void aStarRoute::updateNetsWorker(vector<int>& netIds, int tid) {
	for (int netId: netIds) {
		assert_t(netId >= 0 && netId < database.nets.size());
		auto& net = database.nets[netId];
		net.updatePreIncrement(currentBatchStamp);
		net.updatePreDecrement(currentBatchStamp);	
	}
};

void aStarRoute::updatePresentCongCostWorker(int tid) {
	std::vector<RouteNode>& routeNodes = database.routingGraph.routeNodes;
	for (int rnodeId = tid; rnodeId < database.numNodes; rnodeId += numThread) {
		RouteNode& rnode = routeNodes[rnodeId];
		if (rnode.getNeedUpdateBatchStamp() == currentBatchStamp) {
			rnode.updatePresentCongestionCost(presentCongestionFactor);
		}
	}
}

void aStarRoute::kmeansBasedPartition() {
	vector<int> netIdsToPartition;
	for (Net& net: database.nets) {
		if (net.isLabeled()) continue;
		netIdsToPartition.emplace_back(net.getId());
	}

	vector<int> labels = kmeans(netIdsToPartition, numThread);

	netIdsForThreads.clear();
	netIdsForThreads.resize(numThread);
	assert_t(labels.size() == netIdsToPartition.size());
	for (int i = 0; i < labels.size(); i++) {
		netIdsForThreads[labels[i]].emplace_back(netIdsToPartition[i]);
	}

	numBatches = (database.nets.size() - labeledNetIds.size()) / (256 * numThread);
	netIdBatchesForThreads.resize(numBatches);
	for (int batchId = 0; batchId < numBatches; batchId ++) {
		netIdBatchesForThreads[batchId].resize(numThread);
	}
	
	for (int tid = 0; tid < numThread; tid++) {
		int xmin = 2 * database.device.getDeviceXMax() + 1;
		int ymin = 2 * database.device.getDeviceYMax() + 1;
		int xmax = -2;
		int ymax = -2;
		for (int netId: netIdsForThreads[tid]) {
			Net& net = database.nets[netId];
			xmin = std::min(xmin, net.getXMinBB());
			ymin = std::min(ymin, net.getYMinBB());
			xmax = std::max(xmax, net.getXMaxBB());
			ymax = std::max(ymax, net.getYMaxBB());
		}
		double xcenter = (xmin + xmax) * 1.0 / 2;
		double ycenter = (ymin + ymax) * 1.0 / 2;
		vector<int> order;
		vector<double> angles;

		// Collect deltaX and deltaY for batch processing with oneMKL
		int numNets = netIdsForThreads[tid].size();
		vector<double> deltaXs(numNets);
		vector<double> deltaYs(numNets);

		for (int i = 0; i < numNets; i++) {
			order.emplace_back(i);
			Net& net = database.nets[netIdsForThreads[tid][i]];
			deltaYs[i] = (net.getYMinBB() + net.getYMaxBB()) * 1.0 / 2 - ycenter;
			deltaXs[i] = (net.getXMinBB() + net.getXMaxBB()) * 1.0 / 2 - xcenter;
		}

		// Use oneMKL VML for batch atan2 computation
		angles.resize(numNets);
		mkl_utils::vector_atan2(numNets, deltaYs.data(), deltaXs.data(), angles.data());

		std::sort(order.begin(), order.end(), [&](int& a, int& b){ return angles[a] < angles[b];});
		for (int i = 0; i < netIdsForThreads[tid].size(); i++) {
			int batchId = i * numBatches / netIdsForThreads[tid].size();
			assert_t(batchId < numBatches);
			netIdBatchesForThreads[batchId][tid].emplace_back(netIdsForThreads[tid][order[i]]);
		}
	}
}

double aStarRoute::iou(const Net& centroid, const Net& net) {
	int xA = std::max(centroid.getXMinBB(), net.getXMinBB());
    int yA = std::max(centroid.getYMinBB(), net.getYMinBB());
    int xB = std::min(centroid.getXMaxBB(), net.getXMaxBB());
    int yB = std::min(centroid.getYMaxBB(), net.getYMaxBB());

    int interArea = std::max(0, xB - xA) * std::max(0, yB - yA);
    
    int boxAArea = (centroid.getXMaxBB() - centroid.getXMinBB()) * (centroid.getYMaxBB() - centroid.getYMinBB());
    int boxBArea = (net.getXMaxBB() - net.getXMinBB()) * (net.getYMaxBB() - net.getYMinBB());
    
    int unionArea = boxAArea + boxBArea - interArea;

    return (unionArea > 0) ? static_cast<double>(interArea) / unionArea : 0.0;
}

double aStarRoute::diou(const Net& centroid, const Net& net) {
	double iouValue = iou(centroid, net);
    
    int xA = (centroid.getXMinBB() + centroid.getXMaxBB()) / 2;
    int yA = (centroid.getYMinBB() + centroid.getYMaxBB()) / 2;
    int xB = (net.getXMinBB() + net.getXMaxBB()) / 2;
    int yB = (net.getYMinBB() + net.getYMaxBB()) / 2;

    int cX = (centroid.getXMaxBB() + centroid.getXMinBB()) / 2;
    int cY = (centroid.getYMaxBB() + centroid.getYMinBB()) / 2;

    double d = mkl_utils::scalar_sqrt(static_cast<double>((xB - xA) * (xB - xA) + (yB - yA) * (yB - yA)));
    double c = mkl_utils::scalar_sqrt(static_cast<double>((cX - xB) * (cX - xB) + (cY - yB) * (cY - yB)));

    return 1.0 - iouValue + (d * d) / (c * c);
}

double aStarRoute::giou(const Net& centroid, const Net& net) {
	// compute intersection area
    double inter_x1 = std::max(centroid.getXMinBB(), net.getXMinBB());
    double inter_y1 = std::max(centroid.getYMinBB(), net.getYMinBB());
    double inter_x2 = std::min(centroid.getXMaxBB(), net.getXMaxBB());
    double inter_y2 = std::min(centroid.getYMaxBB(), net.getYMaxBB());

    double inter_area = std::max(0.0, inter_x2 - inter_x1) * std::max(0.0, inter_y2 - inter_y1);

    // compute union area
    double area_a = (centroid.getXMaxBB() - centroid.getXMinBB()) * (centroid.getYMaxBB() - centroid.getYMinBB());
    double area_b = (net.getXMaxBB() - net.getXMinBB()) * (net.getYMaxBB() - net.getYMinBB());
    double union_area = area_a + area_b - inter_area;

    // computer area of outer retangular
    double outer_x1 = std::min(centroid.getXMinBB(), net.getXMinBB());
    double outer_y1 = std::min(centroid.getYMinBB(), net.getYMinBB());
    double outer_x2 = std::max(centroid.getXMaxBB(), net.getXMaxBB());
    double outer_y2 = std::max(centroid.getYMaxBB(), net.getYMaxBB());
    double outer_area = (outer_x2 - outer_x1) * (outer_y2 - outer_y1);

    // compute 1 - giou
    double giou_value = 1 - inter_area / union_area + (outer_area - union_area) / outer_area;
	
	return giou_value;
}

double aStarRoute::furthestVertex(const Net& centroid, const Net& net) {
	double dist = -1;
	double cx = (centroid.getXMinBB() + centroid.getXMaxBB()) / 2.0;
	double cy = (centroid.getYMinBB() + centroid.getYMaxBB()) / 2.0;
	double xRatio = 1;
	double yRatio = 1;
	
	dist = std::max(dist, xRatio * (net.getXMinBB() - cx) * (net.getXMinBB() - cx) + yRatio * (net.getYMinBB() - cy) * (net.getYMinBB() - cy));
	dist = std::max(dist, xRatio * (net.getXMaxBB() - cx) * (net.getXMaxBB() - cx) + yRatio * (net.getYMinBB() - cy) * (net.getYMinBB() - cy));
	dist = std::max(dist, xRatio * (net.getXMinBB() - cx) * (net.getXMinBB() - cx) + yRatio * (net.getYMaxBB() - cy) * (net.getYMaxBB() - cy));
	dist = std::max(dist, xRatio * (net.getXMaxBB() - cx) * (net.getXMaxBB() - cx) + yRatio * (net.getYMaxBB() - cy) * (net.getYMaxBB() - cy));
	return dist;
}

double aStarRoute::distance(const Net& centroid, const Net& net, double weight) {
    return giou(centroid, net) * weight;
}

vector<Net> aStarRoute::initializeCentroids(const vector<int>& netIds, int k) {
    vector<Net> centroids;

	// find first centroid
	vector<int> xStat(database.device.getDeviceXMax() + 1, 0);
	vector<int> yStat(database.device.getDeviceYMax() + 1, 0);
	for (int netId: netIds) {
		Net& net = database.nets[netId];
		for (int x = net.getXMinBB() + 1; x < net.getXMaxBB(); x++) {
			for (int y = net.getYMinBB() + 1; y < net.getYMaxBB(); y++) {
				xStat[x] ++;
				yStat[y] ++;
			}
		}
	}

	double xCenter = 0;
	double yCenter = 0;
	int xdiv = 0;
	int ydiv = 0;

	for (int x = 0; x < xStat.size(); x++) {
		xCenter += x * xStat[x];
		xdiv += xStat[x];
	}
	for (int y = 0; y < yStat.size(); y++) {
		yCenter += y * yStat[y];
		ydiv += yStat[y];
	}

	xCenter /= xdiv;
	yCenter /= ydiv;
	Net firstCentroid(-1);
	firstCentroid.setCenter(xCenter, yCenter);
	firstCentroid.setXMinBB(xCenter - xMargin);
	firstCentroid.setXMaxBB(xCenter + xMargin);
	firstCentroid.setYMinBB(yCenter - yMargin);
	firstCentroid.setYMaxBB(yCenter + yMargin);
	centroids.push_back(firstCentroid);

	// find remaining k-1 centroids
	std::unordered_set<int> usedNetIds;
    while (centroids.size() < k) {
        vector<double> distances(netIds.size(), std::numeric_limits<double>::infinity());

        // compute distance to the nearest centroid
		vector<std::thread> threads;
		std::function<void(int)> calculate = [&](int tid) {
			for (int i = tid; i < netIds.size(); i += k) {
				for (const Net& centroid : centroids) {
					Net& net = database.nets[netIds[i]];
					double dist = distance(centroid, net, net.getConnectionSize());
					distances[i] = std::min(distances[i], dist);
				}
			}
		};

        for (int tid = 0; tid < k; tid++) {
			threads.emplace_back(calculate, tid);
		}
		for (int tid = 0; tid < k; tid++) {
			threads[tid].join();
		}

        // assign to new cluster(centroid)
		double maxDist = -2;
		int maxDistNetId = -1;
        for (size_t i = 0; i < distances.size(); ++i) {
			if (usedNetIds.find(netIds[i]) != usedNetIds.end()) continue;
			if (distances[i] > maxDist) {
				maxDist = distances[i];
				maxDistNetId = netIds[i];
			}
        }
		centroids.emplace_back(database.nets[maxDistNetId]);
		usedNetIds.insert(maxDistNetId);
    }

    return centroids;
}

vector<int> aStarRoute::kmeans(const vector<int>& netIds, int k) {
    vector<Net> centroids = initializeCentroids(netIds, k);
    vector<int> labels(netIds.size(), -1);
    bool changed = true;
	vector<int> changedFlags(k, 0);
	int iter = 0;
	vector<double> sizeFactors(k, 1);
	double maxPunish = 0.3;

	vector<int> oldCounts;

	std::function<void(int)> calculate = [&](int tid) {
		for (size_t i = tid; i < netIds.size(); i += k) {
			int nearest_centroid = -1;
			double minDist = std::numeric_limits<double>::infinity();

			for (size_t j = 0; j < centroids.size(); ++j) {
				Net& net = database.nets[netIds[i]];
				double distValue = distance(centroids[j], net, sizeFactors[j] * net.getConnectionSize());
				if (distValue < minDist) {
					minDist = distValue;
					nearest_centroid = j;
				}
			}

			if (labels[i] != nearest_centroid) {
				labels[i] = nearest_centroid;
				changedFlags[tid] = true;
			}
		}
	};

    while (changed && iter < 300) {
        changed = false;
		std::fill(changedFlags.begin(), changedFlags.end(), 0);
		vector<std::thread> threads;
		for (int tid = 0; tid < k; tid++) {
			threads.emplace_back(calculate, tid);
		}
		for (int tid = 0; tid < k; tid++) {
			threads[tid].join();
			changed |= changedFlags[tid];
		}

		threads.clear();

        // update centroids
        vector<Net> newCentroids(k);
		for (int i = 0; i < k; i++) {
			newCentroids[i].setXMinBB(0);
			newCentroids[i].setXMaxBB(0);
			newCentroids[i].setYMinBB(0);
			newCentroids[i].setYMaxBB(0);
		}
        vector<int> counts(k, 0);

        for (size_t i = 0; i < netIds.size(); ++i) {
			Net& net = database.nets[netIds[i]];
            newCentroids[labels[i]].setXMinBB(newCentroids[labels[i]].getXMinBB() + net.getXMinBB() * net.getConnectionSize());
			newCentroids[labels[i]].setXMaxBB(newCentroids[labels[i]].getXMaxBB() + net.getXMaxBB() * net.getConnectionSize());
			newCentroids[labels[i]].setYMinBB(newCentroids[labels[i]].getYMinBB() + net.getYMinBB() * net.getConnectionSize());
			newCentroids[labels[i]].setYMaxBB(newCentroids[labels[i]].getYMaxBB() + net.getYMaxBB() * net.getConnectionSize());
            counts[labels[i]] += net.getConnectionSize();
        }

		int notEmptyClustersCnt = 0;
        for (size_t j = 0; j < k; ++j) {
            if (counts[j] > 0) {
                newCentroids[j].setXMinBB(newCentroids[j].getXMinBB() / counts[j]) ;
                newCentroids[j].setXMaxBB(newCentroids[j].getXMaxBB() / counts[j]) ;
                newCentroids[j].setYMinBB(newCentroids[j].getYMinBB() / counts[j]) ;
                newCentroids[j].setYMaxBB(newCentroids[j].getYMaxBB() / counts[j]) ;
				newCentroids[j].setCenter((newCentroids[j].getXMinBB() + newCentroids[j].getXMaxBB()) / 2, (newCentroids[j].getYMinBB() + newCentroids[j].getYMaxBB()) / 2);
				notEmptyClustersCnt ++;
            }
        }

		if (notEmptyClustersCnt < k) {
			Net reInitialized(-1);
			double xCenter = 0;
			double yCenter = 0;
			for (size_t j = 0; j < k; ++j) {
				if (counts[j] > 0) {
					xCenter += newCentroids[j].getXCenter();
					yCenter += newCentroids[j].getYCenter();
				}
			}
			reInitialized.setCenter(xCenter / notEmptyClustersCnt, yCenter / notEmptyClustersCnt);
			reInitialized.setXMinBB(xCenter - 3);
			reInitialized.setXMaxBB(xCenter + 3);
			reInitialized.setYMinBB(yCenter - 15);
			reInitialized.setYMaxBB(yCenter + 15);
			for (size_t j = 0; j < k; ++j) {
				if (counts[j] == 0) {
					newCentroids[j].setXMinBB(reInitialized.getXMinBB());
					newCentroids[j].setXMaxBB(reInitialized.getXMaxBB());
					newCentroids[j].setYMinBB(reInitialized.getYMinBB());
					newCentroids[j].setYMaxBB(reInitialized.getYMaxBB());
				}
			}
		}

        centroids = newCentroids;
		oldCounts = counts;
		iter ++;
    }

	log() << "[kmeans] iter " << iter++ << " finished." << endl;
	stringstream ss;
	double averageClusterSize = 0;
	for (int j = 0; j < k; j++) {
		ss << std::setw(5) << oldCounts[j] << " ";
		averageClusterSize += oldCounts[j];
	}
	averageClusterSize = averageClusterSize * 1.0 / k;
	log() << "[kmeans] cluster sizes: " << ss.str() << std::endl;

	double stdDevOfClusterSize = 0;
	for (int j = 0; j < k; j++) {
		stdDevOfClusterSize += (oldCounts[j] - averageClusterSize) * (oldCounts[j] - averageClusterSize);
	}
	stdDevOfClusterSize = mkl_utils::scalar_sqrt(stdDevOfClusterSize * 1.0 / k);
	clusterUnbalanceRatio = stdDevOfClusterSize / averageClusterSize;

	assert_t(labels.size() == netIds.size());
	return labels;
}

/* mark the high-fanout nets*/
void aStarRoute::labelNets() {
	// test ->
	int maxArea = 0;
	int minArea = 100000;
	std::unordered_map<int, vector<int>> areaMap;
	for (Net& net: database.nets) {
		if (net.getConnectionSize() <= 0) continue;
		int netId = net.getId();
		int area = (net.getXMaxBB() - net.getXMinBB() + 1) * (net.getYMaxBB() - net.getYMinBB() + 1);
		maxArea = area > maxArea ? area : maxArea;
		minArea = area < minArea ? area : minArea;
		if (areaMap.find(area) == areaMap.end()) {
			areaMap[area] = std::vector<int>();
		}
		areaMap.at(area).push_back(netId);
	}

	connectionIdsLabeled.clear();
	int connCnt = 0;
	int netCnt = 0;
	for (int area = maxArea; area >= 0; area --) {
		if (areaMap.find(area) == areaMap.end()) continue;
		int tmpSum = 0;
		for (int netId: areaMap.at(area)) {
			Net& net = database.nets[netId];
			tmpSum += net.getConnectionSize();
		}
		if (connCnt + tmpSum >= labeledConnectionsRatio * database.indirectConnections.size()) {
			log() << "break area (not included): " << area << endl;
			break;
		}
		connCnt += tmpSum;
		for (int netId: areaMap.at(area)) {
			netCnt ++;
			Net& net = database.nets[netId];
			net.setLabeled(true);
			labeledNetIds.emplace_back(netId);
			vector<int>& connectionIds = net.getConnectionsByRef();
			connectionIdsLabeled.insert(connectionIdsLabeled.end(), connectionIds.begin(), connectionIds.end());
		}
	}
	log() << netCnt << "/" << database.numNets << " nets are labeled. ratio = " << netCnt * 1.0 / database.numNets << endl;
	log() << connectionIdsLabeled.size() << "/" << database.indirectConnections.size() << " connections are labeled, ratio = " << connectionIdsLabeled.size() * 1.0 / database.indirectConnections.size() << endl;

	utils::BoxT<int> bbox;
	for (int connId: connectionIdsLabeled) {
		Connection& conn = database.indirectConnections[connId];
		bbox = bbox.UnionWith(conn.getBBox());
	}
	log() << "Connections bbox: " << bbox << std::endl;
	treeForLabeledNets = new PartitionTree(database.indirectConnections, connectionIdsLabeled, bbox);
	treeForLabeledNets->calculateNodeLevel();
	treeForLabeledNets->schedule(treeForLabeledNets->getLeafNodes());
}

