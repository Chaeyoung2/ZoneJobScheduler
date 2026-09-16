#pragma once

#include "Job.h"

#include <functional>
#include <thread>

class ZoneScheduler;

using JobFactory = std::function<Job(int)>;

class Producer
{
public:
	Producer(
		ZoneScheduler& scheduler,
		int jobCount,
		int totalZoneCount,
		JobFactory factory);

	~Producer();

	void run();
	void join();

private:
	ZoneScheduler& zoneScheduler;
	int jobsPerProducer;
	const int zoneCount;
	JobFactory jobFactory;
	std::thread producerThread;
};
