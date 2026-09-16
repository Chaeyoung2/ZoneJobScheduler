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
	ZoneScheduler& zoneScheduler;
	std::thread workerThread;
	bool isShutDown = false;
};
