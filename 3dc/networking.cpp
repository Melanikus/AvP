// Modified 2026-11-07 by Melanikus for the AvP port:
//   the co-op lobby: a game's description says if it has one, and in the lobby nothing is passed on and a silent joiner goes sooner. Credit: Melanikus and Bobblen (the co-op campaign system follows Bobblen's avp-forever, rewritten)
// Modified 2026-11-06 by Melanikus for the AvP port:
//   the switches and the two helpers a Hudson run with net= needs (the lines that use them came with the network step, 2026-11-08, below)
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
//   the settings file's port and speeds are checked, and one the game can't use is put back to the game's own, with a log line
//   the screen says which connection a joining player is, never their address or port
//   Net_DescribeConnections says each connection's number and round trip, never its address, for a tester's mark (mark_key.cpp)
//   every 10 seconds the log says each connection's round trip, loss and resends, by number, and when one starts or stops struggling
//   (net_stats.h)
// Modified 2026-11-08 by Melanikus for the AvP port:
//   the 10-second report also says how this machine's frames went, and how often other players' and NPCs' figures waited for news
//   (net_watch.h)
// Modified 2026-11-08 by Melanikus for the AvP port (Hudson's branch, never pushed):
//   a Hudson run with net= only uses this machine's own address, and logs who joins, what arrives and who leaves
// Modified 2026-11-10 by Melanikus for the AvP port (the owner's ruling for beta.2: "Everything over nyc3"):
//   there's no search for games on this network any more, neither asking nor answering, and no socket for it: every game is found on
//   the server's online list, and played through its relay, sealed (REMOVED-CODE.md says what went)
// Modified 2026-11-09 by Melanikus for the AvP port (Hudson's branch, never pushed):
//   a net= run's "this machine only" also holds through the relay: it uses only a relay on this machine, hosting or joining
// Modified 2026-11-10 by Melanikus for the AvP port (#175):
//   the relay is the one its server's servers.cfg line names, and its keys come proved by that line (relay_list.h), so the -dev setting
//   RelayKey and its checks here are gone

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
#include "net_relay.h"
#include "net_identity.h"
#include "net_identity_run.h"
#include "net_peer_v2.h"
#include "net_relay_v2.h"
#include "net_relay_keys.h"
#include "relay_shared.h"
#include "relay_list.h"
#include "build_identity.h"
#include "net_watch.h"
#include <assert.h>
#include <random>

#include "MemoryStream.h"

static ENetHost *host = NULL;

static ENetAddress  ServerAddress;
static ENetAddress  ClientAddress;
static ENetPeer     *ServerPeer = NULL;
static bool net_IsInitialised = false;

// A Hudson run with net= is two games on this machine, and it must open nothing on any network the machine is on: the host and the client use 127.0.0.1 only,
// and there's no socket for finding games, because the client knows where to go. Only hudson.cpp sets this, before the game's networking starts. See
// tools/hudson/run-net-pair.ps1, which checks what really happened, and ends both games if either has anything open on another address.
bool NetLoopbackOnly = false;

// Whether an address is this machine's own, 127.x.x.x, the only kind a Hudson run with net= may ask. ENet keeps an address's four numbers in the
// order they're sent on the network, and the game only runs on x86, which keeps the first byte lowest, so the 127 is the lowest byte.
static bool IsThisMachinesOwnAddress(enet_uint32 address)
{
	return (address & 0xFF) == 127;
}


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


// How many messages one client may send in a second. A game sends one a frame, and this is far above any frame rate that matters. Past it they're dropped, so a flood can't use the host to reach everyone else.
static const int kMostMessagesEachSecond = 1000;
static unsigned int messagesSecond[kMostClients + 1];
static int messagesUsed[kMostClients + 1];

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

static uint32_t incomingBandwidth = 0;
static uint32_t outgoingBandwidth = 0;

SessionDescription netSession; // main game session

static void LogDroppedMessage(const char *why);   // further down: a game on the relays' list that can't be joined is logged as dropped

// Modified 2026-11-08 by Melanikus for the AvP port: THE RELAY (net_relay.h, and AVPx Dedicated Server's PROTOCOL.md at 7014de1).
// There's a relay in each region of the world, and the list (relay_list.h) names them. A host times each with a few echoes, opens a room on
// the nearest, and its game goes on the list; every joiner comes through that relay, each from a port of its own there, so a player nobody can
// reach from the internet can still play. The relays' own little messages arrive on the game's own socket, so RelayIntercept takes them off it
// before ENet reads anything, and ENet only ever sees the game's packets. They're sent from that same socket too (RelayV2SendAll), so a relay
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

// ---- PROTOCOL VERSION 2: THE HOST AND EACH JOINER SEAL THEIR GAME END TO END. Added 2026-11-09 by Melanikus for the AvP port.
// (CRYPTO.md sections 8 and 9, in the AVPx-Dedicated-Server repository; net_peer_v2.h does the work, and this is where the game meets it.)
// Through a relay, the host and each joiner run handshake B over the joiner's own port on the relay, and from then on every datagram ENet
// sends between them is sealed, so the relay only passes on what it can neither read nor change. The joiner checks the host is the one
// the list named (by the whole id the listing carries: SERVER-BLOCKERS item 6). The relay's own small messages ('AVPR') are as they were.
// The rules, Primary's three among them (agreed 2026-11-09):
//   - No game byte ever goes to the relay unsealed. Once a game has used a relay, ENet's every datagram to the relay's address goes
//     through RelaySealedSend, which seals it or drops it; nothing else sends ENet's datagrams there.
//   - A handshake that fails ends the join, with the player told why. There's no going back to plain datagrams.
//   - A host starts a handshake only for a port the relay has named (NEW_JOINER, which the relay sends only once the joiner has said hello
//     there): a datagram from any other port starts nothing.
//   - Nothing from a joiner's port reaches ENet but a sealed datagram that opened.
// Over this network (a LAN game), nothing changes.
static NetIdentity runIdentity;                                    // this run's identity, made ready the first time a relay is used
static bool runIdentityLookedFor = false;                          // whether the kept identity has been looked for since the network started
static NetIdentityStoreResult runIdentityKept = kNetIdentityStoreNoFile;   // and what that found (net_identity_run.h)
static NetPeerJoiner relayEndToEndJoiner;                         // a joiner's one handshake, with its host
static uint16_t relayEndToEndPort = 0;                             // and its port on the relay, once it has one; 0 for none
static NetPeerHostSide relayEndToEndHosts[kNetRelayMostJoiners];   // a host's handshakes, one for each joiner's port
static bool relaySealing = false;                                  // this game's ENet sends through RelaySealedSend (set by the relay's use)
static uint8_t relayOpened[kRelayMostPacketBytes];                 // a game datagram just opened, for ENet to read in its place
static uint32_t relayEndToEndDropped = 0;                          // ENet's datagrams dropped on their way out because nothing could seal them
// Added 2026-11-09 by Melanikus for the AvP port: how many of the game's datagrams went out sealed through the relay, and how many came in
// and opened, for the log's line at the game's end. A count of what happened, never what was in them.
static uint32_t relayEndToEndSealed = 0;
static uint32_t relayEndToEndOpened = 0;
static_assert(kNetRelayCheckCodeBytes == kNetHandshakeBCheckCodeBytes, "networking.h's check code is as long as net_handshake_b.h's");
static NetPeerStartLimit relayEndToEndStarts;                      // how many handshakes a host starts, per named port and per second (Primary's finding 1)

// This run's identity, made ready once (net_identity_run.h). Modified 2026-11-09 by Melanikus for the AvP port (#21): the one kept from
// earlier runs, sealed by Windows for this user (net_identity_store.h); on the first run, a new one, made from the operating system's
// randomness and kept. A kept one that can't be used is never replaced here: only the player's yes does that (Net_MakeNewIdentity), so
// friends and an approval never see a new fingerprint the player didn't choose. False if there's none, and then nothing goes through a
// relay. The log says what was found, in the store's own fixed words: never a fingerprint, never anything from the file.
static bool RelayIdentityReady()
{
	if (runIdentity.made)
	{
		return true;
	}

	if (runIdentityLookedFor)
	{
		return false;
	}

	runIdentityLookedFor = true;
	const char *why = NULL;
	runIdentityKept = NetIdentityStore_Load(&runIdentity, &why);

	if (runIdentityKept == kNetIdentityStoreLoaded)
	{
		LogString("net: this player's identity is the one kept from earlier runs");
		return true;
	}

	LogString(std::string("net: the kept identity: ") + NetIdentityStore_Words(runIdentityKept) + (why ? std::string(" (") + why + ")" : std::string()));

	if (runIdentityKept != kNetIdentityStoreNoFile)
	{
		LogString("net: so there's no identity this run until the player chooses to make a new one, and nothing goes through a relay");
		return false;
	}

	if (!NetIdentityStore_MakeAndKeep(&runIdentity))
	{
		LogString("net: a new identity couldn't be made and kept, so there's none this run, and nothing goes through a relay");
		return false;
	}

	LogString("net: a new identity was made for this player and kept");
	return true;
}

// Primary's first condition: the relay's own messages, as version 1 reads them, go through this one function, which version 2's sealed
// control channel will replace. True if it was one of them.
static bool RelayControlDatagram(uint16_t fromPort, const uint8_t *data, size_t size, uint32_t now)
{
	return relayHosting ? relayHost.Hear(fromPort, data, size, now) : relayJoiner.Hear(fromPort, data, size, now);
}

// A host's handshake for a joiner's port: the one there is, or, with mayStart, a new one if the relay has named that port (NEW_JOINER).
// NULL otherwise. A side that failed, or that has no port, is free for a new one.
static NetPeerHostSide *RelayEndToEndSide(uint16_t port, bool mayStart, uint32_t now)
{
	NetPeerHostSide *unused = NULL;

	for (size_t i = 0; i < kNetRelayMostJoiners; i++)
	{
		NetPeerHostSide &side = relayEndToEndHosts[i];

		if (port != 0 && side.Port() == port && !side.Failed())
		{
			return &side;
		}

		if (!unused && (side.Port() == 0 || side.Failed()))
		{
			unused = &side;
		}
	}

	if (!mayStart || !unused || !relayHost.NamedJoinerPort(port) || !RelayIdentityReady())
	{
		return NULL;
	}

	// Each start costs the host a B2, and a failed side frees its place: so a port starts only a few, and the host only a few a second
	// (net_peer_v2.h, NetPeerStartLimit). One that's refused is dropped as a stranger's.
	if (!relayEndToEndStarts.Allow(port, now))
	{
		return NULL;
	}

	return unused->Start(relayRegion, port, &runIdentity, now) ? unused : NULL;
}

// Sends what a handshake wants sent, as it is (its datagrams are the handshake's own, 'AVPE'), to that joiner's port on the relay
template <typename Peer>
static void RelayEndToEndSendAll(ENetHost *from, Peer &peer, uint16_t port)
{
	NetPeerSend sends[kNetPeerMostSends];
	size_t const count = peer.TakeSends(sends, kNetPeerMostSends);

	for (size_t i = 0; i < count && from && port != 0; i++)
	{
		ENetAddress to = relayAddress;
		to.port = port;
		ENetBuffer buffer;
		buffer.data = sends[i].bytes;
		buffer.dataLength = sends[i].size;
		enet_socket_send(from->socket, &to, &buffer, 1);
	}
}

// A joiner through the relay has gone (its connection closed): its handshake ends, its keys wiped, and its place is free
static void RelayEndToEndJoinerLeft(uint16_t port)
{
	for (size_t i = 0; i < kNetRelayMostJoiners; i++)
	{
		if (port != 0 && relayEndToEndHosts[i].Port() == port)
		{
			relayEndToEndHosts[i].Reset();
		}
	}
}

static void RelayV2Forget();   // further down: the channel to the relay (step 7)
static void RelayV2Hear(uint16_t fromPort, const uint8_t *data, size_t size, uint32_t now);
static void RelayV2JoinerFrame();
static bool HasMagic(const uint8_t *data, size_t size, const char magic[5]);
static NetID IDOfConnection(const ENetPeer *peer);   // further down, beside the game's own messages
extern void Net_PlayerHasGone(NetID id);

// Whether this game has used a channel to its relay (step 7; added 2026-11-09 by Melanikus for the AvP port, Secondary's finding
// 2.1). Once it has, the relay is heard sealed-only until the game ends, even after the channel itself has ended, so a plain version 1
// message from it is never read again. RelayV2Begin sets it, and only the game's end (RelayEndToEndForget) clears it.
static bool relayV2Used = false;

// Every handshake is ended, its keys wiped: when a game through a relay ends, or the relay is given up. Modified 2026-11-09 by Melanikus
// for the AvP port: the channel to the relay too (step 7), so whatever ends a game through the relay ends that as well. And version 1's
// two machines, which hold the room's token and pass, or a joiner's ticket (Secondary's finding 4.3): v2 only ever sent those sealed, so
// they're secrets, and they open nothing once the room or the join is over.
static void RelayEndToEndForget()
{
	RelayV2Forget();
	relayV2Used = false;
	relayHost.Reset();
	relayJoiner.Reset();
	relayEndToEndJoiner.Reset();
	relayEndToEndPort = 0;

	for (size_t i = 0; i < kNetRelayMostJoiners; i++)
	{
		relayEndToEndHosts[i].Reset();
	}

	relayEndToEndStarts.Reset();
}

// A datagram from a joiner's port on the relay, that wasn't the relay's own: a handshake datagram, or a sealed game datagram. 0 with an
// opened game datagram put in its place, for ENet to read; 1 for anything else, which ENet never sees.
static int RelayEndToEndHeard(ENetHost *on, uint32_t now)
{
	size_t opened = 0;
	uint16_t const port = on->receivedAddress.port;

	if (relayHosting)
	{
		NetPeerHostSide *const side = RelayEndToEndSide(port, true, now);

		if (!side)
		{
			relayStrangersDropped++;
			return 1;
		}

		// The moment a joiner's handshake B is done on this side (its B5 checked), the log says so, once (added 2026-11-09 by Melanikus for
		// the AvP port): fixed words, with no key, code or address
		bool const wasReady = side->Ready();
		side->Hear(on->receivedData, on->receivedDataLength, relayOpened, sizeof(relayOpened), &opened, now);
		RelayEndToEndSendAll(on, *side, port);

		if (!wasReady && side->Ready())
		{
			LogString("net: a joiner's handshake B is done: from here its game is sealed end to end");
		}
	}
	else
	{
		if (relayEndToEndPort == 0 || port != relayEndToEndPort)
		{
			relayStrangersDropped++;
			return 1;
		}

		relayEndToEndJoiner.Hear(on->receivedData, on->receivedDataLength, relayOpened, sizeof(relayOpened), &opened, now);
	}

	if (opened == 0)
	{
		return 1;
	}

	relayEndToEndOpened++;
	on->receivedData = relayOpened;
	on->receivedDataLength = opened;
	return 0;
}

// ENet's send hook (our ENet's patch 007): every datagram ENet sends comes here instead of going to the socket. One for anywhere but the
// relay goes as it always did. One for the relay is sealed for the joiner's port it goes to, or, if nothing there can seal it yet, dropped
// (ENet counts it lost, and sends again later). Answers as enet_socket_send does.
static int ENET_CALLBACK RelaySealedSend(ENetHost *from, const ENetAddress *address, const ENetBuffer *buffers, size_t bufferCount)
{
	if (!from || !address || (!buffers && bufferCount > 0))
	{
		return -1;
	}

	if (address->host != relayAddress.host)
	{
		return enet_socket_send(from->socket, address, buffers, bufferCount);
	}

	static uint8_t plain[kNetSealedGameMostTextBytes];
	static uint8_t sealed[kNetSealedGameMostBytes];
	size_t total = 0;

	for (size_t i = 0; i < bufferCount; i++)
	{
		total += buffers[i].dataLength;
	}

	if (total > sizeof(plain))
	{
		relayEndToEndDropped++;
		return static_cast<int>(total);
	}

	size_t at = 0;

	for (size_t i = 0; i < bufferCount; i++)
	{
		memcpy(plain + at, buffers[i].data, buffers[i].dataLength);
		at += buffers[i].dataLength;
	}

	uint32_t const now = enet_time_get();
	size_t written = 0;
	bool done = false;

	if (relayHosting)
	{
		NetPeerHostSide *const side = RelayEndToEndSide(address->port, false, now);
		done = side && side->Seal(plain, total, sealed, sizeof(sealed), &written, now);
	}
	else if (relayEndToEndPort != 0 && address->port == relayEndToEndPort)
	{
		done = relayEndToEndJoiner.Seal(plain, total, sealed, sizeof(sealed), &written, now);
	}

	NetCrypto_Wipe(plain, total);

	if (!done || written == 0)
	{
		relayEndToEndDropped++;
		return static_cast<int>(total);
	}

	ENetBuffer buffer;
	buffer.data = sealed;
	buffer.dataLength = written;
	int const sent = enet_socket_send(from->socket, address, &buffer, 1);
	relayEndToEndSealed += sent < 0 ? 0 : 1;
	return sent < 0 ? sent : static_cast<int>(total);
}

// Why a joiner's handshake B ended, in the game's own fixed words, for the player (CRYPTO.md section 16's numbers): never the host's words
static const char *RelayEndToEndWhyText(int why)
{
	switch (why)
	{
		case kNetHandshakeBWrongHost: return "this isn't the host the list named";
		case kNetPeerNoAnswer: return "the host doesn't answer";
		case kNetSealedGameTooManyBad: return "the connection was attacked, and closed";
		default: return "the host's game couldn't be checked";
	}
}

// Set when a join's handshake B failed, for Net_RelayJoinFailedWhy; NULL otherwise
static const char *relayEndToEndJoinFailed = NULL;

// The joiner's handshake B with its host, over its port on the relay, after the relay's READY and before ENet sends a byte. NULL when the
// keys are in; otherwise the game's own words for why not. While it waits, the window keeps answering and the player can give up (the
// menu's hook), as in the relay's own handshake. The joiner's schedule gives up after 7.5 s (net_peer_v2.h); this loop ends a little past
// that by the clock, and by its number of turns in case the clock stands still.
static const char *RelayEndToEndJoin(const char *region, uint16_t port, const uint8_t hostId[kNetIdentityIdBytes])
{
	if (!host || port == 0 || !RelayIdentityReady())
	{
		return RelayEndToEndWhyText(kNetHandshakeBBadStart);
	}

	uint32_t const started = enet_time_get();
	relayEndToEndPort = port;
	relaySealing = true;
	host->sendHook = RelaySealedSend;

	if (!relayEndToEndJoiner.Start(region, port, hostId, &runIdentity, started))
	{
		return RelayEndToEndWhyText(relayEndToEndJoiner.Why());
	}

	static uint8_t packet[kRelayMostPacketBytes];
	uint32_t const mostMs = 9000;
	size_t const mostTurns = mostMs / kRelayWaitMs + 100;

	for (size_t turn = 0; turn < mostTurns && !relayEndToEndJoiner.Ready() && !relayEndToEndJoiner.Failed() && enet_time_get() - started < mostMs; turn++)
	{
		if (relayJoinWaitHook && !relayJoinWaitHook())
		{
			return NetRelay_WhyText(kNetRelayGivenUp);
		}

		RelayEndToEndSendAll(host, relayEndToEndJoiner, port);
		RelayV2JoinerFrame();   // the channel to the relay keeps its turn meanwhile (step 7, added 2026-11-09 by Melanikus for the AvP port)
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

			uint32_t const now = enet_time_get();

			// From anywhere but the relay: dropped. The relay's own (a READY sent again): its machine's. From another port: dropped.
			if (from.host != relayAddress.host)
			{
				relayStrangersDropped++;
				continue;
			}

			// Modified 2026-11-09 by Melanikus for the AvP port (step 7): the relay's own messages come through the channel to it, opened
			// ('AVPR'); a plain version 1 message is never read here, and goes on to B, which drops what isn't its own
			if (HasMagic(packet, static_cast<size_t>(got), "AVPR"))
			{
				RelayV2Hear(from.port, packet, static_cast<size_t>(got), now);
				continue;
			}

			if (from.port != port)
			{
				relayStrangersDropped++;
				continue;
			}

			// No game datagram comes before ENet has connected, so one that opens here is dropped with the rest
			size_t opened = 0;
			relayEndToEndJoiner.Hear(packet, static_cast<size_t>(got), relayOpened, sizeof(relayOpened), &opened, now);
		}

		relayEndToEndJoiner.Tick(enet_time_get());
	}

	// B5, the moment B4 has checked, before ENet's first datagram
	RelayEndToEndSendAll(host, relayEndToEndJoiner, port);

	if (relayEndToEndJoiner.Ready())
	{
		LogString("net: handshake B with the host is done: from here the game is sealed end to end");   // fixed words (added 2026-11-09 by Melanikus for the AvP port)
		return NULL;
	}

	return relayEndToEndJoiner.Failed() ? RelayEndToEndWhyText(relayEndToEndJoiner.Why()) : RelayEndToEndWhyText(kNetPeerNoAnswer);
}

// Each frame, beside RelayHostFrame: the handshakes' resends and timers, and what they want sent. A host's side that has failed is ended,
// so its keys go and its place is free.
static void RelayEndToEndFrame()
{
	if (!host || !relaySealing)
	{
		return;
	}

	uint32_t const now = enet_time_get();

	if (relayEndToEndPort != 0)
	{
		relayEndToEndJoiner.Tick(now);
		RelayEndToEndSendAll(host, relayEndToEndJoiner, relayEndToEndPort);
	}

	// A port the relay doesn't name any more (its joiner connected, or the relay gave it up) starts afresh the next time it's named
	for (size_t i = 0; i < kNetPeerStartLimitPorts && relayHosting; i++)
	{
		uint16_t const counted = relayEndToEndStarts.PortAt(i);

		if (counted != 0 && !relayHost.NamedJoinerPort(counted))
		{
			relayEndToEndStarts.Forget(counted);
		}
	}

	for (size_t i = 0; i < kNetRelayMostJoiners; i++)
	{
		NetPeerHostSide &side = relayEndToEndHosts[i];

		if (side.Port() == 0)
		{
			continue;
		}

		side.Tick(now);
		RelayEndToEndSendAll(host, side, side.Port());

		if (side.Failed())
		{
			side.Reset();
		}
	}
}

// Hudson's branch, for server #14's run through a relay on another machine (2026-11-09). A run with net= opens nothing beyond this machine,
// with one exception the run asks for by its address: the one relay it was given (Net_AllowOneRemoteRelay, from Hudson's remoterelay= word).
// Modified 2026-11-10 by Melanikus for the AvP port (#175): the relay is always at its server's servers.cfg line's address (relay_list.h),
// which is never a name, so the run is given that address, and a name is refused.
//  - That relay is used at that one address, and at no other (TheRemoteRelay).
//  - The run's sockets then go on this machine's address that Windows sends to that relay from, never on 0.0.0.0 (LoopbackOnlyAddress).
//  - Every socket there drops whatever comes from anywhere but that relay, before ENet reads a byte of it (RelayIntercept).
// tools/hudson/run-net-pair.ps1 -RemoteRelay checks from outside the game that nothing else is open.
static uint32_t remoteRelayGiven = 0;      // the relay's address the run was given, as ENetAddress.host keeps it; 0: no relay beyond this machine
static uint32_t remoteRelayAddress = 0;    // the same address, once the list has named a relay there (for the run's result); 0 until then
static uint32_t remoteRelayRoute = 0;      // this machine's address towards it, where the run's sockets go; 0 until it's been found

bool Net_AllowOneRemoteRelay(const char *address)
{
	remoteRelayGiven = 0;
	remoteRelayAddress = 0;
	remoteRelayRoute = 0;

	// An empty address takes back any given before: this machine only, as a run with net= always was (a test's way to start again)
	if (address && !address[0])
	{
		return true;
	}

	// One plain IPv4 address and nothing else, read by the same rule as a servers.cfg line's (relay_list_text.h)
	uint32_t given = 0;

	if (!address || !RelayList_ReadIPv4(address, &given))
	{
		return false;
	}

	// As ENet keeps an address: the first number in the lowest byte, as x86 lays out network order (see IsThisMachinesOwnAddress)
	uint32_t const asENetKeepsIt = ((given >> 24) & 0xFFu) | ((given >> 8) & 0xFF00u) | ((given << 8) & 0xFF0000u) | ((given << 24) & 0xFF000000u);

	// "Any address" (0.0.0.0) and "everyone on this network" (255.255.255.255) are never one relay
	if (asENetKeepsIt == ENET_HOST_ANY || asENetKeepsIt == ENET_HOST_BROADCAST)
	{
		return false;
	}

	remoteRelayGiven = asENetKeepsIt;
	return true;
}

// The one address the run let through, for hudson.cpp to write in the run's result (this file never turns an address into text); 0 if none
uint32_t Net_RemoteRelayAddress()
{
	return remoteRelayAddress;
}

// Whether a relay from the list is the one the run was given: at the address it was given, and no other
static bool TheRemoteRelay(const RelayListRelay &relay)
{
	if (remoteRelayGiven == 0 || relay.address != remoteRelayGiven)
	{
		return false;
	}

	if (remoteRelayAddress == 0)
	{
		remoteRelayAddress = relay.address;
		LogString("net: a Hudson run with net= uses the one relay it was given, at the address it was given (the run's result says which)");
	}

	return true;
}

// Where a socket of a run with net= goes: this machine's own address, 127.0.0.1, unless the run was given a relay and the list has named
// it. Then it's this machine's address that Windows sends to that relay from, found by connecting a UDP socket to the relay, which sends
// nothing at all, and asking the socket where it is. If that can't be found, it stays 127.0.0.1, and the relay just can't be reached.
static void LoopbackOnlyAddress(ENetAddress *address)
{
	enet_address_set_host_ip(address, "127.0.0.1");

	if (remoteRelayGiven == 0)
	{
		return;
	}

	if (remoteRelayRoute == 0)
	{
		RelayListRelay relays[kRelayListMostRelays];
		int named = 0;

		// The list's relays as they are now, asked once
		if (!RelayList_Relays(relays, &named))
		{
			named = 0;
		}

		for (int i = 0; i < named && i < static_cast<int>(kRelayListMostRelays); i++)
		{
			if (relays[i].port < 1 || relays[i].port > 65535 || !TheRemoteRelay(relays[i]))
			{
				continue;
			}

			ENetSocket const probe = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
			ENetAddress to;
			to.host = remoteRelayAddress;
			to.port = static_cast<uint16_t>(relays[i].port);
			ENetAddress mine;
			mine.host = ENET_HOST_ANY;
			mine.port = 0;

			if (probe != ENET_SOCKET_NULL && enet_socket_connect(probe, &to) == 0 && enet_socket_get_address(probe, &mine) == 0
				&& mine.host != ENET_HOST_ANY && mine.host != ENET_HOST_BROADCAST)
			{
				remoteRelayRoute = mine.host;
			}

			if (probe != ENET_SOCKET_NULL)
			{
				enet_socket_destroy(probe);
			}

			break;
		}
	}

	if (remoteRelayRoute != 0)
	{
		address->host = remoteRelayRoute;
	}
}

// Whether a run's socket is on the way to the relay beyond this machine, and so must hear that relay alone
static bool OnTheRemoteRoute(const ENetAddress &address)
{
	return remoteRelayRoute != 0 && address.host == remoteRelayRoute;
}

// One relay from the list, as an address for ENet: its control port, and its address. Modified 2026-11-10 by Melanikus for the AvP port
// (#175): the relay is at its server's servers.cfg line's address, which the line's reader held to the address rule (servers_cfg.h), so
// nothing here waits for a name service. This checks the two that must never be sent to again, in case that rule ever changes: "any
// address" (0.0.0.0) and "everyone on this network" (255.255.255.255). False for those, or for a port that isn't one.
static bool RelayLookUp(const RelayListRelay &relay, ENetAddress *out)
{
	if (relay.port < 1 || relay.port > 65535 || relay.address == ENET_HOST_ANY || relay.address == ENET_HOST_BROADCAST)
	{
		return false;
	}

	// Hudson's branch: a run with net= opens nothing beyond this machine, so it uses only a relay on this machine (127.x), hosting or
	// joining, or the one relay beyond it that the run was given by its address (TheRemoteRelay, above). A relay anywhere
	// else is left out here, before anything is sent to it.
	if (NetLoopbackOnly && !IsThisMachinesOwnAddress(relay.address) && !TheRemoteRelay(relay))
	{
		LogString("net: a Hudson run with net= uses only a relay on this machine, so a relay elsewhere is left out");
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

// ---- PROTOCOL VERSION 2'S CHANNEL TO THE RELAY: handshake A, then the sealed control channel (step 7; CRYPTO.md sections 6, 7 and 12.1,
// net_relay_v2.h). Added 2026-11-09 by Melanikus for the AvP port. Every relay game since #175 PR 3a.
// The rules (AvP-docs/engine/v2-step7-design.md, agreed with Primary):
//   - Keys before anything. A relay is used only once its keys have come from the list, proved by its server's servers.cfg line together
//     with the list's own key (relay_list.h; modified 2026-11-10 by Melanikus for the AvP port, #175, in place of -dev's RelayKey). Until
//     then nothing, not even the cookie round, is sent to it.
//   - Every version 1 message to the relay goes sealed, through the session (Seal), and only messages the session opened reach net_relay.h's
//     machines. A plain version 1 datagram from the relay is dropped, never read.
//   - A datagram from the relay is sorted by its first four bytes, never its size: 'AVPR' is the session's, 'AVPE' and 'AVPG' the end to
//     end's (B and the sealed game), and anything else is dropped.
//   - A session that fails ends the room or the join, with the player told why. There's no going back to version 1.
// The session borrows this run's identity (pqIK proves the player to the relay too), which lives until Net_Deinitialise, longer than any
// session. The relay keeps one session per address and port, so a second copy on the same PC has its own.
int const kRelayV2None = 0;        // not in use
int const kRelayV2Keys = 1;        // waiting for the relay's keys from the list
int const kRelayV2Timing = 2;      // timing the relay with the cookie round, for the list's round trip (the host only)
int const kRelayV2Handshake = 3;   // handshake A under way
int const kRelayV2Ready = 4;       // the sealed channel is open
static int relayV2Step = kRelayV2None;
static bool relayV2Started = false;          // whether handshake A's session has started
static bool relayV2Times = false;            // the host times the relay, for the list; a joiner doesn't need to
static NetRelayV2Session relayV2Session;
static NetRelayV2Picker relayV2Picker;
static NetRelayStaticKeys relayV2Keys;       // the relay's keys, once they've come proved by the server's line
static ENetAddress relayV2Address;           // the relay, at its control port
static char relayV2Region[kRelayListRegionLetters + 1];
static uint32_t relayV2Ms = 0;               // the cookie round's round trip
static uint32_t relayV2SealsRefused = 0;     // version 1 messages the session wouldn't seal (not open, or over): never sent plain

static bool HasMagic(const uint8_t *data, size_t size, const char magic[5])
{
	return data && size >= 4 && memcmp(data, magic, 4) == 0;
}

// Everything of the channel goes: the session's keys wiped, the timing and the relay's keys forgotten
static void RelayV2Forget()
{
	relayV2Session.Wipe();
	relayV2Picker.Reset();
	memset(&relayV2Keys, 0, sizeof(relayV2Keys));
	memset(&relayV2Address, 0, sizeof(relayV2Address));
	relayV2Region[0] = 0;
	relayV2Step = kRelayV2None;
	relayV2Started = false;
	relayV2Ms = 0;
}

// The session's words for why it ended, the game's own fixed words. Refusal 22 (a player the relay hasn't approved) gets no answer at all
// from the relay, so the game can't tell it from 7: the words say what the player can do, and never claim which it was (Primary's words).
static const char *RelayV2WhyText(int why)
{
	switch (why)
	{
		case kNetRelayV2NoAnswer: return "the relay didn't answer (if you haven't been approved on this server yet, ask for access on the online page)";
		case kNetRelayV2BadCookie: return "the relay's answer wasn't one it could have sent";
		default: return "the handshake with the relay failed";
	}
}

// Starts using a relay, by asking the list for its keys. Nothing is sent to the relay yet. False, with why in fixed words, if it can't be
// used. Modified 2026-11-10 by Melanikus for the AvP port (#175): the keys come proved by the server's servers.cfg line (relay_list.h), so
// there's no setting to hold them to.
static bool RelayV2Begin(const ENetAddress &relay, const char *region, bool times, const char **why)
{
	RelayV2Forget();

	if (!region || !RelayList_AskForRelayKeys(region))
	{
		*why = "the relay's keys couldn't be asked for";
		return false;
	}

	relayV2Address = relay;
	strncpy(relayV2Region, region, kRelayListRegionLetters);
	relayV2Region[kRelayListRegionLetters] = 0;
	relayV2Times = times;
	relayV2Step = kRelayV2Keys;
	relayV2Used = true;
	return true;
}

// What the session and the timing want sent, straight from this host's socket to the relay's address, at the port each names
static void RelayV2SendAll(ENetHost *on)
{
	if (!on)
	{
		return;
	}

	NetRelayV2Send sends[kNetRelayV2MostSends];
	size_t const count = relayV2Session.TakeSends(sends, kNetRelayV2MostSends);

	for (size_t i = 0; i < count; i++)
	{
		ENetAddress to = relayV2Address;
		to.port = sends[i].toPort;
		ENetBuffer buffer;
		buffer.data = sends[i].bytes;
		buffer.dataLength = sends[i].size;
		enet_socket_send(on->socket, &to, &buffer, 1);
	}

	NetRelayV2PickerSend asks[kNetRelayV2MostPickerSends];
	size_t const askCount = relayV2Picker.TakeSends(asks, kNetRelayV2MostPickerSends);

	for (size_t i = 0; i < askCount; i++)
	{
		ENetAddress to = relayV2Address;
		to.port = asks[i].port;
		ENetBuffer buffer;
		buffer.data = asks[i].bytes;
		buffer.dataLength = kNetV2DatagramBytes;
		enet_socket_send(on->socket, &to, &buffer, 1);
	}
}

// One turn of the channel: the keys, then (for the host) the timing, then handshake A, then the open channel. Gives back the step it's at.
// When the channel can't go on, it gives back kRelayV2None with why in fixed words, and everything of it has gone.
static int RelayV2Turn(ENetHost *on, uint32_t now, const char **why)
{
	*why = NULL;

	if (relayV2Step == kRelayV2None)
	{
		*why = "the channel to the relay isn't open";
		return kRelayV2None;
	}

	if (relayV2Step == kRelayV2Keys)
	{
		const char *listWhy = NULL;
		int const came = RelayList_RelayKeysCame(relayV2Keys.p384, relayV2Keys.kem, &listWhy);

		if (came == kRelayKeysWaiting)
		{
			return relayV2Step;
		}

		// The keys come only proved by the server's line (relay_list.h): anything else, and nothing is sent to the relay
		if (came != kRelayKeysCame)
		{
			*why = listWhy ? listWhy : "the relay's keys couldn't be had";
			RelayV2Forget();
			return kRelayV2None;
		}

		relayV2Step = relayV2Times ? kRelayV2Timing : kRelayV2Handshake;

		if (relayV2Times)
		{
			uint8_t ids[kNetRelayEchoes * kNetV2IdBytes];
			uint16_t const port = relayV2Address.port;
			RelayRandomBytes(ids, sizeof(ids));

			if (!relayV2Picker.Start(1, &port, ids, now))
			{
				*why = "the relay couldn't be timed";
				RelayV2Forget();
				return kRelayV2None;
			}
		}
	}

	if (relayV2Step == kRelayV2Timing)
	{
		relayV2Picker.Tick(now);
		RelayV2SendAll(on);

		if (!relayV2Picker.Done(now))
		{
			return relayV2Step;
		}

		if (relayV2Picker.Best() < 0)
		{
			*why = RelayV2WhyText(kNetRelayV2NoAnswer);
			RelayV2Forget();
			return kRelayV2None;
		}

		relayV2Ms = relayV2Picker.BestMs();
		relayV2Step = kRelayV2Handshake;
	}

	// Handshake A starts: its draws, a fresh id, and this run's identity, which the session borrows
	if (relayV2Step == kRelayV2Handshake && !relayV2Started)
	{
		NetHandshakeADraws draws;
		memset(&draws, 0, sizeof(draws));
		uint8_t id[kNetV2IdBytes];

		if (!RelayIdentityReady() || !NetCrypto_Random(id, sizeof(id)) || !NetHandshakeA_Draw(&relayV2Keys, &draws))
		{
			NetHandshakeA_WipeDraws(&draws);
			*why = "the handshake with the relay couldn't start";
			RelayV2Forget();
			return kRelayV2None;
		}

		NetHandshakeAPlayer const me = { runIdentity.p384, runIdentity.statics, runIdentity.kemPrivate };

		if (!relayV2Session.Start(relayV2Region, &relayV2Keys, &me, relayV2Address.port, id, &draws, now))
		{
			*why = "the handshake with the relay couldn't start";
			RelayV2Forget();
			return kRelayV2None;
		}

		relayV2Started = true;
	}

	relayV2Session.Tick(now);
	RelayV2SendAll(on);

	if (relayV2Session.Failed())
	{
		*why = RelayV2WhyText(relayV2Session.Why());
		RelayV2Forget();
		return kRelayV2None;
	}

	relayV2Step = relayV2Session.Ready() ? kRelayV2Ready : kRelayV2Handshake;
	return relayV2Step;
}

// The messages the session has opened go to net_relay.h's machines, as if they had come plain from the port they came from (those
// machines check the port), through the one door version 1's messages always go through.
// THE PORT ISN'T SEALED (Secondary's finding 3.1, and why it's safe today): the seal covers the message, not the UDP port it came from,
// so someone on the path could deliver a real sealed message first from another port, and the real one would then be a replay. But the
// machines take each kind of message from one port only (the host from the control port; a joiner READY from its own port, the rest from
// the control port), so a moved message is simply dropped, which is no more than such a person could do by dropping it. If a kind of
// message is ever taken from two ports with different meanings, the port has to go inside the sealed message first.
// The copies here hold the room's token, a joiner's ticket and the list's pass, which v2 seals to keep secret, so they're wiped when
// they've been read (Secondary's finding 5.1; modified 2026-11-09 by Melanikus for the AvP port).
static void RelayV2TakeOpened(uint32_t now)
{
	NetRelayV2Opened opened[kNetRelayV2MostOpened];
	size_t const count = relayV2Session.TakeOpened(opened, kNetRelayV2MostOpened);

	for (size_t i = 0; i < count; i++)
	{
		RelayControlDatagram(opened[i].fromPort, opened[i].message, kNetSealedControlMessageBytes, now);
	}

	NetCrypto_Wipe(opened, sizeof(opened));
}

// A datagram from the relay whose first four bytes are 'AVPR': the timing's answer, or the session's
static void RelayV2Hear(uint16_t fromPort, const uint8_t *data, size_t size, uint32_t now)
{
	if (relayV2Step == kRelayV2Timing)
	{
		relayV2Picker.Hear(0, fromPort, data, size, now);
	}
	else if (relayV2Step == kRelayV2Handshake || relayV2Step == kRelayV2Ready)
	{
		relayV2Session.Hear(fromPort, data, size, now);
		RelayV2TakeOpened(now);
	}
}

// What one of net_relay.h's machines wants sent: through the v2 channel, each message sealed, never plain (modified 2026-11-10 by Melanikus
// for the AvP port, #175 PR 3a: version 1's plain sends are gone)
template <typename End>
static void RelayControlSendAll(ENetHost *on, End &end)
{
	NetRelaySend sends[kNetRelayMostSends];
	size_t const count = end.TakeSends(sends, kNetRelayMostSends);
	uint32_t const now = enet_time_get();

	for (size_t i = 0; i < count; i++)
	{
		if (relayV2Step != kRelayV2Ready || !relayV2Session.Seal(sends[i].bytes, sends[i].port, now))
		{
			relayV2SealsRefused++;
		}
	}

	// The messages before they were sealed: the room's token, a joiner's ticket, a cookie. Wiped, as the session wipes its own copies
	// (Secondary's finding 5.1; added 2026-11-09 by Melanikus for the AvP port).
	NetCrypto_Wipe(sends, sizeof(sends));
	RelayV2SendAll(on);
}

// Each frame while joined through a relay: the channel's resends and its end, and the join's own messages through it.
// The host's channel has its turn in RelayHostFrame. If a joiner's channel ends, the game goes on over handshake B's sealed datagrams, as
// the relay keeps passing them; the log says so.
static void RelayV2JoinerFrame()
{
	if (!host || relayHosting || relayV2Step == kRelayV2None)
	{
		return;
	}

	const char *why = NULL;

	if (RelayV2Turn(host, enet_time_get(), &why) == kRelayV2None)
	{
		LogString(std::string("net: the joiner's channel to the relay has ended: ") + (why ? why : "no reason given"));
		return;
	}

	RelayControlSendAll(host, relayJoiner);
}

// Whether a connection comes through the relay: from the relay's address, while this host is open through it
static bool PeerIsThroughRelay(const ENetPeer *peer)
{
	return relayHosting && !relayPicking && peer->address.host == relayAddress.host;
}

// SEALED OR NOTHING (#188; networking.h says the rule). Added 2026-11-10 by Melanikus for the AvP port. The game's own fixed words for each
// way a game is refused or ended, and what's counted for the log. For every game since #175 PR 3a.
// Modified 2026-11-10 by Melanikus for the AvP port (the owner's ruling for beta.2: "Everything over nyc3"): there's no plain game, at home or anywhere, so a game
// not through a server's relay is refused, and so is a host with no server to go through.
static const char *const kNotThroughTheList = "that game isn't on the online list, and a game is played only through a server's relay, sealed";
static const char *const kNoServerSet = "no server is set in servers.cfg, and a game is hosted only through a server's relay, sealed";
static const char *const kServerNotYet = "the server hasn't answered yet";
static const char *const kOnAnotherServer = "that game is on another server in servers.cfg: pick it on the Servers page, and it's used from the next start";   // 2026-11-11
static const char *const kServerNotAgreed = "you haven't said yes to the server in servers.cfg yet";   // #175's last part (2026-11-10)
static const char *const kRelayCantBeUsed = "the server's relay can't be used";
static const char *const kNoIdentity = "this game's identity couldn't be made";
static const char *hostFailedWhy = NULL;       // why the last Net_HostGame failed (Net_HostFailedWhy)
static const char *joinRefusedWhy = NULL;      // why the last Net_ConnectToSession refused a game before opening anything (Net_JoinRefusedWhy)

// An online host whose relay is lost: the game can't go on sealed, so it has ended, and nothing at all reaches ENet any more, until the host
// is destroyed (#188). Never a way back to a plain game.
static int ENET_CALLBACK NothingIntercept(ENetHost *on, ENetEvent *event)
{
	(void)on;
	(void)event;
	return 1;
}

// ENet shows us every packet that arrives, before it reads it (ENetHost::intercept). Giving back 1 means "taken": ENet never sees it.
static int ENET_CALLBACK RelayIntercept(ENetHost *on, ENetEvent *event)
{
	(void)event;
	uint32_t const now = enet_time_get();

	// Hudson's branch: a run's socket on the way to the one relay beyond this machine hears that relay alone, whatever else is going on
	if (OnTheRemoteRoute(on->address) && on->receivedAddress.host != remoteRelayAddress)
	{
		relayStrangersDropped++;
		return 1;
	}

	// Modified 2026-11-09 by Melanikus for the AvP port (protocol version 2, step 7): with the channel to a relay in use, only that
	// relay is heard, on any of its ports, and what it sends is sorted by its first four bytes. 'AVPR' is the channel's (the timing,
	// handshake A and sealed control); once it's open, 'AVPE' and 'AVPG' are the end to end's (B and the sealed game). Anything else from it,
	// a plain version 1 message among them, is dropped, never read.
	// AND ONCE THE CHANNEL HAS ENDED (Secondary's finding 2.1; modified 2026-11-09 by Melanikus for the AvP port): a joiner's game goes on
	// over B's sealed datagrams, and the relay is still heard only this way until that game ends (relayV2Used). Its 'AVPR' then has no
	// channel to open it, so it's dropped too: a plain version 1 message is never read again in a game that has used the channel.
	bool const channelOn = relayV2Step != kRelayV2None;

	if (channelOn || relayV2Used)
	{
		if (on->receivedAddress.host != (channelOn ? relayV2Address.host : relayAddress.host))
		{
			relayStrangersDropped++;
			return 1;
		}

		if (HasMagic(on->receivedData, on->receivedDataLength, "AVPR"))
		{
			if (channelOn)
			{
				RelayV2Hear(on->receivedAddress.port, on->receivedData, on->receivedDataLength, now);
			}
			else
			{
				relayStrangersDropped++;
			}

			return 1;
		}

		bool const endToEnd = HasMagic(on->receivedData, on->receivedDataLength, "AVPE") || HasMagic(on->receivedData, on->receivedDataLength, "AVPG");

		// B's and the sealed game's datagrams count once the channel is open (and not while a host is still timing the relays), or, after
		// it has ended, for a joiner, whose game goes on over them. A host whose channel ends gives the relay up (RelayHostFrame).
		bool const gameGoesOn = channelOn ? relayV2Step == kRelayV2Ready && !(relayHosting && relayPicking) : !relayHosting;

		if (!gameGoesOn || !endToEnd)
		{
			relayStrangersDropped++;
			return 1;
		}

		return RelayEndToEndHeard(on, now);
	}

	// Modified 2026-11-10 by Melanikus for the AvP port (#175 PR 3a, version 2 only): without the channel, in use or used, nothing from a relay
	// is the game's: version 1's plain relay path is gone. The one way here is Hudson's socket on the way to its relay beyond this machine,
	// before the channel opens, and it hears nothing then.
	relayStrangersDropped++;
	return 1;
}

// Modified 2026-11-09 by Melanikus for the AvP port: the host's news about its relay has changed, for its screen to say once
// (Net_RelayHostStateNow). Only Listed has a region and a round trip, and only RoomGone and Ended a why: the game's own fixed words.
static void RelayHostNews(int what, const char *why)
{
	relayHostNews.changes++;
	relayHostNews.what = what;
	relayHostNews.region[0] = 0;
	relayHostNews.ms = 0;
	relayHostNews.why = (what == kNetRelayHostRoomGone || what == kNetRelayHostEnded) ? why : NULL;

	if (what == kNetRelayHostListed)
	{
		memcpy(relayHostNews.region, relayRegion, sizeof(relayHostNews.region));
		relayHostNews.ms = relayHostMs;
	}
}

// A host starts going through a relay, when the list has named some: it times each, and asks the nearest for a room (RelayHostFrame). False
// with none named, or none that can be used. Modified 2026-11-10 by Melanikus for the AvP port (#188, and #175 PR 3a): false means the game
// isn't hosted at all (sealed or nothing), and why says why, in the game's own fixed words; the caller says it.
static bool RelayHostStart(const char **why)
{
	*why = NULL;
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
		// The server hasn't answered yet, or can't be used: Net_ServerState tells the two apart, so a host waits for the first (#188)
		const char *serverWhy = NULL;
		*why = RelayList_ServerState(&serverWhy) == kRelayServerCantUse && serverWhy ? serverWhy : kServerNotYet;
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
		// Modified 2026-11-10 by Melanikus for the AvP port (#175): nothing is looked up now, as the relay is at its server's servers.cfg
		// line's address, so the words say only that it can't be used
		*why = kRelayCantBeUsed;
		return false;
	}

	// Modified 2026-11-09 by Melanikus for the AvP port (protocol version 2): this run's identity, whose whole id the listing carries and
	// handshake B proves to each joiner. Without one, nothing could be sealed, so the game isn't hosted online at all (#188).
	if (!RelayIdentityReady())
	{
		*why = kNoIdentity;
		return false;
	}

	RelayEndToEndForget();
	RelayList_HostIdentity(runIdentity.id);

	// Modified 2026-11-09 by Melanikus for the AvP port (protocol version 2, step 7): nothing is sent to the relay until its
	// keys have come (RelayV2Begin, RelayV2Turn). Modified 2026-11-10 (#175): it's the server's one relay, whose keys its servers.cfg line
	// proves (relay_list.h). Version 1's timing of every relay with plain echoes is gone (#175 PR 3a).
	if (!RelayV2Begin(relayCandidates[0], relayCandidateRegions[0], true, why))
	{
		*why = *why ? *why : kRelayCantBeUsed;
		return false;
	}

	relayHosting = true;
	relayPicking = true;
	host->duplicatePeers = kMostClients;
	host->intercept = RelayIntercept;

	host->sendHook = RelaySealedSend;
	relaySealing = true;

	Con_PrintMessage("net: this game is finding the nearest relay");
	RelayHostNews(kNetRelayHostFinding, NULL);
	return true;
}

// The log's line at the end of a game's time through the relay: how many of its datagrams went out sealed, and how many came in and
// opened. Counts only, never what was in them. Then they start again from 0. (Added 2026-11-09 by Melanikus for the AvP port.)
static void RelayLogSealedCounts()
{
	LogString("net: through the relay, " + std::to_string(relayEndToEndSealed) + " datagram(s) went out sealed, and " + std::to_string(relayEndToEndOpened) + " came in and opened");
	relayEndToEndSealed = 0;
	relayEndToEndOpened = 0;
}

// Modified 2026-11-10 by Melanikus for the AvP port (#188, and #175 PR 3a): reason is why, in the game's own fixed words. The game can't go on
// sealed, so it has ended (kNetRelayHostEnded, with reason): nothing reaches ENet any more (NothingIntercept), and the search port never
// opens. Version 1's give-up into a game open on this network is gone.
static void RelayHostGiveUp(const char *reason)
{
	bool const wasOpen = relayHosting && !relayPicking;   // while still timing the relays, nobody has come through one

	// A host that loses its relay (a revoke ends the room this way: the relay just stops answering) says what went sealed until then
	if (wasOpen && relaySealing)
	{
		RelayLogSealedCounts();
	}
	relayHosting = false;
	relayPicking = false;

	if (host)
	{
		// Nothing at all, Hudson's run on the way to its relay beyond this machine included (#188)
		host->intercept = NothingIntercept;
		host->duplicatePeers = kMostConnectionsFromOneAddress;
	}

	// Added 2026-11-09 by Melanikus for the AvP port (Secondary's finding 4.1): every connection that came through the relay ends here, at
	// once. It can't work any more, as what the game sends it is dropped (below), and with the intercept gone ENet would otherwise read
	// whatever came from the relay's address as that joiner's, unsealed, until the connection timed out: exactly what v2's sealing is
	// there to stop, the relay included. Ending a connection this way tells ENet nothing more of it will come (enet_peer_reset), so the
	// game is told here that each player in it has gone, as when a connection closes. While hosting through a relay nothing else is heard
	// (RelayIntercept), so these are all through it.
	if (wasOpen && host)
	{
		int ended = 0;

		for (size_t i = 0; i < host->peerCount; i++)
		{
			ENetPeer *const peer = &host->peers[i];

			if (peer->state == ENET_PEER_STATE_DISCONNECTED || peer->address.host != relayAddress.host)
			{
				continue;
			}

			NetID const id = IDOfConnection(peer);
			peer->data = NULL;
			enet_peer_reset(peer);
			ended++;

			if (id != 0)
			{
				netSession.nPlayers--;
				Net_PlayerHasGone(id);
			}
		}

		if (ended > 0)
		{
			LogString("net: " + std::to_string(ended) + " connection(s) through the relay ended with it");
		}
	}

	// Modified 2026-11-09 by Melanikus for the AvP port (protocol version 2): every handshake ends, its keys wiped, and so does the channel
	// to the relay (step 7), inside RelayEndToEndForget. The send hook stays, so anything ENet still has for the relay's address is
	// dropped, never sent unsealed.
	RelayEndToEndForget();
	Con_PrintError(std::string("net: ") + reason + ", so this online game has ended: it can't go on sealed, and it never opens on this network instead");
	RelayHostNews(kNetRelayHostEnded, reason);
}

// Each frame while hosting through a relay: time the relays, then keep the room, say hello to joiners, and keep the list told
static void RelayHostFrame()
{
	if (!relayHosting || !host)
	{
		return;
	}

	uint32_t const now = enet_time_get();

	// Modified 2026-11-09 by Melanikus for the AvP port (protocol version 2, step 7): the channel to the relay, each frame:
	// its keys, its timing, handshake A, then the open channel. Until it's open nothing else is done; once it is, the room is asked for
	// through it, as version 1 asks after its echoes. A channel that can't go on ends the room, and the player is told why.
	const char *why = NULL;
	int const step = RelayV2Turn(host, now, &why);

	if (step == kRelayV2None)
	{
		if (relayRoomListed)
		{
			RelayList_HostRoomClosed();
			relayRoomListed = false;
		}

		// The online game ends (#188)
		RelayHostGiveUp(why ? why : "the channel to the relay ended");
		return;
	}

	if (step != kRelayV2Ready)
	{
		return;
	}

	if (relayPicking)
	{
		relayPicking = false;
		relayAddress = relayV2Address;
		strncpy(relayRegion, relayV2Region, kRelayListRegionLetters);
		relayRegion[kRelayListRegionLetters] = 0;
		relayHostMs = relayV2Ms < 1 ? 1 : (relayV2Ms > static_cast<uint32_t>(kRelayMostHostMs) ? kRelayMostHostMs : static_cast<int>(relayV2Ms));
		relayHost.Start(relayAddress.port, now);
		relayListedAt = now;
		Con_PrintMessage("net: this game's sealed channel to the relay is open, " + std::to_string(relayHostMs) + " ms away");
	}

	relayHost.Tick(now);
	RelayControlSendAll(host, relayHost);

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
		// Kept here, as giving up wipes the machine that knows it (modified 2026-11-09 by Melanikus for the AvP port)
		int const lostWhy = relayHost.LostWhy();
		Con_PrintError(std::string("net: this game's room on the relay is gone, so nobody more can join through it: ") + NetRelay_WhyText(lostWhy));

		if (relayRoomListed)
		{
			RelayList_HostRoomClosed();
			relayRoomListed = false;
		}

		RelayHostGiveUp(NetRelay_WhyText(lostWhy));
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
		RelayControlSendAll(host, relayHost);
	}

	// The channel to the relay ends once HOST_CLOSE has gone through it, its keys wiped (step 7, added 2026-11-09 by Melanikus for the
	// AvP port). A message it wouldn't seal was never sent plain, and the log counts them.
	if (relayV2SealsRefused > 0)
	{
		LogString("net: " + std::to_string(relayV2SealsRefused) + " message(s) to the relay weren't sent, as the sealed channel wasn't open for them");
		relayV2SealsRefused = 0;
	}

	RelayV2Forget();

	if (relayRoomListed && !relayLostSaid)
	{
		RelayList_HostRoomClosed();
	}

	if (relayStrangersDropped > 0)
	{
		LogString("net: while going through a relay, " + std::to_string(relayStrangersDropped) + " packet(s) from elsewhere were dropped");
	}

	// Modified 2026-11-09 by Melanikus for the AvP port (protocol version 2): the handshakes end, their keys wiped
	if (relayEndToEndDropped > 0)
	{
		LogString("net: while going through a relay, " + std::to_string(relayEndToEndDropped) + " datagram(s) the game sent couldn't be sealed yet, so were dropped");
	}

	if (relayEndToEndStarts.Refused() > 0)
	{
		LogString("net: while going through a relay, " + std::to_string(relayEndToEndStarts.Refused()) + " handshake(s) weren't started, because a joiner's port, or the host, had started as many as it may");
	}

	// What went sealed through the relay in all (added 2026-11-09 by Melanikus for the AvP port)
	if (relaySealing)
	{
		RelayLogSealedCounts();
	}

	relayEndToEndDropped = 0;
	RelayEndToEndForget();

	relayHosting = false;
	relayPicking = false;
	relayRoomListed = false;

	// Modified 2026-11-10 by Melanikus for the AvP port (#175 PR 3b): the host is destroyed next, and until then it hears nothing, never
	// everyone (tools/check-online-play.ps1's rule L: no host's intercept is ever cleared)
	if (host)
	{
		host->intercept = NothingIntercept;
	}
}

// A JOIN THROUGH A RELAY: the relay's handshake for a game's code, on this new ENet host's own socket, before ENet connects. relayAddress is
// the game's relay. It takes at most the joiner's own limits (10 s for an answer, 15 s for READY), and never more than kRelayJoinMostMs in
// all. Gives back the relay's port to connect the game to, or 0, with relayJoinFailedWhy saying why. Everything heard from anywhere but the
// relay is dropped.
static uint16_t RelayHandshake(const char *region, const char *code)
{
	uint32_t const started = enet_time_get();

	// Modified 2026-11-09 by Melanikus for the AvP port (protocol version 2, step 7): the channel to the relay comes
	// first, its keys and then handshake A, and the join's own messages go through it once it's open. Nothing is sent to the relay before
	// its keys have come proved by its server's servers.cfg line (modified 2026-11-10, #175), and a channel that can't open ends the join:
	// there's no going back to version 1.
	bool joinerStarted = false;
	const char *channelWhy = NULL;

	if (!host || !RelayV2Begin(relayAddress, region, false, &channelWhy))
	{
		relayEndToEndJoinFailed = channelWhy ? channelWhy : "the relay couldn't be used";
		return 0;
	}

	// Until the channel is open the join hasn't started, so nothing left from an earlier join may look like its end
	relayJoiner.Reset();

	static uint8_t packet[kRelayMostPacketBytes];

	for (size_t turn = 0; turn < kRelayJoinMostTurns && !relayJoiner.Ready() && !relayJoiner.Failed() && enet_time_get() - started < kRelayJoinMostMs; turn++)
	{
		// Where it's got to, for the menu's hook to say (Net_RelayJoinStep)
		relayJoinStep = joinerStarted && relayJoiner.Introducing() ? kNetRelayJoinStepIntroducing : kNetRelayJoinStepReaching;

		// The window keeps answering, and the player can give up (the menu's hook)
		if (relayJoinWaitHook && !relayJoinWaitHook())
		{
			relayJoinStep = kNetRelayJoinStepNone;
			relayJoinFailedWhy = kNetRelayGivenUp;
			RelayV2Forget();
			return 0;
		}

		// The channel's turn: once it's open, the join starts, and its messages go sealed
		int const step = RelayV2Turn(host, enet_time_get(), &channelWhy);

		if (step == kRelayV2None)
		{
			relayJoinStep = kNetRelayJoinStepNone;
			relayEndToEndJoinFailed = channelWhy ? channelWhy : "the channel to the relay ended";
			return 0;
		}

		if (step == kRelayV2Ready && !joinerStarted)
		{
			// The joiner's own line for what the host's "sealed channel to the relay is open" says (added 2026-11-09 by Melanikus for the
			// AvP port): its keys matched, and handshake A is done
			LogString("net: the joiner's sealed channel to the relay is open");

			if (!relayJoiner.Start(relayAddress.port, code, enet_time_get()))
			{
				relayJoinFailedWhy = kNetRelayRefusedNoGame;
				RelayV2Forget();
				return 0;
			}

			joinerStarted = true;
		}

		if (joinerStarted)
		{
			RelayControlSendAll(host, relayJoiner);
		}

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

			// Only the channel's own datagrams are heard ('AVPR'), and the join's messages come out of it opened; a plain version 1 message
			// from the relay is dropped, never read
			if (from.host == relayAddress.host && HasMagic(packet, static_cast<size_t>(got), "AVPR"))
			{
				RelayV2Hear(from.port, packet, static_cast<size_t>(got), enet_time_get());
			}
		}

		if (joinerStarted)
		{
			relayJoiner.Tick(enet_time_get());
		}
	}

	relayJoinStep = kNetRelayJoinStepNone;

	if (!relayJoiner.Ready())
	{
		relayJoinFailedWhy = relayJoiner.Failed() ? relayJoiner.FailedWhy() : kNetRelayNoAnswer;
		RelayV2Forget();   // the channel goes with the join (step 7, added 2026-11-09 by Melanikus for the AvP port)
		return 0;
	}

	// A READY sent again later is the relay's, so the intercept takes it off ENet's hands
	host->intercept = RelayIntercept;
	relayJoinFailedWhy = kNetRelayFine;
	return relayJoiner.Port();
}

const char *Net_RelayJoinFailedWhy()
{
	// Modified 2026-11-09 by Melanikus for the AvP port (protocol version 2): when the relay's part went well and handshake B failed, B's
	// own words
	return relayEndToEndJoinFailed ? relayEndToEndJoinFailed : NetRelay_WhyText(relayJoinFailedWhy);
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

// THE GAMES PAGE'S OWN ROUND TRIP TO THE RELAY, before any join, for its ping column. Added 2026-11-10 by Melanikus for the AvP port (PO's
// ruling, from the owner's look at the showcase: the column said "?"). The server's one relay is timed with version 2's cookie round, as a
// host's channel times it (net_relay_v2.h's NetRelayV2Picker): kNetRelayEchoes COOKIE_ASKs, each 1,200 bytes and answered with 64, which
// keeps nothing on the relay, opens nothing, and needs no approval. The list names the relay only once the player has said yes to its line
// and the server's keys have matched it (relay_list.h), so nothing is sent before that. The asks go from a socket of the page's own, open
// only while they're timed, and closed with the page (Net_StopTimingTheRelay); only an answer from the relay's own address is heard,
// and the picker takes only a cookie, from its control port, for an ask it sent. Until it's done the page shows "...".
int const kPageTimingNotYet = 0;     // the list hasn't named the relay yet: before the yes, or before the server's check
int const kPageTimingMeasuring = 1;
int const kPageTimingTimed = 2;
int const kPageTimingCantTime = 3;   // no answer in time, or the socket couldn't be made

static ENetSocket pageTimingSocket = ENET_SOCKET_NULL;
static NetRelayV2Picker pageTimer;
static ENetAddress pageTimedRelay;
static char pageTimedRegion[kRelayListRegionLetters + 1];
static int pageTiming = kPageTimingNotYet;
static uint32_t pageTimedMs = 0;

static void ClosePageTimingSocket()
{
	if (pageTimingSocket != ENET_SOCKET_NULL)
	{
		enet_socket_destroy(pageTimingSocket);
		pageTimingSocket = ENET_SOCKET_NULL;
	}
}

void Net_StopTimingTheRelay(void)
{
	ClosePageTimingSocket();
	pageTimer.Reset();
	pageTiming = kPageTimingNotYet;
	pageTimedMs = 0;
	pageTimedRegion[0] = 0;
}

void Net_TimeTheRelayForThePage(void)
{
	if (pageTiming == kPageTimingTimed || pageTiming == kPageTimingCantTime)
	{
		return;
	}

	uint32_t const now = enet_time_get();

	// The first time the list names the relay: the socket, and the first ask
	if (pageTiming == kPageTimingNotYet)
	{
		RelayListRelay relays[kRelayListMostRelays];
		int named = 0;

		if (!RelayList_Relays(relays, &named) || named <= 0)
		{
			return;
		}

		if (!RelayLookUp(relays[0], &pageTimedRelay))
		{
			pageTiming = kPageTimingCantTime;
			return;
		}

		strncpy(pageTimedRegion, relays[0].region, kRelayListRegionLetters);
		pageTimedRegion[kRelayListRegionLetters] = 0;
		uint8_t ids[kNetRelayEchoes * kNetV2IdBytes];
		RelayRandomBytes(ids, sizeof(ids));
		uint16_t const port = pageTimedRelay.port;
		pageTimingSocket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);

		ENetAddress any;
		any.host = ENET_HOST_ANY;
		any.port = 0;

		// Hudson's branch: a run with net= puts this socket on this machine's own address too, or on the way to its one relay beyond it
		if (NetLoopbackOnly)
		{
			LoopbackOnlyAddress(&any);
		}

		if (pageTimingSocket == ENET_SOCKET_NULL || enet_socket_set_option(pageTimingSocket, ENET_SOCKOPT_NONBLOCK, 1) < 0
			|| enet_socket_bind(pageTimingSocket, &any) < 0 || !pageTimer.Start(1, &port, ids, now))
		{
			ClosePageTimingSocket();
			pageTiming = kPageTimingCantTime;
			return;
		}

		pageTiming = kPageTimingMeasuring;
		LogString("net: the games page is timing the server's relay");
	}

	// What has come: from the relay's own address only. Anything else is dropped unread.
	static uint8_t packet[kRelayMostPacketBytes];

	for (size_t n = 0; n < kRelayMostPacketsAWait; n++)
	{
		enet_uint32 condition = ENET_SOCKET_WAIT_RECEIVE;

		if (enet_socket_wait(pageTimingSocket, &condition, 0) < 0 || !(condition & ENET_SOCKET_WAIT_RECEIVE))
		{
			break;
		}

		ENetAddress from;
		ENetBuffer buffer;
		buffer.data = packet;
		buffer.dataLength = sizeof(packet);
		int const got = enet_socket_receive(pageTimingSocket, &from, &buffer, 1);

		if (got <= 0)
		{
			break;
		}

		if (from.host == pageTimedRelay.host)
		{
			pageTimer.Hear(0, from.port, packet, static_cast<size_t>(got), now);
		}
	}

	pageTimer.Tick(now);

	NetRelayV2PickerSend sends[kNetRelayV2MostPickerSends];
	size_t const sendCount = pageTimer.TakeSends(sends, kNetRelayV2MostPickerSends);

	for (size_t i = 0; i < sendCount; i++)
	{
		ENetAddress to = pageTimedRelay;
		to.port = sends[i].port;
		ENetBuffer buffer;
		buffer.data = sends[i].bytes;
		buffer.dataLength = kNetV2DatagramBytes;
		enet_socket_send(pageTimingSocket, &to, &buffer, 1);
	}

	if (pageTimer.Done(now))
	{
		uint32_t ms = 0;
		bool const timed = pageTimer.RelayMs(0, &ms) && ms <= static_cast<uint32_t>(kRelayMostHostMs);
		ClosePageTimingSocket();
		pageTiming = timed ? kPageTimingTimed : kPageTimingCantTime;
		pageTimedMs = timed ? ms : 0;
		LogString(timed ? "net: the games page has timed the server's relay" : "net: the server's relay didn't answer the games page's timing in time");
	}
}

bool Net_RelayBeingTimed(const char *region)
{
	return region && pageTiming == kPageTimingMeasuring && strncmp(region, pageTimedRegion, sizeof(pageTimedRegion)) == 0;
}

// The page's own time for that region's relay, or -1
static int PageTimedMs(const char *region)
{
	if (!region || pageTiming != kPageTimingTimed || strncmp(region, pageTimedRegion, sizeof(pageTimedRegion)) != 0)
	{
		return -1;
	}

	return static_cast<int>(pageTimedMs);
}

// Modified 2026-11-10 by Melanikus for the AvP port (#175 PR 3a): the joiner's round trip to its relay, from the channel's own timing (the
// cookie round, as the host's own is), once a join has made it, for the relay of that region. Version 1's timing of every relay with plain
// echoes before a join is gone: a version 2 relay answers no plain echo (Primary, beta2-server's RelayCore.java), and a version 2 game sends
// no plain message. Modified again the same day (PO's ruling): before a join, the games page's own timing of the relay, above; -1 without
// either, when the page shows "?" (or "..." while it's timing), and for any other region.
int Net_JoinerMsToRelay(const char *region)
{
	if (!region || relayV2Ms == 0 || relayV2Ms > static_cast<uint32_t>(kRelayMostHostMs) || strncmp(region, relayV2Region, sizeof(relayV2Region)) != 0)
	{
		return PageTimedMs(region);
	}

	return static_cast<int>(relayV2Ms);
}

// A join through a relay: the relay of the game's region, from the relays the list names, looked up, then its handshake (RelayHandshake).
// Gives back the relay's port to connect the game to, or 0, with relayJoinFailedWhy saying why.
static uint16_t RelayJoin(const char *region, const char *code)
{
	relayJoinFailedWhy = kNetRelayNoRelay;
	relayEndToEndJoinFailed = NULL;
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
			return RelayLookUp(relays[i], &relayAddress) ? RelayHandshake(region, code) : 0;
		}
	}

	return 0;
}

// Modified 2026-11-08 by Melanikus for the AvP port: a game from the relays' list (relay_list.h, PROTOCOL.md 7014de1) goes on the same list
// of sessions a search of this network fills, and is joined the same way (Net_ConnectToSession). What the list says is the host's word, sent
// to the list and on to us, so each part is checked here, as a search's answer was before the search went: the lobby state, a code, a region, the
// names' lengths, a slot left, and the level, which has to be one this machine has, picked by its number, its name and its game style, as a
// game found on this network is (GetLocalMultiplayerLevelIndex). Gives back whether it went on the list.
static bool AddRelaySession(const char *code, const char *region, const char *name, int players, int maxPlayers, int version, uint16_t gameStyle,
	uint16_t levelNumber, const char *levelName, uint8_t coopLobby, const uint8_t hostId[kNetIdentityIdBytes])
{
	if (!code || !NetRelay_CodeIsGood(code) || !hostId || !region || !name || !levelName || !CoopLobby_StateIsReal(coopLobby))
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
	// Modified 2026-11-09 by Melanikus for the AvP port (protocol version 2): the host's whole id, which handshake B checks the host by
	memcpy(entry.relayHostId, hostId, sizeof(entry.relayHostId));
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

	// Modified 2026-11-09 by Melanikus for the AvP port: the games page's columns, as a LAN game's were: the level's name cut to
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
// Whether a good line is the one this run uses (-1 is). Added 2026-11-11 by Melanikus for the AvP port: this run has the relay keys of
// that line alone, so a game listed by another line's server can be shown but not joined.
static bool OnTheLineInUse(int line)
{
	RelayListLineShown shown;
	return line < 0 || (RelayList_LineAt(line, &shown) && shown.inUse);
}

bool Net_AddRelaySession(const RelayListGame &game, int line)
{
	if (game.gameStyle < 0 || game.gameStyle > 255 || game.levelNumber < 0 || game.levelNumber > 100 || game.coopLobby < 0 || game.coopLobby > 255)
	{
		LogDroppedMessage("a game on the relays' list had a number out of its range");
		return false;
	}

	if (!AddRelaySession(game.code, game.region, game.name, game.players, game.maxPlayers, game.version, static_cast<uint16_t>(game.gameStyle),
		static_cast<uint16_t>(game.levelNumber), game.level, static_cast<uint8_t>(game.coopLobby), game.hostId))
	{
		return false;
	}

	// The line that listed it, and whether this run can join it (added 2026-11-11 by Melanikus for the AvP port)
	SESSION_DESC &added = SessionData[NumberOfSessionsFound - 1];
	added.serverLine = line;

	if (!OnTheLineInUse(line))
	{
		added.AllowedToJoin = false;
	}

	return true;
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

// (Removed 2026-11-10 by Melanikus for the AvP port, the owner's ruling for beta.2, "Everything over nyc3": the search for games on this
// network. That was the socket for finding games (OpenDiscoverySocket, CloseDiscoverySocket), the question sent to the whole network or a
// typed address (AskAboutGames), the answers read and kept (Net_UpdateSessionList, Net_AddSession, Net_StopSearchingForSessions), and the
// host's side, which answered anyone who asked, from anywhere (Net_CheckForSessionRequest, and its log, net_search_log.h). Every game is
// now found on the server's online list, and played through its relay, sealed. REMOVED-CODE.md says where it was.)

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
	// the game's own, with a line in the log saying so. The port is the one the host plays on: it has to be a port there is. A speed of 0
	// means "as much as there is", and one below 0 means nothing.
	int const port = Config_GetInt("[Networking]", "PortNumber", (int)netPortNumber);
	int const incoming = Config_GetInt("[Networking]", "IncomingBandwidth", 0);
	int const outgoing = Config_GetInt("[Networking]", "OutgoingBandwidth", 0);

	netPortNumber = (uint32_t)NetSettingOrOwn("PortNumber", port, port >= 1 && port <= 65535, (int)netPortNumber);
	incomingBandwidth = (uint32_t)NetSettingOrOwn("IncomingBandwidth", incoming, incoming >= 0, 0);
	outgoingBandwidth = (uint32_t)NetSettingOrOwn("OutgoingBandwidth", outgoing, outgoing >= 0, 0);

	// Nothing is opened on the network here, because this runs every time the game starts, and most games are never on a network.

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

	// This run's identity ends with the network (protocol version 2, added 2026-11-09 by Melanikus for the AvP port): its key destroyed
	// inside the operating system's cryptography, and every byte of the rest wiped. Modified 2026-11-09 by Melanikus for the AvP port (#21):
	// the kept one is looked for again the next time the network starts.
	NetIdentity_Free(&runIdentity);
	runIdentityLookedFor = false;
	runIdentityKept = kNetIdentityStoreNoFile;

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

// Modified 2026-11-10 by Melanikus for the AvP port (the owner's ruling for beta.2: "Everything over nyc3"): a game is hosted online, through the relay of the server
// servers.cfg names, sealed, or not at all. With no server named, nothing is hosted, and the screen says why (Net_HostFailedWhy). A skirmish,
// the game's multiplayer for one against the computer, opens no network at all, so it needs no server.
NetResult Net_HostGame(char *playerName, char *sessionName, int species, uint16_t gameStyle, uint16_t level)
{
	printf("Net_HostGame\n");
	hostFailedWhy = NULL;

	bool const online = !netGameData.skirmishMode;

	if (online && !Relay_ServerName()[0])
	{
		hostFailedWhy = kNoServerSet;
		Con_PrintError(std::string("net: this game can't be hosted: ") + kNoServerSet);
		return NET_FAIL;
	}

	// Added 2026-11-10 by Melanikus for the AvP port (#175's last part): an online game only once the player has said yes to the server,
	// before anything is made. Its screen shows the line and asks (RelayList_LineToAgree), then hosts again.
	const char *notAgreedWhy = NULL;

	if (online && Net_ServerState(&notAgreedWhy) == kNetServerNeedsYes)
	{
		hostFailedWhy = kServerNotAgreed;
		Con_PrintError(std::string("net: this game can't be hosted online: ") + kServerNotAgreed);
		return NET_FAIL;
	}

	// The server hasn't answered yet: asked now, if it wasn't (relay_list.cpp asks once at a time), and the host waits for it (Net_ServerState)
	if (online)
	{
		RelayList_FindRelays();
	}

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

			if (NetLoopbackOnly)
			{
				LoopbackOnlyAddress(&ServerAddress);
			}

			ServerAddress.port = netPortNumber;

			// The game's limits, a checksum on every packet and one channel: see net_host.h
			host = Net_MakeENetHost(&ServerAddress, kMostClients, kMostMessageBytes, incomingBandwidth, outgoingBandwidth);
			if (NULL == host)
			{
				Con_PrintError("Failed to create ENet server");
				return NET_FAIL;
			}

			// Hudson's branch: a host on the way to the one relay beyond this machine hears that relay alone, from the start (RelayIntercept)
			if (OnTheRemoteRoute(ServerAddress))
			{
				host->intercept = RelayIntercept;
			}

			// The game goes through the server's relay, sealed, or it isn't hosted at all, and a lost relay ends it (RelayHostGiveUp).
			// Modified 2026-11-10 by Melanikus for the AvP port (the owner's ruling for beta.2: "Everything over nyc3"): there's no game at
			// home any more, and no search port to find one by.
			const char *notOnline = NULL;

			if (!RelayHostStart(&notOnline))
			{
				hostFailedWhy = notOnline ? notOnline : kRelayCantBeUsed;
				Con_PrintError(std::string("net: this game can't be hosted online: ") + hostFailedWhy + ", so it isn't hosted at all (sealed or nothing)");
				enet_host_destroy(host);
				host = NULL;
				return NET_FAIL;
			}

			// No search on this network ever finds it: it goes through the relay alone
			Con_PrintMessage("net: this game won't answer searches on this network while it goes through a relay");

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

const char *Net_HostFailedWhy(void)
{
	return hostFailedWhy;
}

int Net_ServerState(const char **why)
{
	*why = NULL;

	if (!Relay_ServerName()[0])
	{
		return kNetServerNone;
	}

	int const state = RelayList_ServerState(why);

	// Added 2026-11-10 by Melanikus for the AvP port (#175's last part): a line without the player's yes is never asked
	if (state == kRelayServerNeedsYes)
	{
		return kNetServerNeedsYes;
	}

	return state == kRelayServerGood ? kNetServerReady : state == kRelayServerCantUse ? kNetServerCantUse : kNetServerAsking;
}

const char *Net_JoinRefusedWhy(void)
{
	return joinRefusedWhy;
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

	// What a joiner through a relay dropped from elsewhere (a host says its own as it leaves its room, in RelayHostLeave)
	if (relayJoinedThrough && relayStrangersDropped > 0)
	{
		LogString("net: while going through a relay, " + std::to_string(relayStrangersDropped) + " packet(s) from elsewhere were dropped");
	}

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

	// The game has closed, and the list was only for this game
	CoopRemoved_Forget(removedPlayers);

	// Modified 2026-11-09 by Melanikus for the AvP port (protocol version 2): a joiner's handshake goes with its game, its keys wiped, once
	// the goodbyes above have gone sealed (a host's went in RelayHostLeave), and the log says how many of the game's datagrams couldn't be
	// sealed and were dropped
	if (relayEndToEndPort != 0 && relayEndToEndDropped > 0)
	{
		LogString("net: while going through a relay, " + std::to_string(relayEndToEndDropped) + " datagram(s) the game sent couldn't be sealed yet, so were dropped");
	}

	// And what went sealed through the relay in all (added 2026-11-09 by Melanikus for the AvP port)
	if (relayEndToEndPort != 0)
	{
		RelayLogSealedCounts();
	}

	relayEndToEndDropped = 0;
	relayEndToEndSealed = 0;
	relayEndToEndOpened = 0;
	RelayEndToEndForget();
	relaySealing = false;
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

// What the frames and the figures did since the last full lines (net_watch.h). They start again with each set of lines, and with a new game.
static NetFrameWatch netFrames = {};
static NetFigureWatch netFigures = {};

void Net_NoteFrame(int frameTime)
{
	NetFrameWatch_Note(&netFrames, NetFrameWatch_Milliseconds(frameTime));
}

void Net_NoteFigureWait(unsigned int waitedMs, unsigned int jumpMm, bool aPlayer)
{
	NetFigureWatch_Note(&netFigures, waitedMs, jumpMm, aPlayer);
}

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
		netFrames = NetFrameWatch();
		netFigures = NetFigureWatch();

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

		// How the frames went, and how often the figures had to wait, over the same time
		unsigned int const seconds = (now - netStatsLastLines + 500) / 1000;
		NetFrameWatch_Line(netFrames, seconds, words, sizeof(words));
		LogString(words);
		NetFigureWatch_Line(netFigures, seconds, words, sizeof(words));
		LogString(words);
		netFrames = NetFrameWatch();
		netFigures = NetFigureWatch();

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

// For a test run only: the connection, for a test to send something an honest game never would
ENetHost *HudsonTest_TheConnection()
{
	return host;
}

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

	// Nor a handshake its answers (protocol version 2, added 2026-11-09 by Melanikus for the AvP port)
	RelayEndToEndFrame();
	RelayV2JoinerFrame();   // and a joiner's channel to the relay (step 7)
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

// Modified 2026-11-09 by Melanikus for the AvP port: whether a network game is going (hosting, joined, or joining), which goes on while its
// window isn't the one in front (win_func.cpp, CheckForWindowsMessages). Everything that keeps it in the game runs on the game's own thread.
// A single player game, and a skirmish, which has no network, still wait.
bool Net_GameGoesOnWhileInactive(void)
{
	return host != NULL;
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

		// Protocol version 2's handshakes, on either side (added 2026-11-09 by Melanikus for the AvP port): their resends and timers.
		// Nothing unless this game uses a relay.
		RelayEndToEndFrame();
		RelayV2JoinerFrame();   // and a joiner's channel to the relay (step 7, added 2026-11-09 by Melanikus for the AvP port)

		// A host's own upkeep: connections that never said who they are, and the older versions told no. (The answers to a search on this
		// network went on 2026-11-10, with the search: see Net_FindAvPSessions.)
		if (AvP.Network == I_Host) {
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

// Added 2026-11-09 by Melanikus for the AvP port (protocol version 2): the check codes for the screens (networking.h). Each copies the six
// digits a handshake made, and only once they're whole.
static bool CopyCheckCode(const char *code, char out[kNetRelayCheckCodeBytes])
{
	if (!code || strnlen(code, kNetRelayCheckCodeBytes) != kNetRelayCheckCodeBytes - 1)
	{
		return false;
	}

	memcpy(out, code, kNetRelayCheckCodeBytes - 1);
	out[kNetRelayCheckCodeBytes - 1] = 0;
	return true;
}

bool Net_RelayCheckCodeNow(char out[kNetRelayCheckCodeBytes])
{
	if (!out)
	{
		return false;
	}

	out[0] = 0;
	return relayEndToEndPort != 0 && relayEndToEndJoiner.Ready() && CopyCheckCode(relayEndToEndJoiner.CheckCode(), out);
}

bool Net_JoinRequestCheckCode(NetJoinWho who, char out[kNetRelayCheckCodeBytes])
{
	if (!out)
	{
		return false;
	}

	out[0] = 0;
	ENetPeer *const asking = JoinRequestPeer();

	if (!asking || who.connection != (int)joinRequestConnection || who.connectID != joinRequestConnectID || !PeerIsThroughRelay(asking))
	{
		return false;
	}

	NetPeerHostSide *const side = RelayEndToEndSide(asking->address.port, false, enet_time_get());
	return side && side->Ready() && CopyCheckCode(side->CheckCode(), out);
}

// Added 2026-11-09 by Melanikus for the AvP port (#21): this run's identity, for the access page (net_identity_run.h)
NetIdentityStoreResult Net_KeptIdentityResult(void)
{
	RelayIdentityReady();
	return runIdentityKept;
}

bool Net_OurIdentity(uint8_t statics[kNetIdentityStaticBytes], uint8_t fingerprint[kNetIdentityFingerprintBytes])
{
	if (!statics || !fingerprint)
	{
		return false;
	}

	if (!RelayIdentityReady())
	{
		memset(statics, 0, kNetIdentityStaticBytes);
		memset(fingerprint, 0, kNetIdentityFingerprintBytes);
		return false;
	}

	memcpy(statics, runIdentity.statics, kNetIdentityStaticBytes);
	memcpy(fingerprint, runIdentity.fingerprint, kNetIdentityFingerprintBytes);
	return true;
}

bool Net_MakeNewIdentity(void)
{
	// Not while a network game is open: its handshakes and sealed datagrams belong to the identity they began with
	if (host)
	{
		LogString("net: a new identity isn't made while a network game is open");
		return false;
	}

	NetIdentity_Free(&runIdentity);
	runIdentityLookedFor = true;

	if (!NetIdentityStore_MakeAndKeep(&runIdentity))
	{
		LogString("net: the new identity the player asked for couldn't be made and kept, so there's none this run");
		return false;
	}

	runIdentityKept = kNetIdentityStoreLoaded;
	LogString("net: a new identity was made for this player, as they asked, and kept in place of any kept before");
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

					// A Hudson run's log says so too, in the same words, with no address either
					if (NetLoopbackOnly)
					{
						LogString("net: " + sstream.str());
					}

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

						// A Hudson run's log says what arrives, thinned out as the drops are (LogThisDrop), so a long run doesn't fill it
						if (NetLoopbackOnly)
						{
							static unsigned long long messagesArrived = 0;

							if (LogThisDrop(messagesArrived))
							{
								std::stringstream arrived;
								arrived << "net: message " << messagesArrived << " arrived, " << dataSize << " bytes";
								LogString(arrived.str());
							}
						}
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

					// Modified 2026-11-09 by Melanikus for the AvP port (protocol version 2): a joiner through the relay has gone, so its
					// handshake ends and its keys go
					if (AvP.Network == I_Host && PeerIsThroughRelay(eEvent.peer))
					{
						RelayEndToEndJoinerLeft(eEvent.peer->address.port);
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

							if (NetLoopbackOnly)
							{
								LogString("net: the other player has left");
							}
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

// (Removed 2026-11-10 by Melanikus for the AvP port, the owner's ruling for beta.2, "Everything over nyc3": Net_ConnectToThisMachine, a
// Hudson run's plain join to the game on this machine. A run joins through the relay, as a player does.)

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
	relayStrangersDropped = 0;
	RelayEndToEndForget();   // the last game's handshakes and channel, if anything kept them (added 2026-11-09 by Melanikus for the AvP port)

	ServerAddress.host = SessionData[sessionNumber].host;
	ServerAddress.port = SessionData[sessionNumber].port;

	// create ENet client. It has no address of its own, because nobody needs to find it: it talks to its host, and gets whatever port is free. That also means two games on one machine don't fight over a port.
	// A Hudson run with net= is the exception: it asks for this machine's own address, on any free port, so that nothing is open on any network the machine is on.
	// With one relay beyond this machine given to the run, its address towards that relay instead (LoopbackOnlyAddress).
	ENetAddress ourAddress;

	LoopbackOnlyAddress(&ourAddress);
	ourAddress.port = 0;

	// Modified 2026-11-10 by Melanikus for the AvP port (the owner's ruling for beta.2: "Everything over nyc3"): a game is joined only through the relay, sealed. One
	// that isn't, from wherever it came, is plain, and refused before anything is opened, with the reason (Net_JoinRefusedWhy).
	joinRefusedWhy = NULL;

	if (!SessionData[sessionNumber].throughRelay)
	{
		joinRefusedWhy = kNotThroughTheList;
		Con_PrintError(std::string("Net_ConnectToSession - ") + kNotThroughTheList);
		return NET_FAIL;
	}

	// Added 2026-11-11 by Melanikus for the AvP port: a game another line's server listed isn't joined tonight, as this run has the relay
	// keys of the line it uses alone. Refused before anything is opened, with the reason.
	if (!OnTheLineInUse(SessionData[sessionNumber].serverLine))
	{
		joinRefusedWhy = kOnAnotherServer;
		Con_PrintError(std::string("Net_ConnectToSession - ") + kOnAnotherServer);
		return NET_FAIL;
	}

	// One connection, to the host, with the game's limits: see net_host.h
	host = Net_MakeENetHost(NetLoopbackOnly ? &ourAddress : NULL, 1, kMostMessageBytes, incomingBandwidth, outgoingBandwidth);

	if (host == NULL)
	{
		Con_PrintError("Net_ConnectToSession - Failed to create Enet client");
		return NET_FAIL;
	}

	// Hudson's branch: a joiner on the way to the one relay beyond this machine hears that relay alone, from the start (RelayIntercept)
	if (NetLoopbackOnly && OnTheRemoteRoute(ourAddress))
	{
		host->intercept = RelayIntercept;
	}

	// Modified 2026-11-08 by Melanikus for the AvP port: a game from the relays' list. The relay's handshake goes first, from this new
	// host's own socket, and then the game connects to its port on the relay, as if that were the host (net_relay.h).
	if (SessionData[sessionNumber].throughRelay)
	{
		uint16_t const relayPort = RelayJoin(SessionData[sessionNumber].relayRegion, SessionData[sessionNumber].relayCode);

		if (relayPort == 0)
		{
			Con_PrintError(std::string("Net_ConnectToSession - couldn't join through the relay: ") + Net_RelayJoinFailedWhy());
			RelayEndToEndForget();   // every way a join fails forgets and wipes (Secondary's finding 4.2; added 2026-11-09 by Melanikus for the AvP port)
			enet_host_destroy(host);
			host = NULL;
			return NET_FAIL;
		}

		ServerAddress = relayAddress;
		ServerAddress.port = relayPort;

		// Modified 2026-11-09 by Melanikus for the AvP port (protocol version 2): handshake B with the host the list named, before ENet
		// sends a byte. A failure ends the join: nothing is ever sent through the relay unsealed.
		relayEndToEndJoinFailed = RelayEndToEndJoin(SessionData[sessionNumber].relayRegion, relayPort, SessionData[sessionNumber].relayHostId);

		if (relayEndToEndJoinFailed)
		{
			Con_PrintError(std::string("Net_ConnectToSession - couldn't join through the relay: ") + relayEndToEndJoinFailed);
			RelayEndToEndForget();
			enet_host_destroy(host);
			host = NULL;
			return NET_FAIL;
		}

		// For the line in play saying which relay this game goes through (Net_RelayPlayStateNow)
		relayJoinedThrough = true;
		strncpy(relayJoinedRegion, SessionData[sessionNumber].relayRegion, kRelayListRegionLetters);
		relayJoinedRegion[kRelayListRegionLetters] = 0;
	}

	// The CONNECT's data is 0, and a host doesn't look at it (modified 2026-11-10 by Melanikus for the AvP port, #175 PR 3a: the v2 draft's
	// same-commit word, JBLD, went with the draft; network version 116 is what two builds must share)
	ServerPeer = enet_host_connect(host, &ServerAddress, kNetChannels, 0);
	if (ServerPeer == NULL)
	{
		Con_PrintError("Net_ConnectToSession - Failed to connect to server host");
		RelayEndToEndForget();   // B's keys and the channel go too (Secondary's finding 4.2; added 2026-11-09 by Melanikus for the AvP port)
		relayJoinedThrough = false;
		enet_host_destroy(host);
		host = NULL;
		return NET_FAIL;
	}

	enet_peer_timeout(ServerPeer, 0, kConnectionPatienceMilliseconds, kConnectionPatienceMilliseconds);

	ENetEvent eEvent;

	/* see if we actually connected */
	/* Wait up to 3 seconds for the connection attempt to succeed. */
	// Modified 2026-11-09 by Melanikus for the AvP port (protocol version 2): through a relay, the 3 seconds go in 50 ms slices, with the
	// handshake's turn between them, so B5 goes again if it was lost (the host opens nothing until B5 has checked). It ends at the first
	// event, as the one wait did, and by its number of turns too, in case the clock stands still.
	bool connected = false;

	if (SessionData[sessionNumber].throughRelay)
	{
		uint32_t const waitStarted = enet_time_get();

		for (size_t turn = 0; turn < 3000 / kRelayWaitMs + 100 && enet_time_get() - waitStarted < 3000; turn++)
		{
			int const got = enet_host_service(host, &eEvent, kRelayWaitMs);

			if (got != 0)
			{
				connected = got > 0 && eEvent.type == ENET_EVENT_TYPE_CONNECT;
				break;
			}

			RelayEndToEndFrame();
		}
	}
	else
	{
		connected = (enet_host_service(host, &eEvent, 3000) > 0) && (eEvent.type == ENET_EVENT_TYPE_CONNECT);
	}

	if (connected)
	{
		Con_PrintDebugMessage("Net_ConnectToSession - we connected to server!");
	}
	else
	{
		Con_PrintError("Net_ConnectToSession - failed to connect to server!");

		RelayEndToEndForget();   // the handshake's keys go too (protocol version 2, added 2026-11-09 by Melanikus for the AvP port)
		relayJoinedThrough = false;

		// There's no game, so nothing should stay open
		enet_host_destroy(host);
		host = NULL;
		ServerPeer = NULL;
		return NET_FAIL;
	}

	// Modified 2026-11-09 by Melanikus for the AvP port (Secondary's finding 4.2): a join that fails here ends as the failures above do.
	// Before, the connection, the channel to the relay and B's keys all stayed, and the channel went on talking to the relay after the
	// player was told the join had failed.
	if (!Net_CreatePlayer(playerName)) {
		RelayEndToEndForget();
		relayJoinedThrough = false;
		enet_host_destroy(host);
		host = NULL;
		ServerPeer = NULL;
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

void Net_FindAvPSessions()
{
	// Modified 2026-11-10 by Melanikus for the AvP port (the owner's ruling for beta.2: "Everything over nyc3"): nothing on this network
	// is asked about games, neither the whole network nor a typed address, nothing answers, and no socket is opened for it (see the note
	// above NetSettingOrOwn): every game is played through a server's relay, and found on its online list. A search only starts the
	// list of games found again, empty, for the games page to fill from the online list.
	NumberOfSessionsFound = 0;
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
