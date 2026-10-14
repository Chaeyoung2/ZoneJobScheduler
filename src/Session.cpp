#include "Session.h"

Session::Session(SOCKET socket, std::uint64_t sessionId)
	: m_socket(socket),
	m_sessionId(sessionId)
{}

Session::~Session()
{
	closesocket(m_socket);
}

std::uint64_t Session::getSessionId() const
{
	return m_sessionId;
}

const char* Session::getRecvBuffer() const
{
	return m_recvBuffer.data();
}

bool Session::startRecv()
{
	// 지난 recv의 결과가 남아 있으면 안 되므로 매번 0으로 초기화한다.
	m_recvOverlapped = {};

	WSABUF wsaBuf{};
	wsaBuf.buf = m_recvBuffer.data();
	wsaBuf.len = static_cast<ULONG>(m_recvBuffer.size());
	DWORD flags = 0;

	if (WSARecv(m_socket, &wsaBuf, 1, nullptr, &flags, &m_recvOverlapped, nullptr) == SOCKET_ERROR)
	{
		// WSA_IO_PENDING은 "걸렸고 나중에 완료된다"는 뜻이라 실패가 아니다.
		return WSAGetLastError() == WSA_IO_PENDING;
	}

	// 즉시 완료되어도 완료 통지는 IOCP로 똑같이 온다.
	return true;
}