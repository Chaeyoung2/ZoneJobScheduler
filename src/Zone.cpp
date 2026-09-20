#include "Zone.h"

#include <algorithm>

Zone::Zone()
	: m_isScheduled(false)
{
	for (int i = 0; i < m_actorCount; ++i)
	{
		m_actors.emplace_back();
	}
}

Zone::~Zone() = default;

bool Zone::submit(const Job& job)
{
	m_jobQueue.push(job);

	return trySchedule();
}

JobQueue& Zone::getJobQueue()
{
	return m_jobQueue;
}

void Zone::takeDamageAll(int damage)
{
	for (auto& actor : m_actors)
	{
		actor.takeDamage(damage);
	}
}

bool Zone::trySchedule()
{
	bool expected = false;
	if (m_isScheduled.compare_exchange_strong(expected, true) == false)
	{
		return false;
	}

	return true;
}

void Zone::markUnscheduled()
{
	m_isScheduled.store(false);
}

bool Zone::allActorsHaveHp(int expectedHp) const
{
	return std::ranges::all_of(
		m_actors,
		[expectedHp](const Actor& actor)
		{
			return actor.getHp() == expectedHp;
		});
}
