// Tests closing a network game's connections without losing what was said last (3dc/net_close.cpp). Written 2026-11-07 by Melanikus
// for the AvP port.
//
// WHAT IT TESTS
//
// This uses the real ENet, the game's own copy from vcpkg, and two ENet hosts on this machine's own loopback address (127.0.0.1),
// which never leaves the machine. "The leaver" sends one last message and leaves. "The other end" is slow to look, the way a game
// is when one of its frames took long, and then reads everything that came. Three ways of leaving are tried:
// - destroying the host at once, as the game did: the last message never goes at all
// - closing at once with enet_peer_disconnect_now: the message goes, but the goodbye arrives with it, and ENet throws the message
//   away when it reads the goodbye
// - Net_CloseConnectionsAfterWhatsWaiting: the message arrives, and the goodbye after it
// The first two are here to show the test can tell the difference. If ENet ever changed so that they kept the message too, those
// checks would fail and say so, and this file would need another look.
//
// Then: a connection whose other end never answers is only waited for as long as asked, and a host with nothing open, or no host,
// is done at once.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with 3dc/net_close.cpp and links vcpkg's ENet, using tests/net_close_test.args. tests.yml installs
// that ENet before building the tests. The program gives back a non-zero result if any check failed.

#include "net_close.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <thread>

static int checksFailed = 0;

// Called CloseCheck, not Check: Snyk Code joins functions with the same name and signature across files, and many tests have a Check
static void CloseCheck(bool good, const std::string &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	fflush(stdout);
	checksFailed += good ? 0 : 1;
}

static const char kLastWord[] = "the last word";

// The two ends of one connection
struct Pair
{
	ENetHost *leaver;
	ENetHost *other;
	ENetPeer *leaversConnection;
};

// What the other end saw, in order
struct Seen
{
	bool gotTheMessage;
	bool gotTheGoodbye;
	bool messageCameFirst;
};

// Whether an address is this machine's own loopback address, 127.0.0.1, which never leaves the machine. Says it as well.
static bool IsOnThisMachine(ENetAddress const &address, const char *what)
{
	char name[64] = "";

	enet_address_get_host_ip(&address, name, sizeof(name));
	printf("        %s: %s port %u\n", what, name, (unsigned)address.port);

	return strcmp(name, "127.0.0.1") == 0;
}

static ENetHost *MakeHostOnThisMachine()
{
	ENetAddress here;

	enet_address_set_host_ip(&here, "127.0.0.1");
	here.port = 0;   // any port the system has free

	ENetHost *const host = enet_host_create(&here, 1, 1, 0, 0);

	// What the system really gave the socket, read back, not what was asked for
	ENetAddress bound;

	if (host && enet_socket_get_address(host->socket, &bound) == 0)
	{
		CloseCheck(IsOnThisMachine(bound, "a host's socket is on"), "the socket is on 127.0.0.1 only");
	}
	else
	{
		CloseCheck(false, "the socket's own address can be read back");
	}

	return host;
}

// Two hosts, and a connection from the leaver to the other end, both sides seeing it open. False if that couldn't be done in 3 s.
static bool MakePair(Pair *pair)
{
	pair->leaver = MakeHostOnThisMachine();
	pair->other = MakeHostOnThisMachine();
	pair->leaversConnection = NULL;

	if (!pair->leaver || !pair->other)
	{
		return false;
	}

	ENetAddress there;

	if (enet_socket_get_address(pair->other->socket, &there) != 0)
	{
		return false;
	}

	enet_address_set_host_ip(&there, "127.0.0.1");
	pair->leaversConnection = enet_host_connect(pair->leaver, &there, 1, 0);

	if (!pair->leaversConnection)
	{
		return false;
	}

	CloseCheck(IsOnThisMachine(pair->leaversConnection->address, "the connection goes to"), "and the connection goes to 127.0.0.1 only");

	DWORD const startedAt = GetTickCount();
	bool otherHasIt = false;

	while (GetTickCount() - startedAt < 3000)
	{
		ENetEvent event;

		if (enet_host_service(pair->other, &event, 1) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
		{
			otherHasIt = true;
		}

		enet_host_service(pair->leaver, &event, 1);

		if (otherHasIt && pair->leaversConnection->state == ENET_PEER_STATE_CONNECTED)
		{
			return true;
		}
	}

	return false;
}

static void SendTheLastWord(Pair const &pair)
{
	ENetPacket *const packet = enet_packet_create(kLastWord, sizeof(kLastWord), ENET_PACKET_FLAG_RELIABLE);

	// ENet keeps a packet it takes, and frees it when it's been sent. One it won't take is still ours, so it's freed here.
	bool const taken = packet != NULL && enet_peer_send(pair.leaversConnection, 0, packet) == 0;

	if (packet != NULL && !taken)
	{
		enet_packet_destroy(packet);
	}

	CloseCheck(taken, "ENet takes the last word to send");
}

// The other end reads whatever has come, for up to forMilliseconds or until the goodbye
static Seen WatchTheOtherEnd(ENetHost *other, DWORD forMilliseconds)
{
	Seen seen = { false, false, false };
	DWORD const startedAt = GetTickCount();

	while (!seen.gotTheGoodbye && GetTickCount() - startedAt < forMilliseconds)
	{
		ENetEvent event;
		int const got = enet_host_service(other, &event, 5);

		if (got < 0)
		{
			break;
		}

		if (got > 0 && event.type == ENET_EVENT_TYPE_RECEIVE)
		{
			if (event.packet->dataLength == sizeof(kLastWord) && memcmp(event.packet->data, kLastWord, sizeof(kLastWord)) == 0)
			{
				seen.gotTheMessage = true;
				seen.messageCameFirst = !seen.gotTheGoodbye;
			}

			enet_packet_destroy(event.packet);
		}
		else if (got > 0 && event.type == ENET_EVENT_TYPE_DISCONNECT)
		{
			seen.gotTheGoodbye = true;
		}
	}

	return seen;
}

// How long the other end takes before it looks: much longer than it takes for everything to arrive on this machine, so that the
// last message and the goodbye are both waiting when it does
static const DWORD kSlowFrame = 300;

static void TestDestroyingAtOnce()
{
	Pair pair;

	CloseCheck(MakePair(&pair), "two ENet hosts on this machine connect");
	SendTheLastWord(pair);
	enet_host_destroy(pair.leaver);
	Sleep(kSlowFrame);

	Seen const seen = WatchTheOtherEnd(pair.other, 1000);

	CloseCheck(!seen.gotTheMessage, "destroying the host at once, as the game did, loses the last message");
	enet_host_destroy(pair.other);
}

static void TestClosingAtOnce()
{
	Pair pair;

	CloseCheck(MakePair(&pair), "two ENet hosts connect again");
	SendTheLastWord(pair);
	enet_host_flush(pair.leaver);
	enet_peer_disconnect_now(pair.leaversConnection, 0);
	enet_host_destroy(pair.leaver);
	Sleep(kSlowFrame);

	Seen const seen = WatchTheOtherEnd(pair.other, 1000);

	CloseCheck(seen.gotTheGoodbye && !seen.gotTheMessage, "closing at once sends the goodbye, and the last message arriving with it is thrown away");
	enet_host_destroy(pair.other);
}

static void TestClosingAfterWhatsWaiting()
{
	Pair pair;

	CloseCheck(MakePair(&pair), "two ENet hosts connect for the proper way");
	SendTheLastWord(pair);

	// The leaver waits for the other end, so it closes on its own thread while the other end is slow to look
	int stillClosing = -1;
	std::thread leaving([&pair, &stillClosing]()
	{
		stillClosing = Net_CloseConnectionsAfterWhatsWaiting(pair.leaver, 3000);
		enet_host_destroy(pair.leaver);
	});

	Sleep(kSlowFrame);

	Seen const seen = WatchTheOtherEnd(pair.other, 3000);

	leaving.join();
	CloseCheck(seen.gotTheMessage && seen.gotTheGoodbye && seen.messageCameFirst, "closing after what's waiting: the last message arrives, and then the goodbye");
	CloseCheck(stillClosing == 0, "and the leaver heard the other end answer, so nothing was still closing");
	enet_host_destroy(pair.other);
}

static void TestAnEndThatNeverAnswers()
{
	Pair pair;

	CloseCheck(MakePair(&pair), "two ENet hosts connect, and then one stops looking");
	SendTheLastWord(pair);

	DWORD const startedAt = GetTickCount();
	int const stillClosing = Net_CloseConnectionsAfterWhatsWaiting(pair.leaver, 300);
	DWORD const took = GetTickCount() - startedAt;

	CloseCheck(stillClosing == 1, "a connection whose other end never answers is still closing when the wait ends");
	CloseCheck(took >= 300 && took < 300 + 500, "and the wait was as long as asked, not longer: " + std::to_string(took) + " ms");
	enet_host_destroy(pair.leaver);
	enet_host_destroy(pair.other);
}

static void TestNothingToClose()
{
	ENetHost *const alone = MakeHostOnThisMachine();

	CloseCheck(alone != NULL, "an ENet host with no connections");

	DWORD const startedAt = GetTickCount();

	CloseCheck(Net_CloseConnectionsAfterWhatsWaiting(alone, 3000) == 0 && GetTickCount() - startedAt < 100, "has nothing to close, and doesn't wait");
	enet_host_destroy(alone);
	CloseCheck(Net_CloseConnectionsAfterWhatsWaiting(NULL, 3000) == 0, "and no host at all is nothing to close");
}

int main()
{
	if (enet_initialize() != 0)
	{
		printf("FAILED  ENet could not start\n");
		return 1;
	}

	TestDestroyingAtOnce();
	TestClosingAtOnce();
	TestClosingAfterWhatsWaiting();
	TestAnEndThatNeverAnswers();
	TestNothingToClose();

	enet_deinitialize();
	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "The closing test passed");

	return checksFailed ? 1 : 0;
}
