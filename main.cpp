#include "ZoneScheduler.h"
#include "ThreadPool.h"
#include "Producer.h"

#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

int main()
{
	constexpr int zoneCount = 4;
	constexpr int workerCount = 8;
	constexpr int producerCount = 4;
	constexpr int jobsPerProducer = 1000;
	constexpr int expectedJobCount = producerCount * zoneCount * jobsPerProducer;

	std::atomic<int> executedJobCount = 0;

	ZoneScheduler scheduler(zoneCount);
	ThreadPool threadPool(workerCount, scheduler);

	std::vector<std::unique_ptr<Producer>> producers;
	producers.reserve(producerCount);

	for (int i = 0; i < producerCount; i++)
	{
		producers.push_back(std::make_unique<Producer>(
			scheduler, 
			jobsPerProducer, 
			zoneCount, 
			executedJobCount));
	}

	for (auto& producer : producers)
	{
		producer->join();
	}

	scheduler.shut_down();
	threadPool.join();

	const int actualJobCount = executedJobCount.load(std::memory_order_relaxed);

	std::cout 
		<< "Expected jobs: " << expectedJobCount << '\n'
		<< "Executed jobs: " << actualJobCount << '\n';

	assert(actualJobCount == expectedJobCount);
}
