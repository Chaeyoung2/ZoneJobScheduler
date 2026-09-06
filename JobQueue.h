#pragma once
#include <queue>
#include <mutex>

class Job;
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
	std::queue job_queue;
	bool is_shutdown;
};