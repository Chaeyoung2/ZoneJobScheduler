#include "ZoneScheduler.h"

#include "Zone.h"

ZoneScheduler::ZoneScheduler(int zoneCount)
{
	for (int zoneId = 0; zoneId < zoneCount; ++zoneId)
	{
		zones.push_back(std::make_unique<Zone>(zoneId));
	}
}

ZoneScheduler::~ZoneScheduler() = default;

bool ZoneScheduler::submit(int zoneId, const Job& job)
{
	if (zones[zoneId]->submit(job) == false)
	{
		return false;
	}

	readyQueue.push(zones[zoneId].get());

	return true;
}

Zone* ZoneScheduler::getZone(int zoneId)
{
	return zones[zoneId].get();
}

ReadyQueue& ZoneScheduler::getReadyQueue()
{
	return readyQueue;
}

void ZoneScheduler::shutDown()
{
	readyQueue.shutDown();
}
