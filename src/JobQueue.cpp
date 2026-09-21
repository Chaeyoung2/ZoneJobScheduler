#include "JobQueue.h"

#include <utility>

void JobQueue::push(const Job& job)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_jobQueue.push(job);
}

std::optional<Job> JobQueue::tryPop()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_jobQueue.empty())
	{
		return std::nullopt;
	}

	std::optional<Job> job{ std::move(m_jobQueue.front()) };
	m_jobQueue.pop();

	return job;
}

bool JobQueue::empty() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_jobQueue.empty();
}
