#include "ZoneScheduler.h"

#include "Zone.h"

ZoneScheduler::ZoneScheduler(int zoneCount)
{
	for (int zoneId = 0; zoneId < zoneCount; ++zoneId)
	{
		m_zones.push_back(std::make_unique<Zone>(zoneId));
	}
}

ZoneScheduler::~ZoneScheduler() = default;

bool ZoneScheduler::submit(int zoneId, const Job& job)
{
	if (m_zones[zoneId]->submit(job) == false)
	{
		return false;
	}

	m_readyQueue.push(m_zones[zoneId].get());

	return true;
}

Zone* ZoneScheduler::getZone(int zoneId)
{
	return m_zones[zoneId].get();
}

ReadyQueue& ZoneScheduler::getReadyQueue()
{
	return m_readyQueue;
}

void ZoneScheduler::shutDown()
{
	m_readyQueue.shutDown();
}
