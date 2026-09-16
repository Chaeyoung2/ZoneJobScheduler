#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>

class Zone;

class ReadyQueue
{
public:
	void push(Zone* zone);
	bool pop(Zone*& zone);
	void shutDown();

private:
	std::mutex m_mutex;
	std::condition_variable m_cv;
	std::queue<Zone*> m_readyQueue;
	bool m_isShutDown = false;
};
