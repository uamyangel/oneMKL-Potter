#pragma once
#include "global.h"
#include <set>

class RouteResult {
public:
	int netId = -1;
	std::set<RouteNode*> branches;
};
