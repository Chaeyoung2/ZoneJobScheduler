#pragma once
#include <thread>
#include <atomic>
#include "ZoneScheduler.h"

class Producer
{
public:
	Producer(ZoneScheduler& scheduler, int jobCount, int totalZoneCount, std::atomic<int>& executedCount)
		: zoneScheduler(scheduler), jobsPerProducer(jobCount), zoneCount(totalZoneCount), executedJobCount(executedCount), producerThread(&Producer::run, this)
	{
	}

	~Producer() 
	{
	}

	void run()
	{
		for (int i = 0; i < zoneCount; i++)
		{
			for (int k = 0; k < jobsPerProducer; k++)
			{
				zoneScheduler.submit(i/*zoneId*/, make_job(i));
			}
		}
		
	}

	void join()
	{
		producerThread.join();
	}

	Job make_job(int zoneId)
	{
		return [&scheduler = zoneScheduler, &executedCount = executedJobCount, zoneId]()
			{
				scheduler.get_zone(zoneId)->take_damage_all(1);
				executedCount.fetch_add(1, std::memory_order_relaxed);
			};
	}

private:
	ZoneScheduler& zoneScheduler;
	int jobsPerProducer;
	const int zoneCount;
	std::atomic<int>& executedJobCount;
	std::thread producerThread;
};
