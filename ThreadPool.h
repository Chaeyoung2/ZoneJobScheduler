#pragma once
#include <vector>
#include <thread>
#include <memory>
#include "ZoneScheduler.h"
#include "Worker.h"

class ThreadPool
{
public:
    ThreadPool(size_t _num_workers, ZoneScheduler& _zonescheduler)
    {
		workers.reserve(_num_workers);

        for (int i = 0; i < _num_workers; i++)
		{
            workers.emplace_back(std::make_unique<Worker>(_zonescheduler));
		}
    }
    ~ThreadPool()
    {
        for (auto& w : workers)
        {
            w->join();
        }
    }

private:
    std::vector<std::unique_ptr<Worker>> workers;
};
