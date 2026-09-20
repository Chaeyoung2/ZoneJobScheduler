#include "JobQueue.h"

void JobQueue::push(const Job& job)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_jobQueue.push(job);
}

bool JobQueue::tryPop(Job& job)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_jobQueue.empty())
	{
		return false;
	}

	job = m_jobQueue.front();
	m_jobQueue.pop();

	return true;
}

bool JobQueue::empty() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_jobQueue.empty();
}
