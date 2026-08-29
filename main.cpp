#include "JobQueue.h"
#include "ThreadPool.h"
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <iostream>
#include <chrono>

void test_no_lost_or_duplicate_jobs()
{
	JobQueue queue;
	constexpr int kProducers = 4;
	constexpr int kJobsPerProducer = 1000;
	std::atomic<int> executedCount{ 0 };

	constexpr int kConsumers = 3;
	std::vector<std::thread> consumers;
	for (int i = 0; i < kConsumers; ++i)
	{
		consumers.emplace_back([&queue, &executedCount]
			{
				JobQueue::Job job;
				while (queue.pop(job))
				{
					job();
				}
			});
	}

	std::vector<std::thread> producers;
	for (int i = 0; i < kProducers; ++i)
	{
		producers.emplace_back([&queue, &executedCount]
			{
				for (int j = 0; j < kJobsPerProducer; ++j)
				{
					queue.push([&executedCount] { executedCount++; });
				}
			});
	}

	for (auto& t : producers)
		t.join();

	queue.shutdown();
	for (auto& t : consumers)
		t.join();

	int expected = kProducers * kJobsPerProducer;
	std::cout << "[no_lost_or_duplicate] expected=" << expected << " actual=" << executedCount << "\n";
	assert(executedCount == expected);
}

void test_with_thread_pool()
{
	JobQueue queue;
	constexpr int kProducers = 4;
	constexpr int kJobsPerProducer = 1000;
	std::atomic<int> executedCount{ 0 };
	{
		ThreadPool pool(queue);

		std::vector<std::thread> producers;
		for (int i = 0; i < kProducers; ++i)
		{
			producers.emplace_back([&queue, &executedCount]
				{
					for (int j = 0; j < kJobsPerProducer; ++j)
					{
						queue.push([&executedCount] { executedCount++; });
					}
				});
		}

		for (auto& t : producers)
			t.join();
	}

	int expected = kProducers * kJobsPerProducer;
	std::cout << "[no_lost_or_duplicate] expected=" << expected << " actual=" << executedCount << "\n";
	assert(executedCount == expected);
}

int main()
{
	test_with_thread_pool();
	std::cout << "PASS\n";
}