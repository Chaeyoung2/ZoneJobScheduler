#include "ThreadPool.h"

#include "Worker.h"
#include "ZoneScheduler.h"

#include <stdexcept>

ThreadPool::ThreadPool(
	std::size_t numWorkers,
	ZoneScheduler& scheduler)
	: m_zoneScheduler(scheduler)
{
	if (numWorkers == 0)
	{
		throw std::invalid_argument(
			"numWorkers must be greater than zero");
	}

	m_workers.reserve(numWorkers);

	try
	{
		for (std::size_t i = 0; i < numWorkers; ++i)
		{
			m_workers.push_back(std::make_unique<Worker>(scheduler));
		}
	}
	catch (...)
	{
		m_zoneScheduler.shutDown();
		join();
		throw;
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
