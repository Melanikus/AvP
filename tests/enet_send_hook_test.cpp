// Tests our ENet patch 007 (vcpkg-overlay-ports/enet/007-a-seal-before-the-wire.patch): every datagram an ENet host sends goes through
// the hook a program sets (ENetHost::sendHook), and nothing goes around it. Written 2026-11-09 by Melanikus for the AvP port.
//
// WHY IT MATTERS. Through a version 2 relay, the game seals every one of ENet's datagrams end to end (CRYPTO.md section 9): the hook is
// where that happens. A datagram that left by another way would leave unsealed. So this checks, on real ENet hosts:
//   1. with no hook, ENet connects as it always has;
//   2. with a hook that passes everything on to the socket, a connection is made, and the hook saw every datagram the host sent: as
//      many as ENet counted, plus the token answer patch 006 sends outside any connection (a COOKIE), which ENet doesn't count. Each went
//      to the address it was meant for;
//   3. with a hook that drops everything (as the game does before it has keys), nothing reaches the other side at all, not even the
//      token answer, ENet goes on without an error, as if the datagrams were lost on the way, and no connection is made;
//   4. a hook that reports an error makes enet_host_service report one, as the socket's own error would;
//   5. with the hook taken away again, the same host connects as before.
//
// Everything is on this machine's own loopback address, 127.0.0.1, which never leaves the machine, and the test checks that every socket
// it makes is there. A network test: GitHub runs it (team rule, 2026-11-07); it isn't run on the user's machine.

#include "net_host.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

static int hookChecksFailed = 0;

// Called HookCheck, not Check: Snyk Code joins functions with the same name and signature across files, and many tests have a Check
static void HookCheck(bool good, const std::string &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	fflush(stdout);
	hookChecksFailed += good ? 0 : 1;
}

static ENetAddress Loopback()
{
	ENetAddress here;
	enet_address_set_host_ip(&here, "127.0.0.1");
	here.port = 0;   // any port the system has free
	return here;
}

// What the system really gave a socket, read back: this machine's loopback address, compared as the number it is
static bool BoundToLoopback(ENetSocket socket)
{
	ENetAddress bound;
	return enet_socket_get_address(socket, &bound) == 0 && bound.host == ENET_HOST_TO_NET_32(INADDR_LOOPBACK);
}

// What the hook saw
struct Seen
{
	int calls = 0;
	int cookies = 0;            // datagrams carrying patch 006's COOKIE answer
	bool everyAddressGood = true;
	ENetAddress expected = {};
	int mode = 0;               // 0: pass on to the socket; 1: drop, as if sent; 2: report an error for a connection's datagrams
};

static Seen seen;

static size_t TotalBytes(const ENetBuffer *buffers, size_t count)
{
	size_t total = 0;

	for (size_t i = 0; i < count; i++)
	{
		total += buffers[i].dataLength;
	}

	return total;
}

// A datagram outside a connection: the connection number "none", the checksum, then the command. A COOKIE is patch 006's token answer.
// Modified 2026-11-09 by Melanikus for the AvP port: the connection number must be exactly "none" (ENET_PROTOCOL_MAXIMUM_PEER_ID, with no
// flags, as patch 006 writes it) before byte 6 is read as the command. A datagram of a connection that carries its sent time has a 4-byte
// header, so its command is at byte 8, and its byte 6 is part of the checksum: one in 16 of those looked like a COOKIE, and the count of
// what the hook saw then failed by chance (GitHub, split-120's first run: "2 token answer(s)" with only 2 datagrams in all).
static bool IsACookie(const ENetBuffer *buffers, size_t count)
{
	std::vector<uint8_t> all;

	for (size_t i = 0; i < count; i++)
	{
		const uint8_t *const bytes = static_cast<const uint8_t *>(buffers[i].data);
		all.insert(all.end(), bytes, bytes + buffers[i].dataLength);
	}

	size_t const commandAt = sizeof(enet_uint16) + sizeof(enet_uint32);

	if (all.size() <= commandAt)
	{
		return false;
	}

	enet_uint16 connection = 0;
	memcpy(&connection, all.data(), sizeof(connection));
	return ENET_NET_TO_HOST_16(connection) == ENET_PROTOCOL_MAXIMUM_PEER_ID
		&& (all[commandAt] & ENET_PROTOCOL_COMMAND_MASK) == ENET_PROTOCOL_COMMAND_COOKIE;
}

static int ENET_CALLBACK TheHook(ENetHost *host, const ENetAddress *address, const ENetBuffer *buffers, size_t bufferCount)
{
	seen.calls++;
	seen.cookies += IsACookie(buffers, bufferCount) ? 1 : 0;
	seen.everyAddressGood = seen.everyAddressGood && address != NULL && address->host == seen.expected.host && address->port == seen.expected.port;

	if (seen.mode == 1)
	{
		return static_cast<int>(TotalBytes(buffers, bufferCount));   // dropped, as if it had gone
	}

	// The token answer goes on, so the client comes back and the host has a connection to answer; every datagram of a connection
	// then fails. (The token answer itself is sent outside any connection, where ENet has no one to report an error to.)
	if (seen.mode == 2 && !IsACookie(buffers, bufferCount))
	{
		return -1;
	}

	return enet_socket_send(host->socket, address, buffers, bufferCount);
}

struct HookPair
{
	ENetHost *host = NULL;     // made the game's way: it checksums, and asks a CONNECT for a token (patch 006)
	ENetHost *client = NULL;
	ENetPeer *clientsSide = NULL;
	ENetAddress hostAddress = {};
	ENetAddress clientAddress = {};
	bool hostFailed = false;   // enet_host_service gave -1 on the host
};

static bool MakeHosts(HookPair *pair)
{
	ENetAddress here = Loopback();
	pair->host = Net_MakeENetHost(&here, 7, 4000, 0, 0);
	pair->client = enet_host_create(&here, 1, 0, 0, 0);

	if (!pair->host || !pair->client || !BoundToLoopback(pair->host->socket) || !BoundToLoopback(pair->client->socket)
		|| enet_socket_get_address(pair->host->socket, &pair->hostAddress) != 0 || enet_socket_get_address(pair->client->socket, &pair->clientAddress) != 0)
	{
		return false;
	}

	pair->client->checksum = enet_crc32;   // the host throws away any packet without the right checksum
	enet_address_set_host_ip(&pair->hostAddress, "127.0.0.1");
	enet_address_set_host_ip(&pair->clientAddress, "127.0.0.1");
	return true;
}

static void EndHosts(HookPair *pair)
{
	if (pair->client)
	{
		enet_host_destroy(pair->client);
	}

	if (pair->host)
	{
		enet_host_destroy(pair->host);
	}

	*pair = HookPair();
}

// The client connects, and both sides run for up to milliseconds. Says whether the connection was made.
static bool Connect(HookPair *pair, DWORD milliseconds)
{
	pair->clientsSide = enet_host_connect(pair->client, &pair->hostAddress, kNetChannels, 0);

	if (!pair->clientsSide)
	{
		return false;
	}

	bool hostConnected = false;
	DWORD const startedAt = GetTickCount();

	while (GetTickCount() - startedAt < milliseconds)
	{
		ENetEvent event;
		int const got = enet_host_service(pair->host, &event, 1);
		pair->hostFailed = pair->hostFailed || got < 0;
		hostConnected = hostConnected || (got > 0 && event.type == ENET_EVENT_TYPE_CONNECT);
		enet_host_service(pair->client, &event, 1);

		if (hostConnected && pair->clientsSide->state == ENET_PEER_STATE_CONNECTED)
		{
			return true;
		}
	}

	return false;
}

static void TestNoHook()
{
	printf("\n1. No hook\n");
	HookPair pair;
	bool const made = MakeHosts(&pair);
	HookCheck(made && pair.host->sendHook == NULL, "a host is made with no send hook, on 127.0.0.1 only");
	HookCheck(made && Connect(&pair, 3000), "and a client connects to it as it always has");
	EndHosts(&pair);
}

static void TestPassingHook()
{
	printf("\n2. A hook that passes every datagram on\n");
	HookPair pair;
	bool const made = MakeHosts(&pair);
	seen = Seen();
	seen.expected = pair.clientAddress;
	seen.mode = 0;

	if (made)
	{
		pair.host->sendHook = TheHook;
	}

	bool const connected = made && Connect(&pair, 3000);
	HookCheck(connected && !pair.hostFailed, "a client connects to a host whose every datagram goes through the hook");
	HookCheck(made && seen.calls > 0 && static_cast<enet_uint32>(seen.calls) == pair.host->totalSentPackets + static_cast<enet_uint32>(seen.cookies),
		"the hook saw " + std::to_string(seen.calls) + " datagrams: all " + std::to_string(made ? pair.host->totalSentPackets : 0)
		+ " ENet counted, and " + std::to_string(seen.cookies) + " token answer(s) sent outside a connection");
	HookCheck(seen.cookies >= 1, "patch 006's token answer went through the hook too: it's sent outside any connection, from another place in ENet");
	HookCheck(seen.everyAddressGood, "and every one was for the client's own address and port");
	EndHosts(&pair);
}

static void TestDroppingHook()
{
	printf("\n3. A hook that drops every datagram, as the game's does before it has keys\n");
	HookPair pair;
	bool const made = MakeHosts(&pair);
	seen = Seen();
	seen.expected = pair.clientAddress;
	seen.mode = 1;

	if (made)
	{
		pair.host->sendHook = TheHook;
	}

	enet_uint32 const before = made ? pair.client->totalReceivedPackets : 0;
	bool const connected = made && Connect(&pair, 1500);
	HookCheck(made && !connected, "no connection is made in 1.5 s");
	HookCheck(seen.calls >= 1 && seen.cookies >= 1, "the host did try to answer: the hook was given " + std::to_string(seen.calls) + " datagram(s)");
	HookCheck(made && pair.client->totalReceivedPackets == before, "and not one of them reached the client: it received nothing at all");
	HookCheck(!pair.hostFailed, "and the host's ENet went on with no error, as if they were lost on the way");

	// 5. The hook taken away: the same host now connects
	printf("\n5. The hook taken away again\n");

	if (made)
	{
		pair.host->sendHook = NULL;
		enet_host_destroy(pair.client);
		ENetAddress here = Loopback();
		pair.client = enet_host_create(&here, 1, 0, 0, 0);
		pair.clientsSide = NULL;
	}

	bool const freshClient = made && pair.client != NULL && BoundToLoopback(pair.client->socket);

	if (freshClient)
	{
		pair.client->checksum = enet_crc32;
	}

	HookCheck(freshClient && Connect(&pair, 3000), "with no hook, the same host connects a new client, straight through its socket again");
	EndHosts(&pair);
}

static void TestFailingHook()
{
	printf("\n4. A hook that reports an error\n");
	HookPair pair;
	bool const made = MakeHosts(&pair);
	seen = Seen();
	seen.expected = pair.clientAddress;
	seen.mode = 2;

	if (made)
	{
		pair.host->sendHook = TheHook;
	}

	bool const connected = made && Connect(&pair, 1500);

	// Modified 2026-11-09 by Melanikus for the AvP port: one check for each thing, with its numbers, so a failure says which (GitHub's run on
	// a410ef23 failed the four together, and this test only runs there). The third is the one that matters most: nothing the hook refused
	// may reach the client.
	char said[220];
	snprintf(said, sizeof(said), "a hook that gives -1 for a connection's datagrams: no connection is made (made %d, connected %d)", made ? 1 : 0, connected ? 1 : 0);
	HookCheck(made && !connected, said);
	snprintf(said, sizeof(said), "the hook was given the token answer and at least one of the connection's datagrams (%d in all, %d token answer(s))", seen.calls, seen.cookies);
	HookCheck(seen.calls >= 2 && seen.cookies >= 1, said);
	snprintf(said, sizeof(said), "nothing but the token answers reached the client (it received %u, and %d token answer(s) were passed on)",
		made ? static_cast<unsigned>(pair.client->totalReceivedPackets) : 0u, seen.cookies);
	HookCheck(made && pair.client->totalReceivedPackets == static_cast<enet_uint32>(seen.cookies), said);
	HookCheck(pair.hostFailed, "and ENet reports the -1 from enet_host_service, as it would the socket's own error");
	EndHosts(&pair);
}

// Added 2026-11-09 by Melanikus for the AvP port: the test's own reader of token answers, on two datagrams made by hand, so its rule is
// tried every run and not only when a checksum happens to look like a COOKIE. A connection's datagram with its sent time, whose byte 6
// (in its checksum) reads as a COOKIE, isn't one; patch 006's own layout is.
static void TestTheCookieReader()
{
	printf("\n0. What the test counts as a token answer\n");
	uint8_t connected[12] = {};
	enet_uint16 const peer = ENET_HOST_TO_NET_16(static_cast<enet_uint16>(1 | ENET_PROTOCOL_HEADER_FLAG_SENT_TIME));
	memcpy(connected, &peer, sizeof(peer));
	connected[6] = ENET_PROTOCOL_COMMAND_COOKIE;   // part of the checksum, by chance
	connected[8] = ENET_PROTOCOL_COMMAND_VERIFY_CONNECT | ENET_PROTOCOL_COMMAND_FLAG_ACKNOWLEDGE;
	uint8_t cookie[12] = {};
	enet_uint16 const none = ENET_HOST_TO_NET_16(static_cast<enet_uint16>(ENET_PROTOCOL_MAXIMUM_PEER_ID));
	memcpy(cookie, &none, sizeof(none));
	cookie[6] = ENET_PROTOCOL_COMMAND_COOKIE;
	ENetBuffer one;
	one.data = connected;
	one.dataLength = sizeof(connected);
	ENetBuffer two;
	two.data = cookie;
	two.dataLength = sizeof(cookie);
	HookCheck(!IsACookie(&one, 1), "a connection's datagram whose checksum happens to read as a COOKIE at byte 6 isn't counted as a token answer");
	HookCheck(IsACookie(&two, 1), "and patch 006's token answer is: the connection number \"none\", the checksum, then COOKIE");
}

int main()
{
	if (enet_initialize() != 0)
	{
		printf("FAILED  ENet could not start\n");
		return 1;
	}

	printf("ENet patch 007: every datagram a host sends goes through its send hook\n");
	TestTheCookieReader();
	TestNoHook();
	TestPassingHook();
	TestDroppingHook();
	TestFailingHook();
	enet_deinitialize();
	printf("\n%s: %d check(s) failed\n", hookChecksFailed == 0 ? "PASSED" : "FAILED", hookChecksFailed);
	return hookChecksFailed == 0 ? 0 : 1;
}
