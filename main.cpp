#include "ZoneScheduler.h"
#include "ThreadPool.h"
#include "Producer.h"

#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>
#include <array>
#include <thread>

int main()
{
	constexpr int zoneCount = 4;
	constexpr int workerCount = 8;
	constexpr int producerCount = 4;
	constexpr int jobsPerProducer = 1000;
	constexpr int expectedJobCount = producerCount * zoneCount * jobsPerProducer;

	std::atomic<int> executedJobCount = 0;

	std::array<std::atomic<int>, zoneCount> activeJobCounts{};
	std::atomic<bool> concurrentExecutionDetected = false;

	ZoneScheduler scheduler(zoneCount);
	ThreadPool threadPool(workerCount, scheduler);

	JobFactory jobFactory = [&](int zoneId) -> Job
		{
			return [&, zoneId]()
				{
					const int previousActiveCount = activeJobCounts[zoneId].fetch_add(1, std::memory_order_relaxed);

					if (previousActiveCount != 0)
					{
						concurrentExecutionDetected.store(true, std::memory_order_relaxed);
					}

					std::this_thread::yield();

					scheduler.get_zone(zoneId)->take_damage_all(1);

					activeJobCounts[zoneId].fetch_sub(1, std::memory_order_relaxed);

					executedJobCount.fetch_add(1, std::memory_order_relaxed);
				};
		};

	std::vector<std::unique_ptr<Producer>> producers;
	producers.reserve(producerCount);

	for (int i = 0; i < producerCount; i++)
	{
		producers.push_back(std::make_unique<Producer>(
			scheduler, 
			jobsPerProducer, 
			zoneCount, 
			jobFactory));
	}

	for (auto& producer : producers)
	{
		producer->join();
	}

	scheduler.shut_down();
	threadPool.join();

	const int actualJobCount = executedJobCount.load(std::memory_order_relaxed);

	const bool wasConcurrentExecutionDetected = concurrentExecutionDetected.load(std::memory_order_relaxed);

	std::cout
		<< "Expected jobs: " << expectedJobCount << '\n'
		<< "Executed jobs: " << actualJobCount << '\n'
		<< "Same-zone overlap detected: "
		<< std::boolalpha
		<< wasConcurrentExecutionDetected
		<< '\n';

	assert(actualJobCount == expectedJobCount);
	assert(wasConcurrentExecutionDetected == false);

	for (const auto& activeJobCount : activeJobCounts)
	{
		assert(activeJobCount.load(std::memory_order_relaxed) == 0);
	}
}
