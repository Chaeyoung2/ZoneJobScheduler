#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

using Job = std::function<void()>;
class JobQueue {

public:
	void shut_down()
	{
		std::unique_lock<std::mutex> lock(mutex);
		is_shutdown = true;
		lock.unlock();

		cv.notify_all();
	}
	void push(const Job& job);
	bool pop(Job& job);

private:
	std::mutex mutex;
	std::condition_variable cv;
	std::queue<Job> job_queue;
	bool is_shutdown;
};