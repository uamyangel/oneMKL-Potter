#pragma once
#include "global.h"
#include "utils/geo.h"
#include "routeNode.h"
#include<set>

class Net {
public:
	Net(int id_) : id(id_), indirectSource(-1) {}
	Net(int id_, int source_) : id(id_), indirectSource(source_) {}
	Net() : indirectSource(-1) {};

	int getIndirectSource() const {return indirectSource;}
	vector<int> getIndirectSinks() const {return indirectSinks;}
	RouteNode* getIndirectSourceRNode() const {return indirectSourceRNode;}
	vector<RouteNode*> getIndirectSinkRNodes() const {return indirectSinkRNodes;}
	int getIndirectSourcePin() const {return indirectSourcePin;}
	vector<int> getIndirectSinkPins() const {return indirectSinkPins;}
	RouteNode* getIndirectSourcePinRNode() const {return indirectSourcePinRNode;}
	vector<RouteNode*> getIndirectSinkPinRNodes() const {return indirectSinkPinRNodes;}

	int getDirectSourcePin() const {return directSourcePin;}
	vector<int> getDirectSinkPins() const {return directSinkPins;}
	RouteNode* getDirectSourcePinRNode() const {return directSourcePinRNode;}
	vector<RouteNode*> getDirectSinkPinRNodes() const {return directSinkPinRNodes;}

	int getId() const {return id;}
	vector<int> getConnections() const {return indirectConns;}
	vector<int>& getConnectionsByRef() {return indirectConns;}
	vector<int> getDirectConnections() const {return directConns;}
	int getConnectionSize() const {return indirectConns.size();}
	int getDirectConnectionSize() const {return directConns.size();}
	double getXCenter() const {return xCenter;}
	double getYCenter() const {return yCenter;}
	int getXMinBB() const {return xmin;}
	int getXMaxBB() const {return xmax;}
	int getYMinBB() const {return ymin;}
	int getYMaxBB() const {return ymax;}
	int getArea() const {return (xmax - xmin) * (ymax - ymin);}
	short getDoubleHpwl() const {return doubleHpwl;}
	int getOriId() const {return oriId;}
    // vector<std::pair<str_idx, str_idx>> getSourceSitePins() const {return sourceSitePins;}
    // int getSourceSitePinSize() const {return sourceSitePins.size();}
    // vector<std::pair<str_idx, str_idx>> getSinkSitePins() const {return sinkSitePins;}
    // int getSinkSitePinSize() const {return sinkSitePins.size();}
	// vector<obj_idx> getSinkPinIds() const {return sinkPinIds;}
	bool getHasSubNet() const {return hasSubNet;}
	bool getIsSubNet() const {return isSubNet;}
	bool isLabeled() const {return labeled;}
	vector<int> getSubNetIds() const {return subNetIds;}

	int countConnectionsOfUser(RouteNode* rnode) const {
		if (usersConnectionCounts.find(rnode) != usersConnectionCounts.end()) {
			int cnt = usersConnectionCounts.at(rnode);
			return cnt;
		}
		return 0;
	}
	bool incrementUser(RouteNode* rnode) {
		if (usersConnectionCounts.find(rnode) == usersConnectionCounts.end()) {
			usersConnectionCounts[rnode] = 1; 
			return true;
		}
		else {
			usersConnectionCounts[rnode] ++;
			return false;
		}
	} 
	bool decrementUser(RouteNode* rnode) {
		usersConnectionCounts[rnode] --;
		if (usersConnectionCounts[rnode] == 0) {
			usersConnectionCounts.erase(rnode);
			return true;
		} else
			return false;
	}

	void preDecrementUser(RouteNode* rnode) {
		if (userConnectionToDecrement.find(rnode) == userConnectionToDecrement.end()) {
			userConnectionToDecrement[rnode] = 1;
		} else {
			userConnectionToDecrement[rnode] ++;
		}
		assert_t(userConnectionToDecrement[rnode] <= usersConnectionCounts[rnode]);
	}

	void updatePreDecrement(int batchStamp) {
		for (auto iter = userConnectionToDecrement.begin(); iter != userConnectionToDecrement.end(); iter ++) {
			RouteNode* rnode = iter->first;
			int cnt = iter->second;
			bool isErased = false;
			auto iter_ = usersConnectionCounts.find(rnode);
			assert_t(iter_ != usersConnectionCounts.end() && cnt <= iter_->second);
			for (int i = 0; i < cnt; i++) {
				isErased |= decrementUser(rnode);
			}
			if (isErased) {
				rnode->decrementOccupancy();
				rnode->setNeedUpdateBatchStamp(batchStamp);
			}
			 
		}
	}

	void clearPreDecrement()  {
		userConnectionToDecrement.clear();
	}

	int getPreDecrementUser(RouteNode* rnode) {
		auto iter = userConnectionToDecrement.find(rnode);
		if (iter == userConnectionToDecrement.end()) {
			return 0;
		} else {
			return iter->second;
		}
	}

	void preIncrementUser(RouteNode* rnode) {
		if (userConnectionToIncrement.find(rnode) == userConnectionToIncrement.end()) {
			userConnectionToIncrement[rnode] = 1;
		} else {
			userConnectionToIncrement[rnode] ++;
		}
	}

	void updatePreIncrement(int batchStamp) {
		for (auto iter = userConnectionToIncrement.begin(); iter != userConnectionToIncrement.end(); iter ++) {
			RouteNode* rnode = iter->first;
			int cnt = iter->second;
			bool newlyAdd = false;
			// auto iter_ = usersConnectionCounts.find(rnode);
			// assert_t(iter_ != usersConnectionCounts.end() && cnt <= iter_->second);
			for (int i = 0; i < cnt; i++) {
				newlyAdd |= incrementUser(rnode);
			}
			if (newlyAdd) {
				rnode->incrementOccupancy();
				rnode->setNeedUpdateBatchStamp(batchStamp);
			}
		}
	}

	void clearPreIncrement() {
		userConnectionToIncrement.clear();
	}

	int getPreIncrementUser(RouteNode* rnode) {
		auto iter = userConnectionToIncrement.find(rnode);
		if (iter == userConnectionToIncrement.end()) {
			return 0;
		} else {
			return iter->second;
		}
	}

	void setIndirectSourceRNode(RouteNode* n) {
		if (indirectSourceRNode == nullptr) {
			indirectSourceRNode = n;
			indirectSource = n->getId();
		}
		else if (indirectSourceRNode != n) {
			log() << "Difference source in net " << id << " " << n->getId() << " vs " << indirectSourceRNode->getId() << endl;
			assert_t(0);
		}
	}
	void addIndirectSinkRNode(RouteNode* n) {
		indirectSinkRNodes.emplace_back(n); 
		indirectSinks.emplace_back(n->getId());
	}
	void setIndirectSourcePinRNode(RouteNode* n) {
		if (indirectSourcePinRNode == nullptr) {
			indirectSourcePinRNode = n;
			indirectSourcePin = n->getId();
		} else if (indirectSourcePinRNode != n) {
			log() << "Difference indirect source pin in net " << id << " " << n->getId() << " vs " << indirectSourcePinRNode->getId() << endl;
			assert_t(0);
		}
	}
	void addIndirectSinkPinRNode(RouteNode* n) {
		indirectSinkPinRNodes.emplace_back(n);
		indirectSinkPins.emplace_back(n->getId());
	}
	void setDirectSourcePinRNode(RouteNode* n) {
		if (directSourcePinRNode == nullptr) {
			directSourcePinRNode = n;
			directSourcePin = n->getId();
		} else if (directSourcePinRNode != n) {
			log() << "Difference indirect source pin in net " << id << " " << n->getId() << " vs " << directSourcePinRNode->getId() << endl;
			assert_t(0);
		}
	}
	void addDirectSinkPinRNode(RouteNode* n) {
		directSinkPinRNodes.emplace_back(n);
		directSinkPins.emplace_back(n->getId());
	}
	void addConns(int conn) {indirectConns.emplace_back(conn);}
	void addDirectConns(int conn) {directConns.emplace_back(conn);}
	void addSubNetId(int subNetId) {subNetIds.emplace_back(subNetId);}
	void setCenter(double x, double y) {xCenter = x; yCenter = y;}
	void setDoubleHpwl(short l) {doubleHpwl = l;}
	void setXMinBB(int x) {xmin = x;}
	void setXMaxBB(int x) {xmax = x;}
	void setYMinBB(int y) {ymin = y;}
	void setYMaxBB(int y) {ymax = y;}
	void updateXMinBB(int x) {xmin = x < xmin ? x : xmin;}
	void updateXMaxBB(int x) {xmax = x > xmax ? x : xmax;}
	void updateYMinBB(int y) {ymin = y < ymin ? y : ymin;}
	void updateYMaxBB(int y) {ymax = y > ymax ? y : ymax;}
	void setId(int v) {id = v;}
	void setOriId(int id) {oriId = id;}
	// void setSourceSitePins(vector<std::pair<str_idx, str_idx>>& sp) {sourceSitePins = sp;}
	// void setSinkSitePins(vector<std::pair<str_idx, str_idx>>& sp) {sinkSitePins = sp;}
	// void addSinkPinIds(obj_idx i) {sinkPinIds.emplace_back(i);}
	void setHasSubNet(bool has) {hasSubNet = has;}
	void setIsSubNet(bool is) {isSubNet = is;}
	void setLabeled(bool is) {labeled = is;}

	friend inline std::ostream& operator<<(std::ostream& os, const Net& net) {
		os << "id: " << net.id << " source: " << net.indirectSource << " sink: " << net.indirectSinks.size() << " | source: " << net.directSourcePin << " sink: " << net.directSinkPins.size(); 
		return os;
	}

private:
	int indirectSource = -1;
	vector<int> indirectSinks;
	RouteNode* indirectSourceRNode = nullptr;
	vector<RouteNode*> indirectSinkRNodes;
	int indirectSourcePin = -1;
	vector<int> indirectSinkPins;
	RouteNode* indirectSourcePinRNode = nullptr; // real pin
	vector<RouteNode*> indirectSinkPinRNodes; // real pin

	int directSourcePin = -1;
	vector<int> directSinkPins;
	RouteNode* directSourcePinRNode = nullptr; // real pin
	vector<RouteNode*> directSinkPinRNodes; // real pin
	unordered_map<RouteNode*, int> usersConnectionCounts;
	unordered_map<RouteNode*, int> userConnectionToDecrement;
	unordered_map<RouteNode*, int> userConnectionToIncrement;

	int id;
	int oriId;
	vector<int> indirectConns; // connectionIds
	vector<int> directConns;
    // vector<std::pair<str_idx, str_idx>> sourceSitePins;
    // vector<std::pair<str_idx, str_idx>> sinkSitePins;
	// vector<obj_idx> sinkPinIds;
	double xCenter;
	double yCenter;
	short doubleHpwl;

	int xmin = INT32_MAX;
	int xmax = INT32_MIN;
	int ymin = INT32_MAX;
	int ymax = INT32_MIN;

	bool hasSubNet = false;
	bool isSubNet = false;
	bool labeled = false;
	vector<int> subNetIds;
};	

class Connection {
public:
	// Connection(int id_, int oriId_, int netId_, int source_, int sink_, int xmin_, int xmax_, int ymin_, int ymax_) : id(id_), oriId(oriId_), netId(netId_), source(source_), sink(sink_) {
	// 	bbox.Set(xmin_, ymin_, xmax_, ymax_);
	// 	xmin = xmin_;
	// 	xmax = xmax_;
	// 	ymin = ymin_;
	// 	ymax = ymax_;
	// }
	Connection(int id_, int netId_, int source_, int sink_) : id(id_), netId(netId_), source(source_), sink(sink_) {
		rnodes.reserve(16);  // Reserve capacity to reduce dynamic reallocations
	}
	const utils::BoxT<int>& getBBox() const {return bbox;}
	int getXMin() const {return xmin;}
	int getXMax() const {return xmax;}
	int getYMin() const {return ymin;}
	int getYMax() const {return ymax;}
	int getXMinBB() const {return bbox.lx();}
	int getXMaxBB() const {return bbox.hx();}
	int getYMinBB() const {return bbox.ly();}
	int getYMaxBB() const {return bbox.hy();}

	int getNetId() const {return netId;}
	int getHPWL() const { return hpwl;}
	bool getRouted() const {return isRouted;}
	bool getRoutedThisIter() const {return isRoutedThisIter;}
	const std::vector<RouteNode*>& getRNodes() const {return rnodes;}
	RouteNode* getRNode(int i) const {return rnodes[i];}
	int getRNodeSize() const {return rnodes.size();}
	int getSource() const {return source;}
	int getSink() const {return sink;}
	int getId() const {return id;}
	// int getOriId() const {return oriId;}
	RouteNode* getSourceRNode() const {return sourceRNode;}
	RouteNode* getSinkRNode() const {return sinkRNode;}
	RouteNode* getTargetRNode() const {return targetRNode;}
	bool isCrossSLR() const {return false;}
    vector<obj_idx> getIntToSinkPath() const {return intToSinkPath;}
    vector<obj_idx> getSourceToIntPath() const {return sourceToIntPath;}
	int getNumNodesExplored() const {return numNodesExplored;}
	int getLastRoutedIter() const {return lastRoutedIter;}
	int getOriNetId() const {return oriNetId;}

	void setNetId(int netId_) {netId = netId_;}
	void setXMin(int v) {xmin = v;}
	void setXMax(int v) {xmax = v;}
	void setYMin(int v) {ymin = v;}
	void setYMax(int v) {ymax = v;}
	void updateBBox() {bbox.Set(xmin, ymin, xmax, ymax);}
	void updateBBox(int xmin, int ymin, int xmax, int ymax) {bbox.Set(xmin, ymin, xmax, ymax);}

	void setSourceRNode(RouteNode* node) {sourceRNode = node;}
	void setSinkRNode(RouteNode* node) {sinkRNode = node;}
	void setTargetRNode(RouteNode* node) {targetRNode = node;}
	void setIntToSinkPath(vector<obj_idx> intToSinkPath_) {intToSinkPath = intToSinkPath_;}
	void setSourceToIntPath(vector<obj_idx> sourceToIntPath_) {sourceToIntPath = sourceToIntPath_;}
	void setNumNodesExplored(int num) {numNodesExplored = num;}
	void setLastRoutedIter(int iter) {lastRoutedIter = iter;}
	void setOriNetId(int oriNetId_) {oriNetId = oriNetId_;}

	void computeHPWL() {
		hpwl = bbox.hp(); //TODO: use the location of source and sink nodes to compute HPWL instead of bbox
	}
	void setRouted(bool routed) {isRouted = routed;}
	void setRoutedThisIter(bool routed) {isRoutedThisIter = routed;}
	void addRNode(RouteNode* rnode) {rnodes.emplace_back(rnode);}
	void resetRoute() {rnodes.clear();}
	bool isCongested() const {
		for (auto rnode : rnodes) {if (rnode->isOverUsed()) return true;}
		return false;
	}

	friend inline std::ostream& operator<<(std::ostream& os, const Connection& conn) {
		os << "id: " << conn.id << " source: " << conn.source << " sink: " << conn.sink << " bbox: " << conn.bbox; 
		return os;
	}

private:
	int source;
	int sink;
	int id;
	int oriId; // original connection id in rapidWright
	int netId; // the id from the indirect connection list; not the original id in rapdiwright
	int oriNetId = -1;
	utils::BoxT<int> bbox;
	int xmin;
	int xmax;
	int ymin;
	int ymax;

	bool isRouted;
	bool isRoutedThisIter;
	int lastRoutedIter = 0;
	int hpwl;
	std::vector<RouteNode*> rnodes; // from sink to source

	RouteNode* sourceRNode = nullptr;
	RouteNode* sinkRNode = nullptr;
	RouteNode* targetRNode = nullptr;

    vector<obj_idx> intToSinkPath;
    vector<obj_idx> sourceToIntPath;

	int numNodesExplored = -1;
};	