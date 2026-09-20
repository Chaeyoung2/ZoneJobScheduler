#include "Producer.h"

#include "ZoneScheduler.h"

#include <utility>

Producer::Producer(
	ZoneScheduler& scheduler,
	int jobCount,
	int totalZoneCount,
	JobFactory factory)
	: m_zoneScheduler(scheduler),
	m_jobsPerProducer(jobCount),
	m_zoneCount(totalZoneCount),
	m_jobFactory(std::move(factory)),
	m_producerThread(&Producer::run, this)
{
}

Producer::~Producer()
{
	join();
}

void Producer::run()
{
	for (int zoneId = 0; zoneId < m_zoneCount; ++zoneId)
	{
		for (int jobIndex = 0; jobIndex < m_jobsPerProducer; ++jobIndex)
		{
			if (m_zoneScheduler.submit(zoneId, m_jobFactory(zoneId)) == false)
			{
				return;
			}
		}
	}
}

void Producer::join()
{
	if (m_producerThread.joinable())
	{
		m_producerThread.join();
	}
}
