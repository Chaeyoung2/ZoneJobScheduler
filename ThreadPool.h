#pragma once
#include <vector>
#include <thread>
#include <memory>
#include "ZoneScheduler.h"
#include "Worker.h"

class ThreadPool
{
public:
    ThreadPool(size_t numWorkers, ZoneScheduler& scheduler)
    {
		workers.reserve(numWorkers);

        for (int i = 0; i < numWorkers; i++)
		{
            workers.emplace_back(std::make_unique<Worker>(scheduler));
		}
    }
    ~ThreadPool()
    {
    }

    void join()
    {
        for (auto& w : workers)
        {
            w->join();
        }
    }

private:
    std::vector<std::unique_ptr<Worker>> workers;
};
