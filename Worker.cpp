#include "Worker.h"

#include "Job.h"
#include "JobQueue.h"
#include "ReadyQueue.h"
#include "Zone.h"
#include "ZoneScheduler.h"

Worker::Worker(ZoneScheduler& scheduler)
	: zoneScheduler(scheduler),
	  workerThread(&Worker::run, this)
{
}

Worker::~Worker() = default;

void Worker::run()
{
	ReadyQueue& readyQueue = zoneScheduler.getReadyQueue();

	while (true)
	{
		Zone* zone = nullptr;

		if (readyQueue.pop(zone) == false)
		{
			break;
		}

		JobQueue& jobQueue = zone->getJobQueue();

		Job job;
		while (jobQueue.tryPop(job))
		{
			job();
		}

		zone->setScheduled(false);

		if (jobQueue.getEmpty() == false)
		{
			if (zone->setScheduled(true))
			{
				readyQueue.push(zone);
			}
		}
	}
}

void Worker::join()
{
	workerThread.join();
}

void Worker::shutDown()
{
	isShutDown = true;
}
