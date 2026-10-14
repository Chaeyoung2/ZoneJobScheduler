#include "IocpServer.h"
#include "Session.h"

#include <WS2tcpip.h>
#include <cstdio>

IocpServer::IocpServer(int networkThreadCount)
	: m_networkThreadCount(networkThreadCount)
{
}

IocpServer::~IocpServer()
{
	stop();
}

bool IocpServer::start(std::uint16_t port)
{
	if (m_isStarted)
	{
		return false;
	}

	m_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
	if (m_iocp == nullptr)
	{
		return false;
	}

	m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

	if (m_listenSocket == INVALID_SOCKET
		|| bind(m_listenSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR
		|| listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		closesocket(m_listenSocket);
		m_listenSocket = INVALID_SOCKET;
		CloseHandle(m_iocp);
		m_iocp = nullptr;
		return false;
	}

	int addressLength = sizeof(address);
	getsockname(m_listenSocket, reinterpret_cast<sockaddr*>(&address), &addressLength);
	m_port = ntohs(address.sin_port);

	m_isStarted = true;

	for (int i = 0; i < m_networkThreadCount; ++i)
	{
		m_networkThreads.emplace_back(&IocpServer::networkLoop, this);
	}

	m_acceptThread = std::thread(&IocpServer::acceptLoop, this);
	return true;
}

void IocpServer::stop()
{
	if (m_isStarted == false)
	{
		return;
	}

	m_isStarted = false;

	closesocket(m_listenSocket);
	m_listenSocket = INVALID_SOCKET;
	m_acceptThread.join();

	for (int i = 0; i < m_networkThreadCount; ++i)
	{
		PostQueuedCompletionStatus(m_iocp, 0, 0, nullptr);
	}

	for (auto& thread : m_networkThreads)
	{
		thread.join();
	}
	m_networkThreads.clear();

	CloseHandle(m_iocp);
	m_iocp = nullptr;
}

std::uint16_t IocpServer::getPort() const
{
	return m_port;
}

int IocpServer::getClosedSessionCount() const
{
	return m_closedSessionCount.load();
}

long long IocpServer::getReceivedBytes() const
{
	return m_receivedBytes.load();
}

void IocpServer::acceptLoop()
{
	while (true)
	{
		SOCKET clientSocket = accept(m_listenSocket, nullptr, nullptr);
		if (clientSocket == INVALID_SOCKET)
		{
			break;
		}

		Session* session = new Session(clientSocket, m_nextSessionId++);

		if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(clientSocket), m_iocp,
			reinterpret_cast<ULONG_PTR>(session), 0) == nullptr)
		{
			delete session;
			continue;
		}

		std::printf("[accept] session %llu connected\n", session->getSessionId());

		if (session->startRecv() == false)
		{
			closeSession(session);
		}
	}
}

void IocpServer::networkLoop()
{
	while (true)
	{
		DWORD transferredBytes = 0;
		ULONG_PTR completionKey = 0;
		OVERLAPPED* overlapped = nullptr;

		const BOOL succeeded = GetQueuedCompletionStatus(
			m_iocp, &transferredBytes, &completionKey, &overlapped, INFINITE);

		if (overlapped == nullptr)
		{
			break;
		}

		Session* session = reinterpret_cast<Session*>(completionKey);

		if (succeeded == FALSE || transferredBytes == 0)
		{
			closeSession(session);
			continue;
		}

		m_receivedBytes.fetch_add(transferredBytes);
		std::printf("[network] session %llu received %lu bytes: %.*s\n",
			session->getSessionId(), transferredBytes,
			static_cast<int>(transferredBytes), session->getRecvBuffer());

		if (session->startRecv() == false)
		{
			closeSession(session);
		}
	}
}

void IocpServer::closeSession(Session* session)
{
	std::printf("[network] session %llu closed\n", session->getSessionId());
	delete session;
	m_closedSessionCount.fetch_add(1);
}