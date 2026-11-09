// relay-probe: tries the game's side of the relay against a real relay, with no game and no player. Written 2026-11-09 by Melanikus for
// the AvP port.
//
// WHY IT'S HERE. The game's relay code (3dc/net_relay.h) has its own tests, and the relay has its own, but each side tests against what it
// thinks the other does. This program puts the two together: it runs the game's own code against a real relay, end to end. The Tests
// workflow starts the relay (AVPx Dedicated Server, the Java server, at a pinned commit) on its own machine, on 127.0.0.1, and runs this
// against it on every change. Later, players and testers can run it against a live relay, to see whether a game gets through from their
// own network before anyone asks a friend to join.
//
// WHAT IT DOES, as one line each in the output:
//   1. Times the relay: three ECHOes, as a host does to find the nearest relay (NetRelayPicker).
//   2. Opens a room on it, as a host does (NetRelayHost): HOST_OPEN, the cookie round, HOST_OPENED with a pass for the list.
//   3. Joins that room, as a joiner does (NetRelayJoiner), from a second socket of its own: JOIN, the cookie round, JOIN_PORT, the
//      hellos, READY.
//   4. Sends made-up game packets both ways through the relay, and checks that each arrives, byte for byte.
//   5. Waits for the room's first keep: HOST_KEEP, and HOST_KEPT with a fresh pass.
//   6. Closes the room (HOST_CLOSE), and checks the relay then passes nothing more for it.
// Then it says how many of the relay's packets the game's reader refused, which must be none. It stops at the first step that fails.
//
// The work is all done by the game's own code: the same state machines and the same reader (NetRelay_Read) as the game. Only the two
// sockets around them are this program's. It has no part of the game in it, and nothing of the game's network code (ENet).
//
// WHAT IT NEVER DOES:
//   - It sends only to the relay it's given: its control port, and the one joiner port the relay hands out. A packet from anywhere but
//     the relay's address is dropped, as the game drops it, and counted.
//   - It never writes an address, a code, a token, a ticket or a pass (the user's ruling: no IPs). Only fixed words, round trips and
//     counts.
//   - Every wait has a limit. The whole run takes about 20 seconds, and never more than a minute.
//
// USE:  relay_probe [-dev] <the relay's name> [<its control port>]
//   The name is a DNS name, such as relay.example.com, never an address. The control port is 27500 unless given.
//   -dev allows a relay on this machine or this network, as the game's own -dev does (RelayList_AddressRefusal). The Tests workflow
//   uses it, for its relay on 127.0.0.1.
//
// EXIT CODE: 0 when every step passed; 1 when a step failed (the output says which, and why); 2 for arguments it can't use; 3 when it
// couldn't open its sockets or look the name up.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <chrono>
#include <random>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "net_relay.h"
#include "relay_list_text.h"

// ---- The sockets: Windows' and everyone else's are the same calls with different names

#ifdef _WIN32
typedef SOCKET ProbeSocket;
static const ProbeSocket kNoSocket = INVALID_SOCKET;

static void CloseSocket(ProbeSocket s)
{
	closesocket(s);
}
#else
typedef int ProbeSocket;
static const ProbeSocket kNoSocket = -1;

static void CloseSocket(ProbeSocket s)
{
	close(s);
}
#endif

int const kPassed = 0;
int const kStepFailed = 1;
int const kBadArguments = 2;
int const kCantStart = 3;

size_t const kMostPacketBytes = 1500;   // the most it reads of one packet: more than any it sends, or any of the relay's
size_t const kGamePackets = 10;         // made-up game packets sent each way in step 4
uint32_t const kWaitMs = 10;            // how long it waits for a packet each time round, so the state machines are told the time often
uint32_t const kMostRunMs = 60000;      // the whole run never takes longer than this, whatever the steps' own limits

// The time in milliseconds, from a clock that only goes forward. It comes round past 2^32 now and then, which the state machines allow for.
static uint32_t NowMs()
{
	return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

static void Say(const char *line)
{
	printf("relay-probe: %s\n", line);
	fflush(stdout);
}

// One step's result, and the program's end if it failed
static int Failed(const char *step, const char *why)
{
	printf("relay-probe: %s: FAILED: %s\n", step, why);
	fflush(stdout);
	return kStepFailed;
}

// A made-up game packet: never starting with AVPR, so nobody could take it for one of the relay's messages, and a different size each
// time, from 9 bytes up to 1,143 (the relay passes up to 1,500), never the 64 of a relay message, with random bytes after its first five
static std::vector<uint8_t> GamePacket(uint8_t direction, size_t number, std::mt19937 &random)
{
	std::vector<uint8_t> packet(9 + number * 126);
	packet[0] = 'P';
	packet[1] = 'R';
	packet[2] = 'O';
	packet[3] = direction;
	packet[4] = static_cast<uint8_t>(number);

	for (size_t i = 5; i < packet.size(); i++)
	{
		packet[i] = static_cast<uint8_t>(random() & 0xFF);
	}

	return packet;
}

// ---- The probe: a host's socket and a joiner's, the game's three state machines, and what came through

class Probe
{
public:
	Probe()
	{
	}

	~Probe()
	{
		if (hostSocket != kNoSocket)
		{
			CloseSocket(hostSocket);
		}

		if (joinerSocket != kNoSocket)
		{
			CloseSocket(joinerSocket);
		}
	}

	// Two sockets, each on a port of the system's choosing, as the game's own ENet socket is. False if either can't be had.
	bool Open(uint32_t address, uint16_t port)
	{
		relayAddress = address;
		controlPort = port;
		hostSocket = OpenOne();
		joinerSocket = OpenOne();
		return hostSocket != kNoSocket && joinerSocket != kNoSocket;
	}

	// One time round: tells each state machine the time, sends what each wants sent, then waits up to kWaitMs for one packet and hands it
	// to whichever end it's for
	void Turn()
	{
		uint32_t const now = NowMs();

		if (picking)
		{
			picker.Tick(now);
			SendAll(hostSocket, picker);
		}

		host.Tick(now);
		SendAll(hostSocket, host);
		joiner.Tick(now);
		SendAll(joinerSocket, joiner);

		uint8_t bytes[kMostPacketBytes];
		size_t size = 0;
		uint16_t fromPort = 0;
		ProbeSocket const to = Receive(bytes, sizeof(bytes), &size, &fromPort);

		if (to == hostSocket)
		{
			HostHears(bytes, size, fromPort);
		}
		else if (to == joinerSocket)
		{
			JoinerHears(bytes, size, fromPort);
		}
	}

	// Sends one made-up game packet from one end to the other, through the joiner's port on the relay
	bool SendGamePacket(bool fromTheHost, const std::vector<uint8_t> &packet)
	{
		return SendTo(fromTheHost ? hostSocket : joinerSocket, joiner.Port(), packet.data(), packet.size());
	}

	// How many of the relay's packets the game's reader refused, at all three ends together
	uint32_t Refused() const
	{
		uint32_t refused = 0;

		for (int why = 1; why < kNetRelayNotKinds; why++)
		{
			refused += picker.NotMessages(why) + host.NotMessages(why) + joiner.NotMessages(why);
		}

		return refused;
	}

	uint32_t SendsDropped() const
	{
		return picker.SendsDropped() + host.SendsDropped() + joiner.SendsDropped();
	}

	NetRelayPicker picker;
	NetRelayHost host;
	NetRelayJoiner joiner;
	bool picking = false;                                // step 1: the host's socket's packets from the control port go to the picker
	std::vector<std::vector<uint8_t> > atTheHost;        // game packets that reached the host, in the order they came
	std::vector<std::vector<uint8_t> > atTheJoiner;      // and the joiner
	uint32_t strangers = 0;                              // packets from anywhere but the relay's address, dropped

private:
	ProbeSocket hostSocket = kNoSocket;
	ProbeSocket joinerSocket = kNoSocket;
	uint32_t relayAddress = 0;   // as sin_addr keeps it (network byte order)
	uint16_t controlPort = 0;

	static ProbeSocket OpenOne()
	{
		ProbeSocket const s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

		if (s == kNoSocket)
		{
			return kNoSocket;
		}

		sockaddr_in here;
		memset(&here, 0, sizeof(here));
		here.sin_family = AF_INET;
		here.sin_addr.s_addr = htonl(INADDR_ANY);
		here.sin_port = 0;

		if (bind(s, reinterpret_cast<const sockaddr *>(&here), sizeof(here)) != 0)
		{
			CloseSocket(s);
			return kNoSocket;
		}

		return s;
	}

	// Sends bytes to one of the relay's ports. Only ever to the relay's own address.
	bool SendTo(ProbeSocket s, uint16_t port, const uint8_t *bytes, size_t size) const
	{
		if (port == 0 || size == 0 || size > kMostPacketBytes)
		{
			return false;
		}

		sockaddr_in to;
		memset(&to, 0, sizeof(to));
		to.sin_family = AF_INET;
		to.sin_addr.s_addr = relayAddress;
		to.sin_port = htons(port);
		long long const sent = sendto(s, reinterpret_cast<const char *>(bytes), static_cast<int>(size), 0, reinterpret_cast<const sockaddr *>(&to), sizeof(to));
		return sent == static_cast<long long>(size);
	}

	template <typename End>
	void SendAll(ProbeSocket s, End &end)
	{
		NetRelaySend sends[kNetRelayMostSends];
		size_t const n = end.TakeSends(sends, kNetRelayMostSends);

		for (size_t i = 0; i < n; i++)
		{
			SendTo(s, sends[i].port, sends[i].bytes, kNetRelayMessageBytes);
		}
	}

	// Waits up to kWaitMs for a packet on either socket. Gives back the socket it came to, with its bytes and the relay port it came from,
	// or kNoSocket if none came, or it came from anywhere but the relay's address (dropped, and counted).
	ProbeSocket Receive(uint8_t *bytes, size_t room, size_t *size, uint16_t *fromPort)
	{
		fd_set ready;
		FD_ZERO(&ready);
		FD_SET(hostSocket, &ready);
		FD_SET(joinerSocket, &ready);
		timeval wait;
		wait.tv_sec = 0;
		wait.tv_usec = static_cast<long>(kWaitMs * 1000);

		// The first number is the highest socket's plus one, which select needs everywhere but on Windows
		int const highest = static_cast<int>(hostSocket > joinerSocket ? hostSocket : joinerSocket) + 1;

		if (select(highest, &ready, NULL, NULL, &wait) <= 0)
		{
			return kNoSocket;
		}

		ProbeSocket const s = FD_ISSET(hostSocket, &ready) ? hostSocket : joinerSocket;
		sockaddr_in from;
		memset(&from, 0, sizeof(from));
		socklen_t fromSize = sizeof(from);
		long long const got = recvfrom(s, reinterpret_cast<char *>(bytes), static_cast<int>(room), 0, reinterpret_cast<sockaddr *>(&from), &fromSize);

		if (got <= 0 || from.sin_family != AF_INET)
		{
			return kNoSocket;
		}

		if (from.sin_addr.s_addr != relayAddress)
		{
			strangers++;
			return kNoSocket;
		}

		*size = static_cast<size_t>(got);
		*fromPort = ntohs(from.sin_port);
		return s;
	}

	// A packet at the host's socket, from the relay: from its control port it's the relay's, for the picker or the host; from a joiner's
	// port it's a game packet the joiner sent, and that joiner's game has connected, so the host's hellos to it stop
	void HostHears(const uint8_t *bytes, size_t size, uint16_t fromPort)
	{
		uint32_t const now = NowMs();

		if (fromPort == controlPort)
		{
			if (picking)
			{
				picker.Hear(0, fromPort, bytes, size, now);
			}
			else
			{
				host.Hear(fromPort, bytes, size, now);
			}

			return;
		}

		host.JoinerConnected(fromPort);
		atTheHost.push_back(std::vector<uint8_t>(bytes, bytes + size));
	}

	// A packet at the joiner's socket, from the relay: the joiner's state machine takes the relay's own messages; anything else from its
	// port, once it's in, is a game packet from the host
	void JoinerHears(const uint8_t *bytes, size_t size, uint16_t fromPort)
	{
		if (joiner.Hear(fromPort, bytes, size, NowMs()))
		{
			return;
		}

		if (joiner.Ready() && fromPort == joiner.Port())
		{
			atTheJoiner.push_back(std::vector<uint8_t>(bytes, bytes + size));
		}
	}
};

// Turns until done() says so, or ms have gone by, or the whole run's limit. True if done.
template <typename Done>
static bool TurnUntil(Probe &probe, uint32_t ms, uint32_t runStartedAt, Done done)
{
	uint32_t const started = NowMs();

	while (!done())
	{
		uint32_t const now = NowMs();

		if (now - started >= ms || now - runStartedAt >= kMostRunMs)
		{
			return false;
		}

		probe.Turn();
	}

	return true;
}

// How many of the packets sent arrived exactly, each once
static size_t ArrivedExactly(const std::vector<std::vector<uint8_t> > &sent, const std::vector<std::vector<uint8_t> > &arrived)
{
	std::vector<bool> used(arrived.size(), false);
	size_t found = 0;

	for (const std::vector<uint8_t> &packet : sent)
	{
		for (size_t i = 0; i < arrived.size(); i++)
		{
			if (!used[i] && arrived[i] == packet)
			{
				used[i] = true;
				found++;
				break;
			}
		}
	}

	return found;
}

// ---- The arguments, read as hostile (AVP Tao 3): only what's described at the top is taken

struct Arguments
{
	bool developer;
	const char *name;
	uint16_t controlPort;
};

static bool ReadArguments(int count, char **given, Arguments *out)
{
	out->developer = false;
	out->name = NULL;
	out->controlPort = kNetRelayDefaultControlPort;
	int at = 1;

	if (at < count && strcmp(given[at], "-dev") == 0)
	{
		out->developer = true;
		at++;
	}

	if (at >= count || count - at > 2)
	{
		return false;
	}

	// A DNS name, by the list's own rule for a relay's name (Relay_IsServerName), never an address
	if (strlen(given[at]) > static_cast<size_t>(kRelayListHostLetters) || !Relay_IsServerName(given[at]))
	{
		return false;
	}

	out->name = given[at++];

	if (at < count)
	{
		const char *const digits = given[at];
		size_t const letters = strlen(digits);
		unsigned long port = 0;

		if (letters < 1 || letters > 5)
		{
			return false;
		}

		for (size_t i = 0; i < letters; i++)
		{
			if (digits[i] < '0' || digits[i] > '9')
			{
				return false;
			}

			port = port * 10 + static_cast<unsigned long>(digits[i] - '0');
		}

		if (port < 1 || port > 65535)
		{
			return false;
		}

		out->controlPort = static_cast<uint16_t>(port);
	}

	return true;
}

// Looks the relay's name up, IPv4 only, as the game does. The first address it gives, in network byte order. False if it gives none.
static bool LookUp(const char *name, uint32_t *address)
{
	addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	addrinfo *found = NULL;

	if (getaddrinfo(name, NULL, &hints, &found) != 0 || !found)
	{
		return false;
	}

	bool got = false;

	for (const addrinfo *a = found; a && !got; a = a->ai_next)
	{
		if (a->ai_family == AF_INET && a->ai_addr && a->ai_addrlen >= sizeof(sockaddr_in))
		{
			*address = reinterpret_cast<const sockaddr_in *>(a->ai_addr)->sin_addr.s_addr;
			got = true;
		}
	}

	freeaddrinfo(found);
	return got;
}

// ---- The steps

static int RunTheSteps(Probe &probe, uint16_t controlPort)
{
	uint32_t const runStartedAt = NowMs();
	char line[240];
	std::random_device source;   // the operating system's own source of randomness

	// Step 1: three echoes from the host's socket, as a host times the relays before it opens a room
	uint8_t echoBytes[kNetRelayEchoes * kNetRelaySecretBytes];

	for (size_t i = 0; i < sizeof(echoBytes); i++)
	{
		echoBytes[i] = static_cast<uint8_t>(source() & 0xFF);
	}

	uint16_t const ports[1] = { controlPort };
	probe.picking = true;

	if (!probe.picker.Start(1, ports, echoBytes, NowMs()))
	{
		return Failed("step 1, echoes", "the echoes couldn't be started");
	}

	TurnUntil(probe, kNetRelayEchoes * kNetRelayEchoEveryMs + kNetRelayEchoAnswerWithinMs + 1000, runStartedAt,
		[&probe]() { return probe.picker.Done(NowMs()); });
	probe.picking = false;

	if (probe.picker.Best() != 0)
	{
		return Failed("step 1, echoes", "no echo was answered within a second");
	}

	snprintf(line, sizeof(line), "step 1, echoes: answered, the best round trip %u ms", static_cast<unsigned>(probe.picker.BestMs()));
	Say(line);

	// Step 2: a room, as a host opens one
	probe.host.Start(controlPort, NowMs());
	TurnUntil(probe, kNetRelayGiveUpAskingMs + 1000, runStartedAt, [&probe]() { return !probe.host.Asking(); });

	if (!probe.host.Open())
	{
		return Failed("step 2, a room", probe.host.LostWhy() != kNetRelayFine ? NetRelay_WhyText(probe.host.LostWhy()) : "the relay didn't answer in time");
	}

	char code[kNetRelayCodeLetters];
	uint8_t pass[kNetRelayPassBytes];
	uint64_t passTime = 0;

	if (!probe.host.RoomForTheList(code, pass, &passTime) || NetRelay_AllZero(pass, sizeof(pass)) || passTime == 0)
	{
		return Failed("step 2, a room", "it opened without a pass for the list");
	}

	uint32_t const passesAtOpen = probe.host.PassesHeard();
	Say("step 2, a room: open, with a pass for the list");

	// Step 3: joining that room from the second socket, as a joiner does with a game from the list
	if (!probe.joiner.Start(controlPort, code, NowMs()))
	{
		return Failed("step 3, joining it", "the room's code isn't 8 code letters");
	}

	TurnUntil(probe, kNetRelayGiveUpAskingMs + kNetRelayWaitForReadyMs + 1000, runStartedAt,
		[&probe]() { return probe.joiner.Ready() || probe.joiner.Failed(); });

	if (!probe.joiner.Ready())
	{
		return Failed("step 3, joining it", probe.joiner.Failed() ? NetRelay_WhyText(probe.joiner.FailedWhy()) : "the relay didn't answer in time");
	}

	Say("step 3, joining it: in, through one of the relay's joiner ports");

	// Step 4: made-up game packets, each way, through the joiner's port on the relay
	std::mt19937 random(source());
	std::vector<std::vector<uint8_t> > toTheHost, toTheJoiner;

	for (size_t i = 0; i < kGamePackets; i++)
	{
		toTheHost.push_back(GamePacket('J', i, random));
		toTheJoiner.push_back(GamePacket('H', i, random));
	}

	for (size_t i = 0; i < kGamePackets; i++)
	{
		if (!probe.SendGamePacket(false, toTheHost[i]) || !probe.SendGamePacket(true, toTheJoiner[i]))
		{
			return Failed("step 4, game packets", "one couldn't be sent");
		}

		probe.Turn();
	}

	TurnUntil(probe, 3000, runStartedAt,
		[&probe]() { return probe.atTheHost.size() >= kGamePackets && probe.atTheJoiner.size() >= kGamePackets; });
	size_t const reachedTheHost = ArrivedExactly(toTheHost, probe.atTheHost);
	size_t const reachedTheJoiner = ArrivedExactly(toTheJoiner, probe.atTheJoiner);
	snprintf(line, sizeof(line), "%u of %u reached the host unchanged, and %u of %u the joiner, with %u and %u in all", static_cast<unsigned>(reachedTheHost),
		static_cast<unsigned>(kGamePackets), static_cast<unsigned>(reachedTheJoiner), static_cast<unsigned>(kGamePackets),
		static_cast<unsigned>(probe.atTheHost.size()), static_cast<unsigned>(probe.atTheJoiner.size()));

	if (reachedTheHost != kGamePackets || reachedTheJoiner != kGamePackets || probe.atTheHost.size() != kGamePackets || probe.atTheJoiner.size() != kGamePackets)
	{
		return Failed("step 4, game packets", line);
	}

	if (probe.host.HellosWaiting() != 0)
	{
		return Failed("step 4, game packets", "they came, but the host is still saying hello to the joiner's port");
	}

	char said[300];
	snprintf(said, sizeof(said), "step 4, game packets: %s", line);
	Say(said);

	// Step 5: the room's first keep, which comes kNetRelayKeepEveryMs after it opened, and brings a fresh pass
	TurnUntil(probe, kNetRelayKeepEveryMs + 3000, runStartedAt, [&probe, passesAtOpen]() { return probe.host.PassesHeard() > passesAtOpen || !probe.host.Open(); });

	if (!probe.host.Open())
	{
		return Failed("step 5, keeping the room", NetRelay_WhyText(probe.host.LostWhy()));
	}

	if (probe.host.PassesHeard() <= passesAtOpen)
	{
		return Failed("step 5, keeping the room", "no HOST_KEPT came with a fresh pass");
	}

	Say("step 5, keeping the room: kept, with a fresh pass");

	// Step 6: HOST_CLOSE, then a packet from the joiner, which the relay mustn't pass on: the room is over
	probe.host.Close();
	probe.Turn();
	TurnUntil(probe, 500, runStartedAt, []() { return false; });
	size_t const atTheHostBefore = probe.atTheHost.size();
	probe.SendGamePacket(false, GamePacket('L', 1, random));
	TurnUntil(probe, 1000, runStartedAt, []() { return false; });

	if (probe.atTheHost.size() != atTheHostBefore)
	{
		return Failed("step 6, closing", "the relay still passed a packet for the room after HOST_CLOSE");
	}

	Say("step 6, closing: closed, and the relay passes nothing more for it");

	// Last: every relay packet went through the game's own reader. Any it refused means the game and the relay read bytes differently.
	snprintf(line, sizeof(line), "the game's reader refused %u of the relay's packets; %u packets came from anywhere else, and were dropped",
		static_cast<unsigned>(probe.Refused()), static_cast<unsigned>(probe.strangers));
	Say(line);

	if (probe.Refused() != 0)
	{
		return Failed("the relay's packets", "the game's reader refused some: the game and the relay read the same bytes differently");
	}

	if (probe.SendsDropped() != 0)
	{
		return Failed("the probe itself", "a state machine had more to send than it keeps between turns");
	}

	if (NowMs() - runStartedAt >= kMostRunMs)
	{
		return Failed("the probe itself", "the run went past its minute");
	}

	Say("every step passed");
	return kPassed;
}

int main(int count, char **given)
{
	Arguments arguments;

	if (!ReadArguments(count, given, &arguments))
	{
		Say("use: relay_probe [-dev] <the relay's name> [<its control port>]. The name is a DNS name, never an address.");
		return kBadArguments;
	}

#ifdef _WIN32
	WSADATA sockets;

	if (WSAStartup(MAKEWORD(2, 2), &sockets) != 0)
	{
		Say("couldn't start Windows' sockets");
		return kCantStart;
	}
#endif

	int result = kCantStart;
	uint32_t address = 0;

	if (!LookUp(arguments.name, &address))
	{
		Say("the relay's name gave no address");
	}
	else if (RelayList_AddressRefusal(ntohl(address), arguments.developer) != kRelayAddressUsable)
	{
		Say(RelayList_AddressWords(RelayList_AddressRefusal(ntohl(address), arguments.developer)));
	}
	else
	{
		Probe probe;

		if (!probe.Open(address, arguments.controlPort))
		{
			Say("couldn't open its two sockets");
		}
		else
		{
			result = RunTheSteps(probe, arguments.controlPort);
		}
	}

#ifdef _WIN32
	WSACleanup();
#endif

	return result;
}
