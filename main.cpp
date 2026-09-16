#include "ZoneScheduler.h"
#include "ThreadPool.h"
#include "Actor.h"
#include "Producer.h"
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>

int main()
{
	const int zoneCount = 4;
	const int workerThreadCount = 8;
	const int producerThreadCount = 4;
	const int jobsPerProducer = 1000;
	const int actorCount = 100;
	const int expectedJobCount = producerThreadCount * zoneCount * jobsPerProducer;

	std::atomic<int> executedJobCount = 0;

	// zone scheduler를 만든다.
	ZoneScheduler scheduler(zoneCount);

	// thread pool을 만든다.
	ThreadPool pool(workerThreadCount, scheduler);

	// producer thread를 만든다.
	// // thread는 복사할 수 없으므로 producer 객체를 vector 안에 직접 저장하지 말고,
	// // 주소가 안정적인 별도 객체로 생성하여 unique_ptr을 저장하는 방향이 적절하다. (vector가 재할당되더라도 이동하는 것은 unique_ptr임)
	std::vector<std::unique_ptr<Producer>> producers;

	for (int i = 0; i < producerThreadCount; i++)
	{
		producers.emplace_back(
			std::make_unique<Producer>(scheduler, jobsPerProducer, zoneCount, executedJobCount));
	}

	for (auto& p : producers)
		p->join();

	scheduler.shut_down();

	pool.join();

	const int actualJobCount = executedJobCount.load(std::memory_order_relaxed);

	std::cout << "Expected jobs: " << expectedJobCount << '\n'
		<< "Executed jobs: " << actualJobCount << '\n';

	assert(actualJobCount == expectedJobCount);
}
