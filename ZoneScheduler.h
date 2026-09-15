#pragma once
#include <map>
#include <thread>
#include "Zone.h"
#include "ReadyQueue.h"

using zob = std::function<void()>;
class ZoneScheduler 
{
public:
	ZoneScheduler(int zone_count) 
	{ 
		for (int i = 0; i < zone_count; i++)
			zones.push_back(std::make_unique<Zone>(i));
	}
	~ZoneScheduler() 
	{
	}

	bool submit(int zone_id, const Job& job)
	{
		if (zones[zone_id]->submit(job) == false)
			return false;

		ready_queue.push(zones[zone_id].get());

		return true;
	}

	Zone* get_zone(int zoneID)
	{
		return zones[zoneID].get();
	}

	ReadyQueue& get_ready_queue()
	{
		return ready_queue;
	}

	void shut_down()
	{
		ready_queue.shut_down();
	}

private:
	std::vector<std::unique_ptr<Zone>> zones;
	ReadyQueue ready_queue;
};