#pragma once
#include "global.h"
#include "utils/geo.h"
#include "utils/MTStat.h"
#include "db/connection.h"
#include <string>

enum Axis {X, Y};

class PartitionTree;

class PartitionTreeNode 
{
public:
	PartitionTreeNode() : left(nullptr), right(nullptr), middle(nullptr){};

	std::vector<int> connectionIds; // the union of all its children
	utils::BoxT<int> bbox;
	PartitionTreeNode* left;
	PartitionTreeNode* right;
	PartitionTreeNode* middle;
	Axis axis;
	int cutlinePos;
	int level;
	std::string type;
};

class PartitionTree 
{
public:
	PartitionTreeNode* getRoot() {return root;}
	PartitionTree(vector<Connection>& connections_, const vector<int>& sortedConnectionIds, utils::BoxT<int> bbox);	
	void print();
	vector<PartitionTreeNode*> getLeafNodes();
	void calculateNodeLevel();
	int schedule(vector<PartitionTreeNode*> treeNodes);

	vector<vector<PartitionTreeNode*>> scheduledTreeNodes; // partitionTree leaf nodes in the same vector can be routed in parallel
	int scheduledLevel;
	std::unordered_map<PartitionTreeNode*, int> treeNodeLevelMap; // partitionTree leaf nodes in the same vector can be routed in parallel

private:
	int dfsPrint(PartitionTreeNode* root, int level, bool isPrint = true);
	void buildTree(PartitionTreeNode* root);
	int schedule();
	int schedule(PartitionTreeNode* root, int level);
	void debugScheduledTreeNodes();

	vector<Connection>& connections;
	PartitionTreeNode* root;
};

class PartitionBBox {
public: 
    int xMin;
    int xMax;
    int yMin;
    int yMax;

    int level = -1;
    // vector<int> connectionIds;
    vector<int> netIds;
    PartitionBBox* LChild = nullptr;
    PartitionBBox* RChild = nullptr;

    PartitionBBox(): xMin(INT_MAX), xMax(INT_MIN), yMin(INT_MAX), yMax(INT_MIN) {}

    PartitionBBox(int xMin_, int xMax_, int yMin_, int yMax_):
        xMin(xMin_), xMax(xMax_), yMin(yMin_), yMax(yMax_) {}

    PartitionBBox(int xMin_, int xMax_, int yMin_, int yMax_, int level_):
        xMin(xMin_), xMax(xMax_), yMin(yMin_), yMax(yMax_), level(level_) {}

    bool isContainsNet(Net& net) {
        return net.getXMinBB() >= xMin && net.getYMinBB() >= yMin && net.getXMaxBB() <= xMax && net.getYMaxBB() <= yMax;
    }

    double getXCenter() {
        return (double)(xMax + xMin) / 2;
    }
    double getYCenter() {
        return (double)(yMax + yMin) / 2;
    }

    void init(int xMin_, int xMax_, int yMin_, int yMax_) {
        xMin = xMin_; xMax = xMax_; yMin = yMin_; yMax = yMax_;
    }
    
    bool isLeaf() {
        return LChild == nullptr && RChild == nullptr;
    }
};