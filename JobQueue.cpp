#include "JobQueue.h"

void JobQueue::shutDown()
{
	std::unique_lock<std::mutex> lock(mutex);
	isShutdown = true;
	lock.unlock();

	cv.notify_all();
}

void JobQueue::push(const Job& job)
{
	std::unique_lock<std::mutex> lock(mutex);
	jobQueue.push(job);
	cv.notify_one();
}

bool JobQueue::pop(Job& job)
{
	std::unique_lock<std::mutex> lock(mutex);

	cv.wait(lock, [this]()
		{
			return jobQueue.empty() == false || isShutdown;
		});

	if (jobQueue.empty())
	{
		return false;
	}

	job = jobQueue.front();
	jobQueue.pop();

	return true;
}

bool JobQueue::tryPop(Job& job)
{
	std::lock_guard<std::mutex> lock(mutex);

	if (jobQueue.empty())
	{
		return false;
	}

	job = jobQueue.front();
	jobQueue.pop();

	return true;
}

bool JobQueue::getEmpty()
{
	std::lock_guard<std::mutex> lock(mutex);
	return jobQueue.empty();
}
