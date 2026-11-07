// Tests ENet, the library that carries the game's messages, against packets made to harm it, and the game's own limits on it
// (3dc/net_host.cpp). Written 2026-11-07 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// Anyone who can reach a game's port can connect, and then send ENet anything at all, not only what an honest copy of ENet
// would. ENet reads every packet before the game sees any of it. Our review of ENet found ways a connected machine could make
// the host hold or do far too much, and our ENet patch 005 (vcpkg-overlay-ports/enet) fixes them. Each test below connects a
// real ENet client to a host made the game's way, then builds packets by hand and sends them from the client's own socket. So
// they come from the connected address, with the connection's number and checksum, just as a hostile copy of the game's would:
// - a connection that asks for 255 channels gets the game's one (the game's limit, in net_host.cpp)
// - empty messages held behind a missing one count against the limit on what's waiting, and stop at it (patch 005)
// - a message in pieces that cover only part of it comes out with the rest cleared, not with leftover memory (patch 005)
// - a packet of 40 commands that each ask for an answer gets 32 answers, the most an honest ENet puts in a packet (patch 005)
// - a packet shorter than its own header changes nothing, even when leftover bytes would match its checksum (patch 005)
// And honest messages still work: a 3,000-byte message, which ENet sends in pieces, arrives whole, 500 small ones arrive in
// order, and once the host has read them all, nothing is counted as waiting.
//
// To show leftover memory, ENet is given this test's own malloc, which fills every new block with 0xAA bytes.
//
// Everything is on this machine's own loopback address, 127.0.0.1, which never leaves the machine, and the test checks that
// every socket it makes is there.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with 3dc/net_host.cpp and links vcpkg's ENet, built from our patched port, using
// tests/enet_hostile_test.args. tests.yml installs that ENet before building the tests. The program gives back a non-zero
// result if any check failed.

#include "net_host.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	fflush(stdout);
	checksFailed += good ? 0 : 1;
}

// ENet's memory comes from here, so that a block's old contents can be seen: every new one starts as 0xAA bytes
static void *ENET_CALLBACK FilledMalloc(size_t size)
{
	void *const memory = malloc(size);

	if (memory)
	{
		memset(memory, 0xAA, size);
	}

	return memory;
}

static void ENET_CALLBACK PlainFree(void *memory)
{
	free(memory);
}

static void ENET_CALLBACK OutOfMemory(void)
{
	printf("FAILED  ENet ran out of memory\n");
	exit(1);
}

// Room for the largest message in these tests. The game passes its own size, 3,072 bytes and a header, and net_start_test
// checks that it does.
static const size_t kLargestMessage = 4000;

// Whether an address is this machine's own loopback address, 127.0.0.1, which never leaves the machine. Says it as well.
static bool IsOnThisMachine(const ENetAddress &address, const char *what)
{
	char name[64] = "";

	enet_address_get_host_ip(&address, name, sizeof(name));
	printf("        %s: %s port %u\n", what, name, (unsigned)address.port);

	return strcmp(name, "127.0.0.1") == 0;
}

static ENetAddress ThisMachine()
{
	ENetAddress here;

	enet_address_set_host_ip(&here, "127.0.0.1");
	here.port = 0;   // any port the system has free
	return here;
}

// What the system really gave a socket, read back, not what was asked for
static bool SocketIsOnThisMachine(ENetSocket socket, const char *what)
{
	ENetAddress bound;

	return enet_socket_get_address(socket, &bound) == 0 && IsOnThisMachine(bound, what);
}

static void Append(std::vector<uint8_t> &to, const void *what, size_t size)
{
	if (size > 0)
	{
		const uint8_t *const bytes = (const uint8_t *)what;

		to.insert(to.end(), bytes, bytes + size);
	}
}

static void SendBytes(ENetSocket socket, const ENetAddress &to, const std::vector<uint8_t> &bytes)
{
	ENetBuffer buffer;

	buffer.data = (void *)bytes.data();
	buffer.dataLength = bytes.size();
	enet_socket_send(socket, &to, &buffer, 1);
}

// One connection: a host made the game's way, and a plain ENet client standing in for whoever connects
struct Pair
{
	ENetHost *host;
	ENetHost *client;
	ENetPeer *hostsSide;     // the connection, as the host has it
	ENetPeer *clientsSide;   // the connection, as the client has it
	ENetAddress hostAddress;
};

// Gives the host up to the time asked to read what's come, and hands back the messages it gave the game
static std::vector<std::vector<uint8_t>> LetTheHostRead(Pair &pair, DWORD milliseconds)
{
	std::vector<std::vector<uint8_t>> messages;
	DWORD const startedAt = GetTickCount();

	do
	{
		ENetEvent event;

		while (enet_host_service(pair.host, &event, 1) > 0)
		{
			if (event.type == ENET_EVENT_TYPE_RECEIVE)
			{
				messages.push_back(std::vector<uint8_t>(event.packet->data, event.packet->data + event.packet->dataLength));
				enet_packet_destroy(event.packet);
			}
		}
	}
	while (GetTickCount() - startedAt < milliseconds);

	return messages;
}

// A host made with Net_MakeENetHost, a client that asks for channelsAsked channels, and a connection between them that both
// sides see open. False if that couldn't be done in 3 s.
static bool MakePair(Pair *pair, size_t channelsAsked)
{
	ENetAddress here = ThisMachine();

	pair->host = Net_MakeENetHost(&here, 7, kLargestMessage, 0, 0);
	pair->client = enet_host_create(&here, 1, 0, 0, 0);
	pair->hostsSide = NULL;
	pair->clientsSide = NULL;

	if (!pair->host || !pair->client)
	{
		return false;
	}

	// The host throws away any packet without the right checksum, so the client has to put one on
	pair->client->checksum = enet_crc32;

	Check(SocketIsOnThisMachine(pair->host->socket, "the host's socket is on") && SocketIsOnThisMachine(pair->client->socket, "the client's socket is on"), "both sockets are on 127.0.0.1 only");

	if (enet_socket_get_address(pair->host->socket, &pair->hostAddress) != 0)
	{
		return false;
	}

	enet_address_set_host_ip(&pair->hostAddress, "127.0.0.1");
	pair->clientsSide = enet_host_connect(pair->client, &pair->hostAddress, channelsAsked, 0);

	if (!pair->clientsSide)
	{
		return false;
	}

	DWORD const startedAt = GetTickCount();

	while (GetTickCount() - startedAt < 3000)
	{
		ENetEvent event;

		if (enet_host_service(pair->host, &event, 1) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
		{
			pair->hostsSide = event.peer;
		}

		enet_host_service(pair->client, &event, 1);

		if (pair->hostsSide && pair->clientsSide->state == ENET_PEER_STATE_CONNECTED)
		{
			return true;
		}
	}

	return false;
}

static void EndPair(Pair &pair)
{
	if (pair.client)
	{
		enet_host_destroy(pair.client);
	}

	if (pair.host)
	{
		enet_host_destroy(pair.host);
	}
}

// The number of the next reliable message the host is waiting for on channel 0
static enet_uint16 NextNumber(const Pair &pair)
{
	return (enet_uint16)(pair.hostsSide->channels[0].incomingReliableSequenceNumber + 1);
}

// Sends commands from the client's own socket, under the header and checksum the host expects on that connection, as ENet
// itself does when it sends (enet_protocol_send_outgoing_commands): the connection's number with its session and a flag
// saying a time follows, the time (ENet only answers commands in a packet that has one), then the checksum, worked out with
// the connection's own number in its place
static void SendByHand(Pair &pair, const std::vector<uint8_t> &commands)
{
	std::vector<uint8_t> packet;
	enet_uint16 const peerID = ENET_HOST_TO_NET_16((enet_uint16)(pair.clientsSide->outgoingPeerID | ENET_PROTOCOL_HEADER_FLAG_SENT_TIME | (pair.clientsSide->outgoingSessionID << ENET_PROTOCOL_HEADER_SESSION_SHIFT)));
	enet_uint16 const sentTime = ENET_HOST_TO_NET_16((enet_uint16)(enet_time_get() & 0xFFFF));
	enet_uint32 const connection = pair.clientsSide->connectID;

	Append(packet, &peerID, sizeof(peerID));
	Append(packet, &sentTime, sizeof(sentTime));

	size_t const checksumAt = packet.size();

	Append(packet, &connection, sizeof(connection));
	Append(packet, commands.data(), commands.size());

	ENetBuffer buffer;

	buffer.data = packet.data();
	buffer.dataLength = packet.size();

	enet_uint32 const checksum = enet_crc32(&buffer, 1);

	memcpy(&packet[checksumAt], &checksum, sizeof(checksum));
	SendBytes(pair.client->socket, pair.hostAddress, packet);
}

static void AddReliable(std::vector<uint8_t> &commands, enet_uint16 number, const void *data, enet_uint16 size)
{
	ENetProtocolSendReliable command;

	command.header.command = (enet_uint8)(ENET_PROTOCOL_COMMAND_SEND_RELIABLE | ENET_PROTOCOL_COMMAND_FLAG_ACKNOWLEDGE);
	command.header.channelID = 0;
	command.header.reliableSequenceNumber = ENET_HOST_TO_NET_16(number);
	command.dataLength = ENET_HOST_TO_NET_16(size);
	Append(commands, &command, sizeof(command));
	Append(commands, data, size);
}

static void AddPing(std::vector<uint8_t> &commands, enet_uint16 number)
{
	ENetProtocolPing command;

	command.header.command = (enet_uint8)(ENET_PROTOCOL_COMMAND_PING | ENET_PROTOCOL_COMMAND_FLAG_ACKNOWLEDGE);
	command.header.channelID = 0xFF;
	command.header.reliableSequenceNumber = ENET_HOST_TO_NET_16(number);
	Append(commands, &command, sizeof(command));
}

// One piece of a message that comes in pieces, the way ENet sends a message too big for one packet
static void AddPiece(std::vector<uint8_t> &commands, enet_uint16 number, enet_uint16 firstNumber, enet_uint32 pieces, enet_uint32 piece, enet_uint32 messageSize, enet_uint32 offset, const void *data, enet_uint16 size)
{
	ENetProtocolSendFragment command;

	command.header.command = (enet_uint8)(ENET_PROTOCOL_COMMAND_SEND_FRAGMENT | ENET_PROTOCOL_COMMAND_FLAG_ACKNOWLEDGE);
	command.header.channelID = 0;
	command.header.reliableSequenceNumber = ENET_HOST_TO_NET_16(number);
	command.startSequenceNumber = ENET_HOST_TO_NET_16(firstNumber);
	command.dataLength = ENET_HOST_TO_NET_16(size);
	command.fragmentCount = ENET_HOST_TO_NET_32(pieces);
	command.fragmentNumber = ENET_HOST_TO_NET_32(piece);
	command.totalLength = ENET_HOST_TO_NET_32(messageSize);
	command.fragmentOffset = ENET_HOST_TO_NET_32(offset);
	Append(commands, &command, sizeof(command));
	Append(commands, data, size);
}

// Reads what the host sent the client, straight from the client's socket, for the time asked, and counts the answers
// (acknowledgements) to commands numbered from first up to, not including, last
static int CountAnswers(Pair &pair, enet_uint16 first, enet_uint16 last, DWORD milliseconds)
{
	int answers = 0;
	DWORD const startedAt = GetTickCount();

	while (GetTickCount() - startedAt < milliseconds)
	{
		uint8_t bytes[4096];
		ENetBuffer buffer;
		ENetAddress from;

		buffer.data = bytes;
		buffer.dataLength = sizeof(bytes);

		int const length = enet_socket_receive(pair.client->socket, &from, &buffer, 1);

		if (length <= 0)
		{
			Sleep(1);
			continue;
		}

		// The header: the connection's number, the time it was sent if the flag says so, and the checksum
		enet_uint16 peerID = 0;

		memcpy(&peerID, bytes, sizeof(peerID));
		peerID = ENET_NET_TO_HOST_16(peerID);

		size_t at = sizeof(enet_uint16) + ((peerID & ENET_PROTOCOL_HEADER_FLAG_SENT_TIME) ? sizeof(enet_uint16) : 0) + sizeof(enet_uint32);

		while (at + sizeof(ENetProtocolCommandHeader) <= (size_t)length)
		{
			ENetProtocolCommandHeader header;

			memcpy(&header, bytes + at, sizeof(header));

			size_t const size = enet_protocol_command_size(header.command);

			if (size == 0 || at + size > (size_t)length)
			{
				break;
			}

			int const kind = header.command & ENET_PROTOCOL_COMMAND_MASK;

			if (kind == ENET_PROTOCOL_COMMAND_ACKNOWLEDGE)
			{
				ENetProtocolAcknowledge answer;

				memcpy(&answer, bytes + at, sizeof(answer));

				enet_uint16 const number = ENET_NET_TO_HOST_16(answer.receivedReliableSequenceNumber);

				answers += (number >= first && number < last) ? 1 : 0;
			}
			else if (kind == ENET_PROTOCOL_COMMAND_SEND_RELIABLE)
			{
				ENetProtocolSendReliable message;

				memcpy(&message, bytes + at, sizeof(message));
				at += ENET_NET_TO_HOST_16(message.dataLength);
			}
			else if (kind == ENET_PROTOCOL_COMMAND_SEND_FRAGMENT)
			{
				ENetProtocolSendFragment piece;

				memcpy(&piece, bytes + at, sizeof(piece));
				at += ENET_NET_TO_HOST_16(piece.dataLength);
			}

			at += size;
		}
	}

	return answers;
}

static void TestTheGamesOneChannel()
{
	Pair pair;
	bool const connected = MakePair(&pair, 255);

	Check(connected, "a client that asks for 255 channels connects to a host made the game's way");
	Check(connected && pair.hostsSide->channelCount == 1, "and the host gives it the game's one channel: " + std::to_string(connected ? pair.hostsSide->channelCount : 0));
	Check(connected && pair.clientsSide->channelCount == 1, "and tells the client so");
	EndPair(pair);
}

static void TestEmptyMessagesBehindAMissingOne()
{
	Pair pair;
	bool const connected = MakePair(&pair, 1);

	Check(connected, "a client connects, to send empty messages behind one it never sends");

	if (!connected)
	{
		EndPair(pair);
		return;
	}

	// A small limit, so the test needn't send 8 MB: the rule is the same at any size
	size_t const limit = 64 * 1024;

	pair.host->maximumWaitingData = limit;

	// The message numbered next never comes, so ENet has to hold everything after it, in order, until it does
	enet_uint16 const next = NextNumber(pair);
	int const sent = 3000;

	for (int first = 0; first < sent; first += 32)
	{
		std::vector<uint8_t> commands;

		for (int i = first; i < first + 32 && i < sent; i++)
		{
			AddReliable(commands, (enet_uint16)(next + 1 + i), NULL, 0);
		}

		SendByHand(pair, commands);
		LetTheHostRead(pair, 0);
	}

	LetTheHostRead(pair, 200);

	size_t const held = enet_list_size(&pair.hostsSide->channels[0].incomingReliableCommands);

	Check(held > 0 && held < (size_t)sent && pair.hostsSide->totalWaitingData >= limit, "empty messages held behind a missing one count against the limit on what's waiting, and stop at it: " + std::to_string(held) + " of " + std::to_string(sent) + " held, " + std::to_string(pair.hostsSide->totalWaitingData) + " bytes counted against " + std::to_string(limit));
	EndPair(pair);
}

static void TestPiecesStartCleared()
{
	Pair pair;
	bool const connected = MakePair(&pair, 1);

	Check(connected, "a client connects, to send a message in pieces that don't cover it");

	if (!connected)
	{
		EndPair(pair);
		return;
	}

	enet_uint16 const first = NextNumber(pair);
	uint8_t const a = 'A', b = 'B';
	std::vector<uint8_t> commands;

	AddPiece(commands, first, first, 2, 0, 3000, 0, &a, 1);
	AddPiece(commands, (enet_uint16)(first + 1), first, 2, 1, 3000, 1, &b, 1);
	SendByHand(pair, commands);

	std::vector<std::vector<uint8_t>> const messages = LetTheHostRead(pair, 500);
	bool const arrived = messages.size() == 1 && messages[0].size() == 3000 && messages[0][0] == 'A' && messages[0][1] == 'B';
	size_t leftover = 0;

	for (size_t i = 2; arrived && i < messages[0].size(); i++)
	{
		leftover += messages[0][i] != 0 ? 1 : 0;
	}

	Check(arrived, "a message in two 1-byte pieces that says it's 3,000 bytes long comes out 3,000 bytes long, with its 2 bytes");
	Check(arrived && leftover == 0, "and the 2,998 bytes no piece covered are cleared, not left as they were in memory: " + std::to_string(leftover) + " weren't");
	EndPair(pair);
}

static void TestTooManyCommandsInAPacket()
{
	Pair pair;
	bool const connected = MakePair(&pair, 1);

	Check(connected, "a client connects, to send a packet of 40 commands");

	if (!connected)
	{
		EndPair(pair);
		return;
	}

	// Whatever the host sent while connecting isn't counted
	CountAnswers(pair, 0, 0, 50);

	std::vector<uint8_t> commands;

	for (int i = 0; i < 40; i++)
	{
		AddPing(commands, (enet_uint16)(1000 + i));
	}

	SendByHand(pair, commands);
	LetTheHostRead(pair, 100);

	int const answers = CountAnswers(pair, 1000, 1040, 300);

	Check(answers == 32, "a packet of 40 commands that each ask for an answer gets 32 answers, the most an honest ENet puts in a packet: " + std::to_string(answers));
	EndPair(pair);
}

static void TestAPacketShorterThanItsHeader()
{
	Pair pair;
	bool const connected = MakePair(&pair, 1);

	Check(connected, "a client connects, to send a packet shorter than its own header");

	if (!connected)
	{
		EndPair(pair);
		return;
	}

	// The 2 bytes the short packet will be: the connection's number, with no time on it. With the checksum, a header is 6.
	enet_uint16 const peerID = ENET_HOST_TO_NET_16((enet_uint16)(pair.clientsSide->outgoingPeerID | (pair.clientsSide->outgoingSessionID << ENET_PROTOCOL_HEADER_SESSION_SHIFT)));
	ENetBuffer two;

	two.data = (void *)&peerID;
	two.dataLength = sizeof(peerID);

	enet_uint32 const matching = enet_crc32(&two, 1);

	// First, from another socket, a packet the host drops at once, because it names a connection the host doesn't have. Its
	// bytes 2 to 5 stay in ENet's receive buffer, and they're the checksum the short packet would match if ENet read past it.
	ENetSocket const other = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
	ENetAddress here = ThisMachine();
	bool const otherIsHere = other != ENET_SOCKET_NULL && enet_socket_bind(other, &here) == 0 && SocketIsOnThisMachine(other, "the other socket is on");

	Check(otherIsHere, "a second socket, on 127.0.0.1 only");

	std::vector<uint8_t> planted;
	enet_uint16 const nobody = ENET_HOST_TO_NET_16(0x0FFE);

	Append(planted, &nobody, sizeof(nobody));
	Append(planted, &matching, sizeof(matching));

	std::vector<uint8_t> shortPacket;

	Append(shortPacket, &peerID, sizeof(peerID));

	if (otherIsHere)
	{
		SendBytes(other, pair.hostAddress, planted);
	}

	SendBytes(pair.client->socket, pair.hostAddress, shortPacket);

	// The host starts its byte counts again once a second, so that doesn't happen during this
	pair.host->bandwidthThrottleEpoch = enet_time_get();

	enet_uint32 const before = pair.hostsSide->incomingDataTotal;

	LetTheHostRead(pair, 50);

	enet_uint32 const taken = pair.hostsSide->incomingDataTotal - before;

	Check(otherIsHere && taken == 0, "a 2-byte packet, shorter than its own header, is dropped, even when the bytes after it would match its checksum: " + std::to_string(taken) + " bytes taken from it");

	if (other != ENET_SOCKET_NULL)
	{
		enet_socket_destroy(other);
	}

	EndPair(pair);
}

static void TestHonestMessages()
{
	Pair pair;
	bool const connected = MakePair(&pair, 1);

	Check(connected, "a client connects, to send honest messages");

	if (!connected)
	{
		EndPair(pair);
		return;
	}

	std::vector<uint8_t> big(3000);

	for (size_t i = 0; i < big.size(); i++)
	{
		big[i] = (uint8_t)(i * 7 + 1);
	}

	int const smallOnes = 500;

	enet_peer_send(pair.clientsSide, 0, enet_packet_create(big.data(), big.size(), ENET_PACKET_FLAG_RELIABLE));

	for (int i = 0; i < smallOnes; i++)
	{
		std::string const words = "message " + std::to_string(i);

		enet_peer_send(pair.clientsSide, 0, enet_packet_create(words.data(), words.size(), ENET_PACKET_FLAG_RELIABLE));
	}

	std::vector<std::vector<uint8_t>> messages;
	DWORD const startedAt = GetTickCount();

	while (messages.size() < (size_t)(1 + smallOnes) && GetTickCount() - startedAt < 5000)
	{
		ENetEvent event;

		enet_host_service(pair.client, &event, 1);

		std::vector<std::vector<uint8_t>> const read = LetTheHostRead(pair, 0);

		messages.insert(messages.end(), read.begin(), read.end());
	}

	bool inOrder = messages.size() == (size_t)(1 + smallOnes);

	for (int i = 0; inOrder && i < smallOnes; i++)
	{
		std::string const words = "message " + std::to_string(i);

		inOrder = messages[1 + i] == std::vector<uint8_t>(words.begin(), words.end());
	}

	Check(!messages.empty() && messages[0] == big, "an honest 3,000-byte message, which ENet sends in pieces, arrives whole");
	Check(inOrder, "and 500 small messages after it arrive, in order: " + std::to_string(messages.empty() ? 0 : messages.size() - 1));
	Check(pair.hostsSide->totalWaitingData == 0, "and once the host has read them all, nothing is counted as waiting: " + std::to_string(pair.hostsSide->totalWaitingData));
	EndPair(pair);
}

int main()
{
	ENetCallbacks callbacks = { FilledMalloc, PlainFree, OutOfMemory };

	if (enet_initialize_with_callbacks(ENET_VERSION, &callbacks) != 0)
	{
		printf("FAILED  ENet could not start\n");
		return 1;
	}

	TestTheGamesOneChannel();
	TestEmptyMessagesBehindAMissingOne();
	TestPiecesStartCleared();
	TestTooManyCommandsInAPacket();
	TestAPacketShorterThanItsHeader();
	TestHonestMessages();

	enet_deinitialize();
	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "The hostile ENet test passed");

	return checksFailed ? 1 : 0;
}
