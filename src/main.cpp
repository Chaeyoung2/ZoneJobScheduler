#include "ZoneScheduler.h"
#include "Zone.h"
#include "ThreadPool.h"
#include "Producer.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>


bool runJobExecutionTest();
bool runSameZoneFifoTest();
bool runDifferentZoneParallelismTest();

bool runJobExecutionTest()
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
			return [&, zoneId](Zone& zone)
				{
					const int previousActiveCount = activeJobCounts[zoneId].fetch_add(1, std::memory_order_relaxed);

					if (previousActiveCount != 0)
					{
						concurrentExecutionDetected.store(true, std::memory_order_relaxed);
					}

					std::this_thread::yield();

					zone.takeDamageAll(1);

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


	bool allJobCountsReturnedToZero = true;

	for (const auto& activeJobCount : activeJobCounts)
	{
		if (activeJobCount.load(std::memory_order_relaxed) != 0)
		{
			allJobCountsReturnedToZero = false;
			break;
		}
	}

	return actualJobCount == expectedJobCount
		&& wasConcurrentExecutionDetected == false
		&& allJobCountsReturnedToZero;
}

bool runActorStateTest()
{
	constexpr int damageJobCount = 25;
	constexpr int expectedHp = 75;
	constexpr int zoneCount = 1;
	constexpr size_t workerCount = 2;
	constexpr int damageAmount = 1;

	ZoneScheduler scheduler(zoneCount);
	ThreadPool threadPool(workerCount, scheduler);

	for (int i = 0; i < damageJobCount; ++i)
	{
		scheduler.submit(0, [damageAmount](Zone& zone)
			{
				zone.takeDamageAll(damageAmount);
			});
	}

	scheduler.shutDown();
	threadPool.join();

	const bool allActorsHaveExpectedHp = 
		scheduler.getZone(0).allActorsHaveHp(expectedHp);

	std::cout
		<< "All actors have expected HP: "
		<< std::boolalpha
		<< allActorsHaveExpectedHp
		<< '\n';

	return allActorsHaveExpectedHp;
}

bool runSameZoneFifoTest()
{
	constexpr int orderedJobCount = 1000;

	ZoneScheduler orderScheduler(1);
	ThreadPool orderThreadPool(4, orderScheduler);

	std::vector<int> executionOrder(orderedJobCount, -1);

	std::atomic<int> executionIndex = 0;

	for (int jobSequence = 0; jobSequence < orderedJobCount; ++jobSequence)
	{
		orderScheduler.submit(0, [&, jobSequence](Zone&)
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


	return executionIndex.load(std::memory_order_relaxed) == orderedJobCount
		&& wasOrderPreserved;
}

bool runDifferentZoneParallelismTest()
{
	constexpr int parallelZoneCount = 2;
	constexpr int parallelWorkerCount = 2;
	constexpr auto jobDuration = std::chrono::milliseconds(100);

	ZoneScheduler parallelScheduler(parallelZoneCount);
	ThreadPool parallelThreadPool(parallelWorkerCount, parallelScheduler);

	std::atomic<int> activeJobCount = 0;
	std::atomic<bool> differentZoneOverlapDetected = false;

	Job parallelJob = [&](Zone&)
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

	return activeJobCount.load(std::memory_order_relaxed) == 0
		&& wasDifferentZoneOverlapDetected;
}

int main()
{
	const bool jobExecutionPassed = runJobExecutionTest();
	const bool actorStatePassed = runActorStateTest();
	const bool fifoPassed = runSameZoneFifoTest();
	const bool parallelExecutionPassed = runDifferentZoneParallelismTest();

	return jobExecutionPassed &&
		actorStatePassed &&
		fifoPassed &&
		parallelExecutionPassed
		? EXIT_SUCCESS
		: EXIT_FAILURE;
}
