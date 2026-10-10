#include "ThreadPool.h"
#include "ZoneScheduler.h"

#include <algorithm>
#include <array>
#include <atomic>
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
	constexpr int hotZoneId = 0; // Zone 0 = Hot Zone, Zone 1~7 = Cold Zone
	constexpr int repeatCount = 5;

	constexpr auto runDuration = std::chrono::milliseconds(200);
	constexpr auto coldInterval = std::chrono::microseconds(100);
	constexpr int coldTickCount = static_cast<int>(runDuration / coldInterval);
	constexpr std::int64_t hotBacklogLimit = 1'000; // Hot Zone 큐에 쌓아 둘 최대 Job 수

	std::uint64_t sinkGuard = 0;

	struct BenchConfig
	{
		int workerCount;
		bool isHotZoneOn;
		int spinIterations;
	};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
	struct alignas(64) ZoneRecord
	{
		std::vector<std::int64_t> latenciesNs;
		std::uint64_t sink = 0;
	};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

	struct RunResult
	{
		std::int64_t p50Ns;
		std::int64_t p99Ns;
		std::int64_t maxNs;
		std::int64_t coldJobCount;
		std::int64_t hotJobCount;
	};

	std::uint64_t spinWork(int iterations)
	{
		std::uint64_t value = 1;

		for (int i = 0; i < iterations; ++i)
		{
			value = value * 6364136223846793005ULL + 1442695040888963407ULL;
		}

		return value;
	}

	// 1us 동안 spinWork 몇번 돌 수 있는지 측정
	int calibrateSpinIterationsPerMicrosecond()
	{
		volatile int opaqueIterations = 10'000'000; // 최적화 못 하도록
		const int calibrationIterations = opaqueIterations;

		const auto start = Clock::now();
		sinkGuard += spinWork(calibrationIterations);
		const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start);

		const std::int64_t elapsedNs = std::max<std::int64_t>(elapsed.count(), 1);

		return static_cast<int>(calibrationIterations * 1000LL / elapsedNs);
	}

	RunResult runOnce(const BenchConfig& config)
	{
		ZoneScheduler scheduler(zoneCount);
		ThreadPool threadPool(static_cast<std::size_t>(config.workerCount), scheduler);
		std::vector<ZoneRecord> records(zoneCount);

		for (ZoneRecord& record : records)
		{
			record.latenciesNs.reserve(coldTickCount);
		}

		const int spinIterations = config.spinIterations;

		alignas(64) std::atomic<std::int64_t> hotExecutedJobs = 0;
		std::atomic<bool> isHotRunning = true;
		std::int64_t hotSubmittedJobs = 0;
		std::thread hotSubmitter;

		if (config.isHotZoneOn)
		{
			ZoneRecord& hotRecord = records[hotZoneId];

            // Hot Zone Submitter
			hotSubmitter = std::thread([&scheduler, &hotRecord, &hotExecutedJobs, &isHotRunning, &hotSubmittedJobs, spinIterations]()
				{
					while (isHotRunning.load())
					{
						if (hotSubmittedJobs - hotExecutedJobs.load() >= hotBacklogLimit)
						{
							continue;
						}

						scheduler.submit(hotZoneId, [&hotRecord, &hotExecutedJobs, spinIterations](Zone&)
							{
								hotRecord.sink += spinWork(spinIterations);
								hotExecutedJobs.fetch_add(1);
							});

						++hotSubmittedJobs;
					}
				});

			// Hot Zone이 Worker를 잡은 뒤에 측정을 시작한다.
			while (hotExecutedJobs.load() == 0)
			{
			}
		}

        // Cold Zone Timer Loop
		const auto start = Clock::now();

		for (int tick = 0; tick < coldTickCount; ++tick)
		{
			const auto tickTime = start + tick * coldInterval;

			while (Clock::now() < tickTime)
			{
			}

			for (int zoneId = hotZoneId + 1; zoneId < zoneCount; ++zoneId)
			{
				ZoneRecord& record = records[static_cast<std::size_t>(zoneId)];
				const auto submitTime = Clock::now();

				scheduler.submit(zoneId, [&record, spinIterations, submitTime](Zone&)
					{
                        // Latency
						const auto waited = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - submitTime);
						record.latenciesNs.push_back(waited.count());

						record.sink += spinWork(spinIterations);
					});
			}
		}

		isHotRunning.store(false);

		if (hotSubmitter.joinable())
		{
			hotSubmitter.join();
		}

		scheduler.shutDown();
		threadPool.join();

		std::vector<std::int64_t> latenciesNs;
		latenciesNs.reserve(static_cast<std::size_t>(coldTickCount) * (zoneCount - 1));

		for (const ZoneRecord& record : records)
		{
			latenciesNs.insert(latenciesNs.end(), record.latenciesNs.begin(), record.latenciesNs.end());
			sinkGuard += record.sink;
		}

		const std::int64_t expectedColdJobCount = static_cast<std::int64_t>(coldTickCount) * (zoneCount - 1);
		const auto coldJobCount = static_cast<std::int64_t>(latenciesNs.size());

		if (coldJobCount != expectedColdJobCount || hotExecutedJobs.load() != hotSubmittedJobs)
		{
			std::cerr << "Job count mismatch: cold " << coldJobCount << '/' << expectedColdJobCount
				<< ", hot " << hotExecutedJobs.load() << '/' << hotSubmittedJobs << '\n';
		}

		std::ranges::sort(latenciesNs);

		return RunResult{
			latenciesNs[latenciesNs.size() / 2],
			latenciesNs[latenciesNs.size() * 99 / 100],
			latenciesNs.back(),
			coldJobCount,
			hotExecutedJobs.load(),
		};
	}

	std::int64_t medianOf(std::vector<std::int64_t> values)
	{
		std::ranges::sort(values);

		return values[values.size() / 2];
	}

	RunResult measureMedian(const BenchConfig& config)
	{
		runOnce(config);

		std::vector<std::int64_t> p50s;
		std::vector<std::int64_t> p99s;
		std::vector<std::int64_t> maxes;
		std::vector<std::int64_t> hotJobCounts;
		RunResult last{};

		for (int i = 0; i < repeatCount; ++i)
		{
			last = runOnce(config);

			p50s.push_back(last.p50Ns);
			p99s.push_back(last.p99Ns);
			maxes.push_back(last.maxNs);
			hotJobCounts.push_back(last.hotJobCount);
		}

		return RunResult{ medianOf(p50s), medianOf(p99s), medianOf(maxes), last.coldJobCount, medianOf(hotJobCounts) };
	}

	void printRow(const BenchConfig& config, const RunResult& result)
	{
		const auto toMicroseconds = [](std::int64_t ns)
			{
				return static_cast<double>(ns) / 1000.0;
			};

		std::cout << std::left << std::fixed << std::setprecision(1)
			<< std::setw(9) << config.workerCount
			<< std::setw(6) << (config.isHotZoneOn ? "on" : "off")
			<< std::setw(10) << result.coldJobCount
			<< std::setw(10) << result.hotJobCount
			<< std::setw(12) << toMicroseconds(result.p50Ns)
			<< std::setw(12) << toMicroseconds(result.p99Ns)
			<< toMicroseconds(result.maxNs)
			<< '\n';
	}
}

int main()
{
	std::cout << "CPU threads: " << std::thread::hardware_concurrency() << '\n';

	const int spinIterationsPerMicrosecond = calibrateSpinIterationsPerMicrosecond();
	std::cout << "Spin iterations per 1us: " << spinIterationsPerMicrosecond << '\n';
	std::cout << "Run: " << runDuration.count() << "ms, cold interval: " << coldInterval.count()
		<< "us, hot backlog limit: " << hotBacklogLimit << "\n\n";

	std::cout << std::left
		<< std::setw(9) << "Workers"
		<< std::setw(6) << "Hot"
		<< std::setw(10) << "ColdJobs"
		<< std::setw(10) << "HotJobs"
		<< std::setw(12) << "p50(us)"
		<< std::setw(12) << "p99(us)"
		<< "max(us)" << '\n';

	constexpr std::array workerCounts{ 1, 2, 4, 8 };

	for (const int workerCount : workerCounts)
	{
		for (const bool isHotZoneOn : { false, true })
		{
			const BenchConfig config{ workerCount, isHotZoneOn, spinIterationsPerMicrosecond };

			printRow(config, measureMedian(config));
		}
	}

	std::cout << "\nChecksum: " << sinkGuard << '\n';

	return 0;
}