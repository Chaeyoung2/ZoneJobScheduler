#include "SchedulerTest.h"

#include "Producer.h"
#include "ThreadPool.h"
#include "Zone.h"
#include "ZoneScheduler.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace zonejobscheduler::tests
{
	namespace
	{

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


			const bool allJobCountsReturnedToZero = std::ranges::all_of(
				activeJobCounts,
				[](const std::atomic<int>& activeJobCount)
				{
					return activeJobCount.load(std::memory_order_relaxed) == 0;
				});

			return actualJobCount == expectedJobCount
				&& wasConcurrentExecutionDetected == false
				&& allJobCountsReturnedToZero;
		}

		bool runActorStateTest()
		{
			constexpr int damageJobCount = 25;
			constexpr int expectedHp = 75;
			constexpr int zoneCount = 1;
			constexpr std::size_t workerCount = 2;
			constexpr int damageAmount = 1;

			std::atomic<bool> allActorsHaveExpectedHp{ false };

			ZoneScheduler scheduler(zoneCount);
			ThreadPool threadPool(workerCount, scheduler);

			for (int i = 0; i < damageJobCount; ++i)
			{
				scheduler.submit(0, [damageAmount](Zone& zone)
					{
						zone.takeDamageAll(damageAmount);
					});
			}

			const bool validationAccepted = scheduler.submit(
				0,
				[expectedHp, &allActorsHaveExpectedHp](Zone& zone)
				{
					allActorsHaveExpectedHp.store(zone.allActorsHaveHp(expectedHp), std::memory_order_relaxed);
				});

			scheduler.shutDown();
			threadPool.join();

			const bool passed = validationAccepted && allActorsHaveExpectedHp.load(std::memory_order_relaxed);

			std::cout
				<< "All actors have expected HP: "
				<< std::boolalpha
				<< passed
				<< '\n';

			return passed;
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
			constexpr int zoneCount = 2;
			constexpr int workerCount = 2;
			constexpr int expectedReadyJobCount = 2;
			constexpr auto synchronizationTimeout = std::chrono::seconds(1);

			ZoneScheduler scheduler(zoneCount);
			ThreadPool threadPool(workerCount, scheduler);

			std::mutex gateMutex;
			std::condition_variable gateCv;

			int readyJobCount = 0;
			bool releaseJobs = false;

			Job parallelJob = [&](Zone&)
				{
					std::unique_lock<std::mutex> lock(gateMutex);

					++readyJobCount;
					gateCv.notify_all();

					gateCv.wait(lock, [&]()
						{
							return releaseJobs;
						});
				};

			scheduler.submit(0, parallelJob);
			scheduler.submit(1, parallelJob);

			bool didBothJobsReachGate = false;

			{
				std::unique_lock<std::mutex> lock(gateMutex);

				didBothJobsReachGate = gateCv.wait_for(lock, synchronizationTimeout, [&]()
					{
						return readyJobCount == expectedReadyJobCount;
					});

				releaseJobs = true;
			}

			gateCv.notify_all();

			scheduler.shutDown();
			threadPool.join();

			std::cout
				<< "Different-zone overlap detected: "
				<< std::boolalpha
				<< didBothJobsReachGate
				<< '\n';

			return didBothJobsReachGate;
		}

		bool runSubmitShutdownBoundaryTest()
		{
			constexpr int zoneCount = 2;
			constexpr int workerCount = 2;
			constexpr int initialJobCount = 100;

			std::atomic<int> acceptedJobCount = 0;
			std::atomic<int> executedJobCount = 0;
			std::atomic<bool> submitterAcceptedJob = false;
			std::atomic<bool> rejectionObserved = false;

			ZoneScheduler scheduler(zoneCount);
			ThreadPool threadPool(workerCount, scheduler);

			Job job = [&](Zone&)
				{
					executedJobCount.fetch_add(1, std::memory_order_relaxed);
				};

			for (int i = 0; i < initialJobCount; ++i)
			{
				const int zoneId = i % zoneCount;

				if (scheduler.submit(zoneId, job))
				{
					acceptedJobCount.fetch_add(1, std::memory_order_relaxed);
				}
			}

			std::thread submitter([&]()
				{
					int zoneId = 0;

					while (scheduler.submit(zoneId, job))
					{
						acceptedJobCount.fetch_add(1, std::memory_order_relaxed);

						if (submitterAcceptedJob.exchange(
							true, std::memory_order_release) == false)
						{
							submitterAcceptedJob.notify_one();
						}

						zoneId = (zoneId + 1) % zoneCount;
						std::this_thread::yield();
					}

					rejectionObserved.store(true, std::memory_order_relaxed);
				});

			submitterAcceptedJob.wait(false, std::memory_order_acquire);
			scheduler.shutDown();

			submitter.join();
			threadPool.join();

			const bool submitAfterShutdownRejected = scheduler.submit(0, job) == false;

			const int acceptedCount = acceptedJobCount.load(std::memory_order_relaxed);
			const int executedCount = executedJobCount.load(std::memory_order_relaxed);

			const bool wereAllAcceptedJobsExecuted = acceptedCount == executedCount;
			const bool wasConcurrentSubmissionRejected = rejectionObserved.load(std::memory_order_relaxed);

			std::cout
				<< "Accepted jobs during shutdown: "
				<< acceptedCount
				<< '\n'
				<< "Executed jobs during shutdown: "
				<< executedCount
				<< '\n'
				<< "Submitter observed shutdown: "
				<< std::boolalpha
				<< wasConcurrentSubmissionRejected
				<< '\n'
				<< "Submit after shutdown rejected: "
				<< submitAfterShutdownRejected
				<< '\n';

			return wereAllAcceptedJobsExecuted
				&& wasConcurrentSubmissionRejected
				&& submitAfterShutdownRejected;
		}

		bool runAutomaticThreadCleanupTest()
		{
			constexpr int zoneCount = 2;
			constexpr int workerCount = 2;
			constexpr int jobsPerProducer = 100;
			constexpr int expectedJobCount = zoneCount * jobsPerProducer;

			std::atomic<int> executedJobCount = 0;

			JobFactory jobFactory = [&](int) -> Job
				{
					return [&executedJobCount](Zone&)
						{
							executedJobCount.fetch_add(1, std::memory_order_relaxed);
						};
				};

			ZoneScheduler scheduler(zoneCount);

			{
				ThreadPool threadPool(workerCount, scheduler);

				{
					Producer producer(scheduler, jobsPerProducer, zoneCount, jobFactory);
				}
			}

			const int actualJobCount = executedJobCount.load(std::memory_order_relaxed);
			const bool wereAllJobsExecuted = (actualJobCount == expectedJobCount);
			const bool submitAfterCleanupRejected = (scheduler.submit(0, jobFactory(0)) == false);

			std::cout
				<< "Automatic cleanup executed jobs: "
				<< actualJobCount
				<< " / "
				<< expectedJobCount
				<< '\n'
				<< "Submit after automatic cleanup rejected: "
				<< std::boolalpha
				<< submitAfterCleanupRejected
				<< '\n';

			return wereAllJobsExecuted
				&& submitAfterCleanupRejected;
		}
	}

	bool runAllTests()
	{
		const bool jobExecutionPassed = runJobExecutionTest();
		const bool actorStatePassed = runActorStateTest();
		const bool fifoPassed = runSameZoneFifoTest();
		const bool parallelExecutionPassed = runDifferentZoneParallelismTest();
		const bool concurrentShutdownPassed = runSubmitShutdownBoundaryTest();
		const bool automaticThreadCleanupPassed = runAutomaticThreadCleanupTest();

		return jobExecutionPassed
			&& actorStatePassed
			&& fifoPassed
			&& parallelExecutionPassed
			&& concurrentShutdownPassed
			&& automaticThreadCleanupPassed;
	}
}
