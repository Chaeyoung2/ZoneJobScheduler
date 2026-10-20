// 패킷 여러 개를 한 번에 보내도 패킷마다 응답이 순서대로 오는지 확인한다.
#include "IocpServer.h"
#include "PacketAssembler.h"

#include <WS2tcpip.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace
{
	constexpr int NetworkThreadCount = 2;
	constexpr int PacketCount = 100;
	constexpr int BodySize = 1000;
	constexpr int PacketSize = static_cast<int>(PacketAssembler::HeaderSize) + BodySize;
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

		// 패킷 100개를 하나의 버퍼에 이어 붙인다. id는 0~99로 순서를 표시한다.
		std::vector<char> sent(PacketCount * PacketSize);
		for (int i = 0; i < PacketCount; ++i)
		{
			char* packet = sent.data() + i * PacketSize;
			const auto size = static_cast<std::uint16_t>(PacketSize);
			const auto id = static_cast<std::uint16_t>(i);
			std::memcpy(packet, &size, sizeof(size));
			std::memcpy(packet + sizeof(size), &id, sizeof(id));
			std::memset(packet + PacketAssembler::HeaderSize, 'a' + i % 26, BodySize);
		}

		// 한 번에 보낸다. 서버의 recv 한 번에 패킷 여러 개가 붙어 들어온다.
		SOCKET clientSocket = connectTo(server.getPort());
		std::vector<char> received(sent.size());
		const bool sentAll = clientSocket != INVALID_SOCKET
			&& send(clientSocket, sent.data(), static_cast<int>(sent.size()), 0) == static_cast<int>(sent.size());
		const bool receivedAll = sentAll
			&& recv(clientSocket, received.data(), static_cast<int>(received.size()), MSG_WAITALL)
				== static_cast<int>(received.size());

		// 응답이 패킷 순서 그대로 와야 한다.
		const bool inOrder = receivedAll && sent == received;

		if (clientSocket != INVALID_SOCKET)
		{
			closesocket(clientSocket);
		}
		const bool closed = waitForClosedSessions(server, 1);

		std::printf("[main] received all=%d, in order=%d\n", receivedAll, inOrder);
		std::printf("[main] server packets=%d/%d, sent bytes=%lld/%zu\n",
			server.getReceivedPacketCount(), PacketCount, server.getSentBytes(), sent.size());

		passed = inOrder
			&& closed
			&& server.getReceivedPacketCount() == PacketCount
			&& server.getSentBytes() == static_cast<long long>(sent.size());

		server.stop();
	}

	WSACleanup();

	std::printf(passed ? "[main] PASS\n" : "[main] FAIL\n");
	return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}