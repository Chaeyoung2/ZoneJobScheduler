#pragma once
#include <queue>
#include <mutex>

class Job;
class JobQueue {

public:
	void shut_down();
	void push(const Job& job);
	bool pop(Job& job);

private:
	std::mutex mutex;
	std::condition_variable cv;
	std::queue job_queue;
	bool is_shutdown;
};