#include "PacketAssembler.h"

#include <cstring>

void PacketAssembler::append(const char* data, std::size_t length)
{
	m_buffer.insert(m_buffer.end(), data, data + length);
}

PacketAssembler::PopResult PacketAssembler::tryPop(Packet& packet)
{
	if (m_buffer.size() < HeaderSize)
	{
		return PopResult::NeedMore;
	}

	std::uint16_t size = 0;
	std::uint16_t id = 0;
	std::memcpy(&size, m_buffer.data(), sizeof(size));
	std::memcpy(&id, m_buffer.data() + sizeof(size), sizeof(id));

	// 클라이언트가 보낸 길이는 믿지 않는다.
	// size가 헤더보다 작으면 0바이트씩 꺼내며 무한 반복하고, 너무 크면 끝없이 쌓인다.
	if (size < HeaderSize || size > MaxPacketSize)
	{
		return PopResult::Invalid;
	}

	if (m_buffer.size() < size)
	{
		return PopResult::NeedMore;
	}

	// 본문을 꺼내고, 꺼낸 패킷만큼 앞에서 지운다. 뒤에 붙어 온 바이트는 남는다.
	packet.id = id;
	packet.body.assign(m_buffer.begin() + HeaderSize, m_buffer.begin() + size);
	m_buffer.erase(m_buffer.begin(), m_buffer.begin() + size);
	return PopResult::Packet;
}

std::size_t PacketAssembler::getBufferedSize() const
{
	return m_buffer.size();
}