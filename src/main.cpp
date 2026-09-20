#include "ZoneScheduler.h"
#include "Zone.h"
#include "ThreadPool.h"
#include "Producer.h"

#include <chrono>
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

					scheduler.getZone(zoneId).takeDamageAll(1);

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

	scheduler.shutDown();
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

	// FIFO 검증
	{
		constexpr int orderedJobCount = 1000;

		ZoneScheduler orderScheduler(1);
		ThreadPool orderThreadPool(4, orderScheduler);

		std::vector<int> executionOrder(orderedJobCount, -1);

		std::atomic<int> executionIndex = 0;

		for (int jobSequence = 0; jobSequence < orderedJobCount; ++jobSequence)
		{
			orderScheduler.submit(0, [&, jobSequence]()
				{
					const int index = executionIndex.fetch_add(1, std::memory_order_relaxed);
					executionOrder[index] = jobSequence;
				});
		}

		orderScheduler.shutDown();
		orderThreadPool.join();

		bool wasOrderPreserved = true;

		for (int expectedSequence = 0; expectedSequence < orderedJobCount; ++expectedSequence)
		{
			if (executionOrder[expectedSequence] != expectedSequence)
			{
				wasOrderPreserved = false;
				break;
			}
		}

		std::cout
			<< "Same-zone FIFO preserved: "
			<< std::boolalpha
			<< wasOrderPreserved
			<< '\n';

		assert(executionIndex.load(std::memory_order_relaxed) == orderedJobCount);
		assert(wasOrderPreserved);
	}

	// 서로 다른 Zone의 병렬 실행 검증
	{
		constexpr int parallelZoneCount = 2;
		constexpr int parallelWorkerCount = 2;
		constexpr auto jobDuration = std::chrono::milliseconds(100);

		ZoneScheduler parallelScheduler(parallelZoneCount);
		ThreadPool parallelThreadPool(parallelWorkerCount, parallelScheduler);

		std::atomic<int> activeJobCount = 0;
		std::atomic<bool> differentZoneOverlapDetected = false;

		Job parallelJob = [&]()
			{
				const int previousActiveJobCount = activeJobCount.fetch_add(1, std::memory_order_relaxed);

				if (previousActiveJobCount > 0)
				{
					differentZoneOverlapDetected.store(true, std::memory_order_relaxed);
				}

				std::this_thread::sleep_for(jobDuration);

				activeJobCount.fetch_sub(1, std::memory_order_relaxed);
			};

		parallelScheduler.submit(0, parallelJob);
		parallelScheduler.submit(1, parallelJob);

		parallelScheduler.shutDown();
		parallelThreadPool.join();


		const bool wasDifferentZoneOverlapDetected = differentZoneOverlapDetected.load(std::memory_order_relaxed);

		std::cout
			<< "Different-zone overlap detected: "
			<< std::boolalpha
			<< wasDifferentZoneOverlapDetected
			<< '\n';

		assert(activeJobCount.load(std::memory_order_relaxed) == 0);
		assert(wasDifferentZoneOverlapDetected);
	}
}
