#include "ZoneScheduler.h"
#include "Zone.h"

#include <shared_mutex>
#include <mutex>

ZoneScheduler::ZoneScheduler(int zoneCount)
{
	for (int zoneId = 0; zoneId < zoneCount; ++zoneId)
	{
		m_zones.push_back(std::make_unique<Zone>());
	}
}

ZoneScheduler::~ZoneScheduler() = default;

bool ZoneScheduler::submit(int zoneId, const Job& job)
{
	std::shared_lock<std::shared_mutex> lifecycleLock(m_lifecycleMutex);

	if (m_isAcceptingJobs == false)
	{
		return false;
	}

	Zone& zone = *m_zones[zoneId];

	if (zone.submit(job))
	{
		m_readyQueue.push(zone);
	}

	return true;
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
	std::unique_lock<std::shared_mutex> lifecycleLock(m_lifecycleMutex);

	if (m_isAcceptingJobs == false)
	{
		return;
	}

	m_isAcceptingJobs = false;
	m_readyQueue.shutDown();
}
