#pragma once
#include <queue>
#include <mutex>
#include "Zone.h"

class ReadyQueue
{
public:
	ReadyQueue() {}
	~ReadyQueue() {}

	void push(Zone* zone)
	{
		std::lock_guard<std::mutex> lock(mutex);
		ready_queue.push(zone);

		cv.notify_one();
	}

	bool pop(Zone* zone)
	{
		zone = nullptr;

		std::lock_guard<std::mutex> lock(mutex);
		if (ready_queue.empty() == false)
		{
			zone = ready_queue.front();
			ready_queue.pop();
			return true;
		}
		return false;
	}

	void shut_down()
	{
		cv.notify_all();
	}

	std::condition_variable& get_condition_variable()
	{
		return cv;
	}

	bool empty()
	{
		std::lock_guard<std::mutex> lock(mutex);
		return ready_queue.empty();
	}

	std::mutex& get_mutex()
	{
		return mutex;
	}

private:
	std::mutex mutex;
	std::condition_variable cv;
	std::queue<Zone*> ready_queue;
};