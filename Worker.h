#pragma once
#include <thread>
#include <functional>
#include "ZoneScheduler.h"

using Job = std::function<void()>;

class Worker
{
public:
	Worker(ZoneScheduler& _zone_scheduler)
		: zone_scheduler(_zone_scheduler), worker_thread(&Worker::run, this)
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
		// zone의 job_queue에서 job을 얻어와 실행한다.
		while (true)
		{
			auto& ready_queue = zone_scheduler.get_ready_queue();

			Zone* zone = nullptr;
			if (ready_queue.pop(zone) == true && zone != nullptr)
			{
				// zone의 작업들을 실행한다.
				auto& job_queue = zone->get_job_queue();

				Job job;
				if (job_queue.pop(job) == true)
					job();
			}
		}
	}
	
	void join()
	{
		worker_thread.join();
	}

	void shut_down()
	{
		is_shut_down = true;
	}

private:
	ZoneScheduler& zone_scheduler;
	std::thread worker_thread;
	bool is_shut_down;
};