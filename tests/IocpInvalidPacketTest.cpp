// 잘못된 size의 패킷을 보낸 클라이언트를 서버가 끊는지 확인한다.
#include "IocpServer.h"

#include <WS2tcpip.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace
{
	constexpr int NetworkThreadCount = 2;
	constexpr DWORD ReceiveTimeoutMs = 5000;

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

		// 서버가 끊지 않으면 recv가 영원히 기다리므로 제한 시간을 둔다.
		setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO,
			reinterpret_cast<const char*>(&ReceiveTimeoutMs), sizeof(ReceiveTimeoutMs));

		return clientSocket;
	}

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

		SOCKET clientSocket = connectTo(server.getPort());

		// 1. 정상 패킷(헤더만, size = 4, id = 1)은 에코된다.
		const char validPacket[4] = { 4, 0, 1, 0 };
		char reply[4] = {};
		const bool echoed = clientSocket != INVALID_SOCKET
			&& send(clientSocket, validPacket, 4, 0) == 4
			&& recv(clientSocket, reply, 4, MSG_WAITALL) == 4;

		// 2. size = 0인 위조 헤더를 보내면 서버가 연결을 끊는다.
		//    끊기면 recv가 0(정상 종료)이나 오류를 돌려주고, 끊지 않으면 제한 시간에 걸린다.
		const char invalidPacket[4] = { 0, 0, 1, 0 };
		bool closedByServer = false;
		if (echoed && send(clientSocket, invalidPacket, 4, 0) == 4)
		{
			char byte = 0;
			const int result = recv(clientSocket, &byte, 1, 0);
			closedByServer = result == 0
				|| (result == SOCKET_ERROR && WSAGetLastError() != WSAETIMEDOUT);
		}

		const bool closed = waitForClosedSessions(server, 1);

		std::printf("[main] echoed=%d, closed by server=%d\n", echoed, closedByServer);
		std::printf("[main] server packets=%d, invalid=%d, closed sessions=%d\n",
			server.getReceivedPacketCount(), server.getInvalidPacketCount(), server.getClosedSessionCount());

		passed = echoed
			&& closedByServer
			&& closed
			&& server.getReceivedPacketCount() == 1
			&& server.getInvalidPacketCount() == 1;

		if (clientSocket != INVALID_SOCKET)
		{
			closesocket(clientSocket);
		}
		server.stop();
	}

	WSACleanup();

	std::printf(passed ? "[main] PASS\n" : "[main] FAIL\n");
	return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}