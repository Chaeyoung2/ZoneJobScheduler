#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// [size 2바이트 | id 2바이트 | 본문]
struct Packet
{
	std::uint16_t id = 0;
	std::vector<char> body;
};

class PacketAssembler
{
public:
	static constexpr std::size_t HeaderSize = 4;
	static constexpr std::size_t MaxPacketSize = 4096;

	enum class PopResult
	{
		Packet,		// 패킷 하나를 꺼냈다.
		NeedMore,	// 아직 다 오지 않았다. 다음 recv를 기다린다
		Invalid,	// size가 잘못되었다. 세션을 닫아야 한다
	};

	void append(const char* data, std::size_t length);

	PopResult tryPop(Packet& packet);

	std::size_t getBufferedSize() const;

private:
	std::vector<char> m_buffer; // 조립 버퍼
};