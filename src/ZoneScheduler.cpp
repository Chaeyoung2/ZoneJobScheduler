#include "ZoneScheduler.h"

#include "Zone.h"

ZoneScheduler::ZoneScheduler(int zoneCount)
{
	for (int zoneId = 0; zoneId < zoneCount; ++zoneId)
	{
		m_zones.push_back(std::make_unique<Zone>());
	}
}

ZoneScheduler::~ZoneScheduler() = default;

void ZoneScheduler::submit(int zoneId, const Job& job)
{
	if (m_zones[zoneId]->submit(job))
	{
		m_readyQueue.push(*m_zones[zoneId]);
	}
}

const Zone& ZoneScheduler::getZone(int zoneId) const
{
	return *m_zones[zoneId];
}

ReadyQueue& ZoneScheduler::getReadyQueue()
{
	return m_readyQueue;
}

void ZoneScheduler::shutDown()
{
	m_readyQueue.shutDown();
}
