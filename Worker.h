#pragma once
#include <thread>

class ZoneScheduler;

class Worker
{
public:
	Worker(ZoneScheduler& scheduler);
	~Worker();

	void run();
	void join();
	void shutDown();

private:
	ZoneScheduler& m_zoneScheduler;
	std::thread m_workerThread;
	bool m_isShutDown = false;
};
