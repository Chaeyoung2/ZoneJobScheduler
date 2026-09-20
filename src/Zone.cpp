#include "Zone.h"

Zone::Zone(int id)
	: m_zoneId(id),
	  m_isScheduled(false)
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

	bool expected = false;
	if (m_isScheduled.compare_exchange_strong(expected, true))
	{
		return true;
	}

	return false;
}

JobQueue& Zone::getJobQueue()
{
	return m_jobQueue;
}

Actor& Zone::getActor(int actorIndex)
{
	return m_actors[actorIndex];
}

void Zone::takeDamageAll(int damage)
{
	for (auto& actor : m_actors)
	{
		actor.takeDamage(damage);
	}
}

bool Zone::setScheduled(bool scheduled)
{
	bool expected = !scheduled;
	if (m_isScheduled.compare_exchange_strong(expected, scheduled) == false)
	{
		return false;
	}

	return true;
}
