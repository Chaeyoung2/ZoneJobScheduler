#include "ReadyQueue.h"

void ReadyQueue::push(Zone* zone)
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		readyQueue.push(zone);
	}

	cv.notify_one();
}

bool ReadyQueue::pop(Zone*& zone)
{
	std::unique_lock<std::mutex> lock(mutex);

	cv.wait(lock, [this]()
		{
			return readyQueue.empty() == false || isShutDown;
		});

	if (readyQueue.empty())
	{
		return false;
	}

	zone = readyQueue.front();
	readyQueue.pop();

	return true;
}

void ReadyQueue::shutDown()
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		isShutDown = true;
	}

	cv.notify_all();
}
