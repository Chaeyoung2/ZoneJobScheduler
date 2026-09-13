#pragma once
#include <thread>
#include "ZoneScheduler.h"

class Producer
{
public:
	Producer(ZoneScheduler& _zone_scheduler, int _jobs_per_producer, int _zone_count) 
		: zone_scheduler(_zone_scheduler), jobs_per_producer(_jobs_per_producer), zone_count(_zone_count), producer_thread(&Producer::run, this)
	{
	}

	~Producer() 
	{
	}

	void run()
	{
		for (int i = 0; i < zone_count; i++)
		{
			for (int k = 0; k < jobs_per_producer; k++)
			{
				zone_scheduler.submit(i/*zone_id*/, make_job(i));
			}
		}
		
	}

	void join()
	{
		producer_thread.join();
	}

	Job make_job(int zone_id)
	{
		return [&scheduler = zone_scheduler, zone_id]()
			{
				scheduler.get_zone(zone_id)->take_damage_all(1);
			};
	}

private:
	std::thread producer_thread;
	ZoneScheduler& zone_scheduler;
	const int zone_count;
	int jobs_per_producer;
};
