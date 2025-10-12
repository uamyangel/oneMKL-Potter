#include "partitionTree.h"
#include <limits.h>
#include <string>
#include <queue>
#include <vector>
#include "utils/mkl_utils.h"

using utils::BoxT;

PartitionTree::PartitionTree(vector<Connection>& connections_, const vector<int>& sortedConnectionIds, BoxT<int> bbox) : connections(connections_) 
{
	// initialize root
	root = new PartitionTreeNode;
	root->connectionIds = sortedConnectionIds;
	root->bbox = bbox;
	root->type = "middle";

	buildTree(root);
}

/**
 * @brief Recursively building the RPTT.
 * 
 * @param root The root node of the RPTT.
 */
void PartitionTree::buildTree(PartitionTreeNode* root)
{
	/* Find the optimal cutline. */
	const auto& bbox = root->bbox;
	int W = bbox.hx() - bbox.lx() + 1;
	int H = bbox.hy() - bbox.ly() + 1;

	std::vector<int> xTotalBefore(W - 1, 0);
	std::vector<int> xTotalAfter(W - 1, 0);
	std::vector<int> yTotalBefore(H - 1, 0);
	std::vector<int> yTotalAfter(H - 1, 0);

	for (int connId: root->connectionIds) {
		const Connection& connection = connections[connId];
		int xStart = std::max(bbox.lx(), connection.getXMinBB()) - bbox.lx();
		int xEnd   = std::min(bbox.hx(), connection.getXMaxBB()) - bbox.lx();
		assert(xStart >= 0);
		/* VPR: Fill in the lookups assuming a cutline at x + 0.5. */
		for (int x = xStart; x < W - 1; x++) {
			xTotalBefore[x] ++; // "+= fanouts" in VPR
		}
		for (int x = 0; x < xEnd; x++) {
			xTotalAfter[x] ++;
		}

		int yStart = std::max(bbox.ly(), connection.getYMinBB()) - bbox.ly();
		int yEnd   = std::min(bbox.hy(), connection.getYMaxBB()) - bbox.ly();
		assert(yStart >= 0);
		for (int y = yStart; y < H - 1; y++) {
			yTotalBefore[y] ++;
		}
		for (int y = 0; y < yEnd; y++) {
			yTotalAfter[y] ++;
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
		double balanceScore = mkl_utils::scalar_abs(xTotalBefore[x] - xTotalAfter[x]) * 1.0 / std::max(xTotalBefore[x], xTotalAfter[x]);
		if (balanceScore < bestScore) {
			bestScore = balanceScore;
			bestPos = bbox.lx() + x;
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
		double balanceScore = mkl_utils::scalar_abs(yTotalBefore[y] - yTotalAfter[y]) * 1.0 / std::max(yTotalBefore[y], yTotalAfter[y]);
		if (balanceScore < bestScore) {
			bestScore = balanceScore;
			bestPos = bbox.ly() + y;
			bestAxis = Y;
		}
	}

	/* VPR: Couldn't find a cutline: all cutlines result in a one-way cut */
	if (bestPos != INT_MAX) {
		/* Find the optimal cut, recursively build the sub-trees */
		root->axis = bestAxis;
		root->cutlinePos = bestPos;
		root->left = new PartitionTreeNode;
		root->right = new PartitionTreeNode;
		root->middle = new PartitionTreeNode;
		root->left->type = "left";
		root->right->type = "right";
		root->middle->type = "middle";

		if (bestAxis == X) {
			for (int connId: root->connectionIds) {
				const Connection& connection = connections[connId];
				if (connection.getXMaxBB() <= bestPos) {
					root->left->connectionIds.emplace_back(connId);
				}
				else if (connection.getXMinBB() >= bestPos) {
					root->right->connectionIds.emplace_back(connId);
				}
				else {
					root->middle->connectionIds.emplace_back(connId);
					root->middle->bbox.Update(connection.getXMinBB(), connection.getYMinBB());
					root->middle->bbox.Update(connection.getXMaxBB(), connection.getYMaxBB());
				}
			}
			root->left->bbox.Set(bbox.lx(), bbox.ly(), bestPos, bbox.hy());
			root->right->bbox.Set(bestPos, bbox.ly(), bbox.hx(), bbox.hy());
			buildTree(root->left);
			buildTree(root->right);
			buildTree(root->middle);
		} else {
			assert_t(bestAxis == Y);
			for (int connId: root->connectionIds) {
				const Connection& connection = connections[connId];
				if (connection.getYMaxBB() <= bestPos) {
					root->left->connectionIds.emplace_back(connId);
				}
				else if (connection.getYMinBB() >= bestPos) {
					root->right->connectionIds.emplace_back(connId);
				}
				else {
					root->middle->connectionIds.emplace_back(connId);
					root->middle->bbox.Update(connection.getXMinBB(), connection.getYMinBB());
					root->middle->bbox.Update(connection.getXMaxBB(), connection.getYMaxBB());
				}
			}
			root->left->bbox.Set(bbox.lx(), bbox.ly(), bbox.hx(), bestPos);
			root->right->bbox.Set(bbox.lx(), bestPos, bbox.hx(), bbox.hy());
			buildTree(root->left);
			buildTree(root->right);
			buildTree(root->middle);
		}
	}
}

int PartitionTree::dfsPrint(PartitionTreeNode* root, int level, bool isPrint)
{
	int leftTreeConnCnt = 0;
	int rightTreeConnCnt = 0;
	int middleTreeConnCnt = 0;
	bool hasChildren = root->left != nullptr && root->right != nullptr && root->middle != nullptr;
	bool hasNotChildren = root->left == nullptr && root->right == nullptr && root->middle == nullptr;
	assert_t(hasChildren || hasNotChildren);
	root->level = level;
	if (root->left != nullptr) leftTreeConnCnt = dfsPrint(root->left, level + 1, isPrint);
	if (root->middle != nullptr) middleTreeConnCnt = dfsPrint(root->middle, level + 1, isPrint);
	if (root->right != nullptr) rightTreeConnCnt = dfsPrint(root->right, level + 1, isPrint);
	if (isPrint) {
		for (int i = 0; i < level; i ++)
			std::cout << "    ";
		std::cout << "Level: " << std::setw(2) << level << " #conns " << std::setw(8) << root->connectionIds.size() << " vs (" << std::setw(8) << leftTreeConnCnt << ", " << std::setw(8) << rightTreeConnCnt << ", " << std::setw(8) << middleTreeConnCnt << ") " << root->bbox << std::endl;
	}
	if (hasChildren)
		assert_t(root->connectionIds.size() == leftTreeConnCnt + rightTreeConnCnt + middleTreeConnCnt)
	return root->connectionIds.size();
}

void PartitionTree::calculateNodeLevel()
{
	dfsPrint(root, 0, false);
}

vector<PartitionTreeNode*> PartitionTree::getLeafNodes() 
{
	// calcualteNodeLevel();

	vector<PartitionTreeNode*> nodes;
	std::queue<PartitionTreeNode*> q;
	q.push(root);
	while (!q.empty()) {
		auto n = q.front(); q.pop();
		if (n == nullptr) continue;
		if (n->left == nullptr) {
			assert_t(n->right == nullptr && n->middle == nullptr);
			nodes.emplace_back(n);
		} else {
			q.push(n->left);
			assert_t(n->right != nullptr);
			q.push(n->right);
			if (n->middle != nullptr) q.push(n->middle);
		}
	}
	int cnt = 0;
	for (auto n : nodes)
		cnt += n->connectionIds.size();
	assert_t(cnt == root->connectionIds.size());

	return nodes;
}

int PartitionTree::schedule(vector<PartitionTreeNode*> treeNodes)
{
	int num = treeNodes.size();
	// conflict map
	vector<vector<bool>> hasConflict(num, vector<bool>(num, false));
	for (int i = 0; i < num; i ++) {
		for (int j = i + 1; j < num; j ++) {
			if (treeNodes[i]->bbox.HasStrictIntersectWith(treeNodes[j]->bbox)) {
				hasConflict[i][j] = true;
				hasConflict[j][i] = true;
			}
		}
	}

	// sort nodes by connection num
	vector<int> sortedTreeNodeIds(num, 0);
	for (int i = 0; i < num; i ++) sortedTreeNodeIds[i] = i; 
	std::sort(sortedTreeNodeIds.begin(), sortedTreeNodeIds.end(), [&treeNodes](const int& l, const int& r) {
		return treeNodes[l]->connectionIds.size() > treeNodes[r]->connectionIds.size();
	});

	// schedule
	vector<vector<int>> scheduledTreeNodeIds;
	for (int firNodeId : sortedTreeNodeIds) {
		int level = -1;
		for (int l = 0; l < scheduledTreeNodeIds.size(); l ++) {
			// if (scheduledTreeNodeIds[l].size() == database.numThread) continue;
			bool valid = true;
			for (auto secNodeId : scheduledTreeNodeIds[l]) {
				if (hasConflict[firNodeId][secNodeId]) {
					valid = false;
					break;
				}
			}
			if (valid) {
				level = l;
				break;
			}
		}
		if (level == -1) {
			vector<int> ids = {firNodeId};
			scheduledTreeNodeIds.emplace_back(ids);
		} else 
			scheduledTreeNodeIds[level].emplace_back(firNodeId);
	}
	scheduledTreeNodes.resize(scheduledTreeNodeIds.size());
	for (int i = 0; i < scheduledTreeNodeIds.size(); i ++) {
		for (int nodeId : scheduledTreeNodeIds[i]) {
			scheduledTreeNodes[i].emplace_back(treeNodes[nodeId]);
		}
	}

	return scheduledTreeNodes.size();
}