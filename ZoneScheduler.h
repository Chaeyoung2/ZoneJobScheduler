#pragma once
#include <map>
#include <thread>
#include "Zone.h"
#include "ReadyQueue.h"

using zob = std::function<void()>;
class ZoneScheduler 
{
public:
	ZoneScheduler(int zoneCount)
	{ 
		for (int i = 0; i < zoneCount; i++)
			zones.push_back(std::make_unique<Zone>(i));
	}
	~ZoneScheduler() 
	{
	}

	bool submit(int zoneId, const Job& job)
	{
		if (zones[zoneId]->submit(job) == false)
			return false;

		readyQueue.push(zones[zoneId].get());

		return true;
	}

	Zone* get_zone(int zoneId)
	{
		return zones[zoneId].get();
	}

	ReadyQueue& get_ready_queue()
	{
		return readyQueue;
	}

	void shut_down()
	{
		readyQueue.shut_down();
	}

private:
	std::vector<std::unique_ptr<Zone>> zones;
	ReadyQueue readyQueue;
};
