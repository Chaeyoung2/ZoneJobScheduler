#pragma once
#include "JobQueue.h"
#include <thread>
#include <vector>

class ThreadPool
{
public:
	explicit ThreadPool(JobQueue& queue, size_t numWorkers = std::thread::hardware_concurrency())
		:m_queue(queue)
	{
		numWorkers = numWorkers == 0 ? 1 : numWorkers;

		// consumer
		for (int i = 0; i < numWorkers; ++i)
		{
			m_workers.emplace_back([&] {workerLoop(); });
		}
		 
	}

	~ThreadPool()
	{
		m_queue.shutdown();
		
		for (auto& t : m_workers)
		{
			t.join();
		}
	}

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	JobQueue& m_queue;
	std::vector<std::thread> m_workers;

	void workerLoop()
	{
		JobQueue::Job job;
		while (m_queue.pop(job))
		{
			job();
		}
	}
};