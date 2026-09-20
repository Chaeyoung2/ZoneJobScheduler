#pragma once

#include "Actor.h"
#include "Job.h"
#include "JobQueue.h"

#include <atomic>
#include <vector>

class Worker;
class ZoneScheduler;

class Zone
{
public:
	Zone();
	~Zone();

	void takeDamageAll(int damage);
	bool allActorsHaveHp(int expectedHp) const;

private:
	friend class Worker;
	friend class ZoneScheduler;

	bool submit(const Job& job);
	JobQueue& getJobQueue();
	bool trySchedule();
	void markUnscheduled();

	std::atomic<bool> m_isScheduled;
	JobQueue m_jobQueue;
	std::vector<Actor> m_actors;
	static constexpr int m_actorCount = 100;
};
