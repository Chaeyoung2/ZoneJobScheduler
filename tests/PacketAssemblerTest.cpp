// recv가 쪼개져 오든 붙어서 오든 패킷을 정확히 꺼내는지, 잘못된 size를 거부하는지 확인한다.
// 네트워크 없이 PacketAssembler에 바이트를 직접 넣는다.
#include "PacketAssembler.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
	std::vector<char> makeHeader(std::uint16_t size, std::uint16_t id)
	{
		std::vector<char> bytes(PacketAssembler::HeaderSize);
		std::memcpy(bytes.data(), &size, sizeof(size));
		std::memcpy(bytes.data() + sizeof(size), &id, sizeof(id));
		return bytes;
	}

	std::vector<char> makePacket(std::uint16_t id, const std::string& body)
	{
		const auto size = static_cast<std::uint16_t>(PacketAssembler::HeaderSize + body.size());
		std::vector<char> bytes = makeHeader(size, id);
		bytes.insert(bytes.end(), body.begin(), body.end());
		return bytes;
	}

	void appendBytes(std::vector<char>& to, const std::vector<char>& from)
	{
		to.insert(to.end(), from.begin(), from.end());
	}

	struct TestCase
	{
		const char* name;
		std::vector<char> stream;		// 클라이언트가 보낸 바이트 전체
		std::size_t chunkSize;			// recv 한 번에 오는 바이트 수
		std::vector<Packet> expected;	// 꺼내져야 하는 패킷
		bool expectInvalid;
		std::size_t expectedBuffered;	// 끝났을 때 남아 있어야 하는 바이트
	};

	bool runCase(const TestCase& testCase)
	{
		PacketAssembler assembler;
		std::vector<Packet> popped;
		bool isInvalid = false;

		// recv 완료를 흉내 낸다: chunkSize씩 넣고, 넣을 때마다 꺼낼 수 있는 만큼 꺼낸다.
		for (std::size_t offset = 0; offset < testCase.stream.size() && isInvalid == false;
			offset += testCase.chunkSize)
		{
			const std::size_t length = std::min(testCase.chunkSize, testCase.stream.size() - offset);
			assembler.append(testCase.stream.data() + offset, length);

			while (true)
			{
				Packet packet;
				const PacketAssembler::PopResult result = assembler.tryPop(packet);
				if (result == PacketAssembler::PopResult::Packet)
				{
					popped.push_back(std::move(packet));
					continue;
				}
				isInvalid = result == PacketAssembler::PopResult::Invalid;
				break;
			}
		}

		bool isContentMatched = popped.size() == testCase.expected.size();
		for (std::size_t i = 0; isContentMatched && i < popped.size(); ++i)
		{
			isContentMatched = popped[i].id == testCase.expected[i].id
				&& popped[i].body == testCase.expected[i].body;
		}

		const bool passed = isContentMatched
			&& isInvalid == testCase.expectInvalid
			&& assembler.getBufferedSize() == testCase.expectedBuffered;

		std::printf("%-22s | sent bytes %5zu | chunk %5zu | popped %zu/%zu | invalid %-3s | buffered %zu | %s\n",
			testCase.name, testCase.stream.size(), testCase.chunkSize,
			popped.size(), testCase.expected.size(), isInvalid ? "yes" : "no",
			assembler.getBufferedSize(), passed ? "PASS" : "FAIL");
		return passed;
	}

	Packet expectedPacket(std::uint16_t id, const std::string& body)
	{
		return Packet{ id, std::vector<char>(body.begin(), body.end()) };
	}
}

int main()
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);

	const std::vector<char> move = makePacket(1, "move01");		// 10바이트
	const std::vector<char> attack = makePacket(2, "at");		// 6바이트
	const std::vector<char> chat = makePacket(3, std::string(1000, 'c'));
	const std::vector<char> empty = makePacket(4, "");			// 헤더만 있는 4바이트

	std::vector<char> threePackets;
	appendBytes(threePackets, move);
	appendBytes(threePackets, attack);
	appendBytes(threePackets, chat);

	std::vector<char> moveThenAttack;
	appendBytes(moveThenAttack, move);
	appendBytes(moveThenAttack, attack);

	std::vector<char> moveThenZeroSize = move;
	appendBytes(moveThenZeroSize, makeHeader(0, 1));

	const std::vector<Packet> threeExpected = {
		expectedPacket(1, "move01"), expectedPacket(2, "at"), expectedPacket(3, std::string(1000, 'c')) };

	const std::vector<TestCase> testCases = {
		{ "one packet at once", move, move.size(), { expectedPacket(1, "move01") }, false, 0 },
		{ "split 1 byte each", threePackets, 1, threeExpected, false, 0 },
		{ "three glued at once", threePackets, threePackets.size(), threeExpected, false, 0 },
		{ "boundary inside recv", moveThenAttack, 12,
			{ expectedPacket(1, "move01"), expectedPacket(2, "at") }, false, 0 },
		{ "header only packet", empty, empty.size(), { expectedPacket(4, "") }, false, 0 },
		{ "header not complete", { move[0] }, 1, {}, false, 1 },
		{ "size 0", makeHeader(0, 1), 4, {}, true, 4 },
		{ "size 3", makeHeader(3, 1), 4, {}, true, 4 },
		{ "size over max", makeHeader(static_cast<std::uint16_t>(PacketAssembler::MaxPacketSize + 1), 1), 4, {}, true, 4 },
		{ "valid then size 0", moveThenZeroSize, moveThenZeroSize.size(),
			{ expectedPacket(1, "move01") }, true, 4 },
	};

	int passedCount = 0;
	for (const TestCase& testCase : testCases)
	{
		if (runCase(testCase))
		{
			++passedCount;
		}
	}

	const bool passed = passedCount == static_cast<int>(testCases.size());
	std::printf("[main] cases passed=%d/%zu\n", passedCount, testCases.size());
	std::printf(passed ? "[main] PASS\n" : "[main] FAIL\n");
	return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
