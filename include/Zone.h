#pragma once

#include "Actor.h"
#include "Job.h"
#include "JobQueue.h"

#include <atomic>
#include <vector>

class Zone
{
public:
	Zone();
	~Zone();

	bool submit(const Job& job);
	JobQueue& getJobQueue();
	void takeDamageAll(int damage);
	bool trySchedule();
	void markUnscheduled();

private:
	std::atomic<bool> m_isScheduled;
	JobQueue m_jobQueue;
	std::vector<Actor> m_actors;
	static constexpr int m_actorCount = 100;
};
