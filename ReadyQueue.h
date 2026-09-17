#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>
#include <optional>
#include <functional>

class Zone;

class ReadyQueue
{
public:
	void push(Zone& zone);
	std::optional<std::reference_wrapper<Zone>> pop();
	void shutDown();

private:
	std::mutex m_mutex;
	std::condition_variable m_cv;
	std::queue<std::reference_wrapper<Zone>> m_readyQueue;
	bool m_isShutDown = false;
};
