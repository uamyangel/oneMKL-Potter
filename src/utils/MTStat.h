#pragma once

#include "log.h"
#include <vector>
#include <functional>

class MTStat {
public:
    std::vector<double> durations;
    MTStat(int numOfThreads = 0) : durations(numOfThreads, 0.0) {}
    const MTStat& operator+=(const MTStat& rhs);
    friend std::ostream& operator<<(std::ostream& os, const MTStat mtStat);
};

MTStat runJobsMT(int numJobs, int numThreads, const std::function<void(int)>& handle);