#include "JobQueue.h"
#include "ThreadPool.h"
#include "Actor.h"
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <iostream>

int main()
{
	JobQueue job_queue;

	std::atomic<int> popped_count{ 0 };
	const int producer_count = 4;
	const int jobs_per_producer = 1000;
	const int actor_count = 4;
	const int actor_maxhp = 100;
	bool all_dead = true;

		std::vector<std::thread> producers;
		std::vector<Actor> actors;

	{
		ThreadPool pool(4, job_queue);


		for(int i = 0 ; i < actor_count; ++i)
			actors.emplace_back(actor_maxhp, actor_maxhp);

		for (int i = 0; i < producer_count; ++i)
		{
			producers.emplace_back([&job_queue, &popped_count, &actors, i]()
				{
					for (int j = 0; j < jobs_per_producer; ++j)
					{
						Job job = [i, &popped_count, &actors] {
							popped_count++;
							 actors[i].take_damage(1);
							 };
						job_queue.push(job);
					}
				});
		}

		for (auto& producer : producers)
		{
			producer.join();
		}

		job_queue.shut_down();
		
	}


		for(auto& a : actors)
		{
			if(a.get_hp() > 0)
				all_dead = false;
		}

	assert( 
		(popped_count == producer_count * jobs_per_producer)
		&&
		all_dead == true
	);
}
