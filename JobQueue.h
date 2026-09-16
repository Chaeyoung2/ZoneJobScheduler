#pragma once

#include "Job.h"

#include <condition_variable>
#include <mutex>
#include <queue>

class JobQueue 
{
public:
	void shutDown();
	void push(const Job& job);
	bool pop(Job& job);
	bool tryPop(Job& job);
	bool getEmpty();

private:
	std::mutex mutex;
	std::condition_variable cv;
	std::queue<Job> jobQueue;
	bool isShutdown = false;
};
