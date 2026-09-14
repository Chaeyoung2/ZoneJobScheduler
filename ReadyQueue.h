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
			ready_queue.push(zone);
		}

		cv.notify_one();
	}

	bool pop(Zone*& zone)
	{
		std::unique_lock<std::mutex> lock(mutex);

		cv.wait(lock, [this] {
			return (ready_queue.empty() == false || is_shut_down == true);
			});

		if (ready_queue.empty() == true)
			return false;

		zone = ready_queue.front();
		ready_queue.pop();

		return true;
	}

	bool get_shut_down()
	{
		return is_shut_down;
	}

	void shut_down()
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			is_shut_down = true;
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
	bool is_shut_down = false;
};