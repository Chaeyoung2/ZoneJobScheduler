#include "ThreadPool.h"
#include "ZoneScheduler.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <latch>
#include <memory>

namespace
{
	std::atomic<int> createdCount{ 0 };
	std::atomic<int> destroyedCount{ 0 };

	// 임시 연결
	class FakeSession
	{
	public:
		explicit FakeSession(int playerId)
			: m_playerId(playerId)
		{
			createdCount.fetch_add(1);
		}

		~FakeSession()
		{
			destroyedCount.fetch_add(1);
		}

		FakeSession(const FakeSession&) = delete;
		FakeSession& operator=(const FakeSession&) = delete;

		int getPlayerId() const
		{
			return m_playerId;
		}

		void disconnect()
		{
			m_isConnected.store(false);
		}

		bool isConnected() const
		{
			return m_isConnected.load();
		}

	private:
		int m_playerId;
		std::atomic<bool> m_isConnected{ true };
	};
}

int main()
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);

	constexpr int zoneId = 0;

	std::latch sessionReleased(1);
	std::latch sessionJobDone(1);

	int destroyedAfterRelease = -1;

	{
		ZoneScheduler scheduler(1);
		ThreadPool pool(1, scheduler);

		scheduler.submit(zoneId, [&sessionReleased](Zone&)
			{
				sessionReleased.wait();
			});

		// 세션 수명 해결 테스트
		// 기존 SessionLifetimeRepro.cpp의 new -> shared_ptr 
		std::shared_ptr<FakeSession> session = std::make_shared<FakeSession>(42);

		scheduler.submit(zoneId, [session, &sessionJobDone](Zone&)
			{
				std::printf("[worker] session job: playerId=%d, connected=%d\n",
					session->getPlayerId(), session->isConnected() ? 1 : 0);
				sessionJobDone.count_down();
			});

		std::printf("[main] use_count before release=%ld\n", session.use_count());

		// 기존 SessionLifetimeRepro의 delete -> 카운트만 차감
		session->disconnect();
		session.reset();
		destroyedAfterRelease = destroyedCount.load();
		std::printf("[main] session released, destroyed=%d\n", destroyedAfterRelease);

		sessionReleased.count_down();
		sessionJobDone.wait();
	} // pool 소멸

	const int created = createdCount.load();
	const int destroyed = destroyedCount.load();
	std::printf("[main] created=%d, destroyed=%d\n", created, destroyed);

	if (destroyedAfterRelease != 0 || created != destroyed)
	{
		std::printf("[main] FAIL\n");
		return EXIT_FAILURE;
	}

	std::printf("[main] PASS\n");
	return EXIT_SUCCESS;
}