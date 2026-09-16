#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include "Zone.h"

class ReadyQueue
{
public:
	ReadyQueue() {}
	~ReadyQueue() {}

	void push(Zone* zone)
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			readyQueue.push(zone);
		}

		cv.notify_one();
	}

	bool pop(Zone*& zone)
	{
		std::unique_lock<std::mutex> lock(mutex);

		cv.wait(lock, [this] {
			return (readyQueue.empty() == false || isShutDown == true);
			});

		if (readyQueue.empty() == true)
			return false;

		zone = readyQueue.front();
		readyQueue.pop();

		return true;
	}

	void shutDown()
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			isShutDown = true;
		}
		cv.notify_all();
	}

private:
	std::mutex mutex;
	std::condition_variable cv;
	std::queue<Zone*> readyQueue;
	bool isShutDown = false;
};
