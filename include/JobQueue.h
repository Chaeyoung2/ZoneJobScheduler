#pragma once

#include "Job.h"

#include <mutex>
#include <queue>

class JobQueue 
{
public:
	void push(const Job& job);
	bool tryPop(Job& job);
	bool empty() const;

private:
	mutable std::mutex m_mutex;
	std::queue<Job> m_jobQueue;
};
