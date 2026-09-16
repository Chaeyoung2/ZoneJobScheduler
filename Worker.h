#pragma once
#include <thread>
#include <functional>
#include "ZoneScheduler.h"

using Job = std::function<void()>;

class Worker
{
public:
	Worker(ZoneScheduler& scheduler)
		: zoneScheduler(scheduler), workerThread(&Worker::run, this)
	{
	}

	~Worker() 
	{
	}

	void run()
	{
		// worker: 
		// zone scheduler에서 ready queue를 가져온다.
		// ready queue에서 스케쥴이 되지 않은 zone을 가져온다.
		// zone의 jobQueue에서 job을 얻어와 실행한다.

		auto& readyQueue = zoneScheduler.get_ready_queue();

		while (true)
		{
			Zone* zone = nullptr;

			if (readyQueue.pop(zone) == false)
			{
				break;
			}

			// zone의 작업들을 실행한다.
			auto& jobQueue = zone->get_job_queue();

			Job job;
			while (jobQueue.try_pop(job) == true)
			{
				job();
			}

			zone->set_scheduled(false);

			if (jobQueue.get_empty() == false)
			{
				if (zone->set_scheduled(true) == true)
				{
					readyQueue.push(zone);
				}
			}
		}
	}
	
	void join()
	{
		workerThread.join();
	}

	void shut_down()
	{
		isShutDown = true;
	}

private:
	ZoneScheduler& zoneScheduler;
	std::thread workerThread;
	bool isShutDown = false;
};
