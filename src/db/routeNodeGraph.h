#pragma once
#include "global.h"
#include "routeNode.h"
#include "connection.h"
#include <set>


class RouteNodeGraph 
{
public:
	RouteNodeGraph(){};
	vector<RouteNode> routeNodes;
	bool isAccessible(const RouteNode* childRnode, const Connection& connection);
};
