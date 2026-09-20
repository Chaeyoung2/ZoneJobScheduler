#pragma once
#include "Job.h"
#include "ReadyQueue.h"

#include <memory>
#include <vector>

class Worker;
class Zone;

class ZoneScheduler 
{
public:
	ZoneScheduler(int zoneCount);
	~ZoneScheduler();

	void submit(int zoneId, const Job& job);
	const Zone& getZone(int zoneId) const;
	void shutDown();

private:
	friend class Worker;

	ReadyQueue& getReadyQueue();

	std::vector<std::unique_ptr<Zone>> m_zones;
	ReadyQueue m_readyQueue;
};
