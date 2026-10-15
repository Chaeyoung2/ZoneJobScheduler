#include "IocpServer.h"
#include "Session.h"

#include <WS2tcpip.h>
#include <chrono>
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

	// 남은 세션의 소켓을 닫는다. 지우지는 않는다.
	// 걸려 있던 I/O가 실패로 완료되면 네트워크 스레드가 그 I/O의 지분을 놓는다.
	std::vector<std::shared_ptr<Session>> remaining;
	{
		std::lock_guard lock(m_sessionsMutex);
		for (auto& [sessionId, session] : m_sessions)
		{
			remaining.push_back(session);
		}
	}
	for (auto& session : remaining)
	{
		closeSession(*session);
	}
	remaining.clear();

	// 마지막 완료까지 처리되어 모든 세션이 소멸한 뒤에 네트워크 스레드를 끝낸다.
	while (m_liveSessionCount.load() > 0)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

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

int IocpServer::getLiveSessionCount() const
{
	return m_liveSessionCount.load();
}

int IocpServer::getClosedSessionCount() const
{
	return m_closedSessionCount.load();
}

long long IocpServer::getReceivedBytes() const
{
	return m_receivedBytes.load();
}

long long IocpServer::getSentBytes() const
{
	return m_sentBytes.load();
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

		auto session = std::make_shared<Session>(clientSocket, m_nextSessionId++, m_liveSessionCount);

		// completion key에는 shared_ptr을 담을 수 없어 주소만 넘긴다.
		// 그 주소의 세션은 걸려 있는 I/O의 지분이 살려 둔다.
		if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(clientSocket), m_iocp,
			reinterpret_cast<ULONG_PTR>(session.get()), 0) == nullptr)
		{
			continue;
		}

		{
			std::lock_guard lock(m_sessionsMutex);
			m_sessions.emplace(session->getSessionId(), session);
		}

		std::printf("[accept] session %llu connected\n", session->getSessionId());

		if (session->startRecv() == false)
		{
			closeSession(*session);
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

		// 완료된 I/O의 지분을 넘겨받는다. 이 반복이 끝날 때까지 세션이 살아 있고,
		// 다음 I/O를 걸지 않았다면 반복이 끝날 때 마지막 지분이 사라져 세션이 소멸한다.
		std::shared_ptr<Session> owner = session->releaseIoOwner(overlapped);

		if (succeeded == FALSE || transferredBytes == 0)
		{
			closeSession(*session);
			continue;
		}

		bool isNextIoStarted = false;

		if (session->isRecvOverlapped(overlapped))
		{
			// recv 완료: 받은 만큼 그대로 돌려보낸다.
			m_receivedBytes.fetch_add(transferredBytes);
			isNextIoStarted = session->startSend(transferredBytes);
		}
		else
		{
			// send 완료: 남은 걸 마저 보내거나, 다 보냈으면 다시 recv를 건다.
			m_sentBytes.fetch_add(transferredBytes);
			isNextIoStarted = session->onSendCompleted(transferredBytes);
		}

		if (isNextIoStarted == false)
		{
			closeSession(*session);
		}
	}
}

void IocpServer::closeSession(Session& session)
{
	// 이미 다른 곳에서 닫았으면 아무것도 하지 않는다.
	if (session.close() == false)
	{
		return;
	}

	std::printf("[network] session %llu closed\n", session.getSessionId());

	// 서버의 지분을 놓는다. 호출한 쪽이 지분을 쥐고 있으므로 여기서 소멸하지는 않는다.
	{
		std::lock_guard lock(m_sessionsMutex);
		m_sessions.erase(session.getSessionId());
	}
	m_closedSessionCount.fetch_add(1);
}