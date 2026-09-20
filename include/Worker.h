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

private:
	ZoneScheduler& m_zoneScheduler;
	std::thread m_workerThread;
};
