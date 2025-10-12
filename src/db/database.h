#pragma once
#include "global.h"
#include "connection.h"
#include "routeNode.h"
#include "routeNodeGraph.h"
#include "utils/geo.h"
#include "netlist.h"
#include "device.h"

#include <thread>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <sys/stat.h>
#include <filesystem>

class Database
{
public:
	Database() : layout(0, 0, 108, 300), device(routingGraph), 
		netlist(device, nets, indirectConnections, directConnections, preservedNodes, routingGraph, layout){}
	void readDevice(string deviceName);
	void readNetlist(string netlistName);
	void writeNetlist(string netlistName, const vector<RouteResult>& nodeRoutingResults) {netlist.write(netlistName, nodeRoutingResults);}
	void reduceRouteNode();
	void setRouteNodeChildren();
	void printStatistic();
	void checkRoute();
	void setNumThread(int n) { numThread = n; netlist.numThread = n; }
	int getNumThread() {return numThread;}

	vector<Connection> indirectConnections;
	vector<Connection> directConnections;
	vector<Net> nets;
	vector<bool> preservedNodes;

	RouteNodeGraph routingGraph;
	int numNodes = 0;
	int numNodesInRRG = 0;
	int numEdges = 0;
	int numEdgesInRRG = 0;
	int numConns = 0;
	int numNets = 0;
	utils::BoxT<int> layout;
	int preservedNum = 0;
	bool useRW = false;

	Raw::Device device;
	Raw::Netlist netlist;

	std::string inputName;

	
private:
	int numThread = 16;
};