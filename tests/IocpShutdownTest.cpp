// 클라이언트가 연결된 채로 서버를 종료해도 세션이 모두 정리되는지 확인한다.
#include "IocpServer.h"

#include <WS2tcpip.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace
{
	constexpr int NetworkThreadCount = 2;
	constexpr int ClientCount = 10;

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

	// 에코 한 번: 끝나면 서버는 다음 recv를 걸어 둔 상태가 된다.
	bool echoOnce(SOCKET clientSocket)
	{
		char message[4] = { 4, 0, 1, 0 };
		char reply[4] = {};
		return send(clientSocket, message, 4, 0) == 4
			&& recv(clientSocket, reply, 4, MSG_WAITALL) == 4;
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

	std::vector<SOCKET> clients;
	bool passed = false;
	{
		IocpServer server(NetworkThreadCount);
		if (server.start(0) == false)
		{
			std::printf("[main] server start failed\n");
			WSACleanup();
			return EXIT_FAILURE;
		}

		int echoedCount = 0;
		for (int c = 0; c < ClientCount; ++c)
		{
			SOCKET clientSocket = connectTo(server.getPort());
			if (clientSocket == INVALID_SOCKET)
			{
				break;
			}
			clients.push_back(clientSocket);

			if (echoOnce(clientSocket))
			{
				++echoedCount;
			}
		}
		std::printf("[main] echoed=%d/%d, live sessions before stop=%d\n",
			echoedCount, ClientCount, server.getLiveSessionCount());

		// 클라이언트는 연결을 유지한 채로 서버를 종료한다.
		server.stop();

		std::printf("[main] after stop: closed=%d/%d, live=%d\n",
			server.getClosedSessionCount(), ClientCount, server.getLiveSessionCount());

		passed = echoedCount == ClientCount
			&& server.getClosedSessionCount() == ClientCount
			&& server.getLiveSessionCount() == 0;
	}

	for (SOCKET clientSocket : clients)
	{
		closesocket(clientSocket);
	}
	WSACleanup();

	std::printf(passed ? "[main] PASS\n" : "[main] FAIL\n");
	return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}