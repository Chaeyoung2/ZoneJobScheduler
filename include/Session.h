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
	const char* getRecvBuffer() const;

	bool startRecv();

private:
	static constexpr int RecvBufferSize = 4096;

	SOCKET m_socket;
	std::uint64_t m_sessionId;

	OVERLAPPED m_recvOverlapped{};
	std::array<char, RecvBufferSize> m_recvBuffer{};
};