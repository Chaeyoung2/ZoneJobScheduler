#include "IocpServer.h"

#include <WS2tcpip.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace
{
	constexpr int NetworkThreadCount = 2;

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

		SOCKET clientSocket = connectTo(server.getPort());
		if (clientSocket != INVALID_SOCKET)
		{
			const char message[] = "hello";
			const int length = static_cast<int>(std::strlen(message));
			send(clientSocket, message, length, 0);

			closesocket(clientSocket);

			const bool closed = waitForClosedSessions(server, 1);
			std::printf("[main] closed sessions=%d, received bytes=%lld\n",
				server.getClosedSessionCount(), server.getReceivedBytes());

			passed = closed && server.getReceivedBytes() == length;
		}

		server.stop();
	}

	WSACleanup();

	std::printf(passed ? "[main] PASS\n" : "[main] FAIL\n");
	return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}