// Modified 2026-11-07 by Melanikus for the AvP port:
//   the co-op lobby: a game's description says if it has one, and in the lobby nothing is passed on and a silent joiner goes sooner. Credit: Melanikus and Bobblen (the co-op campaign system follows Bobblen's avp-forever, rewritten)
// Copyright (C) 2011 Barry Duncan. All Rights Reserved.
// The original author of this code can be contacted at: bduncan22@hotmail.com

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// DEVELOPERS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   deleted the switched-off (#if 0) code inside Net_ConnectToAddress; the "connect" console command still
//   does nothing;
//   a network packet too big for the game's message buffer is dropped (and logged, a limited number of
//   times), ENet is told the same size limit, and the list of games found on the LAN has a fixed size
// Modified 2026-11-06 by Melanikus for the AvP port:
//   a connection may stay silent for a named time, because nothing answers while a level loads
//   a connection that never says who it is is dropped, answers to anyone asking about a game are limited, a second game found in one look is read properly, and a joiner must speak this version
//   a system message with no server to send it to fails, and leaving a game forgets the server
//   a message with no network game running fails, where it used to hand ENet an empty pointer
//   nothing is opened on the network when the game starts: the socket for finding games is opened when a player asks for the list, or hosts, and closed when that's over
//   the host answers each client itself and knows who sent what from the connection it came on, so a client can only speak for itself; it passes on what clients say to each other
//   a client has no fixed port of its own, a join that fails lets go of what it opened, and leaving tells the other end straight away
//   a player ID comes from a count, because two people can join within the same thousandth of a second
//   a game message has to pass Net_GameMessageIsGood before anyone reads it or it's passed on, and a client takes the host's word only from whoever gave it its ID
//   LogThisDrop moved to net_checks.h, so that a test can run it, and so can the code that reads the game's messages
//   a connection may stay silent for a named time, and a level load answers all the way through
//   a packet has a checksum, a connection that never finishes starting or a player who goes silent loses their place, one address can hold 4 places, and a client can only send so many bytes each second
//   one address gets only so many answers to a search, and can list 2 games at most
// Modified 2026-11-07 by Melanikus for the AvP port:
//   every ENet host is made by Net_MakeENetHost (net_host.cpp) with the game's limits, and a connection gets the one channel the game uses
//   a message too short to have a header is refused before it's copied
//   the log says when our ENet has told an older version of the game no
//   leaving a network game sends what's still waiting and closes each connection with the other end told, instead of throwing it
//   all away (see net_close.h), and a client says in the log when the host's connection closes
//   a search's question is as big as its answer, and a host answers no other size, so an answer is never bigger than its question
//   the settings file's port and speeds are checked, and one the game can't use is put back to the game's own, with a log line
//   the screen says which connection a joining player is, never their address or port
//   Net_DescribeConnections says each connection's number and round trip, never its address, for a tester's mark (mark_key.cpp)
//   every 10 seconds the log says each connection's round trip, loss and resends, by number, and when one starts or stops struggling
//   (net_stats.h)
// Modified 2026-11-08 by Melanikus for the AvP port:
//   a host's log says when it starts answering searches, or that it can't, when the first search question comes, and every minute how
//   many came and were answered, never from which address (net_search_log.h)

#include "3dc.h"
#include "enet\enet.h"
#include "console.h"
#include "configFile.h"
#include "logString.h"
#include "menus.h"
#include "mp_config.h"
#include "networking.h"
#include "net_checks.h"
#include "coop_lobby.h"
#include "net_host.h"
#include "net_close.h"
#include "net_stats.h"
#include "net_search_log.h"
#include "net_relay.h"
#include "relay_shared.h"
#include "relay_list.h"
#include <assert.h>
#include <random>
#include "MemoryStream.h"

static ENetHost *host = NULL;

static ENetAddress  ServerAddress;
static ENetAddress  ClientAddress;
static ENetPeer     *ServerPeer = NULL;
static bool net_IsInitialised = false;

static ENetSocket broadcastSocket = ENET_SOCKET_NULL;


// How long a machine that's stopped answering may go before ENet drops it. Nothing answers ENet while a level loads, and one machine can take much longer than another. ENet's own number was about 5 seconds, which a joiner on a big level can't meet. A machine that never comes back is still dropped when this runs out.
static const enet_uint32 kConnectionPatienceMilliseconds = 15000;

extern void NewOnScreenMessage(char *messagePtr);

NetID AvPNetID = 0; // rename this
PlayerDetails thisClientPlayer;

// used to hold message data
const int kPacketBufferSize = kMostMessageBytes;
static uint8_t packetBuffer[kPacketBufferSize];

// A game has 8 players at most, and the host is one of them
static const size_t kMostClients = 7;

// As many letters as the address page takes, with room for its end
static const size_t kMostTypedAddressLetters = 16;

// When this search last asked. A broadcast can get lost on a real network, so it asks again each second.
static unsigned int sessionLastAskedAt = 0;
static bool AskEveryoneAboutGames();

// How many messages one client may send in a second. A game sends one a frame, and this is far above any frame rate that matters. Past it they're dropped, so a flood can't use the host to reach everyone else.
static const int kMostMessagesEachSecond = 1000;
static unsigned int messagesSecond[kMostClients + 1];
static int messagesUsed[kMostClients + 1];

// How many times a second this game answers people looking for games. Far more than a real LAN needs.
static const int kMostSessionAnswersEachSecond = 30;

// Anyone can send the question from an address that isn't theirs. This keeps one address from using up the answers meant for everyone.
static const int kMostSessionAnswersToOneAddressEachSecond = 2;
static NetAddressCount sessionAskers[32];

// A search's question is as big as the answer it asks for, so an answer is never bigger than its question. Someone sending questions from an
// address that isn't theirs could otherwise use our hosts to send more at that address than they sent themselves. The question is a header,
// then nothing, to the answer's size, and a host answers no other size.
static const size_t kSearchQuestionBytes = sizeof(MessageHeader) + sizeof(SessionDescription);

// How long a connection has to say who it is. ENet's own handshake has finished by then, so a real client only takes a moment. Without this, seven silent connections would fill the game for good.
static const unsigned int kMostMillisecondsToBeLetIn = 15000;

// When each of the host's connections was made
static unsigned int connectionStartedAt[kMostClients + 1];

// A real client answers ENet's first reply straight away. Someone starting connections from addresses that aren't theirs never does, and each one holds a place until this runs out. ENet's own wait is much longer.
static const unsigned int kMostMillisecondsHalfOpen = 3000;
static unsigned int halfOpenSince[kMostClients + 1];

// A player sends something many times a second, even waiting in a menu, and a level loads in much less than this. So a player silent for this long has gone, and is asked to go.
static const unsigned int kMostMillisecondsSilent = 300000;
// In the co-op lobby a joiner says something every 2 seconds, so this much silence means it has stopped. A seat in the lobby costs the others a place in the game.
static const unsigned int kMostMillisecondsSilentInTheLobby = 30000;

// A host's list of the addresses it has removed, and whether this machine has been removed by its host. See coop_lobby.h.
static CoopRemovedList removedPlayers;
static bool removedByTheHost = false;

// Further down, beside LetClientIn: the host's side of the request to join (version 114)
static void ForgetTheJoinRequest();
static ENetPeer *JoinRequestPeer();
static void LookAfterTheJoinRequest(unsigned int now);

// Modified 2026-11-09 by Melanikus for the AvP port: the joiner's side of the request to join (network version 114). Waiting from when its
// "let me in" goes, until the host gives it its player ID (yes), or closes the connection with a reason (no, busy, no answer). If neither
// comes, it counts as no answer a little after the host's own limit.
static int joinAnswer = kNetJoinAnswerNone;
static enet_uint32 joinAskedAt = 0;
static const enet_uint32 kJoinAnswerSpareMs = 5000;   // past the host's kNetJoinAnswerWithinMs, for the time the answer takes to come

int Net_JoinAnswerNow()
{
	if (joinAnswer == kNetJoinAnswerWaiting && enet_time_get() - joinAskedAt >= kNetJoinAnswerWithinMs + kJoinAnswerSpareMs)
	{
		joinAnswer = kNetJoinAnswerNoAnswer;
	}

	return joinAnswer;
}

static unsigned int lastHeardFrom[kMostClients + 1];

// How many bytes one client may send in a second, on top of how many messages. A real game sends a tenth of this. A host keeps what it's passing on until the other machines say they have it, for as long as the patience, so this also limits how much one client can make it keep.
static const int kMostBytesEachSecond = 512 * 1024;
static unsigned int bytesSecond[kMostClients + 1];
static int bytesUsed[kMostClients + 1];

static uint32_t netPortNumber = 23513;
static const uint32_t broadcastPort = 23514; // checkme

const int kBroadcastID = 255;
static uint32_t sessionCheckTimer = 0;
const uint32_t  sessionCheckTimeout = 5000; // 5 second timeout

static uint32_t incomingBandwidth = 0;
static uint32_t outgoingBandwidth = 0;

SessionDescription netSession; // main game session

static bool OpenDiscoverySocket();   // further down: a host the relays don't answer opens it instead (RelayHostGiveUp)
static void OpenTheSearchPortForAHost();   // further down, beside the search log it tells
static void LogDroppedMessage(const char *why);   // further down: a game on the relays' list that can't be joined is logged as dropped

// Modified 2026-11-08 by Melanikus for the AvP port: THE RELAY (net_relay.h, and AVPx Dedicated Server's PROTOCOL.md at 7014de1).
// There's a relay in each region of the world, and the list (relay_list.h) names them. A host times each with a few echoes, opens a room on
// the nearest, and its game goes on the list; every joiner comes through that relay, each from a port of its own there, so a player nobody can
// reach from the internet can still play. The relays' own little messages arrive on the game's own socket, so RelayIntercept takes them off it
// before ENet reads anything, and ENet only ever sees the game's packets. They're sent from that same socket too (RelaySendAll), so a relay
// sees each player's game through the same door in their router that the game itself uses.
// A host open through a relay listens to the relays alone: a packet from any other address is dropped (RelayIntercept). That's also why its
// limit on connections from one address can be the room's size: every joiner comes from the relay's address, and the relay keeps its own
// limits, 8 joiners to a room. So a host open through a relay isn't also open to its own network: friends there join through the list.
static ENetAddress relayAddress;                                   // the relay the room is on, at its control port
static char relayRegion[kRelayListRegionLetters + 1];              // and its region, as the list names it
static ENetAddress relayCandidates[kNetRelayMostRelays];           // the relays the list named, each at its control port, while timing them
static char relayCandidateRegions[kNetRelayMostRelays][kRelayListRegionLetters + 1];
static size_t relayCandidateCount = 0;
static bool relayHosting = false;              // this machine hosts a game that's going, or open, through a relay
static bool relayPicking = false;              // it's timing the relays, before it asks the nearest for a room
static NetRelayPicker relayPicker;
static NetRelayHost relayHost;
static NetRelayJoiner relayJoiner;
static int relayHostMs = 0;                    // the host's round trip to its relay, for the list
static bool relayRoomListed = false;           // the list has been told the room is open (RelayList_HostRoomOpened)
static uint32_t relayPassesListed = 0;         // the passes the list has been given (NetRelayHost::PassesHeard)
static bool relayLostSaid = false;             // the log has been told the room was lost
static uint32_t relayListedAt = 0;             // when the list was last given the game as it is now: once a second
static uint32_t relayStrangersDropped = 0;     // packets to a host going through a relay, from anywhere but the relays
static int relayJoinFailedWhy = kNetRelayFine; // why the last join through a relay failed
static NetRelayJoinWaitHook relayJoinWaitHook = NULL;   // asked each slice of the join's wait (Net_SetRelayJoinWaitHook)
static int relayJoinStep = kNetRelayJoinStepNone;       // where that join has got to (Net_RelayJoinStep)

// Modified 2026-11-09 by Melanikus for the AvP port: the relay on the screen (Net_RelayHostStateNow, Net_RelayPlayStateNow)
static NetRelayHostState relayHostNews = {};             // what the host is doing with its relay, as its screen is told it
static bool relayJoinedThrough = false;                  // this joiner's game is one it joined through a relay
static char relayJoinedRegion[kRelayListRegionLetters + 1];   // and that relay's region

uint32_t const kRelayListEveryMs = 1000;
size_t const kRelayMostPacketBytes = 1500;     // the most a relay passes on (PROTOCOL.md, section 2)
size_t const kRelayMostPacketsAWait = 16;      // the join's handshake reads at most this many packets each time it waits
uint32_t const kRelayWaitMs = 50;              // and waits this long each time for one
uint32_t const kRelayJoinMostMs = kNetRelayGiveUpAskingMs + kNetRelayWaitForReadyMs + 1000;   // the most a join's handshake can take
size_t const kRelayJoinMostTurns = kRelayJoinMostMs / kRelayWaitMs + 100;   // and the most times it waits, so it ends even if the clock stood still
int const kRelayMostHostMs = 9999;             // the most round trip the list takes (PROTOCOL.md, section 4)

// A pass time the relay's message can bring is one the list's writer can send on: both are the most a JSON number holds exactly
static_assert(kNetRelayMostPassTime == kRelayListMostPassTime, "net_relay.h reads pass times up to the most relay_list_text.h can list");

// A region the screen is given is the list's, so it fits in networking.h's room for one
static_assert(kNetRelayRegionLetters == static_cast<size_t>(kRelayListRegionLetters), "networking.h's region is as long as relay_list_text.h's");

// One relay from the list, as an address for ENet: its control port, and the address its name gave. The list looked the name up on its
// own thread and left out any address that can't be a relay's (RelayList_AddressRefusal), so nothing here waits for the network's name
// service. This checks the two that must never be sent to again, in case the list's rule ever changes: "any address" (0.0.0.0) and
// "everyone on this network" (255.255.255.255). False for those, or for a port that isn't one.
static bool RelayLookUp(const RelayListRelay &relay, ENetAddress *out)
{
	if (relay.port < 1 || relay.port > 65535 || relay.address == ENET_HOST_ANY || relay.address == ENET_HOST_BROADCAST)
	{
		return false;
	}

	out->host = relay.address;
	out->port = static_cast<uint16_t>(relay.port);
	return true;
}

// Fills bytes with random ones, for the echoes' 16 bytes each. std::random_device asks the operating system's own source of randomness.
static void RelayRandomBytes(uint8_t *bytes, size_t count)
{
	std::random_device source;

	for (size_t i = 0; i < count; i += 4)
	{
		unsigned int const word = source();

		for (size_t j = 0; j < 4 && i + j < count; j++)
		{
			bytes[i + j] = static_cast<uint8_t>(word >> (8 * j));
		}
	}
}

// Sends what one of the relay's state machines wants sent, from this ENet host's own socket. Each send names which of addresses it goes to:
// the host and the joiner talk to one relay only, and the picker to each it's timing.
template <typename End>
static void RelaySendAll(ENetHost *from, End &end, const ENetAddress *addresses, size_t addressCount)
{
	NetRelaySend sends[kNetRelayMostSends];
	size_t const count = end.TakeSends(sends, kNetRelayMostSends);

	for (size_t i = 0; i < count && from; i++)
	{
		if (sends[i].relay >= addressCount)
		{
			continue;
		}

		ENetAddress to = addresses[sends[i].relay];
		to.port = sends[i].port;
		ENetBuffer buffer;
		buffer.data = sends[i].bytes;
		buffer.dataLength = kNetRelayMessageBytes;
		enet_socket_send(from->socket, &to, &buffer, 1);
	}
}

// Whether a connection comes through the relay: from the relay's address, while this host is open through it
static bool PeerIsThroughRelay(const ENetPeer *peer)
{
	return relayHosting && !relayPicking && peer->address.host == relayAddress.host;
}

// ENet shows us every packet that arrives, before it reads it (ENetHost::intercept). Giving back 1 means "taken": ENet never sees it.
static int ENET_CALLBACK RelayIntercept(ENetHost *on, ENetEvent *event)
{
	(void)event;
	uint32_t const now = enet_time_get();

	if (relayHosting && relayPicking)
	{
		// While timing the relays, nothing is the game's yet: an answer from one of them is the picker's, and the rest is dropped
		bool fromACandidate = false;

		for (size_t i = 0; i < relayCandidateCount; i++)
		{
			if (on->receivedAddress.host == relayCandidates[i].host)
			{
				fromACandidate = true;
				relayPicker.Hear(i, on->receivedAddress.port, on->receivedData, on->receivedDataLength, now);
			}
		}

		relayStrangersDropped += fromACandidate ? 0 : 1;
		return 1;
	}

	bool const fromRelay = on->receivedAddress.host == relayAddress.host;

	if (relayHosting)
	{
		if (!fromRelay)
		{
			relayStrangersDropped++;
			return 1;
		}

		return relayHost.Hear(on->receivedAddress.port, on->receivedData, on->receivedDataLength, now) ? 1 : 0;
	}

	// A joiner through a relay: a READY sent again after its game has connected is the relay's, not the game's
	return fromRelay && relayJoiner.Hear(on->receivedAddress.port, on->receivedData, on->receivedDataLength, now) ? 1 : 0;
}

// Modified 2026-11-09 by Melanikus for the AvP port: the host's news about its relay has changed, for its screen to say once
// (Net_RelayHostStateNow). Only Listed has a region and a round trip, and only RoomGone a why: the game's own fixed words.
static void RelayHostNews(int what, const char *why)
{
	relayHostNews.changes++;
	relayHostNews.what = what;
	relayHostNews.region[0] = 0;
	relayHostNews.ms = 0;
	relayHostNews.why = what == kNetRelayHostRoomGone ? why : NULL;

	if (what == kNetRelayHostListed)
	{
		memcpy(relayHostNews.region, relayRegion, sizeof(relayHostNews.region));
		relayHostNews.ms = relayHostMs;
	}
}

// A host starts going through a relay, when the list has named some: it times each, and asks the nearest for a room (RelayHostFrame). False
// with none named, or none that looks up, and then the game is found on this network as it always was.
static bool RelayHostStart()
{
	relayHosting = false;
	relayPicking = false;
	relayRoomListed = false;
	relayPassesListed = 0;
	relayLostSaid = false;
	relayStrangersDropped = 0;
	relayCandidateCount = 0;
	relayHostMs = 0;
	relayRegion[0] = 0;

	RelayListRelay relays[kRelayListMostRelays];
	int named = 0;

	if (!host || !RelayList_Relays(relays, &named) || named <= 0)
	{
		return false;
	}

	for (int i = 0; i < named && i < static_cast<int>(kRelayListMostRelays) && relayCandidateCount < kNetRelayMostRelays; i++)
	{
		if (RelayLookUp(relays[i], &relayCandidates[relayCandidateCount]))
		{
			strncpy(relayCandidateRegions[relayCandidateCount], relays[i].region, kRelayListRegionLetters);
			relayCandidateRegions[relayCandidateCount][kRelayListRegionLetters] = 0;
			relayCandidateCount++;
		}
	}

	if (relayCandidateCount == 0)
	{
		Con_PrintError("net: no relay the list named could be looked up, so this game is open on this network only");
		RelayHostNews(kNetRelayHostNoRelay, NULL);
		return false;
	}

	uint16_t ports[kNetRelayMostRelays];
	static uint8_t random[kNetRelayMostRelays * kNetRelayEchoes * kNetRelaySecretBytes];

	for (size_t i = 0; i < relayCandidateCount; i++)
	{
		ports[i] = relayCandidates[i].port;
	}

	RelayRandomBytes(random, relayCandidateCount * kNetRelayEchoes * kNetRelaySecretBytes);

	if (!relayPicker.Start(relayCandidateCount, ports, random, enet_time_get()))
	{
		return false;
	}

	relayHosting = true;
	relayPicking = true;
	host->duplicatePeers = kMostClients;
	host->intercept = RelayIntercept;
	RelaySendAll(host, relayPicker, relayCandidates, relayCandidateCount);
	Con_PrintMessage("net: this game is finding the nearest relay");
	RelayHostNews(kNetRelayHostFinding, NULL);
	return true;
}

// No relay answered the echoes, or the room on the relay is gone: the game is open on this network instead, as it would be with no relays
// at all, so a player can still join it here (Fourth's cold read, finding 4). why is the game's own fixed words.
static void RelayHostGiveUp(const char *why)
{
	relayHosting = false;
	relayPicking = false;

	if (host)
	{
		host->intercept = NULL;
		host->duplicatePeers = kMostConnectionsFromOneAddress;
	}

	Con_PrintError(why);
	OpenTheSearchPortForAHost();
}

// Each frame while hosting through a relay: time the relays, then keep the room, say hello to joiners, and keep the list told
static void RelayHostFrame()
{
	if (!relayHosting || !host)
	{
		return;
	}

	uint32_t const now = enet_time_get();

	if (relayPicking)
	{
		relayPicker.Tick(now);
		RelaySendAll(host, relayPicker, relayCandidates, relayCandidateCount);

		if (!relayPicker.Done(now))
		{
			return;
		}

		int const best = relayPicker.Best();

		if (best < 0)
		{
			RelayHostGiveUp("net: no relay answered, so this game is open on this network only");
			RelayHostNews(kNetRelayHostNoRelay, NULL);
			return;
		}

		relayPicking = false;
		relayAddress = relayCandidates[best];
		strncpy(relayRegion, relayCandidateRegions[best], kRelayListRegionLetters);
		relayRegion[kRelayListRegionLetters] = 0;
		uint32_t const ms = relayPicker.BestMs();
		relayHostMs = ms < 1 ? 1 : (ms > static_cast<uint32_t>(kRelayMostHostMs) ? kRelayMostHostMs : static_cast<int>(ms));
		relayHost.Start(relayAddress.port, now);
		relayListedAt = now;
		// Fixed words: the region is checked, but it's the list's text, so it isn't written (Fourth's cold read, finding 3)
		Con_PrintMessage("net: this game is opening through the nearest relay, " + std::to_string(relayHostMs) + " ms away");
	}

	relayHost.Tick(now);
	RelaySendAll(host, relayHost, &relayAddress, 1);

	char code[kNetRelayCodeLetters];
	uint8_t pass[kNetRelayPassBytes];
	uint64_t passTime = 0;

	if (!relayRoomListed && relayHost.RoomForTheList(code, pass, &passTime))
	{
		relayRoomListed = true;
		relayPassesListed = relayHost.PassesHeard();
		relayListedAt = now;
		RelayList_HostRoomOpened(relayRegion, code, pass, passTime, relayHostMs, netSession);
		Con_PrintMessage("net: the relay has opened this game's room, and it's going on the list");
		RelayHostNews(kNetRelayHostListed, NULL);
	}
	else if (relayRoomListed && relayHost.PassesHeard() != relayPassesListed && relayHost.RoomForTheList(code, pass, &passTime))
	{
		relayPassesListed = relayHost.PassesHeard();
		RelayList_HostKept(pass, passTime, relayHostMs);
	}

	if (relayRoomListed && relayHost.Open() && now - relayListedAt >= kRelayListEveryMs)
	{
		relayListedAt = now;
		RelayList_HostSessionNow(netSession);
	}

	if (!relayHost.Open() && !relayHost.Asking() && !relayLostSaid)
	{
		relayLostSaid = true;
		Con_PrintError(std::string("net: this game's room on the relay is gone, so nobody more can join through it: ") + NetRelay_WhyText(relayHost.LostWhy()));

		if (relayRoomListed)
		{
			RelayList_HostRoomClosed();
			relayRoomListed = false;
		}

		RelayHostGiveUp("net: this game is open on this network instead");
		RelayHostNews(kNetRelayHostRoomGone, NetRelay_WhyText(relayHost.LostWhy()));
	}
}

// The host leaves: HOST_CLOSE, so the relay ends the room at once, and the list is told
static void RelayHostLeave()
{
	// The host's game is over, so whatever its screen was last told about the relay is over too
	if (relayHostNews.what != kNetRelayHostNone)
	{
		RelayHostNews(kNetRelayHostNone, NULL);
	}

	if (!relayHosting)
	{
		return;
	}

	if (!relayPicking)
	{
		relayHost.Close();
		RelaySendAll(host, relayHost, &relayAddress, 1);
	}

	if (relayRoomListed && !relayLostSaid)
	{
		RelayList_HostRoomClosed();
	}

	if (relayStrangersDropped > 0)
	{
		LogString("net: while going through a relay, " + std::to_string(relayStrangersDropped) + " packet(s) from elsewhere were dropped");
	}

	relayHosting = false;
	relayPicking = false;
	relayRoomListed = false;

	if (host)
	{
		host->intercept = NULL;
	}
}

// A JOIN THROUGH A RELAY: the relay's handshake for a game's code, on this new ENet host's own socket, before ENet connects. relayAddress is
// the game's relay. It takes at most the joiner's own limits (10 s for an answer, 15 s for READY), and never more than kRelayJoinMostMs in
// all. Gives back the relay's port to connect the game to, or 0, with relayJoinFailedWhy saying why. Everything heard from anywhere but the
// relay is dropped.
static uint16_t RelayHandshake(const char *code)
{
	uint32_t const started = enet_time_get();

	if (!host || !relayJoiner.Start(relayAddress.port, code, started))
	{
		relayJoinFailedWhy = kNetRelayRefusedNoGame;
		return 0;
	}

	static uint8_t packet[kRelayMostPacketBytes];

	for (size_t turn = 0; turn < kRelayJoinMostTurns && !relayJoiner.Ready() && !relayJoiner.Failed() && enet_time_get() - started < kRelayJoinMostMs; turn++)
	{
		// Where it's got to, for the menu's hook to say (Net_RelayJoinStep)
		relayJoinStep = relayJoiner.Introducing() ? kNetRelayJoinStepIntroducing : kNetRelayJoinStepReaching;

		// The window keeps answering, and the player can give up (the menu's hook)
		if (relayJoinWaitHook && !relayJoinWaitHook())
		{
			relayJoinStep = kNetRelayJoinStepNone;
			relayJoinFailedWhy = kNetRelayGivenUp;
			return 0;
		}

		RelaySendAll(host, relayJoiner, &relayAddress, 1);
		enet_uint32 condition = ENET_SOCKET_WAIT_RECEIVE;

		if (enet_socket_wait(host->socket, &condition, kRelayWaitMs) < 0)
		{
			break;
		}

		for (size_t i = 0; i < kRelayMostPacketsAWait && (condition & ENET_SOCKET_WAIT_RECEIVE); i++)
		{
			ENetAddress from;
			ENetBuffer buffer;
			buffer.data = packet;
			buffer.dataLength = sizeof(packet);
			int const got = enet_socket_receive(host->socket, &from, &buffer, 1);

			if (got <= 0)
			{
				break;
			}

			if (from.host == relayAddress.host)
			{
				relayJoiner.Hear(from.port, packet, static_cast<size_t>(got), enet_time_get());
			}
		}

		relayJoiner.Tick(enet_time_get());
	}

	relayJoinStep = kNetRelayJoinStepNone;

	if (!relayJoiner.Ready())
	{
		relayJoinFailedWhy = relayJoiner.Failed() ? relayJoiner.FailedWhy() : kNetRelayNoAnswer;
		return 0;
	}

	// A READY sent again later is the relay's, so the intercept takes it off ENet's hands
	host->intercept = RelayIntercept;
	relayJoinFailedWhy = kNetRelayFine;
	return relayJoiner.Port();
}

const char *Net_RelayJoinFailedWhy()
{
	return NetRelay_WhyText(relayJoinFailedWhy);
}

void Net_SetRelayJoinWaitHook(NetRelayJoinWaitHook hook)
{
	relayJoinWaitHook = hook;
}

// Modified 2026-11-09 by Melanikus for the AvP port: where a join through a relay has got to, for the menu's wait (Fourth's online screens)
int Net_RelayJoinStep()
{
	return relayJoinStep;
}

// Modified 2026-11-09 by Melanikus for the AvP port: the relay on the screen (networking.h). Both only read.
void Net_RelayHostStateNow(NetRelayHostState *state)
{
	if (state)
	{
		*state = relayHostNews;
	}
}

void Net_RelayPlayStateNow(NetRelayPlayState *state)
{
	if (!state)
	{
		return;
	}

	memset(state, 0, sizeof(*state));
	state->ms = -1;

	// A host whose room on the relay is open: its last keep's round trip, or its echoes' until the first keep has been answered
	if (relayHosting && !relayPicking && relayHost.Open())
	{
		int const keepMs = relayHost.KeepMs();
		state->throughRelay = true;
		state->isHost = true;
		memcpy(state->region, relayRegion, sizeof(state->region));
		state->ms = keepMs >= 0 ? keepMs : (relayHostMs > 0 ? relayHostMs : -1);
		return;
	}

	// A joiner connected through a relay: ENet's round trip to the host, once ENet has timed one (lastReceiveTime is set by the first)
	if (AvP.Network == I_Peer && relayJoinedThrough && ServerPeer && ServerPeer->state == ENET_PEER_STATE_CONNECTED)
	{
		state->throughRelay = true;
		memcpy(state->region, relayJoinedRegion, sizeof(state->region));

		if (ServerPeer->lastReceiveTime > 0)
		{
			state->ms = ServerPeer->roundTripTime > kNetRelayMostKeepMs ? static_cast<int>(kNetRelayMostKeepMs) : static_cast<int>(ServerPeer->roundTripTime);
		}
	}
}

// Modified 2026-11-09 by Melanikus for the AvP port: the joiner's round trip to each relay, for the games page's ping column (Fourth's online
// screens). PROTOCOL.md, "How the game uses it": 3 echoes to each relay the list names, once for each relay, not for each game, and the
// lowest kept. A game's estimate is this and its hostMs. The echoes go from a socket of their own, open only while they're timed, and only
// an answer from a timed relay's own address and control port is heard (NetRelayPicker checks its bytes and count).
static ENetSocket joinerTimingSocket = ENET_SOCKET_NULL;
static NetRelayPicker joinerPicker;
static ENetAddress joinerTimed[kNetRelayMostRelays];
static char joinerTimedRegions[kNetRelayMostRelays][kRelayListRegionLetters + 1];
static size_t joinerTimedCount = 0;
static bool joinerTimingStarted = false;   // since the last Net_ForgetRelayTimes
static bool joinerTimingDone = false;

static void CloseJoinerTimingSocket()
{
	if (joinerTimingSocket != ENET_SOCKET_NULL)
	{
		enet_socket_destroy(joinerTimingSocket);
		joinerTimingSocket = ENET_SOCKET_NULL;
	}
}

void Net_ForgetRelayTimes()
{
	CloseJoinerTimingSocket();
	joinerPicker.Reset();
	joinerTimedCount = 0;
	joinerTimingStarted = false;
	joinerTimingDone = false;
}

void Net_TimeRelaysForJoining()
{
	if (joinerTimingDone)
	{
		return;
	}

	uint32_t const now = enet_time_get();

	// The first time the list has relays: each one looked up as a host does (RelayLookUp), and the first echo to each sent
	if (!joinerTimingStarted)
	{
		RelayListRelay relays[kRelayListMostRelays];
		int named = 0;

		if (!RelayList_Relays(relays, &named) || named <= 0)
		{
			return;
		}

		joinerTimingStarted = true;
		joinerTimedCount = 0;

		for (int i = 0; i < named && i < static_cast<int>(kRelayListMostRelays) && joinerTimedCount < kNetRelayMostRelays; i++)
		{
			if (RelayLookUp(relays[i], &joinerTimed[joinerTimedCount]))
			{
				strncpy(joinerTimedRegions[joinerTimedCount], relays[i].region, kRelayListRegionLetters);
				joinerTimedRegions[joinerTimedCount][kRelayListRegionLetters] = 0;
				joinerTimedCount++;
			}
		}

		uint16_t ports[kNetRelayMostRelays];
		uint8_t random[kNetRelayMostRelays * kNetRelayEchoes * kNetRelaySecretBytes];

		for (size_t i = 0; i < joinerTimedCount; i++)
		{
			ports[i] = joinerTimed[i].port;
		}

		RelayRandomBytes(random, joinerTimedCount * kNetRelayEchoes * kNetRelaySecretBytes);
		joinerTimingSocket = joinerTimedCount > 0 ? enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM) : ENET_SOCKET_NULL;

		ENetAddress any;
		any.host = ENET_HOST_ANY;
		any.port = 0;

		if (joinerTimingSocket == ENET_SOCKET_NULL || enet_socket_set_option(joinerTimingSocket, ENET_SOCKOPT_NONBLOCK, 1) < 0
			|| enet_socket_bind(joinerTimingSocket, &any) < 0 || !joinerPicker.Start(joinerTimedCount, ports, random, now))
		{
			CloseJoinerTimingSocket();
			joinerTimingDone = true;
			return;
		}

		LogString("net: the relays are being timed for the games page");
	}

	// What has come: from a timed relay only, by its address and control port. Anything else is dropped.
	static uint8_t packet[kRelayMostPacketBytes];

	for (size_t n = 0; n < kRelayMostPacketsAWait; n++)
	{
		enet_uint32 condition = ENET_SOCKET_WAIT_RECEIVE;

		if (enet_socket_wait(joinerTimingSocket, &condition, 0) < 0 || !(condition & ENET_SOCKET_WAIT_RECEIVE))
		{
			break;
		}

		ENetAddress from;
		ENetBuffer buffer;
		buffer.data = packet;
		buffer.dataLength = sizeof(packet);
		int const got = enet_socket_receive(joinerTimingSocket, &from, &buffer, 1);

		if (got <= 0)
		{
			break;
		}

		for (size_t i = 0; i < joinerTimedCount; i++)
		{
			if (from.host == joinerTimed[i].host && from.port == joinerTimed[i].port)
			{
				joinerPicker.Hear(i, from.port, packet, static_cast<size_t>(got), now);
				break;
			}
		}
	}

	joinerPicker.Tick(now);

	NetRelaySend sends[kNetRelayMostSends];
	size_t const sendCount = joinerPicker.TakeSends(sends, kNetRelayMostSends);

	for (size_t i = 0; i < sendCount; i++)
	{
		if (sends[i].relay < joinerTimedCount)
		{
			ENetAddress to = joinerTimed[sends[i].relay];
			to.port = sends[i].port;
			ENetBuffer buffer;
			buffer.data = sends[i].bytes;
			buffer.dataLength = kNetRelayMessageBytes;
			enet_socket_send(joinerTimingSocket, &to, &buffer, 1);
		}
	}

	if (joinerPicker.Done(now))
	{
		CloseJoinerTimingSocket();
		joinerTimingDone = true;
		LogString("net: the relays are timed for the games page");
	}
}

int Net_JoinerMsToRelay(const char *region)
{
	if (!region || !joinerTimingDone)
	{
		return -1;
	}

	for (size_t i = 0; i < joinerTimedCount; i++)
	{
		uint32_t ms = 0;

		if (strncmp(region, joinerTimedRegions[i], sizeof(joinerTimedRegions[i])) == 0)
		{
			return joinerPicker.RelayMs(i, &ms) && ms <= static_cast<uint32_t>(kRelayMostHostMs) ? static_cast<int>(ms) : -1;
		}
	}

	return -1;
}

// A join through a relay: the relay of the game's region, from the relays the list names, looked up, then its handshake (RelayHandshake).
// Gives back the relay's port to connect the game to, or 0, with relayJoinFailedWhy saying why.
static uint16_t RelayJoin(const char *region, const char *code)
{
	relayJoinFailedWhy = kNetRelayNoRelay;
	RelayListRelay relays[kRelayListMostRelays];
	int named = 0;

	if (!RelayList_Relays(relays, &named))
	{
		return 0;
	}

	for (int i = 0; i < named && i < static_cast<int>(kRelayListMostRelays); i++)
	{
		if (strncmp(relays[i].region, region, sizeof(relays[i].region)) == 0)
		{
			return RelayLookUp(relays[i], &relayAddress) ? RelayHandshake(code) : 0;
		}
	}

	return 0;
}

// Modified 2026-11-08 by Melanikus for the AvP port: a game from the relays' list (relay_list.h, PROTOCOL.md 7014de1) goes on the same list
// of sessions a search of this network fills, and is joined the same way (Net_ConnectToSession). What the list says is the host's word, sent
// to the list and on to us, so each part is checked here as Net_AddSession checks a search's answer: the lobby state, a code, a region, the
// names' lengths, a slot left, and the level, which has to be one this machine has, picked by its number, its name and its game style, as a
// game found on this network is (GetLocalMultiplayerLevelIndex). Gives back whether it went on the list.
static bool AddRelaySession(const char *code, const char *region, const char *name, int players, int maxPlayers, int version, uint16_t gameStyle,
	uint16_t levelNumber, const char *levelName, uint8_t coopLobby)
{
	if (!code || !NetRelay_CodeIsGood(code) || !region || !name || !levelName || !CoopLobby_StateIsReal(coopLobby))
	{
		LogDroppedMessage("a game on the relays' list wasn't one this game can join");
		return false;
	}

	size_t const regionLetters = strnlen(region, kRelayListRegionLetters + 1);

	if (regionLetters == 0 || regionLetters > kRelayListRegionLetters)
	{
		LogDroppedMessage("a game on the relays' list named a region that isn't one");
		return false;
	}

	if (NumberOfSessionsFound >= MAX_NO_OF_SESSIONS)
	{
		return false;
	}

	// The same game twice is one game
	for (uint32_t i = 0; i < NumberOfSessionsFound; i++)
	{
		if (SessionData[i].throughRelay && memcmp(SessionData[i].relayCode, code, kNetRelayCodeLetters) == 0
			&& strncmp(SessionData[i].relayRegion, region, sizeof(SessionData[i].relayRegion)) == 0)
		{
			return false;
		}
	}

	SESSION_DESC &entry = SessionData[NumberOfSessionsFound];
	memset(&entry, 0, sizeof(entry));
	entry.throughRelay = true;

	// Its GUID, the same each time the page is set up again, so the page keeps its highlight on it (Fourth's cold read, finding 6): the
	// region's letters summed into the first part (FNV-1a), "RL" and 1 in the next two, so it can't be a GUID a LAN game makes, and the code's
	// 8 letters in the last
	uint32_t regionSum = 2166136261u;

	for (size_t i = 0; i < regionLetters; i++)
	{
		regionSum = (regionSum ^ static_cast<uint8_t>(region[i])) * 16777619u;
	}

	entry.Guid.Data1 = regionSum;
	entry.Guid.Data2 = 0x524C;
	entry.Guid.Data3 = 1;
	memcpy(entry.Guid.Data4, code, kNetRelayCodeLetters);
	memcpy(entry.relayCode, code, kNetRelayCodeLetters);
	memcpy(entry.relayRegion, region, regionLetters);
	entry.relayRegion[regionLetters] = 0;
	entry.coopLobby = (char)coopLobby;

	char safeName[kSessionNameSize];
	strncpy(safeName, name, sizeof(safeName) - 1);
	safeName[sizeof(safeName) - 1] = 0;
	int const shownPlayers = players < 0 ? 0 : (players > 8 ? 8 : players);
	int const shownMost = maxPlayers < 0 ? 0 : (maxPlayers > 8 ? 8 : maxPlayers);
	_snprintf(entry.Name, sizeof(entry.Name) - 1, "%s (%d/%d)", safeName, shownPlayers, shownMost);
	entry.Name[sizeof(entry.Name) - 1] = 0;
	entry.AllowedToJoin = shownPlayers < shownMost;

	// Modified 2026-11-09 by Melanikus for the AvP port: the games page's columns, as for a LAN game (Net_AddSession): the level's name cut to
	// fit and ended, the style, and the counts as the name shows them
	strncpy(entry.levelName, levelName, sizeof(entry.levelName) - 1);
	entry.levelName[sizeof(entry.levelName) - 1] = 0;
	entry.gameStyle = gameStyle;
	entry.players = static_cast<uint8_t>(shownPlayers);
	entry.maxPlayers = static_cast<uint8_t>(shownMost);

	if (version != kMultiplayerVersion)
	{
		float const shownVersion = 1.0f + version / 100.0f;
		entry.AllowedToJoin = false;
		_snprintf(entry.Name, sizeof(entry.Name) - 1, "%s (V %.2f)", safeName, shownVersion);
		entry.Name[sizeof(entry.Name) - 1] = 0;
	}
	else
	{
		char safeLevel[kLevelNameSize];
		strncpy(safeLevel, levelName, sizeof(safeLevel) - 1);
		safeLevel[sizeof(safeLevel) - 1] = 0;
		int const localIndex = GetLocalMultiplayerLevelIndex(levelNumber, safeLevel, gameStyle);

		if (localIndex < 0 && coopLobby == kCoopLobbyNone)
		{
			memset(&entry, 0, sizeof(entry));
			return false;
		}

		entry.levelIndex = localIndex < 0 ? 0 : localIndex;
	}

	NumberOfSessionsFound++;
	return true;
}

// The list's own record of a game (relay_list_text.h, Fourth's, checked there to PROTOCOL.md): each number is checked again for the room it's
// kept in here, before it's narrowed, and the rest goes to AddRelaySession
bool Net_AddRelaySession(const RelayListGame &game)
{
	if (game.gameStyle < 0 || game.gameStyle > 255 || game.levelNumber < 0 || game.levelNumber > 100 || game.coopLobby < 0 || game.coopLobby > 255)
	{
		LogDroppedMessage("a game on the relays' list had a number out of its range");
		return false;
	}

	return AddRelaySession(game.code, game.region, game.name, game.players, game.maxPlayers, game.version, static_cast<uint16_t>(game.gameStyle),
		static_cast<uint16_t>(game.levelNumber), game.level, static_cast<uint8_t>(game.coopLobby));
}

// Added 2026-11-09 by Melanikus for the AvP port: where a game from the relays' list is on the list of sessions, by its region and code, as
// AddRelaySession tells two apart; -1 if it isn't there (relay_auto.cpp)
int Net_FindRelaySession(const char *region, const char *code)
{
	if (!region || !code)
	{
		return -1;
	}

	for (uint32_t i = 0; i < NumberOfSessionsFound && i < MAX_NO_OF_SESSIONS; i++)
	{
		if (SessionData[i].throughRelay && memcmp(SessionData[i].relayCode, code, kRelayCodeLetters) == 0
			&& strncmp(SessionData[i].relayRegion, region, sizeof(SessionData[i].relayRegion)) == 0)
		{
			return static_cast<int>(i);
		}
	}

	return -1;
}

extern void MinimalNetCollectMessages(void);
extern void InitAVPNetGameForHost(int species, int gamestyle, int level);
extern void InitAVPNetGameForJoin(void);
extern int DetermineAvailableCharacterTypes(bool ConsiderUsedCharacters);
extern char *GetCustomMultiplayerLevelName(int index, int gameType);
extern bool AddPlayerToGame(NetID id, const char *name);
extern void Net_PlayerHasGone(NetID id);
extern void Net_HostHasGone();
extern bool Net_GameMessageIsGood(const uint8_t *message, size_t size, bool fromTheHost);
extern int Net_CoopLobbyState(void);
extern void Net_NoteRefused(const char *why);
extern bool Net_MessageIsForTheHostAlone(const uint8_t *message, size_t size);
extern void Net_CoopLobbyJoined(int state);
extern bool Net_ClientCanAfford(const uint8_t *message, size_t size, size_t connection);
extern unsigned char DebouncedKeyboardInput[];

static bool Net_CreatePlayer(char *playerName);
static NetResult Net_CreateSession(const char *sessionName, const char *levelName, int maxPlayers, int version, uint16_t gameStyle, uint16_t level);
static NetResult Net_UpdateSessionDescForLobbiedGame(uint16_t gameStyle, uint16_t level);

/* KJL 14:58:18 03/07/98 - AvP's Guid */
// {379CCA80-8BDD-11d0-A078-004095E16EA5}
static const GUID AvPGuid = { 0x379cca80, 0x8bdd, 0x11d0, { 0xa0, 0x78, 0x0, 0x40, 0x95, 0xe1, 0x6e, 0xa5 } };

const uint32_t kMessageHeaderSize = sizeof(MessageHeader);

/* Some notes on how DirectPlay worked (and what AvP might be expecting)

	System messages are sent to ALL players


*/

// Debug function
const char* Net_MessageTypeToString(uint32_t messageType)
{
	switch (messageType)
	{
		case NETSYS_REQUEST_SESSION_INFO:
			return "NETSYS_REQUEST_SESSION_INFO";
			break;
		case NETSYS_SESSION_INFO:
			return "NETSYS_SESSION_INFO";
			break;
		case NET_CREATEPLAYERORGROUP:
			return "NET_CREATEPLAYERORGROUP";
			break;
		case NET_SETPLAYERID:
			return "NET_SETPLAYERID";
			break;
		case NET_DESTROYPLAYERORGROUP:
			return "NET_DESTROYPLAYERORGROUP";
			break;
		case NET_ADDPLAYERTOGROUP:
			return "NET_ADDPLAYERTOGROUP";
			break;
		case NET_DELETEPLAYERFROMGROUP:
			return "NET_DELETEPLAYERFROMGROUP";
			break;
		case NET_SESSIONLOST:
			return "NET_SESSIONLOST";
			break;
		case NET_HOST:
			return "NET_HOST";
			break;
			//	NET_SETPLAYERORGROUPDATA,
			//	NET_SETPLAYERORGROUPNAME,
			//	NET_SETSESSIONDESC,
			//	NET_ADDGROUPTOGROUP,
			//	NET_DELETEGROUPFROMGROUP,
			//	NET_SECUREMESSAGE,
			//	NET_STARTSESSION,
			//	NET_CHAT,
			//	NET_SETGROUPOWNER,
			//	NET_SENDCOMPLETE,
			//	NET_PLAYERTYPE_GROUP,
		case NET_PLAYERTYPE_PLAYER:
			return "NET_PLAYERTYPE_PLAYER";
			break;
			//	NET_RECEIVE_ALL,
		case NET_ID_ALLPLAYERS:
			return "NET_ID_ALLPLAYERS";
			break;
			//	NET_ID_SERVERPLAYER,
		default:
			return "Unknown message type";
			break;
	}
}

static void LogDroppedMessage(const char *why);
static const int kMostGamesFromOneAddress = 2;

static void Net_AddSession(SessionDescription &session)
{
	uint16_t gameStyle = session.gameStyle;
	uint16_t level     = session.level;

	// This comes from anyone who answers a search, like everything else here
	if (!CoopLobby_StateIsReal(session.coopLobby))
	{
		LogDroppedMessage("a game described itself with a lobby state there isn't");
		return;
	}

	// the session description comes from any machine on the LAN: SessionData (and the session menu,
	// AvPMenu_MultiplayerSelectSession) hold MAX_NO_OF_SESSIONS entries, so ignore any more
	if (NumberOfSessionsFound >= MAX_NO_OF_SESSIONS)
	{
		static unsigned long long ignoredSessions = 0;
		if (LogThisDrop(ignoredSessions))
		{
			std::stringstream sstream;
			sstream << "Net_AddSession - session list is full, ignoring session info (" << ignoredSessions << " so far)";
			Con_PrintError(sstream.str());
		}
		return;
	}

	// the host sends both names NUL-terminated (Net_CreateSession); make sure of it
	session.levelName[kLevelNameSize - 1]     = '\0';
	session.sessionName[kSessionNameSize - 1] = '\0';

	// Anyone can answer a search. One address can't fill the list and push real games off it.
	int gamesFromThisAddress = 0;

	for (uint32_t i = 0; i < NumberOfSessionsFound; i++)
	{
		gamesFromThisAddress += SessionData[i].host == session.host ? 1 : 0;
	}

	if (gamesFromThisAddress >= kMostGamesFromOneAddress)
	{
		LogDroppedMessage("one address answered a search with more games than a machine runs");
		return;
	}

	// A game that answers twice is one game
	for (uint32_t i = 0; i < NumberOfSessionsFound; i++)
	{
		if (memcmp(&SessionData[i].Guid, &session.guidInstance, sizeof(session.guidInstance)) == 0)
		{
			return;
		}
	}

	char *name = SessionData[NumberOfSessionsFound].Name;
	const size_t nameSize = sizeof(SessionData[NumberOfSessionsFound].Name);

	SessionData[NumberOfSessionsFound].host = session.host;
	SessionData[NumberOfSessionsFound].port = session.port;
	SessionData[NumberOfSessionsFound].throughRelay = false;   // a slot is used again from search to search, so a relay game's mark goes
	memset(SessionData[NumberOfSessionsFound].relayCode, 0, sizeof(SessionData[NumberOfSessionsFound].relayCode));
	memset(SessionData[NumberOfSessionsFound].relayRegion, 0, sizeof(SessionData[NumberOfSessionsFound].relayRegion));
	SessionData[NumberOfSessionsFound].coopLobby = (char)session.coopLobby;

	// bounded: a long session name used to overflow Name (40 bytes) into host/port
	_snprintf(name, nameSize - 1, "%s (%u/%u)", session.sessionName, session.nPlayers, session.maxPlayers);
	name[nameSize - 1] = '\0';

	// Modified 2026-11-09 by Melanikus for the AvP port: the games page's columns (Fourth's online screens): the level's name, the game's
	// style and its players, as the host said them. The name ends already (above); the page makes it safe and checks the numbers before it
	// uses them (menus.h).
	static_assert(sizeof(SessionData[0].levelName) == kLevelNameSize, "a LAN game's level name fits the games page's column exactly");
	memcpy(SessionData[NumberOfSessionsFound].levelName, session.levelName, kLevelNameSize);
	SessionData[NumberOfSessionsFound].gameStyle = session.gameStyle;
	SessionData[NumberOfSessionsFound].players = session.nPlayers;
	SessionData[NumberOfSessionsFound].maxPlayers = session.maxPlayers;

	SessionData[NumberOfSessionsFound].Guid	= session.guidInstance;

	if (session.nPlayers < session.maxPlayers) {
		SessionData[NumberOfSessionsFound].AllowedToJoin = true;
	}
	else {
		SessionData[NumberOfSessionsFound].AllowedToJoin = false;
	}

	// multiplayer version number (possibly)
	if (session.version != kMultiplayerVersion)
	{
		float version = 1.0f + session.version / 100.0f;
		SessionData[NumberOfSessionsFound].AllowedToJoin = false;
 		_snprintf(name, nameSize - 1, "%s (V %.2f)", session.sessionName, version);
		name[nameSize - 1] = '\0';
	}
	else
	{
		// get the level number in our list of levels (assuming we have the level)

		int local_index = GetLocalMultiplayerLevelIndex(level, session.levelName, gameStyle);
		// A lobby game stays on the list without its map, because its page is where this player is told so. It has no use for the place on the list.
		if (local_index < 0 && session.coopLobby == kCoopLobbyNone)
		{
			// we don't have the level, so ignore this session
			return;
		}

		SessionData[NumberOfSessionsFound].levelIndex = local_index < 0 ? 0 : local_index;
	}

	NumberOfSessionsFound++;
}

// THE SOCKET FOR FINDING GAMES. Other machines on the network can send to it, so it's only open while it has a job: for the few seconds after a player asks for the list of games, and while hosting, so that
// others can find this game. A game doing neither has nothing open on any network. Gives back whether it's open.
static bool OpenDiscoverySocket()
{
	if (ENET_SOCKET_NULL != broadcastSocket)
	{
		return true;
	}

	broadcastSocket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);

	if (ENET_SOCKET_NULL == broadcastSocket)
	{
		Con_PrintError("OpenDiscoverySocket - Couldn't create broadcast socket");
		return false;
	}

	enet_socket_set_option(broadcastSocket, ENET_SOCKOPT_NONBLOCK,  1);
	enet_socket_set_option(broadcastSocket, ENET_SOCKOPT_BROADCAST, 1);

	ENetAddress thisHost;
	thisHost.host = ENET_HOST_ANY;
	thisHost.port = broadcastPort;

	if (enet_socket_bind(broadcastSocket, &thisHost) < 0)
	{
		Con_PrintError("OpenDiscoverySocket - Couldn't bind broadcast socket");
		enet_socket_destroy(broadcastSocket);
		broadcastSocket = ENET_SOCKET_NULL;
		return false;
	}

	return true;
}

// The host's count of the search questions it hears, for the log: see net_search_log.h. Only a host opens it.
static NetSearchLog searchLog = {};
static_assert(kNetSearchPort == broadcastPort, "net_search_log.h's lines name the port a search asks on");

// Added 2026-11-08 by Melanikus for the AvP port: a host that started through a relay and is open on this network after all
// (RelayHostGiveUp) opens the search port with the same lines as one with no relay (Net_HostGame), so the search log counts for it too
static void OpenTheSearchPortForAHost()
{
	if (OpenDiscoverySocket())
	{
		char line[160];
		NetSearchLog_Opened(&searchLog, enet_time_get(), line, sizeof(line));
		LogString(line);
	}
	else
	{
		LogString("net: this game can't answer searches: UDP port " + std::to_string(broadcastPort) + " couldn't be opened, so a search won't find it");
	}
}

static void CloseDiscoverySocket()
{
	if (ENET_SOCKET_NULL != broadcastSocket)
	{
		enet_socket_destroy(broadcastSocket);
		broadcastSocket = ENET_SOCKET_NULL;
	}
}

// The search's time is up, or the player has left the page. A host keeps the socket, because that's how others find its game.
void Net_StopSearchingForSessions()
{
	sessionCheckTimer = 0;

	if (AvP.Network != I_Host)
	{
		CloseDiscoverySocket();
	}
}

bool Net_UpdateSessionList(int &SelectedItem)
{
	if (sessionCheckTimer == 0 || timeGetTime() - sessionCheckTimer >= sessionCheckTimeout)
	{
		Net_StopSearchingForSessions();
		return false;
	}

	if (timeGetTime() - sessionLastAskedAt >= 1000)
	{
		sessionLastAskedAt = timeGetTime();
		AskEveryoneAboutGames();
	}

	GUID OldSessionGuids[MAX_NO_OF_SESSIONS];
	uint32_t OldNumberOfSessions = NumberOfSessionsFound;
	bool changed = false;

	// take a list of the old session guids
	for (uint32_t i = 0; i < NumberOfSessionsFound; i++) {
		OldSessionGuids[i] = SessionData[i].Guid;
	}

	// poll network for session data
	{
		MessageHeader header;
		ENetBuffer buf;
		buf.data = &packetBuffer;
		buf.dataLength = kPacketBufferSize;

		enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;

		while (enet_socket_wait(broadcastSocket, &events, 0) >= 0 && events)
		{
			ENetAddress address;
			int len = enet_socket_receive(broadcastSocket, &address, &buf, 1);

			if (len <= 0) {
				break;
			}

			// An answer. Our own question, if it comes back to us, is this size too (see kSearchQuestionBytes): its header says it's a
			// question, so it's skipped below.
			if (len == sizeof(header) + sizeof(SessionDescription))
			{
				OutputDebugString("Got some session info\n");

				// Every game that answers starts again at the start of the buffer
				MemoryReadStream rs(packetBuffer, kPacketBufferSize);

				rs.GetBytes(&header, sizeof(header));

				if (header.marker == kMarker && header.messageType == NETSYS_SESSION_INFO)
				{
					OutputDebugString("Net_AddSession called\n");
					SessionDescription newSession;
					rs.GetBytes(&newSession, sizeof(newSession));

					// Anyone can send this, so it has to at least say it's this game
					if (memcmp(&newSession.guidApplication, &AvPGuid, sizeof(AvPGuid)) != 0)
					{
						continue;
					}

					// FIXME? - server should set this from their side correctly?
					newSession.host = address.host;

					Net_AddSession(newSession);
				}
			}
			else {
				char tprint[100];
				sprintf(tprint, "Got data of size %d\n", len);
				OutputDebugString(tprint);
			}
		}
	}

	// Have the available sessions changed? first check number of sessions
	if (NumberOfSessionsFound != OldNumberOfSessions) {
		changed = true;
	}
	else
	{
		// now check the guids of the new sessions against our previous list
		for (uint32_t i = 0; i < NumberOfSessionsFound; i++)
		{
			if (!IsEqualGUID(OldSessionGuids[i], SessionData[i].Guid)) {
				changed = true;
			}
		}
	}

	if (changed && OldNumberOfSessions > 0)
	{
		// See if our previously selected session is in the new session list
		int OldSelection = SelectedItem;
		SelectedItem = 0;

		for (uint32_t i = 0; i < NumberOfSessionsFound; i++)
		{
			if (IsEqualGUID(OldSessionGuids[OldSelection], SessionData[i].Guid))
			{
				// found the session
				SelectedItem = i;
				break;
			}
		}
	}

	return changed;
}

// A number from the settings file, if the game can use it; if not, the game's own number, and the log says so. Only the number is written
// in the log, never the file's text, so nothing in the file can make a line that isn't ours.
static int NetSettingOrOwn(const char *name, int fromTheFile, bool usable, int own)
{
	if (usable)
	{
		return fromTheFile;
	}

	LogString(std::string("net: the settings file's ") + name + " is " + std::to_string(fromTheFile) + ", which the game can't use, so it uses " + std::to_string(own));
	return own;
}

NetResult Net_Initialise()
{
	Con_PrintMessage("Initialising ENet networking");

	if (enet_initialize() != 0)
	{
		Con_PrintError("Failed to initialise ENet networking");
		return NET_FAIL;
	}

	// The player's own settings. The settings file can hold any number, so each one is checked, and one the game can't use is put back to
	// the game's own, with a line in the log saying so. The port is the one the host plays on: it has to be a port there is, and not the one
	// a search uses (broadcastPort), which the host opens as well. A speed of 0 means "as much as there is", and one below 0 means nothing.
	int const port = Config_GetInt("[Networking]", "PortNumber", (int)netPortNumber);
	int const incoming = Config_GetInt("[Networking]", "IncomingBandwidth", 0);
	int const outgoing = Config_GetInt("[Networking]", "OutgoingBandwidth", 0);

	netPortNumber = (uint32_t)NetSettingOrOwn("PortNumber", port, port >= 1 && port <= 65535 && port != (int)broadcastPort, (int)netPortNumber);
	incomingBandwidth = (uint32_t)NetSettingOrOwn("IncomingBandwidth", incoming, incoming >= 0, 0);
	outgoingBandwidth = (uint32_t)NetSettingOrOwn("OutgoingBandwidth", outgoing, outgoing >= 0, 0);

	// Nothing is opened on the network here, because this runs every time the game starts, and most games are never on a network. See OpenDiscoverySocket.

	// register console commands
	Con_AddCommand("connect", Net_ConnectToAddress);
	Con_AddCommand("disconnect", Net_Disconnect);

	net_IsInitialised = true;

	Con_PrintMessage("Initialised ENet networking succesfully");

	return NET_OK;
}

void Net_Deinitialise()
{
	Con_PrintMessage("Deinitialising ENet networking");

	// let everyone know we're disconnecting and destroy the host
	Net_Disconnect();

	enet_deinitialize();

	net_IsInitialised = false;
}

// Called in a loop while we wait for the server to send us the game details (not session description)
int Net_ConnectingToSession()
{
	// See if the player has got bored of waiting, or if the host has gone or turned them away
	if (DebouncedKeyboardInput[KEY_ESCAPE] || !ServerPeer)
	{
		Net_Disconnect();
		AvPNetID = 0;
		AvP.Network = I_No_Network;
		return NET_FAIL;
	}

	MinimalNetCollectMessages();
	if (!netGameData.needGameDescription)
	{
		// we now have the game description, so we can go to the configuration menu
		OutputDebugString("returning AVPMENU_MULTIPLAYER_CONFIG_JOIN");
		return AVPMENU_MULTIPLAYER_CONFIG_JOIN;
	}
	return NET_OK;
}

int Net_ConnectingToLobbiedGame(char *playerName)
{
	OutputDebugString("Net_ConnectingToLobbiedGame\n");
	return NET_FAIL;
}

NetResult Net_HostGame(char *playerName, char *sessionName, int species, uint16_t gameStyle, uint16_t level)
{
	printf("Net_HostGame\n");

	if (!net_IsInitialised)
	{
		Con_PrintError("Networking is not initialisedr");
		return NET_FAIL;
	}

	// are we already connected?
	if (host)
	{
		Con_PrintError("already connected to something, so that's being closed first");
		enet_host_destroy(host);
		host = NULL;
	}

	// A host has no server to send to. Whatever was here belonged to the last game.
	ServerPeer = NULL;

	int maxPlayers = DetermineAvailableCharacterTypes(false);
	if (maxPlayers < 1) maxPlayers = 1;
	if (maxPlayers > 8) maxPlayers = 8;

	if (!netGameData.skirmishMode)
	{
		if (!LobbiedGame)
		{
			ServerAddress.host = ENET_HOST_ANY;

			ServerAddress.port = netPortNumber;

			// The game's limits, a checksum on every packet and one channel: see net_host.h
			host = Net_MakeENetHost(&ServerAddress, kMostClients, kMostMessageBytes, incomingBandwidth, outgoingBandwidth);
			if (NULL == host)
			{
				Con_PrintError("Failed to create ENet server");
				return NET_FAIL;
			}

			// Others find the game through this. Without it, the game goes on, and anyone who has its address can still join.
			// The log says which, and from then on what the search port hears (net_search_log.h).
			// Modified 2026-11-08 by Melanikus for the AvP port: through the relay instead, when one is set (RelayHostStart, above). Then
			// nothing on this network can find it by searching, and the search port stays shut. If the relay falls through, RelayHostGiveUp
			// opens it after all, with the same lines (OpenTheSearchPortForAHost).
			if (RelayHostStart())
			{
				Con_PrintMessage("net: this game won't answer searches on this network while it goes through a relay");
			}
			else if (OpenDiscoverySocket())
			{
				char line[160];
				NetSearchLog_Opened(&searchLog, enet_time_get(), line, sizeof(line));
				LogString(line);
			}
			else
			{
				LogString("net: this game can't answer searches: UDP port " + std::to_string(broadcastPort) + " couldn't be opened, so a search won't find it");
			}

			// create session
			char *levelName = GetCustomMultiplayerLevelName(level, gameStyle);
			if (IsCustomLevel(level, gameStyle))
			{
				if ((Net_CreateSession(sessionName, levelName, maxPlayers, kMultiplayerVersion, gameStyle, 100)) != NET_OK) {
					return NET_FAIL;
				}
			}
			else
			{
				if ((Net_CreateSession(sessionName, levelName, maxPlayers, kMultiplayerVersion, gameStyle, level)) != NET_OK) {
					return NET_FAIL;
				}
			}
		}
		else
		{
			// for lobbied games we need to fill in the level number into the existing session description
			if (Net_UpdateSessionDescForLobbiedGame(gameStyle, level) != NET_OK) {
				return NET_FAIL;
			}
		}

		// we're the host
		AvP.Network = I_Host;

		if (!Net_CreatePlayer(playerName)) {
			return NET_FAIL;
		}
	}
	else
	{
		// fake multiplayer (skirmish mode) - need to set the id to an non zero value
		AvPNetID = 100;

		memset(&thisClientPlayer, 0, sizeof(PlayerDetails));
		strncpy(thisClientPlayer.name, playerName, kPlayerNameSize-1);
		thisClientPlayer.name[kPlayerNameSize-1] = '\0';
		thisClientPlayer.ID   = AvPNetID;
		thisClientPlayer.type = 0; // ??
		thisClientPlayer.version = kMultiplayerVersion;
	}

	InitAVPNetGameForHost(species, gameStyle, level);

	Con_PrintMessage("Server running...");

	return NET_OK;
}

// How long leaving waits for the other ends to hear that this machine is closing. On a network that works, that takes a few
// thousandths of a second. A machine that has gone already never answers, so this is the most that's ever spent on one.
static unsigned int const kMostMillisecondsToClose = 1000;

void Net_Disconnect()
{
	OutputDebugString("Net_Disconnect\n");

	// A host's held request to join goes with its game. A joiner's answer stays, for its screen to say why it's back (Net_JoinAnswerNow):
	// the next join starts it again.
	ForgetTheJoinRequest();
	relayJoinedThrough = false;   // the line in play saying which relay the game went through goes with the game too

	// This also runs from the console's disconnect command, in a game that isn't on any network
	if (host)
	{
		// What's still waiting goes first, and then each connection closes with the other end told. That closing is how the others hear
		// this machine has gone. Destroying the host straight away would throw all of that away: see net_close.h.
		int const stillClosing = Net_CloseConnectionsAfterWhatsWaiting(host, kMostMillisecondsToClose);

		if (stillClosing > 0)
		{
			LogString("net: left without hearing back from " + std::to_string(stillClosing) + " connection(s), which may have gone already");
		}

		// Then the room on the relay, after the goodbyes have gone through it
		RelayHostLeave();

		enet_host_destroy(host);
		host = NULL;
	}

	// The ENet host owned this, and has just freed it
	ServerPeer = NULL;

	sessionCheckTimer = 0;

	// A host's search counts for the whole game, before the port closes (net_search_log.h)
	{
		char line[320];

		if (NetSearchLog_Closed(&searchLog, enet_time_get(), line, sizeof(line)))
		{
			LogString(line);
		}
	}

	CloseDiscoverySocket();

	// The game has closed, and the list was only for this game
	CoopRemoved_Forget(removedPlayers);
}

static NetID IDOfConnection(const ENetPeer *peer);

void Net_RemoveThisPlayer(NetID id)
{
	if (AvP.Network != I_Host || !host || id == 0 || id == AvPNetID)
	{
		return;
	}

	for (ENetPeer *peer = host->peers; peer < &host->peers[host->peerCount]; ++peer)
	{
		if (peer->state == ENET_PEER_STATE_CONNECTED && IDOfConnection(peer) == id)
		{
			// Not by the relay's address, which every joiner through the relay has: that would bar them all (debt: so a removed player
			// can come back through the relay)
			if (!PeerIsThroughRelay(peer))
			{
				CoopRemoved_Add(removedPlayers, peer->address.host);
			}

			// They're out at once: nothing more from this connection counts, whatever it does about the goodbye
			peer->data = NULL;
			netSession.nPlayers--;
			Net_PlayerHasGone(id);
			enet_peer_disconnect(peer, kCoopRemovedByTheHost);
			return;
		}
	}
}

bool Net_WasRemovedByTheHost()
{
	return removedByTheHost;
}

uint32_t Net_JoinGame()
{
//	char buf[100];
//	sprintf(buf, "Net_JoinGame at %d\n", timeGetTime());
//	OutputDebugString(buf);

	if (!net_IsInitialised)
	{
		Con_PrintError("Networking is not initialisedr");
		return 0;
	}

	// enumerate sessions
	return NumberOfSessionsFound;
}

// called from the console
void Net_ConnectToAddress()
{
}

int Net_InitLobbiedGame()
{
	OutputDebugString("Net_InitwhoLobbiedGame\n");
	extern char MP_PlayerName[];
	return 0;
}

static void Net_CheckForSessionRequest()
{
	// It couldn't be opened, so others can't find this game, and anyone with its address can still join
	if (ENET_SOCKET_NULL == broadcastSocket)
	{
		return;
	}

	// Every minute, how many questions came and how many were answered (net_search_log.h)
	{
		char line[320];

		if (NetSearchLog_Minute(&searchLog, enet_time_get(), line, sizeof(line)))
		{
			LogString(line);
		}
	}

	enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;

	while (enet_socket_wait(broadcastSocket, &events, 0) >= 0 && events)
	{
		Con_PrintDebugMessage("Net_CheckForSessionRequest got something");
		ENetAddress addr;
		ENetBuffer buf;
		buf.data = packetBuffer;
		buf.dataLength = kPacketBufferSize;

		int len = enet_socket_receive(broadcastSocket, &addr, &buf, 1);
		if (0 == len) {
			return; // blocking - check later
		}
		if (len > 0) {
			Con_PrintDebugMessage("Got a packet of some sort..");
			NetSearchLog_Packet(&searchLog);
		}
		// Only a question as big as its answer: see kSearchQuestionBytes. An older game's question, a header alone, gets nothing.
		if (len == (int)kSearchQuestionBytes)
		{
			MessageHeader header;
			memcpy(&header, packetBuffer, sizeof(MessageHeader));

			// Anyone can send this, from an address that isn't theirs. So it has to look like ours, and this game only answers so many.
			static unsigned int secondStarted = 0;
			static int answersThisSecond = 0;

			if (header.marker != kMarker)
			{
				continue;
			}

			if (NETSYS_REQUEST_SESSION_INFO != header.messageType)
			{
				continue;
			}

			// A question for this game's description. What happens to it is counted for the log, never with its address: the first one
			// gets a line of its own (net_search_log.h).
			char line[200];

			if (!(NetAddressHasRoom(sessionAskers, sizeof(sessionAskers) / sizeof(sessionAskers[0]), addr.host, enet_time_get(), kMostSessionAnswersToOneAddressEachSecond) && NetThisSecondHasRoom(enet_time_get(), &secondStarted, &answersThisSecond, kMostSessionAnswersEachSecond)))
			{
				if (NetSearchLog_Question(&searchLog, kNetSearchTooMany, enet_time_get(), line, sizeof(line)))
				{
					LogString(line);
				}

				continue;
			}

			MemoryWriteStream ws(packetBuffer, kPacketBufferSize);

			header.marker = kMarker;
			header.isSystemMessage = true;
			header.messageType = NETSYS_SESSION_INFO;
			ws.PutBytes(&header, sizeof(header));
			ws.PutBytes(&netSession, sizeof(SessionDescription));

			buf.dataLength = ws.GetBytesWritten();
			buf.data = packetBuffer;

			// 0 means the network can't take it now, and below 0 that it failed: either way the asker gets no answer this time, and the
			// rest wait for the next frame, as before
			int const ret = enet_socket_send(broadcastSocket, &addr, &buf, 1);

			if (NetSearchLog_Question(&searchLog, ret > 0 ? kNetSearchAnswered : kNetSearchNotSent, enet_time_get(), line, sizeof(line)))
			{
				LogString(line);
			}

			if (0 > ret) {
				Con_PrintError("Couldn't send session details");
			}
			else if (ret > 0) {
				Con_PrintDebugMessage("Sent session details");
			}

			return;
		}
	}
}

// call once per frame
// Anyone can cause this as fast as they like, so the log only says so now and then.
static void LogDroppedMessage(const char *why)
{
	static unsigned long long droppedMessages = 0;

	if (LogThisDrop(droppedMessages))
	{
		std::stringstream sstream;
		sstream << "Net_Receive - dropped a message because " << why << " (" << droppedMessages << " so far)";
		Con_PrintError(sstream.str());
	}
}

// Our ENet (patch 006) tells an older version of the game no as soon as it asks to join, without a place or an event, and
// counts it. This puts that in the log, so a host can see why that player never got in.
// THE NETWORK'S NUMBERS IN THE LOG (net_stats.h): about once a second, whether a connection has started or stopped struggling, and every
// 10 seconds its round trip, loss and resends, and the bytes in and out. Connections are named by their number, never their address.
static_assert(kNetStatsLossScale == ENET_PEER_PACKET_LOSS_SCALE, "net_stats.h reads ENet's loss as a share of the same whole");

static ENetHost *netStatsHost = NULL;               // the host these numbers are for: a new game's host starts them again
static unsigned int netStatsLastLook = 0;
static unsigned int netStatsLastLines = 0;
static unsigned int netStatsBytesIn = 0;
static unsigned int netStatsBytesOut = 0;
static bool netStatsStruggling[kMostClients + 1];

static void LogConnectionNumbers()
{
	unsigned int const now = enet_time_get();

	if (host != netStatsHost)
	{
		netStatsHost = host;
		netStatsLastLook = now;
		netStatsLastLines = now;
		netStatsBytesIn = host->totalReceivedData;
		netStatsBytesOut = host->totalSentData;

		for (size_t i = 0; i <= kMostClients; i++)
		{
			netStatsStruggling[i] = false;
		}

		return;
	}

	if (now - netStatsLastLook < 1000)
	{
		return;
	}

	netStatsLastLook = now;
	bool const linesDue = now - netStatsLastLines >= kNetStatsEveryMilliseconds;
	char words[200];

	for (size_t i = 0; i < host->peerCount && i <= kMostClients; i++)
	{
		ENetPeer const &peer = host->peers[i];

		if (peer.state != ENET_PEER_STATE_CONNECTED)
		{
			netStatsStruggling[i] = false;
			continue;
		}

		NetStats_Connection const numbers = { peer.roundTripTime, peer.roundTripTimeVariance, peer.packetLoss, peer.packetsLost, peer.packetsSent };
		bool struggling = false;

		if (NetStats_Change((int)i + 1, netStatsStruggling[i], numbers, &struggling, words, sizeof(words)))
		{
			LogString(words);
		}

		netStatsStruggling[i] = struggling;

		if (linesDue)
		{
			NetStats_ConnectionLine((int)i + 1, numbers, words, sizeof(words));
			LogString(words);
		}
	}

	if (linesDue)
	{
		NetStats_BytesLine(host->totalReceivedData, netStatsBytesIn, host->totalSentData, netStatsBytesOut, now - netStatsLastLines, words, sizeof(words));
		LogString(words);
		netStatsLastLines = now;
		netStatsBytesIn = host->totalReceivedData;
		netStatsBytesOut = host->totalSentData;
	}
}

static void LogOlderVersionsToldNo()
{
	static unsigned long long toldNo = 0;

	if (host->olderConnectsRefused == 0)
	{
		return;
	}

	host->olderConnectsRefused = 0;

	if (LogThisDrop(toldNo))
	{
		std::stringstream sstream;
		sstream << "Net - a player with an older version of the game tried to join, and was told no (" << toldNo << " so far)";
		Con_PrintError(sstream.str());
	}
}

static void ForgetConnectionsNotLetIn()
{
	unsigned int const now = enet_time_get();
	LookAfterTheJoinRequest(now);

	for (size_t i = 0; i < host->peerCount && i <= kMostClients; i++)
	{
		ENetPeer *const peer = &host->peers[i];

		if (peer->state == ENET_PEER_STATE_ACKNOWLEDGING_CONNECT || peer->state == ENET_PEER_STATE_CONNECTION_PENDING)
		{
			// 0 means "isn't half open", so a clock reading 0 counts as 1
			if (halfOpenSince[i] == 0)
			{
				halfOpenSince[i] = now != 0 ? now : 1;
			}
			else if (now - halfOpenSince[i] >= kMostMillisecondsHalfOpen)
			{
				LogDroppedMessage("a connection never finished starting");
				enet_peer_reset(peer);
				halfOpenSince[i] = 0;
			}

			continue;
		}

		halfOpenSince[i] = 0;

		if (peer->state == ENET_PEER_STATE_CONNECTED && peer->data != NULL && now - lastHeardFrom[i] >= (Net_CoopLobbyState() == kCoopLobbyOpen ? kMostMillisecondsSilentInTheLobby : kMostMillisecondsSilent))
		{
			// The ordinary way out, so that the game hears this player has gone
			LogDroppedMessage("a player has sent nothing for too long");
			lastHeardFrom[i] = now;
			enet_peer_disconnect(peer, 0);
			continue;
		}

		// One whose request to join the host is answering isn't dropped for that: its own limit is LookAfterTheJoinRequest's
		if (peer->state == ENET_PEER_STATE_CONNECTED && peer->data == NULL && now - connectionStartedAt[i] >= kMostMillisecondsToBeLetIn
			&& peer != JoinRequestPeer())
		{
			LogDroppedMessage("a connection didn't say who it is in time");
			enet_peer_disconnect_now(peer, 0);
		}
	}
}

// How long a load may go between answers before it answers again. Answering costs little, but this is called for every step of a load.
static const unsigned int kAnswerWhileLoadingEveryMilliseconds = 100;
static bool hasAnsweredWhileLoading = false;
static unsigned int lastAnsweredWhileLoading = 0;
static unsigned int longestGapWhileLoading = 0;

void Net_KeepAnsweringWhileLoading()
{
	if (!host)
	{
		return;
	}

	unsigned int const now = enet_time_get();
	unsigned int const gap = now - lastAnsweredWhileLoading;

	if (hasAnsweredWhileLoading && gap < kAnswerWhileLoadingEveryMilliseconds)
	{
		return;
	}

	if (hasAnsweredWhileLoading && gap > longestGapWhileLoading)
	{
		longestGapWhileLoading = gap;
	}

	hasAnsweredWhileLoading = true;
	lastAnsweredWhileLoading = now;

	// With nowhere to put an event, ENet acknowledges what's arrived and keeps it for later
	enet_host_service(host, NULL, 0);

	// A long level load mustn't lose the room on the relay
	RelayHostFrame();
}

void Net_SayLongestGapWhileLoading()
{
	if (host)
	{
		LogString("net: while loading, the longest gap between answers was " + std::to_string(longestGapWhileLoading) + " ms, and the patience is " + std::to_string(kConnectionPatienceMilliseconds) + " ms");
	}

	hasAnsweredWhileLoading = false;
	longestGapWhileLoading = 0;
}

void Net_ServiceNetwork()
{
	if (host)
	{
		enet_host_service(host, NULL, 0);

		LogConnectionNumbers();

		// The relay's part of hosting, after ENet has handed over what came: the echoes, the room, the keep and the hellos. It does
		// nothing unless this game is hosted through a relay.
		RelayHostFrame();

		// also check for session data requests
		if (AvP.Network == I_Host) {
			Net_CheckForSessionRequest();
			ForgetConnectionsNotLetIn();
			LogOlderVersionsToldNo();
		}
	}
}

// WHO IS WHO. A client only ever talks to the host, and the host passes on what's meant for the others. Anyone who can reach the host's port can connect and send anything at all, and a message can say it's from anyone. So the host doesn't go by that.
// Each connection can be let in once, and gets one player ID, which is kept with the connection. After that, whatever arrives on the connection has to carry that ID. What arrives is checked here, before the rest of the game sees it, and the
// only messages the rest of the game ever sees are game messages, whole ones.
// On a client, who the host is: whoever gave it its ID. A host passes on what the others say, each saying who it's from, and only the host's own word counts as the host's.
static NetID hostPlayerID = 0;

NetID Net_HostPlayerID()
{
	return hostPlayerID;
}

static NetID IDOfConnection(const ENetPeer *peer)
{
	return (NetID)(uintptr_t)peer->data;
}


// Does this look like one of this game's messages at all? Copies its header out if so.
static bool ReadTheHeader(const uint8_t *data, size_t size, MessageHeader &header)
{
	if (size < sizeof(MessageHeader))
	{
		LogDroppedMessage("it's too short to have a header");
		return false;
	}

	memcpy(&header, data, sizeof(MessageHeader));

	if (header.marker != kMarker)
	{
		LogDroppedMessage("it isn't marked as this game's");
		return false;
	}

	return true;
}

static NetResult SendSystemMessageTo(ENetPeer *peer, uint32_t messageType, NetID fromID, NetID toID, const void *messageData, size_t dataSize)
{
	// create a new header and copy it into the packet buffer
	MessageHeader newMessageHeader;

	// It has gaps between its parts, and the whole of it goes out, so nothing left on the stack goes with it
	memset(&newMessageHeader, 0, sizeof(newMessageHeader));
	newMessageHeader.marker = kMarker;
	newMessageHeader.isSystemMessage = 1;
	newMessageHeader.messageType = messageType;
	newMessageHeader.fromID = fromID;
	newMessageHeader.toID   = toID;

	MemoryWriteStream ws(packetBuffer, kPacketBufferSize);
	ws.PutBytes(&newMessageHeader, sizeof(newMessageHeader));

	// put data after header
	if (dataSize && messageData) {
		ws.PutBytes(messageData, dataSize);
	}

	// create ENet packet
	ENetPacket *packet = enet_packet_create(packetBuffer, ws.GetBytesWritten(), ENET_PACKET_FLAG_RELIABLE);
	if (packet == NULL)
	{
		Con_PrintError("SendSystemMessageTo - couldn't create packet");
		return NET_FAIL;
	}

	if (enet_peer_send(peer, 0, packet) != 0)
	{
		Con_PrintError("SendSystemMessageTo - can't send peer packet");

		// ENet frees a packet when it's been delivered, and this one won't be
		enet_packet_destroy(packet);
		return NET_FAIL;
	}

	return NET_OK;
}

// A client that has only just connected has asked to be let in. There's no answer when there's no room: the connection is closed, and the client sees that.
static void LetClientIn(ENetPeer *peer, const uint8_t *details)
{
	PlayerDetails player;

	memcpy(&player, details, sizeof(PlayerDetails));

	// Nothing says this name ends
	player.name[kPlayerNameSize - 1] = '\0';

	// A game that doesn't speak this version means different things by the same numbers
	if (player.version != kMultiplayerVersion)
	{
		LogDroppedMessage("a client asked to join with another version of the game");
		enet_peer_disconnect(peer, 0);
		return;
	}

	// Somebody the host took out of this game
	if (!PeerIsThroughRelay(peer) && CoopRemoved_Has(removedPlayers, peer->address.host))
	{
		Net_NoteRefused(kCoopRefusedRemoved);
		enet_peer_disconnect(peer, kCoopRemovedByTheHost);
		return;
	}

	NetID const id = Net_GetNextPlayerID();

	if (netSession.nPlayers >= netSession.maxPlayers || !AddPlayerToGame(id, player.name))
	{
		enet_peer_disconnect(peer, 0);
		return;
	}

	if (SendSystemMessageTo(peer, NET_SETPLAYERID, AvPNetID, id, &id, sizeof(id)) != NET_OK)
	{
		Net_PlayerHasGone(id);
		enet_peer_disconnect(peer, 0);
		return;
	}

	peer->data = (void *)(uintptr_t)id;
	netSession.nPlayers++;
}

// Modified 2026-11-09 by Melanikus for the AvP port: THE REQUEST TO JOIN (network version 114). Every joiner asks the host first, however
// it came (a search on this network, a typed address, or the relay), and a person at the host answers, from the in-game menu (Fourth's
// online screens). The joiner's request is its "let me in" with its name (NET_CREATEPLAYERORGROUP), as before; now the host holds it.
//   - What needs no question is answered at once, as LetClientIn always did: another version, somebody the host took out, or a full game.
//   - One request is held at a time: a joiner who asks meanwhile is told the host is busy.
//   - Until the answer, nothing is made for the joiner and nothing is sent to it, and the limit on saying who it is (kMostMillisecondsToBeLetIn)
//     doesn't drop it.
//   - Yes lets it in, through LetClientIn, which checks it all again. No, busy and no answer close its connection with a reason of their
//     own (networking.h), so the joiner's screen can say which. Unanswered for kNetJoinAnswerWithinMs counts as no answer.
//   - A test's host lets every joiner in without asking (Net_LetEveryJoinerIn: -dev -relayhost, and Hudson's runs).
// The request is kept by its connection's place and ENet's own number for that connection (connectID), so an answer can never reach
// somebody else who gets the same place later.
static bool letEveryJoinerIn = false;
static bool joinRequestHeld = false;
static size_t joinRequestConnection = 0;
static enet_uint32 joinRequestConnectID = 0;
static enet_uint32 joinRequestSince = 0;
static PlayerDetails joinRequestDetails;

static void ForgetTheJoinRequest()
{
	joinRequestHeld = false;
	memset(&joinRequestDetails, 0, sizeof(joinRequestDetails));
}

// The connection whose request is held, while it's still that joiner's and still not let in; NULL otherwise
static ENetPeer *JoinRequestPeer()
{
	if (!joinRequestHeld || !host || joinRequestConnection >= host->peerCount)
	{
		return NULL;
	}

	ENetPeer *const peer = &host->peers[joinRequestConnection];
	return peer->state == ENET_PEER_STATE_CONNECTED && peer->connectID == joinRequestConnectID && IDOfConnection(peer) == 0 ? peer : NULL;
}

// A joiner's "let me in", with its details: held for the host to answer, or answered at once when no question is needed
static void HoldOrLetIn(ENetPeer *peer, const uint8_t *details)
{
	PlayerDetails player;
	memcpy(&player, details, sizeof(player));
	player.name[kPlayerNameSize - 1] = '\0';

	bool const noQuestion = letEveryJoinerIn || player.version != kMultiplayerVersion || netSession.nPlayers >= netSession.maxPlayers
		|| (!PeerIsThroughRelay(peer) && CoopRemoved_Has(removedPlayers, peer->address.host));

	if (noQuestion)
	{
		LetClientIn(peer, details);
		return;
	}

	if (JoinRequestPeer() == peer)
	{
		LogDroppedMessage("a joiner asked to be let in again while the host was still answering");
		return;
	}

	if (JoinRequestPeer() != NULL)
	{
		LogDroppedMessage("a joiner asked while the host was answering another, and was told the host is busy");
		enet_peer_disconnect(peer, kNetJoinRefusedBusy);
		return;
	}

	joinRequestHeld = true;
	joinRequestConnection = (size_t)(peer - host->peers);
	joinRequestConnectID = peer->connectID;
	joinRequestSince = enet_time_get();
	joinRequestDetails = player;
	LogString("net: a joiner asks to be let in (connection " + std::to_string(joinRequestConnection + 1) + "), and the host is asked");
}

// Each time the network is served, on a host: a request whose joiner has gone goes too, and one unanswered too long is told so
static void LookAfterTheJoinRequest(unsigned int now)
{
	if (!joinRequestHeld)
	{
		return;
	}

	ENetPeer *const asking = JoinRequestPeer();

	if (!asking)
	{
		ForgetTheJoinRequest();
	}
	else if (now - joinRequestSince >= kNetJoinAnswerWithinMs)
	{
		LogString("net: nobody answered a joiner's request in time, so it was told no answer came");
		enet_peer_disconnect(asking, kNetJoinRefusedNoAnswer);
		ForgetTheJoinRequest();
	}
}

bool Net_JoinRequestWaiting(char *nameOut, size_t nameRoom, NetJoinWho *who)
{
	if (!JoinRequestPeer() || !nameOut || nameRoom == 0 || !who)
	{
		return false;
	}

	strncpy(nameOut, joinRequestDetails.name, nameRoom - 1);
	nameOut[nameRoom - 1] = '\0';
	who->connection = (int)joinRequestConnection;
	who->connectID = joinRequestConnectID;
	return true;
}

void Net_AnswerJoinRequest(NetJoinWho who, bool yes)
{
	ENetPeer *const asking = JoinRequestPeer();

	// An answer to a request that's gone, or to somebody else's, does nothing
	if (!asking || who.connection != (int)joinRequestConnection || who.connectID != joinRequestConnectID)
	{
		return;
	}

	PlayerDetails const details = joinRequestDetails;
	ForgetTheJoinRequest();

	if (yes)
	{
		LogString("net: the host let the joiner in");
		LetClientIn(asking, (const uint8_t *)&details);
	}
	else
	{
		LogString("net: the host said no to the joiner");
		enet_peer_disconnect(asking, kNetJoinRefusedNo);
	}
}

void Net_LetEveryJoinerIn(bool yes)
{
	letEveryJoinerIn = yes;
}

// To every client that's been let in, but not to one that has only connected, nor to the one given here. ENet frees a packet once everyone it was sent to has had it, so one that went to nobody has to be freed here.
static void SendToClients(ENetPacket *packet, const ENetPeer *except)
{
	for (ENetPeer *peer = host->peers; peer < &host->peers[host->peerCount]; ++peer)
	{
		if (peer != except && peer->state == ENET_PEER_STATE_CONNECTED && IDOfConnection(peer) != 0)
		{
			enet_peer_send(peer, 0, packet);
		}
	}

	if (packet->referenceCount == 0)
	{
		enet_packet_destroy(packet);
	}
}

// What a host does with something from a client. Gives back whether it's a game message for the game to read.
static bool HostHearsFromClient(ENetPeer *from, const uint8_t *data, size_t size)
{
	MessageHeader header;

	if (!ReadTheHeader(data, size, header))
	{
		return false;
	}

	NetID const id = IDOfConnection(from);

	if (header.isSystemMessage)
	{
		// The only thing a client ever asks the host is to be let in
		if (header.messageType != NET_CREATEPLAYERORGROUP)
		{
			LogDroppedMessage("a client can't send that system message");
		}
		else if (id != 0)
		{
			LogDroppedMessage("a client asked to be let in twice");
		}
		else if (size != sizeof(MessageHeader) + sizeof(PlayerDetails))
		{
			LogDroppedMessage("a client's details are the wrong size");
		}
		else
		{
			HoldOrLetIn(from, data + sizeof(MessageHeader));
		}

		return false;
	}

	if (id == 0)
	{
		LogDroppedMessage("a client that hasn't been let in sent a game message");
		return false;
	}

	if (header.fromID != id)
	{
		LogDroppedMessage("a client sent a game message saying it's from someone else");
		return false;
	}

	size_t const connection = (size_t)(from - host->peers);

	if (connection <= kMostClients && Net_CoopLobbyState() == kCoopLobbyNone)
	{
		lastHeardFrom[connection] = enet_time_get();
	}

	if (connection <= kMostClients && !NetThisSecondHasRoomFor(enet_time_get(), &bytesSecond[connection], &bytesUsed[connection], kMostBytesEachSecond, (int)size))
	{
		LogDroppedMessage("a client sent more bytes in a second than any game does");
		return false;
	}

	if (connection <= kMostClients && !NetThisSecondHasRoom(enet_time_get(), &messagesSecond[connection], &messagesUsed[connection], kMostMessagesEachSecond))
	{
		LogDroppedMessage("a client sent more messages in a second than any game does");
		return false;
	}

	// This comes before it's passed on, because everyone it reaches would believe it. No client speaks for the host.
	if (!Net_GameMessageIsGood(data, size, false))
	{
		LogDroppedMessage("a client's game message isn't made up properly, or says what only a host can");
		return false;
	}

	// This too comes before it's passed on, so a flood costs the host once, and costs nobody else anything
	if (!Net_ClientCanAfford(data, size, connection))
	{
		return false;
	}

	// In the lobby only what the lobby takes counts as being heard from, so sending what's refused doesn't keep a seat
	if (connection <= kMostClients && Net_CoopLobbyState() != kCoopLobbyNone)
	{
		lastHeardFrom[connection] = enet_time_get();
	}

	// Nothing goes from one joiner to another in the lobby, and an answer about the map is only ever for the host
	if (Net_CoopLobbyState() != kCoopLobbyNone || Net_MessageIsForTheHostAlone(data, size))
	{
		return true;
	}

	// Clients can't reach each other, so the host passes this on to the rest
	ENetPacket *packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);

	if (packet == NULL)
	{
		Con_PrintError("HostHearsFromClient - couldn't create packet");
	}
	else
	{
		SendToClients(packet, from);
	}

	return true;
}

// What a client does with something from its host. Gives back whether it's a game message for the game to read.
static bool ClientHearsFromHost(const uint8_t *data, size_t size)
{
	MessageHeader header;

	if (!ReadTheHeader(data, size, header))
	{
		return false;
	}

	if (!header.isSystemMessage)
	{
		if (!Net_GameMessageIsGood(data, size, hostPlayerID != 0 && header.fromID == hostPlayerID))
		{
			LogDroppedMessage("a game message isn't made up properly, or says what only a host can and isn't from the host");
			return false;
		}

		return true;
	}

	NetID id = 0;

	// The only system message a host ever sends is a client's ID, and only once
	if (header.messageType != NET_SETPLAYERID)
	{
		LogDroppedMessage("a host can't send that system message");
	}
	else if (size != sizeof(MessageHeader) + sizeof(NetID))
	{
		LogDroppedMessage("a player ID is the wrong size");
	}
	else if (AvPNetID != 0)
	{
		LogDroppedMessage("this game has already been given its player ID");
	}
	else
	{
		memcpy(&id, data + sizeof(MessageHeader), sizeof(NetID));
		AvPNetID = id;
		thisClientPlayer.ID = id;
		hostPlayerID = header.fromID;
		joinAnswer = kNetJoinAnswerYes;
	}

	return false;
}

NetResult Net_Receive(uint8_t *messageData, size_t &dataSize)
{
	dataSize = 0;

	// A client has nobody else to talk to, so when it has lost its host the game is over, whether the host went or the console's disconnect command closed the connection. This is asked on every call, because a client can be loading a level when it happens.
	if (AvP.Network == I_Peer && !ServerPeer)
	{
		Net_HostHasGone();
		return NET_NO_MESSAGES;
	}

	// this function is called from a loop. only return something other than NET_OK when we've no more messages
	if (host)
	{
		ENetEvent eEvent;
		if (enet_host_check_events(host, &eEvent) > 0) // check server events
		{
			switch (eEvent.type)
			{
				case ENET_EVENT_TYPE_CONNECT:
				{
					enet_peer_timeout(eEvent.peer, 0, kConnectionPatienceMilliseconds, kConnectionPatienceMilliseconds);

					if (AvP.Network == I_Host && (size_t)(eEvent.peer - host->peers) <= kMostClients)
					{
						connectionStartedAt[eEvent.peer - host->peers] = enet_time_get();
						lastHeardFrom[eEvent.peer - host->peers] = enet_time_get();
						bytesUsed[eEvent.peer - host->peers] = 0;
						messagesUsed[eEvent.peer - host->peers] = 0;
					}

					// A joiner through the relay has connected through its port there: the host's hellos on that port can stop
					if (PeerIsThroughRelay(eEvent.peer))
					{
						relayHost.JoinerConnected(eEvent.peer->address.port);
					}

					// Which connection it is, by number, and never the other machine's address or port: players stream and share pictures of
					// their screens, and an address there would tell anyone watching where that player is (the user's ruling: no IPs)
					std::stringstream sstream;
					sstream << "A player is joining (connection " << (eEvent.peer - host->peers) + 1 << ")";
					NewOnScreenMessage((char*)sstream.str().c_str());

					// A connection that has only just been made hasn't been let in yet, whoever had this place in ENet's list before
					eEvent.peer->data = NULL;
					break;
				}
				case ENET_EVENT_TYPE_RECEIVE:
				{
					// the caller's buffer holds kMostMessageBytes bytes (every message we send fits in it, see
					// Net_Send). The hosts are created with maximumPacketSize the same, so ENet should
					// not hand us a larger packet; drop one anyway instead of overrunning the buffer.
					if (eEvent.packet->dataLength > kMostMessageBytes)
					{
						static unsigned long long droppedPackets = 0;
						if (LogThisDrop(droppedPackets))
						{
							std::stringstream sstream;
							sstream << "Net_Receive - dropped a packet of " << eEvent.packet->dataLength
							        << " bytes (larger than the " << kMostMessageBytes << " byte message buffer; "
							        << droppedPackets << " so far)";
							Con_PrintError(sstream.str());
						}
						enet_packet_destroy(eEvent.packet);
						break;
					}

					// Every message has a header. An empty one comes with no bytes at all, and copying from nothing isn't allowed in
					// C++, even copying nothing. Both readers below would refuse it anyway, so it goes here, before the copy.
					if (eEvent.packet->dataLength < kMessageHeaderSize)
					{
						LogDroppedMessage("a message too short to have a header");
						enet_packet_destroy(eEvent.packet);
						break;
					}

					// copy it in, header and all
					memcpy(messageData, eEvent.packet->data, eEvent.packet->dataLength);

					if (AvP.Network == I_Host ? HostHearsFromClient(eEvent.peer, messageData, eEvent.packet->dataLength) : ClientHearsFromHost(messageData, eEvent.packet->dataLength))
					{
						dataSize = eEvent.packet->dataLength;
					}

					enet_packet_destroy(eEvent.packet);
					break;
				}
				case ENET_EVENT_TYPE_DISCONNECT:
				{
					NewOnScreenMessage("ENet got a disconnection\n");

					// A client has one connection, the host's, so on a client this is the host gone. The log says so, so that a
					// run shows why its game ended.
					if (AvP.Network != I_Host && eEvent.peer == ServerPeer)
					{
						LogString("net: the host's connection closed");
					}

					NetID const id = IDOfConnection(eEvent.peer);

					eEvent.peer->data = NULL;

					if (AvP.Network == I_Host)
					{
						// However it went, that player has gone. One that said so first has already left the game, and this changes nothing for them.
						if (id != 0)
						{
							netSession.nPlayers--;
							Net_PlayerHasGone(id);
						}
					}
					else if (eEvent.peer == ServerPeer)
					{
						ServerPeer = NULL;
						removedByTheHost = eEvent.data == kCoopRemovedByTheHost;

						// A request to join that was never answered yes: the host's reason says which answer it was (version 114)
						if (joinAnswer == kNetJoinAnswerWaiting)
						{
							joinAnswer = eEvent.data == kNetJoinRefusedNo ? kNetJoinAnswerNo : eEvent.data == kNetJoinRefusedBusy ? kNetJoinAnswerBusy : kNetJoinAnswerNoAnswer;
						}
					}

					break;
				}
				case ENET_EVENT_TYPE_NONE:
						return NET_NO_MESSAGES;
					break;
				default:
						return NET_NO_MESSAGES;
					break;
			}
		}
		else
		{
			return NET_NO_MESSAGES;
		}

		return NET_OK;
	}

	// The callers go round until this says there's nothing more, and with no host there never will be. A level can still be running with no host: the console's disconnect command leaves it that way.
	return NET_NO_MESSAGES;
}

// used to send a message to the server only
NetResult Net_SendSystemMessage(uint32_t messageType, NetID fromID, NetID toID, const void *messageData, size_t dataSize)
{ 
	// Only a client has a server to send to. This can come up once for each message that arrives, so the log only says so now and then.
	if (!ServerPeer)
	{
		static unsigned long long nobodyToSendTo = 0;

		if (LogThisDrop(nobodyToSendTo))
		{
			std::stringstream sstream;
			sstream << "Net_SendSystemMessage - no server to send " << Net_MessageTypeToString(messageType) << " to (" << nobodyToSendTo << " so far)";
			Con_PrintError(sstream.str());
		}

		return NET_FAIL;
	}

	return SendSystemMessageTo(ServerPeer, messageType, fromID, toID, messageData, dataSize);
}

NetResult Net_Send(uint32_t messageType, NetID fromID, NetID toID, uint8_t *messageData, size_t dataSize)
{
	if (messageData == NULL)
	{
		Con_PrintError("Net_Send - messageData was NULL");
		return NET_FAIL;
	}

	// There's no host before a network game starts, after one ends, or if one couldn't start, and ENet would read through
	// the empty pointer. This can come up once for each message the game wants to send, so the log only says so now and then.
	if (!host)
	{
		static unsigned long long noGameToSendIn = 0;

		if (LogThisDrop(noGameToSendIn))
		{
			std::stringstream sstream;
			sstream << "Net_Send - no network game to send " << Net_MessageTypeToString(messageType) << " in (" << noGameToSendIn << " so far)";
			Con_PrintError(sstream.str());
		}

		return NET_FAIL;
	}

	MessageHeader newMessageHeader;

	// It has gaps between its parts, and the whole of it goes out, so nothing left on the stack goes with it
	memset(&newMessageHeader, 0, sizeof(newMessageHeader));
	newMessageHeader.marker = kMarker;
	newMessageHeader.isSystemMessage = 0;
	newMessageHeader.messageType = messageType;
	newMessageHeader.fromID = fromID;
	newMessageHeader.toID   = toID;

	MemoryWriteStream ws(packetBuffer, kPacketBufferSize);
	
	ws.PutBytes(&newMessageHeader, sizeof(newMessageHeader));
	ws.PutBytes(messageData, dataSize);

	// create ENet packet
	ENetPacket *packet = enet_packet_create(packetBuffer, ws.GetBytesWritten(), ENET_PACKET_FLAG_RELIABLE);
	if (packet == NULL)
	{
		Con_PrintError("Net_Send - couldn't create packet");
		return NET_FAIL;
	}

	// A host sends to everyone it has let in, and a client's only connection is to its host, which passes it on
	if (AvP.Network == I_Host)
	{
		SendToClients(packet, NULL);
	}
	else
	{
		enet_host_broadcast(host, 0, packet);
	}

	return NET_OK;
}

NetResult Net_ConnectToSession(int sessionNumber, char *playerName)
{
	printf("Net_ConnectToSession\n");
	OutputDebugString("Step 1: Connect To Session\n");

	if (host) {
		enet_host_destroy(host);
		host = NULL;
	}

	// The ENet host owned this, and has just freed it
	ServerPeer = NULL;

	// Whatever this was belonged to the last game. The host gives this game's.
	AvPNetID = 0;
	hostPlayerID = 0;
	removedByTheHost = false;
	joinAnswer = kNetJoinAnswerNone;
	relayJoinedThrough = false;
	relayJoinedRegion[0] = 0;

	ServerAddress.host = SessionData[sessionNumber].host;
	ServerAddress.port = SessionData[sessionNumber].port;

	// create ENet client. It has no address of its own, because nobody needs to find it: it talks to its host, and gets whatever port is free. That also means two games on one machine don't fight over a port.
	// One connection, to the host, with the game's limits: see net_host.h
	host = Net_MakeENetHost(NULL, 1, kMostMessageBytes, incomingBandwidth, outgoingBandwidth);

	if (host == NULL)
	{
		Con_PrintError("Net_ConnectToSession - Failed to create Enet client");
		return NET_FAIL;
	}

	// Modified 2026-11-08 by Melanikus for the AvP port: a game from the relays' list. The relay's handshake goes first, from this new
	// host's own socket, and then the game connects to its port on the relay, as if that were the host (net_relay.h).
	if (SessionData[sessionNumber].throughRelay)
	{
		uint16_t const relayPort = RelayJoin(SessionData[sessionNumber].relayRegion, SessionData[sessionNumber].relayCode);

		if (relayPort == 0)
		{
			Con_PrintError(std::string("Net_ConnectToSession - couldn't join through the relay: ") + Net_RelayJoinFailedWhy());
			enet_host_destroy(host);
			host = NULL;
			return NET_FAIL;
		}

		ServerAddress = relayAddress;
		ServerAddress.port = relayPort;

		// For the line in play saying which relay this game goes through (Net_RelayPlayStateNow)
		relayJoinedThrough = true;
		strncpy(relayJoinedRegion, SessionData[sessionNumber].relayRegion, kRelayListRegionLetters);
		relayJoinedRegion[kRelayListRegionLetters] = 0;
	}

	ServerPeer = enet_host_connect(host, &ServerAddress, kNetChannels, 0);
	if (ServerPeer == NULL)
	{
		Con_PrintError("Net_ConnectToSession - Failed to connect to server host");
		enet_host_destroy(host);
		host = NULL;
		return NET_FAIL;
	}

	enet_peer_timeout(ServerPeer, 0, kConnectionPatienceMilliseconds, kConnectionPatienceMilliseconds);

	ENetEvent eEvent;

	/* see if we actually connected */
	/* Wait up to 3 seconds for the connection attempt to succeed. */
	if ((enet_host_service(host, &eEvent, 3000) > 0) && (eEvent.type == ENET_EVENT_TYPE_CONNECT))
	{
		Con_PrintDebugMessage("Net_ConnectToSession - we connected to server!");
	}
	else
	{
		Con_PrintError("Net_ConnectToSession - failed to connect to server!");

		// There's no game, so nothing should stay open
		enet_host_destroy(host);
		host = NULL;
		ServerPeer = NULL;
		return NET_FAIL;
	}

	if (!Net_CreatePlayer(playerName)) {
		return NET_FAIL;
	}

	InitAVPNetGameForJoin();

	// From here on, before anything has been read from the host
	Net_CoopLobbyJoined(SessionData[sessionNumber].coopLobby);

	netGameData.levelNumber = SessionData[sessionNumber].levelIndex;
	netGameData.joiningGameStatus = JOINNETGAME_WAITFORDESC;

	return NET_OK;
}

// this function should be called for the host only?
static bool Net_CreatePlayer(char *playerName)
{
	printf("Net_CreatePlayer\n");
	memset(&thisClientPlayer, 0, sizeof(PlayerDetails));

	if (AvP.Network == I_Host) {
		thisClientPlayer.ID = Net_GetNextPlayerID();
		AvPNetID = thisClientPlayer.ID;
	}

	thisClientPlayer.type = NET_PLAYERTYPE_PLAYER;
	thisClientPlayer.version = kMultiplayerVersion;
	strncpy(thisClientPlayer.name, playerName, kPlayerNameSize - 1);
	thisClientPlayer.name[kPlayerNameSize-1] = 0;

	// don't do this as the host
	if (AvP.Network != I_Host)
	{
		OutputDebugString("Net_CreatePlayer - Sending player struct\n");

		// send struct as system message
		// FIXME - to and from IDs
		if (Net_SendSystemMessage(NET_CREATEPLAYERORGROUP, 0, 0, &thisClientPlayer, sizeof(PlayerDetails)) != NET_OK)
		{
			Con_PrintError("Net_CreatePlayer - Problem sending create player system message!");
			return false;
		}
		else {
			Con_PrintMessage("Net_CreatePlayer - sent create player system message!");

			// The request to join has gone: now the host's answer is waited for (version 114)
			joinAnswer = kNetJoinAnswerWaiting;
			joinAskedAt = enet_time_get();
		}
	}
	return true;
}

// Only a host hands these out. It's a count from 1, so that no two players ever have the same one, and 0 means nobody. Nothing relies on anyone being unable to guess one: everyone in the game can see them all, and the host goes by the connection.
NetID Net_GetNextPlayerID()
{
	static NetID lastPlayerID = 0;

	lastPlayerID++;
	return lastPlayerID;
}

// Asks whoever is at this address about their game.
static bool AskAboutGames(enet_uint32 host)
{
	ENetAddress destAddress;
	destAddress.host = host;
	destAddress.port = broadcastPort;

	// create broadcast header
	MessageHeader newHeader;

	// It has gaps between its parts, and the whole of it goes out, so nothing left on the stack goes with it
	memset(&newHeader, 0, sizeof(newHeader));
	newHeader.marker = kMarker;
	newHeader.isSystemMessage = true;
	newHeader.messageType = NETSYS_REQUEST_SESSION_INFO;
	newHeader.fromID      = kBroadcastID;
	newHeader.toID        = kBroadcastID;

	// The header, then nothing, to as many bytes as the answer it asks for: see kSearchQuestionBytes
	uint8_t question[kSearchQuestionBytes];

	memset(question, 0, sizeof(question));
	memcpy(question, &newHeader, sizeof(newHeader));

	ENetBuffer buf;
	buf.data = question;
	buf.dataLength = sizeof(question);

	return enet_socket_send(broadcastSocket, &destAddress, &buf, 1) >= 0;
}

// Everyone on this network, and the address the player typed, if it's one. False if the network can't be asked at all.
static bool AskEveryoneAboutGames()
{
	extern char IPAddressString[];

	if (!AskAboutGames(ENET_HOST_BROADCAST))
	{
		Con_PrintError("Couldn't send broadcast packet");
		return false;
	}

	if (NetTypedAddressIsGood(IPAddressString, kMostTypedAddressLetters))
	{
		ENetAddress typed;

		if (enet_address_set_host_ip(&typed, IPAddressString) == 0 && !AskAboutGames(typed.host))
		{
			Con_PrintError("Couldn't ask the address that was typed");
		}
	}

	return true;
}

void Net_FindAvPSessions()
{
	// stop this function being called if we're already searching.
	// this is a bit hacky but needs to be here for the moment due to
	// the horrible menu system
	if (sessionCheckTimer != 0) {
		return;
	}

	if (!OpenDiscoverySocket())
	{
		return;
	}

	// A new search starts with an empty list, or games long gone would stay on it, and it only holds so many
	NumberOfSessionsFound = 0;
	sessionLastAskedAt = timeGetTime();

	if (!AskEveryoneAboutGames())
	{
		Net_StopSearchingForSessions();
		return;
	}

	sessionCheckTimer = timeGetTime();
}

static NetResult Net_CreateSession(const char *sessionName, const char *levelName, int maxPlayers, int version, uint16_t gameStyle, uint16_t level)
{
	printf("Net_CreateSession\n");
	memset(&netSession, 0, sizeof(netSession));

#ifdef _WIN32
	CoCreateGuid(&netSession.guidInstance);
#endif

	// set the application guid
	netSession.guidApplication = AvPGuid;

	netSession.host = ServerAddress.host;
	netSession.port = ServerAddress.port;
	netSession.version = version;
	netSession.gameStyle = gameStyle;
	netSession.level     = level;
	// should this be done here? we might want a dedicated server. perhaps a host should be 'connecting' to themselves
	netSession.nPlayers  = 1;
	netSession.maxPlayers = maxPlayers;

	strncpy(netSession.levelName  , levelName,   sizeof(netSession.levelName)   - 1);
	strncpy(netSession.sessionName, sessionName, sizeof(netSession.sessionName) - 1);

	return NET_OK;
}

void Net_SayWhatTheGameIs(uint16_t level, const char *levelName, const char *sessionName, uint8_t coopLobby)
{
	netSession.level = level;
	netSession.coopLobby = coopLobby;

	// Each is cleared first, so nothing of a longer old name stays after the end of a shorter new one
	memset(netSession.levelName, 0, sizeof(netSession.levelName));
	memset(netSession.sessionName, 0, sizeof(netSession.sessionName));
	strncpy(netSession.levelName, levelName, sizeof(netSession.levelName) - 1);
	strncpy(netSession.sessionName, sessionName, sizeof(netSession.sessionName) - 1);
}

NetResult Net_UpdateSessionDescForLobbiedGame(uint16_t gameStyle, uint16_t level)
{
	return NET_OK;
}

// Each connection, by number and round trip, for a tester's mark (networking.h). The number is its place in ENet's list, the same one the
// screen names when a player joins, and never an address.
void Net_DescribeConnections(char *words, size_t size)
{
	if (size == 0)
	{
		return;
	}

	words[0] = '\0';

	if (!host || AvP.Network == I_No_Network)
	{
		return;
	}

	size_t used = 0;

	for (size_t i = 0; i < host->peerCount && used + 1 < size; i++)
	{
		ENetPeer const &peer = host->peers[i];

		if (peer.state != ENET_PEER_STATE_CONNECTED)
		{
			continue;
		}

		int const added = _snprintf(words + used, size - 1 - used, "%sconnection %u at %u ms", used ? ", " : "", (unsigned)i + 1, (unsigned)peer.roundTripTime);

		if (added < 0)
		{
			break;
		}

		used += (size_t)added;
	}

	words[used < size ? used : size - 1] = '\0';
}
