#include "MTStat.h"
#include <thread>
#include <mutex>

MTStat runJobsMT(int numJobs, int numThreads_, const std::function<void(int)>& handle) 
{
    int numThreads = std::min(numJobs, numThreads_);
    MTStat mtStat(std::max(1, numThreads_));
    if (numThreads <= 1) {
        utils::timer threadTimer;
        for (int i = 0; i < numJobs; ++i) {
            handle(i);
        }
        mtStat.durations[0] = threadTimer.elapsed();
    } else {
        int globalJobIdx = 0;
        std::mutex mtx;
        utils::timer threadTimer;
        auto thread_func = [&](int threadIdx) {
            int jobIdx;
            while (true) {
                mtx.lock();
                jobIdx = globalJobIdx++;
                mtx.unlock();
                if (jobIdx >= numJobs) {
                    mtStat.durations[threadIdx] = threadTimer.elapsed();
                    break;
                }
                handle(jobIdx);
            }
        };

        std::thread threads[numThreads];
        for (int i = 0; i < numThreads; i++) {
            threads[i] = std::thread(thread_func, i);
        }
        for (int i = 0; i < numThreads; i++) {
            threads[i].join();
        }
    }
    return mtStat;
}