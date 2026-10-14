#pragma once
#include <WinSock2.h>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

class Session;

class IocpServer
{
public:
	explicit IocpServer(int networkThreadCount);
	~IocpServer();

	IocpServer(const IocpServer&) = delete;
	IocpServer& operator=(const IocpServer&) = delete;

	bool start(std::uint16_t port);
	void stop();

	std::uint16_t getPort() const;
	int getClosedSessionCount() const;
	long long getReceivedBytes() const;

private:
	void acceptLoop();
	void networkLoop();
	void closeSession(Session* session);

	int m_networkThreadCount;
	bool m_isStarted = false;

	SOCKET m_listenSocket = INVALID_SOCKET;
	HANDLE m_iocp = nullptr;
	std::uint16_t m_port = 0;

	std::thread m_acceptThread;
	std::vector<std::thread> m_networkThreads;

	std::uint64_t m_nextSessionId = 1;
	std::atomic<int> m_closedSessionCount{ 0 };
	std::atomic<long long> m_receivedBytes{ 0 };
};