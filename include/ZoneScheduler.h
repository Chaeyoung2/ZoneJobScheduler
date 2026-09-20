#pragma once
#include "Job.h"
#include "ReadyQueue.h"

#include <memory>
#include <vector>
#include <shared_mutex>

class Worker;
class Zone;

class ZoneScheduler 
{
public:
	ZoneScheduler(int zoneCount);
	~ZoneScheduler();

	bool submit(int zoneId, const Job& job);
	const Zone& getZone(int zoneId) const;
	void shutDown();

private:
	friend class Worker;

	ReadyQueue& getReadyQueue();

	std::shared_mutex m_lifecycleMutex;
	bool m_isAcceptingJobs = true;

	std::vector<std::unique_ptr<Zone>> m_zones;
	ReadyQueue m_readyQueue;
};
