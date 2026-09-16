#include "JobQueue.h"

void JobQueue::shutDown()
{
	std::unique_lock<std::mutex> lock(m_mutex);
	m_isShutdown = true;
	lock.unlock();

	m_cv.notify_all();
}

void JobQueue::push(const Job& job)
{
	std::unique_lock<std::mutex> lock(m_mutex);
	m_jobQueue.push(job);
	m_cv.notify_one();
}

bool JobQueue::pop(Job& job)
{
	std::unique_lock<std::mutex> lock(m_mutex);

	m_cv.wait(lock, [this]()
		{
			return m_jobQueue.empty() == false || m_isShutdown;
		});

	if (m_jobQueue.empty())
	{
		return false;
	}

	job = m_jobQueue.front();
	m_jobQueue.pop();

	return true;
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

bool JobQueue::getEmpty() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_jobQueue.empty();
}
