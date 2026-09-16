#pragma once
#include <thread>
#include <atomic>
#include "ZoneScheduler.h"

class Producer
{
public:
	Producer(ZoneScheduler& _zone_scheduler, int _jobs_per_producer, int _zone_count, std::atomic<int>& _executed_job_count)
		: zone_scheduler(_zone_scheduler), jobs_per_producer(_jobs_per_producer), zone_count(_zone_count), executed_job_count(_executed_job_count), producer_thread(&Producer::run, this)
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
		return [&scheduler = zone_scheduler, &executed_count = executed_job_count, zone_id]()
			{
				scheduler.get_zone(zone_id)->take_damage_all(1);
				executed_count.fetch_add(1, std::memory_order_relaxed);
			};
	}

private:
	ZoneScheduler& zone_scheduler;
	int jobs_per_producer;
	const int zone_count;
	std::atomic<int>& executed_job_count;
	std::thread producer_thread;
};
