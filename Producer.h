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
	ZoneScheduler& m_zoneScheduler;
	const int m_jobsPerProducer;
	const int m_zoneCount;
	const JobFactory m_jobFactory;
	std::thread m_producerThread;
};
