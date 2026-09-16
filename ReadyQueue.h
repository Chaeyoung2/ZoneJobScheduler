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

	bool get_shut_down()
	{
		return isShutDown;
	}

	void shut_down()
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			isShutDown = true;
		}
		cv.notify_all();
	}

	std::condition_variable& get_condition_variable()
	{
		return cv;
	}

	bool empty()
	{
		std::lock_guard<std::mutex> lock(mutex);
		return readyQueue.empty();
	}

	std::mutex& get_mutex()
	{
		return mutex;
	}

private:
	std::mutex mutex;
	std::condition_variable cv;
	std::queue<Zone*> readyQueue;
	bool isShutDown = false;
};
