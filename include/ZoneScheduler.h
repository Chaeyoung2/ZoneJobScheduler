#pragma once
#include "Job.h"
#include "ReadyQueue.h"

#include <memory>
#include <vector>
#include <shared_mutex>
#include <stdexcept>

class Worker;
class Zone;

class ZoneScheduler 
{
public:
	explicit ZoneScheduler(int zoneCount);
	~ZoneScheduler();

	bool submit(int zoneId, const Job& job);
	void shutDown();

private:
	friend class Worker;

	ReadyQueue& getReadyQueue();

	std::shared_mutex m_lifecycleMutex;
	bool m_isAcceptingJobs = true;

	std::vector<std::unique_ptr<Zone>> m_zones;
	ReadyQueue m_readyQueue;
};
