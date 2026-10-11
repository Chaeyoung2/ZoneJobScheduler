// Use-After-Free 고의로 발생시키기

#include "ThreadPool.h"
#include "ZoneScheduler.h"

#include <cstdio>
#include <latch>

namespace
{
	// 임시 연결
	class FakeSession
	{
	public:
		explicit FakeSession(int playerId)
			: m_playerId(playerId)
		{
		}

		int getPlayerId() const 
		{
			return m_playerId;
		}

	private:
		int m_playerId;
	};
}

int main()
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);

	constexpr int zoneId = 0;

	std::latch sessionDeleted(1);
	std::latch sessionJobDone(1);

	ZoneScheduler scheduler(1);
	ThreadPool pool(1, scheduler);

	scheduler.submit(zoneId, [&sessionDeleted](Zone&)
		{
			sessionDeleted.wait();
		});

	FakeSession* session = new FakeSession(42);

	scheduler.submit(zoneId, [session, &sessionJobDone](Zone&)
		{
			std::printf("[worker] session job: playerId=%d\n", session->getPlayerId());
			sessionJobDone.count_down();
		});

	// job이 아직 대기 중이지만 세션을 지우도록
	delete session;
	std::printf("[main] session deleted\n");

	// 이제 worker가 지워진 세션에 접근한다
	sessionDeleted.count_down();
	sessionJobDone.wait();

	std::printf("[main] done (ASan 빌드라면 여기까지 오지 않아야 한다)\n");
	return 0;
}

