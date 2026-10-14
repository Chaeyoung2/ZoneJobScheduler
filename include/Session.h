#pragma once
#include <WinSock2.h>

#include <array>
#include <cstdint>

class Session
{
public:
	Session(SOCKET socket, std::uint64_t sessionId);
	~Session();

	Session(const Session&) = delete;
	Session& operator=(const Session&) = delete;

	std::uint64_t getSessionId() const;

	bool isRecvOverlapped(const OVERLAPPED* overlapped) const;

	bool startRecv();
	bool startSend(DWORD length);

	// send 완료 처리: 보낸 양을 더하고, 남은 게 있으면 이어서 보낸다.
	bool onSendCompleted(DWORD transferredBytes);

private:
	bool postSend();

	static constexpr int RecvBufferSize = 4096;

	SOCKET m_socket;
	std::uint64_t m_sessionId;

	OVERLAPPED m_recvOverlapped{};
	std::array<char, RecvBufferSize> m_recvBuffer{};

	OVERLAPPED m_sendOverlapped{};
	DWORD m_sendLength = 0;
	DWORD m_sentBytes = 0;
};