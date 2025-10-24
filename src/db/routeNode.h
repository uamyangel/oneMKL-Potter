#pragma once
#include "global.h"
#include <unordered_map>
#include <set>
#include <atomic>

#define NODE_CAPACITY 1
// class RouteNode;
class RouteNode
{
public:
	// RouteNode(){}
	RouteNode(obj_idx id_, short beginTileXCoordinate_, short beginTileYCoordinate_, short endTileXCoordinate_, short endTileYCoordinate_, float baseCost_, short length_, NodeType type_, bool isNodePinBounce_) :
		id(id_),
		beginTileXCoordinate(beginTileXCoordinate_),
		beginTileYCoordinate(beginTileYCoordinate_),
		endTileXCoordinate(endTileXCoordinate_),
		endTileYCoordinate(endTileYCoordinate_),
		baseCost(baseCost_),
		length(length_),
		type(type_),
		isNodePinBounce(isNodePinBounce_){
		children.reserve(8);  // Reserve capacity to reduce dynamic reallocations
	}
	RouteNode() :
		id(0),
		endTileXCoordinate(0),
		endTileYCoordinate(0),
		beginTileXCoordinate(0),
		beginTileYCoordinate(0),
		length(1),
		isAccessibleWire(false),
		baseCost(0.0f),
		type(static_cast<NodeType>(0)),
		isNodePinBounce(false),
		needUpdateBatchStamp(-1),
		children(),
		occupancy(0),
		presentCongestionCost(1.0f),
		historicalCongestionCost(1.0f) {
		children.reserve(8);  // Reserve capacity to reduce dynamic reallocations
	}
	RouteNode(const RouteNode& that) :
		id(that.id),
		endTileXCoordinate(that.endTileXCoordinate),
		endTileYCoordinate(that.endTileYCoordinate),
		beginTileXCoordinate(that.beginTileXCoordinate),
		beginTileYCoordinate(that.beginTileYCoordinate),
		length(that.length),
		isAccessibleWire(that.isAccessibleWire),
		baseCost(that.baseCost),
		type(that.type),
		isNodePinBounce(that.isNodePinBounce),
		needUpdateBatchStamp(that.needUpdateBatchStamp),
		children(that.children),
		occupancy(that.occupancy.load()),
		presentCongestionCost(that.presentCongestionCost),
		historicalCongestionCost(that.historicalCongestionCost) {}
	obj_idx getId() const {return id;}
	short getCapacity() const {return NODE_CAPACITY;}
	short getEndTileXCoordinate() const {return endTileXCoordinate;}
	short getEndTileYCoordinate() const {return endTileYCoordinate;}
	short getBeginTileXCoordinate() const {return beginTileXCoordinate;}
	short getBeginTileYCoordinate() const {return beginTileYCoordinate;}
	short getLength() const {return length;}
	bool getIsAccesibleWire() const {return isAccessibleWire;}
	float getBaseCost() const {return baseCost;}
	NodeType getNodeType() const {return type;}
	bool getIsNodePinBounce() const {return isNodePinBounce;}

	const std::vector<RouteNode*>& getChildren() const {return children;}
	std::vector<RouteNode*>& getChildrenByRef() {return children;}
	int getChildrenSize() const {return children.size();}

	float getPresentCongestionCost() const {return presentCongestionCost;}
	float getHistoricalCongestionCost() const {return historicalCongestionCost;}

	void setId(obj_idx v) {id = v;}
	void setEndTileXCoordinate(short v) {endTileXCoordinate = v;}
	void setEndTileYCoordinate(short v) {endTileYCoordinate = v;}
	void setBeginTileXCoordinate(short v) {beginTileXCoordinate = v;}
	void setBeginTileYCoordinate(short v) {beginTileYCoordinate = v;}
	void setLength(short v) {length = v;}
	void setIsAccesibleWire(bool v) {isAccessibleWire = v;}
	void setBaseCost(float v) {baseCost = v;}
	void setIsNodePinBounce(bool v) {isNodePinBounce = v;}

	void setChildren(std::vector<RouteNode*> cs) {children = cs;}
	void clearChildren() {children.clear();}
	void addChildren(RouteNode* c) {children.emplace_back(c);}
	void setNodeType(NodeType t) {type = t;}

	void setPresentCongestionCost(float cost) {presentCongestionCost = cost;}
	void updatePresentCongestionCost(float pres_fac) {
        int occ = getOccupancy();
        if (occ < NODE_CAPACITY)
            setPresentCongestionCost(1);
        else 
            setPresentCongestionCost(1 + (occ - NODE_CAPACITY + 1) * pres_fac);
    }
	void setHistoricalCongestionCost(float cost) {historicalCongestionCost = cost;}

	// methods for usersConnectionCounts
	// int getOccupancy() const {return occupancy;}
	int getOccupancy() const {return occupancy.load();}
	
	bool isOverUsed () const {return NODE_CAPACITY < getOccupancy();}

	void incrementOccupancy() {occupancy ++;} 
	void decrementOccupancy() {occupancy --;}

	void setNeedUpdateBatchStamp(int batchStamp) {needUpdateBatchStamp = batchStamp;}
	int getNeedUpdateBatchStamp() const {return needUpdateBatchStamp;}

private:
	obj_idx id;
	// short capacity = 1;
	short endTileXCoordinate;
	short endTileYCoordinate;
	short beginTileXCoordinate;
	short beginTileYCoordinate;
	short length = 1;
	bool isAccessibleWire;
	float baseCost;
	NodeType type;
	bool isNodePinBounce;

	int needUpdateBatchStamp = -1;

	// graph
	std::vector<RouteNode*> children;
	// int occupancy;
	std::atomic<int> occupancy;
	
	float presentCongestionCost = 1;
	float historicalCongestionCost = 1;

	friend class boost::serialization::access;
	template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & id;
		// short capacity = 1;
		ar & endTileXCoordinate;
		ar & endTileYCoordinate;
		ar & beginTileXCoordinate;
		ar & beginTileYCoordinate;
		ar & length;
		ar & isAccessibleWire;
		ar & baseCost;
		ar & type;
		ar & isNodePinBounce;

		// ar & upStreamPathCost;
		// ar & lowerBoundTotalPathCost;
    }
};

// Align to 64-byte cacheline to reduce cache misses and false sharing
class alignas(64) NodeInfo {
public:
    // Hot path data (most frequently accessed, placed first for better cache locality)
    RouteNode* prev;         // 8 bytes - A* backtracking path, most frequently accessed
    double cost;             // 8 bytes - Total cost, frequently compared
    double partialCost;      // 8 bytes - Partial cost
    int isVisited;           // 4 bytes - Visited flag
    int isTarget;            // 4 bytes - Target flag

private:
    // Cold data (less frequently accessed)
    int occChange;           // 4 bytes
    int occChangeBatchStamp; // 4 bytes - iter * numBatches + batchId

    // Padding to exactly 64 bytes (one cacheline)
    // Current: 8+8+8+4+4+4+4 = 40 bytes
    // Padding: 64-40 = 24 bytes
    char padding[24];

public:
	NodeInfo(): prev(nullptr), cost(0), partialCost(0), isVisited(-1), isTarget(-1), occChange(0), occChangeBatchStamp(-1) {}

	void erase() {
		prev = nullptr; cost = 0; partialCost = 0; isVisited = -1; isTarget = -1;
	}

	void write(RouteNode* prev_, double cost_, double partialCost_, int isVisited_, int isTarget_) {
		prev = prev_; cost = cost_; partialCost = partialCost_; isVisited = isVisited_; isTarget = isTarget_;
	}

	void write(NodeInfo* ninfo) { write(ninfo->prev, ninfo->cost, ninfo->partialCost, ninfo->isVisited, ninfo->isTarget); }

	void write(NodeInfo& ninfo) { write(ninfo.prev, ninfo.cost, ninfo.partialCost, ninfo.isVisited, ninfo.isTarget); }

	int getOccChange(int batchStamp) {
		if (batchStamp != occChangeBatchStamp)
			// batchStamp > occChangeBatchStamp, invalid record
			return 0;

		return occChange;
	}

	void incOccChange(int batchStamp) {
		if (batchStamp != occChangeBatchStamp) {
			// batchStamp > occChangeBatchStamp, invalid record
			occChangeBatchStamp = batchStamp;
			occChange = 1;
		} else {
			occChange ++;
		}
	}

	void decOccChange(int batchStamp) {
		if (batchStamp != occChangeBatchStamp) {
			// batchStamp > occChangeBatchStamp, invalid record
			occChangeBatchStamp = batchStamp;
			occChange = -1;
		} else {
			occChange --;
		}
	}
};

// Static assertion to ensure NodeInfo is exactly 64 bytes (one cacheline)
static_assert(sizeof(NodeInfo) == 64, "NodeInfo must be exactly 64 bytes for optimal cache performance");