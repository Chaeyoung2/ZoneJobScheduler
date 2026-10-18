#include "Session.h"

Session::Session(SOCKET socket, std::uint64_t sessionId, std::atomic<int>& liveSessionCount)
	: m_socket(socket),
	m_sessionId(sessionId),
	m_liveSessionCount(liveSessionCount)
{
	m_liveSessionCount.fetch_add(1);
}

Session::~Session()
{
	close();
	m_liveSessionCount.fetch_sub(1);
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

	// 완료가 다른 스레드에서 바로 올 수 있으므로 지분은 걸기 전에 쥔다.
	m_recvOwner = shared_from_this();

	if (WSARecv(m_socket.load(), &wsaBuf, 1, nullptr, &flags, &m_recvOverlapped, nullptr) == SOCKET_ERROR
		&& WSAGetLastError() != WSA_IO_PENDING)
	{
		// 걸리지 않았으니 완료도 오지 않는다. 쥐었던 지분을 돌려놓는다.
		m_recvOwner.reset();
		return false;
	}

	return true;
}

void Session::appendReceived(DWORD transferredBytes)
{
	m_assembler.append(m_recvBuffer.data(), transferredBytes);
}

PacketAssembler::PopResult Session::tryPopPacket(Packet& packet)
{
	return m_assembler.tryPop(packet);
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

std::shared_ptr<Session> Session::releaseIoOwner(const OVERLAPPED* overlapped)
{
	if (isRecvOverlapped(overlapped))
	{
		return std::move(m_recvOwner);
	}
	return std::move(m_sendOwner);
}

bool Session::close()
{
	const SOCKET socket = m_socket.exchange(INVALID_SOCKET);
	if (socket == INVALID_SOCKET)
	{
		return false;
	}

	closesocket(socket);
	return true;
}

bool Session::postSend()
{
	m_sendOverlapped = {};

	// 아직 보내지 않은 부분부터 보낸다.
	WSABUF wsaBuf{};
	wsaBuf.buf = m_recvBuffer.data() + m_sentBytes;
	wsaBuf.len = m_sendLength - m_sentBytes;

	m_sendOwner = shared_from_this();

	if (WSASend(m_socket.load(), &wsaBuf, 1, nullptr, 0, &m_sendOverlapped, nullptr) == SOCKET_ERROR
		&& WSAGetLastError() != WSA_IO_PENDING)
	{
		m_sendOwner.reset();
		return false;
	}

	return true;
}