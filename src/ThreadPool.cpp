#include "ThreadPool.h"

#include "Worker.h"
#include "ZoneScheduler.h"

ThreadPool::ThreadPool(
	std::size_t numWorkers,
	ZoneScheduler& scheduler)
	: m_zoneScheduler(scheduler)
{
	m_workers.reserve(numWorkers);

	for (std::size_t i = 0; i < numWorkers; ++i)
	{
		m_workers.push_back(std::make_unique<Worker>(scheduler));
	}
}

ThreadPool::~ThreadPool()
{
	m_zoneScheduler.shutDown();
	join();
}

void ThreadPool::join()
{
	for (auto& worker : m_workers)
	{
		worker->join();
	}
}
