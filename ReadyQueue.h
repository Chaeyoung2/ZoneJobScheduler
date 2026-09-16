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
	std::mutex mutex;
	std::condition_variable cv;
	std::queue<Zone*> readyQueue;
	bool isShutDown = false;
};
