#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

using Job = std::function<void()>;
class JobQueue 
{
public:
	void shutDown()
	{
		std::unique_lock<std::mutex> lock(mutex);
		isShutdown = true;
		lock.unlock();

		cv.notify_all();
	}
	void push(const Job& job)
	{
		std::unique_lock<std::mutex> lock(mutex);
		jobQueue.push(job);
		cv.notify_one();
	}
	bool pop(Job& job)
	{
		std::unique_lock<std::mutex> lock(mutex);

		cv.wait(lock,
			[this]
			{
				if (jobQueue.empty() == false || isShutdown == true)
					return true;
				return false;
			});

		if (jobQueue.empty())
			return false;

		job = jobQueue.front();
		jobQueue.pop();

		return true;
	}

	bool tryPop(Job& job)
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

	bool getEmpty()
	{
		std::lock_guard<std::mutex> lock(mutex);
		return jobQueue.empty();
	}

private:
	std::mutex mutex;
	std::condition_variable cv;
	std::queue<Job> jobQueue;
	bool isShutdown = false;
};
