#include "Zone.h"

Zone::Zone(int id)
	: zoneId(id),
	  isScheduled(false)
{
	for (int i = 0; i < actorCount; ++i)
	{
		actors.emplace_back();
	}
}

Zone::~Zone() = default;

bool Zone::submit(const Job& job)
{
	jobQueue.push(job);

	bool expected = false;
	if (isScheduled.compare_exchange_strong(expected, true))
	{
		return true;
	}

	return false;
}

JobQueue& Zone::getJobQueue()
{
	return jobQueue;
}

Actor& Zone::getActor(int actorIndex)
{
	return actors[actorIndex];
}

void Zone::takeDamageAll(int damage)
{
	for (auto& actor : actors)
	{
		actor.takeDamage(damage);
	}
}

bool Zone::setScheduled(bool scheduled)
{
	bool expected = !scheduled;
	if (isScheduled.compare_exchange_strong(expected, scheduled) == false)
	{
		return false;
	}

	return true;
}
