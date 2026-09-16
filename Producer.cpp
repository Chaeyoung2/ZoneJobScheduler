#include "Producer.h"

#include "ZoneScheduler.h"

#include <utility>

Producer::Producer(
	ZoneScheduler& scheduler,
	int jobCount,
	int totalZoneCount,
	JobFactory factory)
	: zoneScheduler(scheduler),
	  jobsPerProducer(jobCount),
	  zoneCount(totalZoneCount),
	  jobFactory(std::move(factory)),
	  producerThread(&Producer::run, this)
{
}

Producer::~Producer() = default;

void Producer::run()
{
	for (int zoneId = 0; zoneId < zoneCount; ++zoneId)
	{
		for (int jobIndex = 0; jobIndex < jobsPerProducer; ++jobIndex)
		{
			zoneScheduler.submit(zoneId, jobFactory(zoneId));
		}
	}
}

void Producer::join()
{
	producerThread.join();
}
