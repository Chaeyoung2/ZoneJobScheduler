#include "ReadyQueue.h"

void ReadyQueue::push(Zone& zone)
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_readyQueue.push(std::ref(zone));
	}

	m_cv.notify_one();
}

std::optional<std::reference_wrapper<Zone>> ReadyQueue::pop()
{
	std::unique_lock<std::mutex> lock(m_mutex);

	m_cv.wait(lock, [this]()
		{
			return m_readyQueue.empty() == false || m_isShutDown;
		});

	if (m_readyQueue.empty())
	{
		return std::nullopt;
	}

	std::reference_wrapper<Zone> zone = m_readyQueue.front();
	m_readyQueue.pop();

	return zone;
}

void ReadyQueue::shutDown()
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_isShutDown = true;
	}

	m_cv.notify_all();
}
