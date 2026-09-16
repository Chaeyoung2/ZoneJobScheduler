#include "Worker.h"

#include "Job.h"
#include "JobQueue.h"
#include "ReadyQueue.h"
#include "Zone.h"
#include "ZoneScheduler.h"

Worker::Worker(ZoneScheduler& scheduler)
	: m_zoneScheduler(scheduler),
	  m_workerThread(&Worker::run, this)
{
}

Worker::~Worker() = default;

void Worker::run()
{
	ReadyQueue& readyQueue = m_zoneScheduler.getReadyQueue();

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
	m_workerThread.join();
}

void Worker::shutDown()
{
	m_isShutDown = true;
}
