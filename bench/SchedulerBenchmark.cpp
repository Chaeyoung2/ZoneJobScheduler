#include "ThreadPool.h"
#include "ZoneScheduler.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

namespace
{
	using Clock = std::chrono::steady_clock;

	constexpr int zoneCount = 8;
	constexpr int submitterCount = 4;
	constexpr int repeatCount = 5;

	std::uint64_t sinkGuard = 0;

	struct BenchConfig
	{
		const char* jobName;
		int workerCount;
		int jobCount;
		int spinIterations;
	};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324) // padding이 목적이라서 임시 추가
#endif
	struct alignas(64) ZoneCounter // Counter마다 cache line 하나씩 차지하도록
	{
		std::int64_t executedJobs = 0;
		std::uint64_t sink = 0;
	};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

	std::uint64_t spinWork(int iterations)
	{
		std::uint64_t value = 1;

		for (int i = 0; i < iterations; ++i)
		{
			value = value * 6364136223846793005ULL + 1442695040888963407ULL;
		}

		return value;
	}

	int calibrateSpinIterationsPerMicrosecond()
	{
		volatile int opaqueIterations = 10'000'000; // 측정 위함
		const int calibrationIterations = opaqueIterations;

		const auto start = Clock::now();
		sinkGuard += spinWork(calibrationIterations);
		const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start);

		const std::int64_t elapsedNs = std::max<std::int64_t>(elapsed.count(), 1);

		return static_cast<int>(calibrationIterations * 1000LL / elapsedNs);
	}

	std::chrono::nanoseconds runOnce(const BenchConfig& config)
	{
		ZoneScheduler scheduler(zoneCount);
		ThreadPool threadPool(static_cast<std::size_t>(config.workerCount), scheduler);
		std::vector<ZoneCounter> counters(zoneCount);

		const int jobsPerSubmitter = config.jobCount / submitterCount;
		const int spinIterations = config.spinIterations;

		std::vector<std::thread> submitters;
		submitters.reserve(submitterCount);

		const auto start = Clock::now();

		for (int submitterIndex = 0; submitterIndex < submitterCount; ++submitterIndex)
		{
			submitters.emplace_back([&scheduler, &counters, jobsPerSubmitter, spinIterations, submitterIndex]()
				{
					for (int jobIndex = 0; jobIndex < jobsPerSubmitter; ++jobIndex)
					{
						const int zoneId = (submitterIndex + jobIndex) % zoneCount;
						ZoneCounter& counter = counters[static_cast<std::size_t>(zoneId)];

						scheduler.submit(zoneId, [&counter, spinIterations](Zone&)
							{
								if (spinIterations > 0)
								{
									counter.sink += spinWork(spinIterations);
								}

								++counter.executedJobs;
							});
					}
				});
		}

		for (auto& submitter : submitters)
		{
			submitter.join();
		}

		scheduler.shutDown();
		threadPool.join();

		const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start);

		std::int64_t executedJobCount = 0;

		for (const ZoneCounter& counter : counters)
		{
			executedJobCount += counter.executedJobs;
			sinkGuard += counter.sink;
		}

		const std::int64_t expectedJobCount = static_cast<std::int64_t>(jobsPerSubmitter) * submitterCount;

		if (executedJobCount != expectedJobCount)
		{
			std::cerr << "Job count mismatch: expected " << expectedJobCount
				<< ", executed " << executedJobCount << '\n';
		}

		return elapsed;
	}

	std::chrono::nanoseconds measureMedian(const BenchConfig& config)
	{
		runOnce(config);

		std::vector<std::chrono::nanoseconds> samples;
		samples.reserve(repeatCount);

		for (int i = 0; i < repeatCount; ++i)
		{
			samples.push_back(runOnce(config));
		}

		std::ranges::sort(samples);

		return samples[samples.size() / 2];
	}

	void printRow(const BenchConfig& config, std::chrono::nanoseconds median)
	{
		const double milliseconds = std::chrono::duration<double, std::milli>(median).count();
		const double seconds = std::chrono::duration<double>(median).count();
		const double jobsPerSecond = config.jobCount / seconds;
		const double nsPerJob = static_cast<double>(median.count()) / config.jobCount;

		std::cout << std::left << std::fixed << std::setprecision(1)
			<< std::setw(9) << config.workerCount
			<< std::setw(9) << config.jobName
			<< std::setw(10) << config.jobCount
			<< std::setw(12) << milliseconds
			<< std::setw(14) << std::setprecision(0) << jobsPerSecond
			<< std::setprecision(1) << nsPerJob
			<< '\n';
	}
}

int main()
{
	std::cout << "CPU threads: " << std::thread::hardware_concurrency() << '\n';

	const int spinIterationsPerMicrosecond = calibrateSpinIterationsPerMicrosecond();
	std::cout << "Spin iterations per 1us: " << spinIterationsPerMicrosecond << "\n\n";

	std::cout << std::left
		<< std::setw(9) << "Workers"
		<< std::setw(9) << "Job"
		<< std::setw(10) << "Jobs"
		<< std::setw(12) << "Median(ms)"
		<< std::setw(14) << "Jobs/s"
		<< "ns/job" << '\n';

	constexpr std::array workerCounts{ 1, 2, 4, 8 };

	const std::array jobTypes{
		BenchConfig{ "empty", 0, 1'000'000, 0 },
		BenchConfig{ "spin1us", 0, 200'000, spinIterationsPerMicrosecond },
	};

	for (const BenchConfig& jobType : jobTypes)
	{
		for (const int workerCount : workerCounts)
		{
			BenchConfig config = jobType;
			config.workerCount = workerCount;

			printRow(config, measureMedian(config));
		}
	}

	std::cout << "\nChecksum: " << sinkGuard << '\n';

	return 0;
}