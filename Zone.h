#pragma once
#include <atomic>
#include <mutex>
#include "JobQueue.h"
#include "Actor.h"
using Job = std::function<void()>;

class Zone
{
public:
	Zone(int id) : zoneId(id), isScheduled(false)
	{
		for(int i  =0; i < actorCount; i++)
			actors.emplace_back(Actor());
	}

	~Zone() 
	{
	}

	bool submit(const Job& job) 
	{
		jobQueue.push(job);

		bool expected = false;
		if (isScheduled.compare_exchange_strong(expected, true) == true)
		{
			return true;
		}
		return false;
	}

	JobQueue& getJobQueue()
	{
		return jobQueue;
	}

	Actor& getActor(int actorIndex)
	{
		return actors[actorIndex];
	}

	void takeDamageAll(int damage)
	{
		for (auto& actor : actors)
		{
			actor.takeDamage(damage);
		}
	}

	bool setScheduled(bool scheduled)
	{
		bool expected = !scheduled;
		if (isScheduled.compare_exchange_strong(expected, scheduled) == false)
			return false;
		return true;
	}

private:
	const int zoneId;
	std::atomic<bool> isScheduled;
	JobQueue jobQueue;
	std::vector<Actor> actors;
	int actorCount = 100;
};
