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
	std::vector<std::unique_ptr<Worker>> m_workers;
};
