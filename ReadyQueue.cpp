#include "ReadyQueue.h"

void ReadyQueue::push(Zone* zone)
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_readyQueue.push(zone);
	}

	m_cv.notify_one();
}

bool ReadyQueue::pop(Zone*& zone)
{
	std::unique_lock<std::mutex> lock(m_mutex);

	m_cv.wait(lock, [this]()
		{
			return m_readyQueue.empty() == false || m_isShutDown;
		});

	if (m_readyQueue.empty())
	{
		return false;
	}

	zone = m_readyQueue.front();
	m_readyQueue.pop();

	return true;
}

void ReadyQueue::shutDown()
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_isShutDown = true;
	}

	m_cv.notify_all();
}
