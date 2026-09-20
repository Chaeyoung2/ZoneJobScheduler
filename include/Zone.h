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
	const int m_zoneId;
	std::atomic<bool> m_isScheduled;
	JobQueue m_jobQueue;
	std::vector<Actor> m_actors;
	static constexpr int m_actorCount = 100;
};
