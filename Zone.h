#pragma once
#include <atomic>
#include <mutex>
#include "JobQueue.h"
#include "Actor.h"
using Job = std::function<void()>;

class Zone
{
public:
	Zone(int _zone_id) : zone_id(_zone_id), is_scheduled(false) 
	{
		for(int i  =0; i < actor_count; i++)
			actors.emplace_back(Actor());
	}

	~Zone() 
	{
	}

	bool submit(const Job& job) 
	{
		job_queue.push(job);

		bool expected = false;
		if (is_scheduled.compare_exchange_strong(expected, true) == true)
		{
			return true;
		}
		return false;
	}

	JobQueue& get_job_queue()
	{
		return job_queue;
	}

	Actor& get_actor(int actor_idx)
	{
		return actors[actor_idx];
	}

	void take_damage_all(int damage)
	{
		for (auto& actor : actors)
		{
			actor.take_damage(damage);
		}
	}

private:
	const int zone_id;
	std::atomic<bool> is_scheduled;
	JobQueue job_queue;
	std::vector<Actor> actors;
	int actor_count = 100;
};