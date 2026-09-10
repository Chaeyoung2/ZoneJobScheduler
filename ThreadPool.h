#pragma once
#include <vector>
#include <thread>
#include "JobQueue.h"

class ThreadPool
{
public:
    ThreadPool(size_t _num_workers, JobQueue& _queue)
        : queue(_queue)
    {
        for (int i = 0; i < _num_workers; i++)
		{
			std::function<void()> f = [this]() {
				while (true)
				{
                    Job job;
					if (queue.pop(job) == false)
						break;
					else
						job();
				}};
			
			std::thread t(f);
			workers.push_back(std::move(t));
		}
    }
    ~ThreadPool()
    {
        for (auto& w : workers)
        {
            w.join();
        }
    }

private:
    JobQueue& queue;
    std::vector<std::thread> workers;
};
