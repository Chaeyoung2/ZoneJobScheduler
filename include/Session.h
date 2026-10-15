#pragma once
#include <WinSock2.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

class Session : public std::enable_shared_from_this<Session>
{
public:
	Session(SOCKET socket, std::uint64_t sessionId, std::atomic<int>& liveSessionCount);
	~Session();

	Session(const Session&) = delete;
	Session& operator=(const Session&) = delete;

	std::uint64_t getSessionId() const;

	bool isRecvOverlapped(const OVERLAPPED* overlapped) const;

	bool startRecv();
	bool startSend(DWORD length);

	// send 완료 처리: 보낸 양을 더하고, 남은 게 있으면 이어서 보낸다.
	bool onSendCompleted(DWORD transferredBytes);

	// 완료된 I/O가 쥐고 있던 지분을 꺼내 넘긴다.
	std::shared_ptr<Session> releaseIoOwner(const OVERLAPPED* overlapped);

	// 소켓을 닫는다. 걸려 있던 I/O는 실패로 완료된다. 처음 닫은 호출만 true.
	bool close();

private:
	bool postSend();

	static constexpr int RecvBufferSize = 4096;

	std::atomic<SOCKET> m_socket;
	std::uint64_t m_sessionId;
	std::atomic<int>& m_liveSessionCount;

	OVERLAPPED m_recvOverlapped{};
	std::array<char, RecvBufferSize> m_recvBuffer{};

	OVERLAPPED m_sendOverlapped{};
	DWORD m_sendLength = 0;
	DWORD m_sentBytes = 0;

	// 걸려 있는 I/O가 쥐는 지분. 완료되면 releaseIoOwner로 넘겨준다.
	std::shared_ptr<Session> m_recvOwner;
	std::shared_ptr<Session> m_sendOwner;
};