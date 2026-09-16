#include "ThreadPool.h"

#include "Worker.h"
#include "ZoneScheduler.h"

ThreadPool::ThreadPool(
	std::size_t numWorkers,
	ZoneScheduler& scheduler)
{
	workers.reserve(numWorkers);

	for (std::size_t i = 0; i < numWorkers; ++i)
	{
		workers.push_back(std::make_unique<Worker>(scheduler));
	}
}

ThreadPool::~ThreadPool() = default;

void ThreadPool::join()
{
	for (auto& worker : workers)
	{
		worker->join();
	}
}
