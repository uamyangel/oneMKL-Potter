#include "aStarRoute.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <thread>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <vector>
#include "utils/MTStat.h"
#include "db/routeResult.h"

/**
 * @brief The main process of runtime-first parallel routing
 * 
 */
void aStarRoute::runtimeFirstParallelRouting() {
	auto routeNetGroup = [this](vector<int>& netIds, int tid) {
		// auto start_time = std::chrono::high_resolution_clock::now();
		auto connCompare = [this](const int& i, const int& j) {
			assert_t(i < this->database.indirectConnections.size() && j < this->database.indirectConnections.size());
			return this->database.indirectConnections[i].getHPWL() < this->database.indirectConnections[j].getHPWL();
		}; // large-degree net first and then smaller hpwl first

		for (int netId: netIds) {
			assert_t(netId >= 0 && netId < database.nets.size());
			vector<int> connectionIds = database.nets[netId].getConnections();
			std::sort(connectionIds.begin(), connectionIds.end(), connCompare);
			for (int connectionId: connectionIds) {
				auto& connection = database.indirectConnections[connectionId];
				if (shouldRoute(connection)) {
					ripup(connection, false);
					bool success = routeOneConnection(connectionId, tid, false);
					if (!success) {
						log() << "Routing failure. Connection "<< connection << " Coordinate: [" << connection.getSourceRNode()->getEndTileXCoordinate() << " " << connection.getSinkRNode()->getEndTileXCoordinate() << " " << connection.getSourceRNode()->getEndTileYCoordinate() << " " << connection.getSinkRNode()->getEndTileYCoordinate() << "] " << std::endl;
					}
				}
			}
		}
	};

	vector<std::thread> threads;
	threads.clear();
	for (int tid = 0; tid < numThread; tid++) {
		threads.emplace_back(routeNetGroup, std::ref(netIdsForThreads[tid]), tid);
	}
	for (int tid = 0; tid < numThread; tid++) {
		threads[tid].join();
	}
}

void aStarRoute::regionBasedPartition() {
	netIdsInLevelForThreads.clear();
	netIdsForThreads.clear();
	netIdBatchesForThreads.clear();

	/* first schedule: tree-based partitioning */
	device.init(database.layout.lx() - 1, database.layout.hx() - 1, database.layout.ly(), database.layout.hy());

	numLevel = 1;
	while ((1 << (numLevel - 1)) < numThread) {
        numLevel++;
    }
    levels.resize(numLevel);
    log() << "# levels: " << numLevel << endl;
	
	device.level = 0;
	for (int netId = 0; netId < database.nets.size(); netId++) {
		if (database.nets[netId].isLabeled())
			continue;
		if (database.nets[netId].getConnectionSize() > 0) {
			device.netIds.emplace_back(netId);
		}
	}

    std::deque<PartitionBBox*> q;
    q.emplace_back(&device);
    while (q.empty() == false) {
        PartitionBBox* partition = q.front(); q.pop_front();
        levels[partition->level].emplace_back(partition);

        if(partition->level + 1 >= levels.size()) {
            continue;
        }

		int W = partition->xMax - partition->xMin + 1;
		int H = partition->yMax - partition->yMin + 1;

		std::vector<int> xTotalBefore(W - 1, 0);
		std::vector<int> xTotalAfter(W - 1, 0);
		std::vector<int> yTotalBefore(H - 1, 0);
		std::vector<int> yTotalAfter(H - 1, 0);

		for (int netId: partition->netIds) {
			const Net& net = database.nets[netId];
			if (net.getConnectionSize() <= 0) continue;
			int xCenter = std::min(std::max((int)(net.getXCenter()), partition->xMin), partition->xMax) - partition->xMin;
			int yCenter = std::min(std::max((int)(net.getYCenter()), partition->yMin), partition->yMax) - partition->yMin;
			for (int x = xCenter; x < W - 1; x++) {
				xTotalBefore[x] += net.getConnectionSize();
			}
			for (int x = 0; x < xCenter; x++) {
				xTotalAfter[x] += net.getConnectionSize();
			}
			for (int y = yCenter; y < H - 1; y++) {
				yTotalBefore[y] += net.getConnectionSize();
			}
			for (int y = 0; y < yCenter; y++) {
				yTotalAfter[y] += net.getConnectionSize();
			}
		}

		double bestScore = INT_MAX;
		int bestPos = INT_MAX;
		Axis bestAxis = X;

		int maxXBefore = xTotalBefore[W - 2];
		int maxXAfter = xTotalAfter[0];
		for (int x = 0; x < W - 1; x++) {
			int before = xTotalBefore[x];
			int after = xTotalAfter[x];
			if (before == maxXBefore || after == maxXAfter) /* VPR: Cutting here would leave no nets to the left or right */
				continue;
			// double balanceScore = std::abs(xTotalBefore[x] - xTotalAfter[x]) * 1.0 / std::max(xTotalBefore[x], xTotalAfter[x]);
			double balanceScore = std::abs(xTotalBefore[x] - xTotalAfter[x]);
			if (balanceScore < bestScore) {
				bestScore = balanceScore;
				bestPos = partition->xMin + x;
				bestAxis = X;
			}
		}

		int maxYBefore = yTotalBefore[H - 2];
		int maxYAfter = yTotalAfter[0];
		for (int y = 0; y < H - 1; y++) {
			int before = yTotalBefore[y];
			int after = yTotalAfter[y];
			if (before == maxYBefore || after == maxYAfter) /* VPR: Cutting here would leave no nets to the left or right (sideways) */
				continue;
			// double balanceScore = std::abs(yTotalBefore[y] - yTotalAfter[y]) * 1.0 / std::max(yTotalBefore[y], yTotalAfter[y]);
			double balanceScore = std::abs(yTotalBefore[y] - yTotalAfter[y]);
			if (balanceScore < bestScore) {
				bestScore = balanceScore;
				bestPos = partition->yMin + y;
				bestAxis = Y;
			}
		}

		if (bestAxis == X) {
			if (bestPos <= partition->xMin || bestPos >= partition->xMax)
				continue;

        	PartitionBBox* tempLChild_X = new PartitionBBox(partition->xMin, bestPos, partition->yMin, partition->yMax, partition->level + 1);
        	PartitionBBox* tempRChild_X = new PartitionBBox(bestPos, partition->xMax, partition->yMin, partition->yMax, partition->level + 1);
        	vector<int> tempSelfNetIds_X;
        	for (int netId: partition->netIds) {
        	    if (tempLChild_X->isContainsNet(database.nets[netId])) {
        	        tempLChild_X->netIds.emplace_back(netId);
        	    } else if (tempRChild_X->isContainsNet(database.nets[netId])) {
        	        tempRChild_X->netIds.emplace_back(netId);
        	    } else {
        	        tempSelfNetIds_X.emplace_back(netId);
        	    }
        	}
        	int diff_X = std::abs((int)(tempLChild_X->netIds.size() - tempRChild_X->netIds.size()));
        	int total_X = tempLChild_X->netIds.size() + tempRChild_X->netIds.size();

        	
			partition->LChild = tempLChild_X;
            partition->RChild = tempRChild_X;
            partition->netIds = tempSelfNetIds_X;
			auto netIdComp = [&] (const int& lhs, const int& rhs) {return database.nets[lhs].getXCenter() < database.nets[rhs].getXCenter();};
			std::sort(partition->netIds.begin(), partition->netIds.end(), netIdComp);
		} else {
			if (bestPos <= partition->yMin || bestPos >= partition->yMax)
				continue;

        	PartitionBBox* tempLChild_Y = new PartitionBBox(partition->xMin, partition->xMax, partition->yMin, bestPos, partition->level + 1);
        	PartitionBBox* tempRChild_Y = new PartitionBBox(partition->xMin, partition->xMax, bestPos, partition->yMax, partition->level + 1);
        	vector<int> tempSelfNetIds_Y;
        	for (int netId: partition->netIds) {
        	    if (tempLChild_Y->isContainsNet(database.nets[netId])) {
        	        tempLChild_Y->netIds.emplace_back(netId);
        	    } else if (tempRChild_Y->isContainsNet(database.nets[netId])) {
        	        tempRChild_Y->netIds.emplace_back(netId);
        	    } else {
        	        tempSelfNetIds_Y.emplace_back(netId);
        	    }
        	}
        	int diff_Y = std::abs((int)(tempLChild_Y->netIds.size() - tempRChild_Y->netIds.size()));
        	int total_Y = tempLChild_Y->netIds.size() + tempRChild_Y->netIds.size();

			partition->LChild = tempLChild_Y;
            partition->RChild = tempRChild_Y;
            partition->netIds = tempSelfNetIds_Y;
			auto netIdComp = [&] (const int& lhs, const int& rhs) {return database.nets[lhs].getYCenter() < database.nets[rhs].getYCenter();};
			std::sort(partition->netIds.begin(), partition->netIds.end(), netIdComp);
		}

        q.push_back(partition->LChild);
        q.push_back(partition->RChild);
    }
	
	netIdsInLevelForThreads.resize(numThread);
	for (auto& netIdsInLevel: netIdsInLevelForThreads) {
		netIdsInLevel.resize(levels.size());
	}

	for (int lid = 0; lid < levels.size(); lid++) {
		auto level = levels[lid];
        if (level.size() < numThread) {
			vector<vector<int>> partitionNetIds = inLevelRePartition(level);
			assert_t(partitionNetIds.size() == numThread);
			for (int tid = 0; tid < numThread; tid++) {
				netIdsInLevelForThreads[tid][lid].insert(netIdsInLevelForThreads[tid][lid].end(), partitionNetIds[tid].begin(), partitionNetIds[tid].end());
			}
        } else {
			for (int j = 0; j < level.size(); j++) {
				int tid = j % numThread;
				netIdsInLevelForThreads[tid][lid].insert(netIdsInLevelForThreads[tid][lid].end(), level[j]->netIds.begin(), level[j]->netIds.end());
			}
        }
    }

	netIdsForThreads.resize(numThread);
	for (int tid = 0; tid < numThread; tid++) {
		for (int lid = 0; lid < levels.size(); lid++) {
			netIdsForThreads[tid].insert(netIdsForThreads[tid].end(), netIdsInLevelForThreads[tid][lid].begin(), netIdsInLevelForThreads[tid][lid].end());
		}
	}

	/* second schedule */

	// initialize numBatches
	numBatches = (database.nets.size() - labeledNetIds.size()) / (64 * numThread);
	log() << "numBatches: " << numBatches << " database.nets.size " << database.nets.size() << endl;

	netIdBatchesForThreads.clear();
	netIdBatchesForThreads.resize(numBatches);
	for (int batchId = 0; batchId < numBatches; batchId ++) {
		netIdBatchesForThreads[batchId].resize(numThread);
	}

	vector<int> estimatedWorkloadForThreads(numThread, 0);
	for (int tid = 0; tid < numThread; tid++) {
		for (int netId: netIdsForThreads[tid]) {
			int estimate = 0;
			for (int connId: database.nets[netId].getConnections()) {
				estimate += database.indirectConnections[connId].getHPWL();
			}
			estimatedWorkloadForThreads[tid] += estimate;
		}
	}

	for (int tid = 0; tid < numThread; tid++) {
		int begin = 0;
		int end = 0;
		int previousEstimation = 0;
		for (int batchId = 0; batchId < numBatches; batchId ++) {
			begin = end;
			end = begin + 1;
			int estimatedWorkloadUpperBound = (batchId + 1) * estimatedWorkloadForThreads[tid] / numBatches;
			if (batchId == numBatches - 1) {
				end = netIdsForThreads[tid].size();
			} else {
				for (; end < netIdsForThreads[tid].size(); end++) {
					int netId = netIdsForThreads[tid][end];
					auto& net = database.nets[netId];
					int estimate = 0;
					for (int connId: database.nets[netId].getConnections()) {
						estimate += database.indirectConnections[connId].getHPWL();
					}
					if (previousEstimation + estimate > estimatedWorkloadUpperBound)
						break;
					else
						previousEstimation += estimate;
				}
			}
			for (int i = begin; i < end && i < netIdsForThreads[tid].size(); i++) {
				netIdBatchesForThreads[batchId][tid].push_back(netIdsForThreads[tid][i]);
			}
		}
	}
}

vector<vector<int>> aStarRoute::inLevelRePartition(vector<PartitionBBox*>& level) {
	PartitionBBox levelBox;
	vector<int> levelNetIds;
	int levelFanouts = 0;
	// merge all bbox and netIds
	for (PartitionBBox* partition: level) {
		levelBox.xMin = std::min(levelBox.xMin, partition->xMin);
		levelBox.xMax = std::max(levelBox.xMax, partition->xMax);
		levelBox.yMin = std::min(levelBox.yMin, partition->yMin);
		levelBox.yMax = std::max(levelBox.xMax, partition->xMax);
		levelNetIds.insert(levelNetIds.end(), partition->netIds.begin(), partition->netIds.end());
	}
	for (int netId: levelNetIds) {
		levelFanouts += database.nets[netId].getConnectionSize();
	}

	// sort nets by area
	// assign nets with bigger area first,
	// because assigning nets with small area to inappropriate box causes less overlap
	auto netComp = [&](int lhs, int rhs) {return database.nets[lhs].getArea() > database.nets[rhs].getArea();};
	std::sort(levelNetIds.begin(), levelNetIds.end(), netComp);

	// spilt bbox into numThread pieces
	// prefer square pieces
	std::queue<PartitionBBox> splitQueue;
	splitQueue.push(levelBox);
	while (splitQueue.size() < numThread) {
		PartitionBBox bbox = splitQueue.front(); splitQueue.pop();
		bool splitX = ((bbox.xMax - bbox.xMin) >= (bbox.yMax - bbox.yMin));
		if (splitX) {
			PartitionBBox left (bbox.xMin, (bbox.xMax + bbox.xMin) / 2, bbox.yMin, bbox.yMax);
			PartitionBBox right((bbox.xMax + bbox.xMin) / 2, bbox.xMax, bbox.yMin, bbox.yMax);
			splitQueue.push(left);
			splitQueue.push(right);
		} else {
			PartitionBBox left (bbox.xMin, bbox.xMax, bbox.yMin, (bbox.yMin + bbox.yMax) / 2);
			PartitionBBox right(bbox.xMin, bbox.xMax, (bbox.yMin + bbox.yMax) / 2, bbox.yMax);
			splitQueue.push(left);
			splitQueue.push(right);
		}
	}

	vector<PartitionBBox> levelBoxPieces;
	while (!splitQueue.empty()) {
		levelBoxPieces.emplace_back(splitQueue.front());
		splitQueue.pop();
	}
	assert_t(levelBoxPieces.size() == numThread);

	// assign nets
	// each box has an upperbound of #fanout 
	vector<vector<int>> partitionNetIds(numThread);
	vector<int> partitionFanout(numThread, 0);
	int upperBoundNetNum = (int)(levelFanouts * 1.05 / numThread);
	vector<int> partsIds(numThread);
	for (int i = 0; i < numThread; i++) {
		partsIds[i] = i;
	}
	for (int netId: levelNetIds) {
		auto& net = database.nets[netId];
		auto overlapCompare = [&](int lhs, int rhs){
			int overlapLHS = 0;
			bool isOverlapLHS = ((net.getXMinBB() - levelBoxPieces[lhs].xMax) * (net.getXMaxBB() - levelBoxPieces[lhs].xMin) < 0) && 
								((net.getYMinBB() - levelBoxPieces[lhs].yMax) * (net.getYMaxBB() - levelBoxPieces[lhs].yMin) < 0);
			if (isOverlapLHS) {
				overlapLHS = (std::min(net.getXMaxBB(), levelBoxPieces[lhs].xMax) - std::max(net.getXMinBB(), levelBoxPieces[lhs].xMin)) * 
							 (std::min(net.getYMaxBB(), levelBoxPieces[lhs].yMax) - std::max(net.getYMinBB(), levelBoxPieces[lhs].yMin));
			}
			int overlapRHS = 0;
			bool isOverlapRHS = ((net.getXMinBB() - levelBoxPieces[rhs].xMax) * (net.getXMaxBB() - levelBoxPieces[rhs].xMin) < 0) && 
								((net.getYMinBB() - levelBoxPieces[rhs].yMax) * (net.getYMaxBB() - levelBoxPieces[rhs].yMin) < 0);
			if (isOverlapRHS) {
				overlapRHS = (std::min(net.getXMaxBB(), levelBoxPieces[rhs].xMax) - std::max(net.getXMinBB(), levelBoxPieces[rhs].xMin)) * 
							 (std::min(net.getYMaxBB(), levelBoxPieces[rhs].yMax) - std::max(net.getYMinBB(), levelBoxPieces[rhs].yMin));
			}
			// check partitions with bigger overlap first
			if (overlapLHS != overlapRHS) return overlapLHS > overlapRHS;
			double deltaLHS = std::fabs(net.getXCenter() - levelBoxPieces[lhs].getXCenter()) + std::fabs(net.getYCenter() - levelBoxPieces[lhs].getYCenter());
			double deltaRHS = std::fabs(net.getXCenter() - levelBoxPieces[rhs].getXCenter()) + std::fabs(net.getYCenter() - levelBoxPieces[rhs].getYCenter());
			return deltaLHS < deltaRHS;
		};
		vector<int> partsIds_ = partsIds;
		std::sort(partsIds_.begin(), partsIds_.end(), overlapCompare);
		for (int partId: partsIds_) {
			if (partitionFanout[partId] + database.nets[netId].getConnectionSize() <= upperBoundNetNum) {
				partitionNetIds[partId].emplace_back(netId);
				partitionFanout[partId] += database.nets[netId].getConnectionSize();
				break;
			}
		}
	}
	return partitionNetIds;
}

