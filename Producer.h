#pragma once
#include <functional>
#include <thread>
#include <utility>

#include "ZoneScheduler.h"

using JobFactory = std::function<Job(int)>;

class Producer
{
public:
	Producer(ZoneScheduler& scheduler, int jobCount, int totalZoneCount, JobFactory factory)
		: zoneScheduler(scheduler), jobsPerProducer(jobCount), zoneCount(totalZoneCount), jobFactory(std::move(factory)), producerThread(&Producer::run, this)
	{
	}

	~Producer() 
	{
	}

	void run()
	{
		for (int zoneId = 0; zoneId < zoneCount; ++zoneId)
		{
			for (int jobIndex = 0; jobIndex < jobsPerProducer; ++jobIndex)
			{
				zoneScheduler.submit(zoneId, jobFactory(zoneId));
			}
		}
		
	}

	void join()
	{
		producerThread.join();
	}

private:
	ZoneScheduler& zoneScheduler;
	int jobsPerProducer;
	const int zoneCount;
	JobFactory jobFactory;
	std::thread producerThread;
};
