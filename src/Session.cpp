#include "Session.h"

Session::Session(SOCKET socket, std::uint64_t sessionId)
	: m_socket(socket),
	m_sessionId(sessionId)
{
}

Session::~Session()
{
	closesocket(m_socket);
}

std::uint64_t Session::getSessionId() const
{
	return m_sessionId;
}

bool Session::isRecvOverlapped(const OVERLAPPED* overlapped) const
{
	return overlapped == &m_recvOverlapped;
}

bool Session::startRecv()
{
	m_recvOverlapped = {};

	WSABUF wsaBuf{};
	wsaBuf.buf = m_recvBuffer.data();
	wsaBuf.len = static_cast<ULONG>(m_recvBuffer.size());
	DWORD flags = 0;

	if (WSARecv(m_socket, &wsaBuf, 1, nullptr, &flags, &m_recvOverlapped, nullptr) == SOCKET_ERROR)
	{
		return WSAGetLastError() == WSA_IO_PENDING;
	}

	return true;
}

bool Session::startSend(DWORD length)
{
	m_sendLength = length;
	m_sentBytes = 0;
	return postSend();
}

bool Session::onSendCompleted(DWORD transferredBytes)
{
	m_sentBytes += transferredBytes;

	if (m_sentBytes < m_sendLength)
	{
		return postSend();
	}

	return startRecv();
}

bool Session::postSend()
{
	m_sendOverlapped = {};

	// 아직 보내지 않은 부분부터 보낸다.
	WSABUF wsaBuf{};
	wsaBuf.buf = m_recvBuffer.data() + m_sentBytes;
	wsaBuf.len = m_sendLength - m_sentBytes;

	if (WSASend(m_socket, &wsaBuf, 1, nullptr, 0, &m_sendOverlapped, nullptr) == SOCKET_ERROR)
	{
		return WSAGetLastError() == WSA_IO_PENDING;
	}

	return true;
}