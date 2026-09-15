#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

using Job = std::function<void()>;
class JobQueue 
{
public:
	void shut_down()
	{
		std::unique_lock<std::mutex> lock(mutex);
		is_shutdown = true;
		lock.unlock();

		cv.notify_all();
	}
	void push(const Job& job)
	{
		std::unique_lock<std::mutex> lock(mutex);
		job_queue.push(job);
		cv.notify_one();
	}
	bool pop(Job& job)
	{
		std::unique_lock<std::mutex> lock(mutex);

		cv.wait(lock,
			[this]
			{
				if (job_queue.empty() == false || is_shutdown == true)
					return true;
				return false;
			});

		if (job_queue.empty())
			return false;

		job = job_queue.front();
		job_queue.pop();

		return true;
	}

	bool try_pop(Job& job)
	{
		std::lock_guard<std::mutex> lock(mutex);

		if (job_queue.empty())
		{
			return false;
		}

		job = job_queue.front();
		job_queue.pop();

		return true;
	}

	bool get_empty()
	{
		std::lock_guard<std::mutex> lock(mutex);
		return job_queue.empty();
	}

private:
	std::mutex mutex;
	std::condition_variable cv;
	std::queue<Job> job_queue;
	bool is_shutdown = false;
};