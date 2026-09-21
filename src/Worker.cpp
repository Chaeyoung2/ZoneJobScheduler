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

Worker::~Worker()
{
	join();
}

void Worker::run()
{
	ReadyQueue& readyQueue = m_zoneScheduler.getReadyQueue();

	while (true)
	{
		auto nextZone = readyQueue.pop();

		if (nextZone.has_value() == false)
		{
			break;
		}

		Zone& zone = nextZone->get();
		JobQueue& jobQueue = zone.getJobQueue();

		while (auto job = jobQueue.tryPop())
		{
			(*job)(zone);
		}

		zone.markUnscheduled();

		if (jobQueue.empty() == false)
		{
			if (zone.trySchedule())
			{
				readyQueue.push(zone);
			}
		}
	}
}

void Worker::join()
{
	if (m_workerThread.joinable())
	{
		m_workerThread.join();
	}
}
