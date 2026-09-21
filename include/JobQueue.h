#pragma once

#include "Job.h"

#include <mutex>
#include <queue>
#include <optional>

class JobQueue 
{
public:
	void push(const Job& job);
	std::optional<Job> tryPop();
	bool empty() const;

private:
	mutable std::mutex m_mutex;
	std::queue<Job> m_jobQueue;
};
