#include "IocpServer.h"

#include <WS2tcpip.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

namespace
{
	constexpr int NetworkThreadCount = 2;
	constexpr int ClientCount = 10;
	constexpr int MessagesPerClient = 100;
	constexpr int MaxMessageLength = 10000;

	SOCKET connectTo(std::uint16_t port)
	{
		SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (clientSocket == INVALID_SOCKET)
		{
			return INVALID_SOCKET;
		}

		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_port = htons(port);
		inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

		if (connect(clientSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
		{
			closesocket(clientSocket);
			return INVALID_SOCKET;
		}

		return clientSocket;
	}

	// 서버의 네트워크 스레드가 처리할 시간을 주며 조건이 맞을 때까지 기다린다
	bool waitForClosedSessions(const IocpServer& server, int expected)
	{
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (server.getClosedSessionCount() != expected)
		{
			if (std::chrono::steady_clock::now() > deadline)
			{
				return false;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		return true;
	}

	// TCP는 메시지 경계가 없으므로 원하는 바이트 수를 다 받을 때까지 반복한다.
	bool receiveExactly(SOCKET clientSocket, char* buffer, int length)
	{
		int received = 0;
		while (received < length)
		{
			const int result = recv(clientSocket, buffer + received, length - received, 0);
			if (result <= 0)
			{
				return false;
			}
			received += result;
		}
		return true;
	}

	int getMessageLength(int clientIndex, int messageIndex)
	{
		return 1 + (clientIndex * 31 + messageIndex * 997) % MaxMessageLength;
	}

	// 메시지를 보내고 같은 바이트가 돌아오는지 확인한다. 일치한 메시지 수를 반환한다.
	int runEchoClient(std::uint16_t port, int clientIndex)
	{
		SOCKET clientSocket = connectTo(port);
		if (clientSocket == INVALID_SOCKET)
		{
			return 0;
		}

		int matchedCount = 0;
		std::vector<char> sent;
		std::vector<char> received;

		for (int i = 0; i < MessagesPerClient; ++i)
		{
			const int length = getMessageLength(clientIndex, i);
			sent.resize(length);
			received.assign(length, 0);
			for (int b = 0; b < length; ++b)
			{
				sent[b] = static_cast<char>(clientIndex + i + b);
			}

			if (send(clientSocket, sent.data(), length, 0) != length
				|| receiveExactly(clientSocket, received.data(), length) == false
				|| sent != received)
			{
				break;
			}
			++matchedCount;
		}

		closesocket(clientSocket);
		return matchedCount;
	}
}

int main()
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);

	WSADATA wsaData{};
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::printf("[main] WSAStartup failed\n");
		return EXIT_FAILURE;
	}

	long long expectedBytes = 0;
	for (int c = 0; c < ClientCount; ++c)
	{
		for (int i = 0; i < MessagesPerClient; ++i)
		{
			expectedBytes += getMessageLength(c, i);
		}
	}

	bool passed = false;
	{
		IocpServer server(NetworkThreadCount);
		if (server.start(0) == false)
		{
			std::printf("[main] server start failed\n");
			WSACleanup();
			return EXIT_FAILURE;
		}
		std::printf("[main] server port=%u\n", server.getPort());

		std::atomic<int> totalMatched{ 0 };
		{
			std::vector<std::jthread> clients;
			for (int c = 0; c < ClientCount; ++c)
			{
				clients.emplace_back([&totalMatched, port = server.getPort(), c]()
					{
						totalMatched.fetch_add(runEchoClient(port, c));
					});
			}
		} // jthread 소멸 시 join

		const bool closed = waitForClosedSessions(server, ClientCount);
		const int expectedMessages = ClientCount * MessagesPerClient;

		std::printf("[main] echo matched=%d/%d\n", totalMatched.load(), expectedMessages);
		std::printf("[main] server received=%lld, sent=%lld, expected=%lld bytes\n",
			server.getReceivedBytes(), server.getSentBytes(), expectedBytes);
		std::printf("[main] closed sessions=%d/%d\n", server.getClosedSessionCount(), ClientCount);

		passed = closed
			&& totalMatched.load() == expectedMessages
			&& server.getReceivedBytes() == expectedBytes
			&& server.getSentBytes() == expectedBytes;

		server.stop();
	}

	WSACleanup();

	std::printf(passed ? "[main] PASS\n" : "[main] FAIL\n");
	return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}