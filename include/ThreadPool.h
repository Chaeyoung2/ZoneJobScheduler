#pragma once
#include <cstddef>
#include <memory>
#include <vector>

class Worker;
class ZoneScheduler;

class ThreadPool
{
public:
	ThreadPool(std::size_t numWorkers, ZoneScheduler& scheduler);
	~ThreadPool();

	void join();

private:
	ZoneScheduler& m_zoneScheduler;
	std::vector<std::unique_ptr<Worker>> m_workers;
};
