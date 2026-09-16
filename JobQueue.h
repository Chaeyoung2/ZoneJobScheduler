#pragma once

#include "Job.h"

#include <condition_variable>
#include <mutex>
#include <queue>

class JobQueue 
{
public:
	void shutDown();
	void push(const Job& job);
	bool pop(Job& job);
	bool tryPop(Job& job);
	bool getEmpty() const;

private:
	mutable std::mutex m_mutex;
	std::condition_variable m_cv;
	std::queue<Job> m_jobQueue;
	bool m_isShutdown = false;
};
