#pragma once
#include "Job.h"
#include "ReadyQueue.h"

#include <memory>
#include <vector>

class Zone;

class ZoneScheduler 
{
public:
	ZoneScheduler(int zoneCount);
	~ZoneScheduler();

	bool submit(int zoneId, const Job& job);
	Zone* getZone(int zoneId);
	ReadyQueue& getReadyQueue();
	void shutDown();

private:
	std::vector<std::unique_ptr<Zone>> zones;
	ReadyQueue readyQueue;
};
