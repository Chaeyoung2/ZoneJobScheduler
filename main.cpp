#include "JobQueue.h"
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <iostream>

int main()
{
	JobQueue job_queue;
	std::atomic<int> popped_count{ 0 };

	const int producer_count = 4;
	const int jobs_per_producer = 1000;
	const int consumer_count = 4;

	std::vector<std::thread> producers;
	std::vector<std::thread> consumers;

	// producer 스레드 생성
	for (int i = 0; i < producer_count; ++i)
	{
		producers.emplace_back([&job_queue]()
			{
				for (int j = 0; j < jobs_per_producer; ++j)
				{
					Job job = [] {int a = 2; };
					job_queue.push(job);
				}
			});
	}

	// consumer 스레드 생성
	for (int i = 0; i < consumer_count; ++i)
	{
		consumers.emplace_back([&job_queue, &popped_count]()
			{
				Job job = [] { };
				while(job_queue.pop(job)) {
					job(); 
					popped_count++;
				}
			}
			);
	}

	for (auto& producer : producers)
	{
		producer.join();
	}

	job_queue.shut_down();

	for (auto& consumer : consumers)
	{
		consumer.join();
	}

	assert(popped_count = producer_count * jobs_per_producer);
}
