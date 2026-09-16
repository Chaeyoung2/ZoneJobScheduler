#pragma once

#include "Actor.h"
#include "Job.h"
#include "JobQueue.h"

#include <atomic>
#include <vector>

class Zone
{
public:
	Zone(int id);
	~Zone();

	bool submit(const Job& job);
	JobQueue& getJobQueue();
	Actor& getActor(int actorIndex);
	void takeDamageAll(int damage);
	bool setScheduled(bool scheduled);

private:
	const int zoneId;
	std::atomic<bool> isScheduled;
	JobQueue jobQueue;
	std::vector<Actor> actors;
	int actorCount = 100;
};
