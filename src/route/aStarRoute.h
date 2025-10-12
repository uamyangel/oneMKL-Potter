#pragma once
#include "global.h"
#include "db/database.h"
#include "db/routeNode.h"
#include "partitionTree.h"
#include <queue>
#include <mutex>
#include <future>
#include <atomic>

class aStarRoute {
public:
	aStarRoute(Database& database_, bool isRuntimeFirst_) : database(database_), isRuntimeFirst(isRuntimeFirst_) {
		numThread = database.getNumThread();
		scheduledTreeNodes.resize(100); // initialize with a large size.
		nodeInfosForThreads.resize(numThread);
		occChangeForThreads.resize(numThread);

		log() << "Initializing per-thread data structures for " << numThread << " threads..." << std::endl;

		auto nodeInfos_resize = [&](int tid) {
			try {
				nodeInfosForThreads[tid].resize(database.numNodes);
			} catch (const std::bad_alloc& e) {
				log() << "ERROR: Failed to allocate nodeInfos for thread " << tid << std::endl;
				throw;
			}
		};
		auto occChange_resize = [&](int tid) {
			try {
				occChangeForThreads[tid].resize(database.numNodes);
			} catch (const std::bad_alloc& e) {
				log() << "ERROR: Failed to allocate occChange for thread " << tid << std::endl;
				throw;
			}
		};
		vector<std::thread> jobs;
		for (int tid = 0; tid < numThread; tid ++) {
			jobs.emplace_back(nodeInfos_resize, tid);
		}
		for (int tid = 0; tid < numThread; tid ++) {
			jobs[tid].join();
		}
		jobs.clear();
		for (int tid = 0; tid < numThread; tid ++) {
			jobs.emplace_back(occChange_resize, tid);
		}
		for (int tid = 0; tid < numThread; tid ++) {
			jobs[tid].join();
		}
		// for (auto& nodeInfos: nodeInfosForThreads) {
		// 	nodeInfos.resize(database.numNodes);
		// }
		netIdsForThreads.resize(numThread);
		numOverUsedRNodes.store(0);
	}
	void route();
	bool routeOneConnection(int connectionId, int tid, bool sync);
	vector<RouteResult> nodeRoutingResults;

private:
	// parameters
	bool isRuntimeFirst = false;
	int iter = 0;
	int maxIter = 500;
	bool useParallel = true;
	int numThread = 16;
	int currentBatchStamp = -1;
	int numBatches = 256;
	float presentCongestionMultiplier = 2;
	float maxPresentCongestionFactor = 1000000;

	int xMargin = 3;
	int yMargin = 15;

	int nodesPushed = 0;
	int nodesPopped = 0;
	int expandChildrenNum = 0;

	std::atomic<int> routedConnectionNum;
	std::atomic<int> failRouteNum;
	int connectionIdBase = 0;
	float presentCongestionFactor = 0.5;
	float historicalCongestionFactor = 1;
	double rnodeCostWeight = 1;
	double sharingWeight = 1;
	double rnodeWLWeight = 0.2;
	double estWLWeight = 0.8;
	Database& database;
	std::vector<int> sortedConnectionIds;
	std::atomic<int> numOverUsedRNodes;
	PartitionTree* partitionTree;
	vector<vector<PartitionTreeNode*>> scheduledTreeNodes; // partitionTree leaf nodes in the same vector can be routed in parallel
	int scheduledLevel;
	std::unordered_map<PartitionTreeNode*, int> treeNodeLevelMap; // partitionTree leaf nodes in the same vector can be routed in parallel
	std::mutex mutex;
	bool isCongestedDesign = false;
	int RPTTScheduledLevel;

	// overlap routing related ->
	bool useOverlapRouting = true;
	vector<int> labeledNetIds;
	std::condition_variable cv;
	std::atomic<int> numFinishedWorkers;
	bool exitLabel = false;
	bool readyLabel = false;
	bool finishLabel = true;
	int mainCnt = 0;
	vector<int> connectionIdsLabeled;
	PartitionTree* treeForLabeledNets = NULL;

	vector<vector<vector<int>>> netIdBatchesForThreads; // netIdBatchesForThreads[batchId][tid][]
	vector<vector<int>> netIdsForThreads;
	vector<vector<NodeInfo>> nodeInfosForThreads;
	vector<vector<int>> occChangeForThreads;

	// region-based partitioning ->
	PartitionBBox device;
	vector<vector<PartitionBBox*>> levels;
	vector<vector<vector<int>>> netIdsInLevelForThreads;
	int numLevel;
	// region-based partitioning <-
	
	// kmeans-based partitioning ->
	double clusterUnbalanceRatio = -1;
	bool clusterOverlapping = false;
	double labeledConnectionsRatio = 0.5;
	// kmeans-based partitioning <-

	// overlap routing related <-

	void sortConnections();
	bool shouldRoute(const Connection& connection);
	void ripup(Connection& connection, bool sync);
	bool isAccessible(const RouteNode* rnode, int connectionId);
	double getNodeCost(RouteNode* rnode, const Connection& connection, int occChange, int countSourceUses, int countSourceUsesOrigin, double sharingFactor, bool isTarget, int tid);
	bool saveRouting(Connection& connection, RouteNode* rnode, int tid);
	void updateUsersAndPresentCongestionCost(Connection& connection);
	void dynamicCostFactorUpdating(bool isCongestedDesign);
    bool isAccessiblePinbounce(const RouteNode* child, const Connection& connection);
    bool isAccessiblePinfeedI(RouteNode* child, const Connection& connection, bool isTarget);

	void updateSinkNodeUsage();

	void routePartitionTree(PartitionTree* tree);
	void routePartitionTreeLeafNode(PartitionTreeNode* node);

	void routeIndirectConnections();
	void routeDirectConnections();
	void saveAllRoutingSolutions();
	void fixNetRoutes(const Net& net, std::set<RouteNode*>& netRNodes);

	void updateIndirectConnectionBBox(int x_margin, int y_margin);

	// overlap routing related ->
	void regionBasedPartition();
	vector<vector<int>> inLevelRePartition(vector<PartitionBBox*>& level);
	void stableFirstParallelRouting();
	void runtimeFirstParallelRouting();
	void routeWorker(vector<int>& netIds, int tid);
	void updateNetsWorker(vector<int>& netIds, int tid);
	void updatePresentCongCostWorker(int tid);
	// overlap routing related <-

	void incrementRoutedConnectionNum() { routedConnectionNum++; }
	void incrementFailRouteNum() { failRouteNum++; }

	void printThreadCoverDebug(bool print);
	void computeBatchCover(bool print);

	double distance(const Net& a, const Net& b, double weight);
	double iou(const Net& centroid, const Net& net);
	double diou(const Net& centroid, const Net& net);
	double giou(const Net& centroid, const Net& net);
	double furthestVertex(const Net& centroid, const Net& net);
	vector<Net> initializeCentroids(const vector<int>& netIds, int k);
	vector<int> kmeans(const vector<int>& netIds, int k);
	void kmeansBasedPartition();
	void partition();

	void labelNets();
	void routeLabeledNets();
};

