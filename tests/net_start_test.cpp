// Tests for three things in 3dc/networking.cpp: what the game has open on the network, what it does with a message it has nobody to send to, and who gets into a game and what they can say once they're in. Written 2026-11-06 by Melanikus for the AvP port.
//
// This runs the real file. Only ENet and the rest of the game are stand-ins, in tests/net_start_test_stubs, and they open nothing: they count what was asked for, and the clock is the test's.
//
// WHY THESE THREE: only a client has a server to send to, so anything else asking to send has to be told no. A game that isn't doing anything on a network has no reason to have anything open on one. And anyone who can reach a host can connect and send anything at all, so it believes nothing about who sent what except which connection it came on.
// Modified 2026-11-07 by Melanikus for the AvP port:
//   every ENet host the game makes has the game's limits and one channel (3dc/net_host.cpp), and a message too short for a header is refused before it's copied
//   leaving closes each connection after what's waiting, while the ENet host is still there (3dc/net_close.h), and a client says in the log when the host's connection closes

// Modified 2026-11-08 by Melanikus for the AvP port:
//   its Between is the shared one in source_slice.h, so a marker that isn't there is a failed check of its own (SOURCE-SLICE-1, #128)
#include "enet/enet.h"
#include "networking.h"
#include "net_checks.h"
#include "net_relay.h"
#include "net_peer_v2.h"
#include "net_relay_v2.h"     // this test's stand-in (net_start_test_stubs), not 3dc's: see there
#include "net_relay_keys.h"
#include "net_identity_run.h"
#include "relay_list.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <fstream>
#include <map>
#include <memory>
#include <vector>
#include "source_slice.h"

// menus.cpp says this for itself, and it isn't in networking.h

// What anyone looking for a game is told about this one
extern SessionDescription netSession;

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	fflush(stdout);
	checksFailed += good ? 0 : 1;
}

// The game
uint32_t fakeTime = 1000000;
FakeAvP AvP = { I_No_Network };
FakeNetGameData netGameData = { 0, 0, 0, 0 };
int LobbiedGame = 0;
SESSION_DESC SessionData[MAX_NO_OF_SESSIONS];
uint32_t NumberOfSessionsFound = 0;
unsigned char DebouncedKeyboardInput[256];

static int errorLines = 0;
static std::vector<std::string> commands;
static int gamesStartedForHost = 0;

void Con_AddCommand(char *commandName, funcPointer)
{
	commands.push_back(commandName);
}

void Con_PrintError(const std::string &)
{
	errorLines++;
}

void Con_PrintMessage(const std::string &)
{
}

// Modified 2026-11-08 by Melanikus for the AvP port: the relays' list, Fourth's (relay_list.h, PROTOCOL.md 7014de1). Which relays it names,
// none unless a test says so, and what the game told the list.
static bool relayNamed = true;   // modified 2026-11-11: there's no plain game, so a line's relay is named unless a test says otherwise
static int roomsListed = 0, passesListed = 0, sessionsListed = 0, roomsUnlisted = 0;

// The relay's address, as the list hands it over already looked up: a made-up one, so a test can see what goes to it
static enet_uint32 const kRelayHost = 0x0D0C0B0A;
static enet_uint32 relayAddressNamed = kRelayHost;
static std::string regionListed;
static int hostMsListed = -1;
static uint64_t passTimeListed = 0;

bool RelayList_Relays(RelayListRelay *relays, int *count)
{
	if (!relayNamed)
	{
		return false;
	}

	memset(&relays[0], 0, sizeof(relays[0]));
	strcpy(relays[0].region, "virginia");
	strcpy(relays[0].host, "relay.example");
	relays[0].port = 27500;
	relays[0].address = relayAddressNamed;
	*count = 1;
	return true;
}

// Added 2026-11-10 by Melanikus for the AvP port (#188): the server servers.cfg names, and where it stands. There's a server when the list
// names relays (relayNamed), and it's good then, and still to answer when it doesn't, unless a test says otherwise.
static int serverInTheFile = -1;          // -1: as relayNamed says; 0: no server in the file; 1: a server
static int serverStateGiven = -1;         // -1: as relayNamed says; otherwise the kRelayServer... the stand-in gives
static const char *serverStateWhy = NULL; // with serverStateGiven
static int serverAsked = 0;               // RelayList_FindRelays's calls

const char *Relay_ServerName(void)
{
	bool const set = serverInTheFile < 0 ? relayNamed : serverInTheFile == 1;
	return set ? "Test server" : "";
}

int RelayList_ServerState(const char **why)
{
	*why = NULL;

	if (serverStateGiven >= 0)
	{
		*why = serverStateWhy;
		return serverStateGiven;
	}

	return relayNamed ? kRelayServerGood : kRelayServerAsking;
}

void RelayList_FindRelays(void)
{
	serverAsked++;
}

// networking.cpp asks whether the line a game was listed on is the one this run uses (OnTheLineInUse; added 2026-11-11 by Melanikus for
// the AvP port, every server in servers.cfg going through the same page). Here the first good line is the one in use, and there's no
// other. A game put on the list with no line (-1) is on the line in use, as every game here is, and never asks.
bool RelayList_LineAt(int place, RelayListLineShown *out)
{
	memset(out, 0, sizeof(*out));
	out->inUse = place == 0;
	return place == 0;
}

void RelayList_HostRoomOpened(const char *region, const char *, const uint8_t *, uint64_t passTime, int hostMs, const SessionDescription &)
{
	roomsListed++;
	regionListed = region;
	hostMsListed = hostMs;
	passTimeListed = passTime;
}

void RelayList_HostKept(const uint8_t *, uint64_t passTime, int)
{
	passesListed++;
	passTimeListed = passTime;
}

void RelayList_HostSessionNow(const SessionDescription &)
{
	sessionsListed++;
}

void RelayList_HostRoomClosed(void)
{
	roomsUnlisted++;
}

// Added 2026-11-09 by Melanikus for the AvP port (protocol version 2's draft): the whole id a host hands the list for its listing, which
// every online host does (modified 2026-11-10, #175 PR 3a: version 2 is every game's protocol)
static int identitiesListed = 0;

void RelayList_HostIdentity(const uint8_t hostId[kNetIdentityIdBytes])
{
	identitiesListed += hostId ? 1 : 0;
}

// Added 2026-11-09 by Melanikus for the AvP port (protocol version 2's draft, step 7): the list's side of a relay's keys, and handshake A's
// draws. The list gives the test's own made-up keys. So the channel to the relay opens by itself (with the stand-in
// session, net_start_test_stubs/net_relay_v2.h), and the relay tests from before step 7 still mean what they meant. TestTheRelayChannel says
// otherwise, to see each rule. Modified 2026-11-10 by Melanikus for the AvP port (#175): the real list hands over only keys its server's
// servers.cfg line has proved (relay_list.h), so there's no RelayKey setting here to hold them to.
static NetRelayStaticKeys testRelayKeys;
static int relayKeysGive = kRelayKeysCame;   // what the list answers: the keys, still waiting, or refused
static int relayKeysAsked = 0;
static int drawsMade = 0;

static void MakeTheTestRelayKeys()
{
	for (size_t i = 0; i < sizeof(testRelayKeys.p384); i++)
	{
		testRelayKeys.p384[i] = static_cast<uint8_t>(4 + i * 7);
	}

	for (size_t i = 0; i < sizeof(testRelayKeys.kem); i++)
	{
		testRelayKeys.kem[i] = static_cast<uint8_t>(9 + i * 3);
	}
}

bool RelayList_AskForRelayKeys(const char *region)
{
	relayKeysAsked++;
	return region != NULL;
}

int RelayList_RelayKeysCame(uint8_t p384[kNetCryptoP384PublicBytes], uint8_t kem[kNetPqKemPublicBytes], const char **why)
{
	memset(p384, 0, kNetCryptoP384PublicBytes);
	memset(kem, 0, kNetPqKemPublicBytes);
	*why = relayKeysGive == kRelayKeysRefused ? "the list wouldn't give the relay's keys" : NULL;

	if (relayKeysGive == kRelayKeysCame)
	{
		memcpy(p384, testRelayKeys.p384, kNetCryptoP384PublicBytes);
		memcpy(kem, testRelayKeys.kem, kNetPqKemPublicBytes);
	}

	return relayKeysGive;
}

// Added 2026-11-11 by Melanikus for the AvP port (one model for every line): the per-line calls networking.cpp makes. Here there's one
// good line, place 0, whenever the stand-ins above say servers.cfg names a server (Relay_ServerName). Its relay is the one the list hands
// over above (relayAddressNamed, at 27500) while relayNamed says the list names it; its keys are the test's own, as above; and it's the
// line hosted on, unless a test says otherwise (hostLineGiven). A line's state is what RelayList_ServerState's stand-in says, and this PC
// is approved on it.
static int hostLineGiven = -2;       // -2: the first line while relayNamed says so, else none; otherwise the line RelayList_HostLine gives
static int linesInTheFile = 1;       // how many good lines there are when servers.cfg names a server; a second line's relay is elsewhere
static enet_uint32 const kSecondRelayHost = 0x0D0C0B0B;
static int relayOfAsked = -1;        // the line whose relay was read last
static int keysComeForLine = -1;     // -1: a relay's keys come for the line asked; otherwise the line they come saying they're for
static int keysAskedOnLine = -1;     // the line the newest asking for a relay's keys was for
static std::string keysAskedRegion;  // and the region it named ("" for a host's, which gets its relay's own region back)

int RelayList_LineCount(void)
{
	return Relay_ServerName()[0] ? linesInTheFile : 0;
}

bool RelayList_RelayOf(int place, uint32_t *address, int *port)
{
	relayOfAsked = place;

	if (place < 0 || place >= RelayList_LineCount() || !relayNamed || !address || !port)
	{
		return false;
	}

	*address = place == 0 ? relayAddressNamed : kSecondRelayHost;
	*port = 27500;
	return true;
}

bool RelayList_AskForRelayKeysOn(int place, const char *region)
{
	relayKeysAsked++;
	keysAskedOnLine = place;
	keysAskedRegion = region ? region : "";
	return place >= 0 && place < RelayList_LineCount() && relayNamed;
}

int RelayList_RelayKeysCameOn(int *place, char region[kRelayListRegionLetters + 1], uint8_t p384[kNetCryptoP384PublicBytes],
	uint8_t kem[kNetPqKemPublicBytes], const char **why)
{
	*place = keysComeForLine >= 0 ? keysComeForLine : keysAskedOnLine;
	strcpy(region, "virginia");
	return RelayList_RelayKeysCame(p384, kem, why);
}

int RelayList_HostLine(void)
{
	return hostLineGiven == -2 ? (relayNamed ? 0 : -1) : hostLineGiven;
}

bool RelayList_LineNow(int place, RelayListLineNow *out)
{
	memset(out, 0, sizeof(*out));

	if (place < 0 || place >= RelayList_LineCount())
	{
		return false;
	}

	strcpy(out->line.name, place == 0 ? "Test server" : "Second server");
	out->lineNumber = place + 1;
	out->pingMs = -1;
	out->access = kRelayAccessIsApproved;

	const char *why = NULL;
	int const state = RelayList_ServerState(&why);
	out->state = state == kRelayServerGood ? kRelayLineAnswered : state == kRelayServerNeedsYes ? kRelayLineNoYes
		: state == kRelayServerCantUse ? kRelayLineNoAnswer : kRelayLineAsking;
	out->why = state == kRelayServerCantUse ? why : NULL;
	return true;
}

bool NetHandshakeA_Draw(const NetRelayStaticKeys *, NetHandshakeADraws *out)
{
	drawsMade++;

	if (out)
	{
		memset(out, 0, sizeof(*out));
	}

	return out != NULL;
}

void NetHandshakeA_WipeDraws(NetHandshakeADraws *draws)
{
	if (draws)
	{
		memset(draws, 0, sizeof(*draws));
	}
}

void Con_PrintDebugMessage(const std::string &)
{
}

// What the test's settings file says. A setting that isn't in it gets the game's own number, as with a file that doesn't have the line.
static std::map<std::string, int> settingsFile;

int Config_GetInt(const std::string &, const std::string &variable, int defaultValue)
{
	const std::map<std::string, int>::const_iterator found = settingsFile.find(variable);

	return found == settingsFile.end() ? defaultValue : found->second;
}

// What the game put on the screen, so a test can see it never shows an address
static std::vector<std::string> screenLines;

void NewOnScreenMessage(char *message)
{
	screenLines.push_back(message ? message : "");
}

void MinimalNetCollectMessages()
{
}

void InitAVPNetGameForHost(int, int, int)
{
	gamesStartedForHost++;
}

void InitAVPNetGameForJoin()
{
}

// The game's list of players, which is pldnet.cpp's. Its real checks are looked at below, in TestTheGameUsesThis.
struct FakePlayer
{
	NetID id;
	std::string name;
};

static std::vector<FakePlayer> playersInGame;
static std::vector<NetID> playersGone;
static int playersAsked = 0;
static bool gameHasRoom = true;
static int hostHasGoneCalls = 0;

bool AddPlayerToGame(NetID id, const char *name)
{
	playersAsked++;

	if (!gameHasRoom)
	{
		return false;
	}

	FakePlayer player;

	player.id = id;
	player.name = name;
	playersInGame.push_back(player);
	return true;
}

void Net_PlayerHasGone(NetID id)
{
	playersGone.push_back(id);

	for (size_t i = 0; i < playersInGame.size(); i++)
	{
		if (playersInGame[i].id == id)
		{
			playersInGame.erase(playersInGame.begin() + i);
			break;
		}
	}
}

void Net_HostHasGone()
{
	hostHasGoneCalls++;
}

// Whether a message is made up properly is pldnet.cpp's to say, and that's tested in net_checks_test and in TestTheTableOfMessageParts below. Here it's only what networking.cpp does with the answer.
static bool gameMessagesAreGood = true;
static bool lastAskedFromTheHost = false;
static size_t lastAskedSize = 0;

static bool clientCanAfford = true;

// The co-op lobby's state, which pldnet.cpp keeps. It's here so a check can say a lobby is open.
static int lobbyStateForTheTest = 0;

int Net_CoopLobbyState(void)
{
	return lobbyStateForTheTest;
}

void Net_CoopLobbyJoined(int state)
{
	lobbyStateForTheTest = state;
}

bool Net_MessageIsForTheHostAlone(const uint8_t *, size_t)
{
	return false;
}

void Net_NoteRefused(const char *)
{
}

bool Net_ClientCanAfford(const uint8_t *, size_t, size_t)
{
	return clientCanAfford;
}

bool Net_GameMessageIsGood(const uint8_t *, size_t size, bool fromTheHost)
{
	lastAskedFromTheHost = fromTheHost;
	lastAskedSize = size;
	return gameMessagesAreGood;
}

int DetermineAvailableCharacterTypes(bool)
{
	return 4;
}

char *GetCustomMultiplayerLevelName(int, int)
{
	static char name[] = "a level";
	return name;
}

bool IsCustomLevel(int, int)
{
	return false;
}

int GetLocalMultiplayerLevelIndex(int, char *, int)
{
	return 0;
}

// ENet, counting
static int socketsCreated = 0, socketsDestroyed = 0, socketsOpen = 0;
static int waitsOnASocket = 0, waitsOnNoSocket = 0, datagramsSent = 0;
static int channelSends = 0, sealedSends = 0;   // the stand-in channel's own datagrams, and the messages it sealed (step 7)
static bool socketCreateFails = false, socketBindFails = false, datagramSendFails = false, hostCreateFails = false, packetCreateFails = false;
static std::vector<ENetAddress> socketsBoundTo, datagramsSentTo;

static int hostsCreated = 0, hostsDestroyed = 0, hostsWithNoAddress = 0;
static size_t connectionsAllowed = 0;
// How many channels a host would give each connection, and how many a client asked for
static size_t channelsAllowed = 0, channelsAsked = 0;
static std::vector<ENetAddress> hostsBoundTo;
static bool connectGetsAnAnswer = true, connectCantStart = false;

static const int kFakePeers = 32;
static ENetHost theHost;
static ENetPeer peers[kFakePeers];
static ENetPeer &thePeer = peers[0];
static bool peerIsLive = false;
static int packetsMade = 0, packetsToAPeer = 0, packetsToAPeerThatsGone = 0, packetsAlive = 0;
static int flushes = 0, closedAtOnce = 0, closedGently = 0, flushesWhenLastClosed = 0;
static uint32_t lastMessageSent = 0xFFFFFFFF;

// What's been sent, and on which connection, for the tests to look at
struct SentMessage
{
	ENetPeer *to;
	MessageHeader header;
	std::vector<uint8_t> body;
};

static std::vector<SentMessage> sentMessages;
static std::vector<ENetPacket *> packetsOnTheirWay;
static std::deque<ENetEvent> eventsWaiting;

int enet_initialize()
{
	return 0;
}

void enet_deinitialize()
{
}

// Every packet the stand-in ENet makes, with its bytes, is held here from when it's made until ENet is asked to destroy it. So none
// can be lost on the way, whichever way a test goes. Destroying one really frees it, so a later use of it is still a use of freed
// memory, which the address sanitizer sees (tests/takeouts/sanitize.ps1). Destroying one that isn't held, or the same one twice,
// is counted, and the last check says if it ever happened.
struct StandInPacket
{
	ENetPacket packet;
	std::vector<uint8_t> bytes;
};

static std::map<ENetPacket *, std::unique_ptr<StandInPacket>> packetsHeld;
static int packetsDestroyedThatWerentHeld = 0;

static ENetPacket *MakePacket(const void *data, size_t dataLength)
{
	std::unique_ptr<StandInPacket> made = std::make_unique<StandInPacket>();

	made->bytes.resize(dataLength ? dataLength : 1);

	if (data && dataLength)
	{
		memcpy(made->bytes.data(), data, dataLength);
	}

	made->packet.data = made->bytes.data();
	made->packet.dataLength = dataLength;
	made->packet.referenceCount = 0;

	ENetPacket *const packet = &made->packet;

	packetsHeld[packet] = std::move(made);
	packetsAlive++;
	return packet;
}

void enet_packet_destroy(ENetPacket *packet)
{
	// Written "const auto", not "iterator const": Snyk Code can't read a file with a type from inside a template followed by const
	const auto held = packetsHeld.find(packet);

	if (held == packetsHeld.end())
	{
		packetsDestroyedThatWerentHeld++;
		return;
	}

	packetsHeld.erase(held);
	packetsAlive--;
}

// As the real one does, each packet is freed when the last connection it was sent on has had it
static void EverythingArrives()
{
	for (ENetPacket *const packet : packetsOnTheirWay)
	{
		packet->referenceCount--;

		if (packet->referenceCount == 0)
		{
			enet_packet_destroy(packet);
		}
	}

	packetsOnTheirWay.clear();
}

// The speeds the last host was made with, in bytes a second
static enet_uint32 hostIncomingBandwidth = 0;
static enet_uint32 hostOutgoingBandwidth = 0;

ENetHost *enet_host_create(const ENetAddress *address, size_t peerCount, size_t channelLimit, enet_uint32 incomingBandwidth, enet_uint32 outgoingBandwidth)
{
	if (hostCreateFails)
	{
		return NULL;
	}

	hostsCreated++;
	hostIncomingBandwidth = incomingBandwidth;
	hostOutgoingBandwidth = outgoingBandwidth;
	connectionsAllowed = peerCount;
	channelsAllowed = channelLimit;

	// What real ENet starts a host with (enet_host_create in ENet's host.c), so a check can see the game set its own:
	// any number of places from one address, 32 MB messages, 32 MB waiting, and no checksum
	theHost.duplicatePeers = 0xFFF;
	theHost.maximumPacketSize = 32 * 1024 * 1024;
	theHost.maximumWaitingData = 32 * 1024 * 1024;
	theHost.checksum = NULL;
	theHost.olderConnectsRefused = 0;
	theHost.socket = 77;
	theHost.intercept = NULL;
	theHost.sendHook = NULL;   // patch 007's hook: none until the game sets one (added 2026-11-09 by Melanikus for the AvP port)
	theHost.address.host = address ? address->host : ENET_HOST_ANY;
	theHost.address.port = address ? address->port : 0;

	if (address)
	{
		hostsBoundTo.push_back(*address);
	}
	else
	{
		hostsWithNoAddress++;
	}

	for (ENetPeer &peer : peers)
	{
		peer.state = ENET_PEER_STATE_DISCONNECTED;
		peer.data = NULL;
	}

	theHost.peers = peers;
	theHost.peerCount = peerCount;
	peerIsLive = true;
	return &theHost;
}

void enet_host_destroy(ENetHost *)
{
	hostsDestroyed++;

	// A host owns its peers, so these are gone. The real one's memory is freed here, so nothing should touch it again.
	peerIsLive = false;

	for (ENetPeer &peer : peers)
	{
		peer.state = ENET_PEER_STATE_DISCONNECTED;
	}

	EverythingArrives();

	for (ENetEvent const &event : eventsWaiting)
	{
		if (event.packet)
		{
			enet_packet_destroy(event.packet);
		}
	}

	eventsWaiting.clear();
}

static ENetAddress lastConnectTo;   // where the game last connected to (the relay's tests)
static enet_uint32 lastConnectData = 0xFFFFFFFFu;   // and what its CONNECT said: always 0 since the v2 draft's commit word went (#175 PR 3a)

ENetPeer *enet_host_connect(ENetHost *, const ENetAddress *address, size_t channelCount, enet_uint32 data)
{
	channelsAsked = channelCount;
	lastConnectData = data;

	if (address)
	{
		lastConnectTo = *address;
	}


	if (connectCantStart)
	{
		return NULL;
	}

	thePeer.state = ENET_PEER_STATE_CONNECTED;
	return &thePeer;
}

static int servicesWithNowhereForAnEvent = 0;

int enet_host_service(ENetHost *, ENetEvent *event, enet_uint32)
{
	if (!event)
	{
		servicesWithNowhereForAnEvent++;
	}

	if (event && connectGetsAnAnswer)
	{
		event->type = ENET_EVENT_TYPE_CONNECT;
		event->peer = &thePeer;
		event->packet = NULL;
		return 1;
	}

	return 0;
}

int enet_host_check_events(ENetHost *, ENetEvent *event)
{
	if (eventsWaiting.empty())
	{
		return 0;
	}

	*event = eventsWaiting.front();
	eventsWaiting.pop_front();
	return 1;
}

// How many times the game broadcast, and how many of those with no host
static int broadcasts = 0, broadcastsWithNoHost = 0;

void enet_host_flush(ENetHost *)
{
	flushes++;
}

// A header has a gap inside it that nothing fills, between isSystemMessage and messageType, and the whole of it goes on the wire. What was on this computer's stack mustn't go out in that gap.
static int headersLookedAt = 0;
static int headersWithSomethingInTheGap = 0;

static void LookAtTheGapInAHeader(const void *data)
{
	unsigned char const *const bytes = (unsigned char const *)data;

	headersLookedAt++;

	for (size_t i = offsetof(MessageHeader, isSystemMessage) + 1; i < offsetof(MessageHeader, messageType); i++)
	{
		if (bytes[i] != 0)
		{
			headersWithSomethingInTheGap++;
			return;
		}
	}
}

// Leaves a mark all over the part of the stack the next call will use, so a header nobody cleaned shows it
static __declspec(noinline) void DirtyTheStack()
{
	volatile unsigned char dirt[16384];

	for (size_t i = 0; i < sizeof(dirt); i++)
	{
		dirt[i] = 0xCC;
	}
}

ENetPacket *enet_packet_create(const void *data, size_t dataLength, enet_uint32)
{
	if (packetCreateFails)
	{
		return NULL;
	}

	packetsMade++;

	if (data && dataLength >= sizeof(MessageHeader))
	{
		MessageHeader header;

		memcpy(&header, data, sizeof(header));
		lastMessageSent = header.messageType;
		LookAtTheGapInAHeader(data);
	}

	return MakePacket(data, dataLength);
}

int enet_peer_send(ENetPeer *peer, uint8_t, ENetPacket *packet)
{
	packetsToAPeer++;
	packetsToAPeerThatsGone += peerIsLive ? 0 : 1;

	// The real one refuses a connection that isn't there
	if (peer->state != ENET_PEER_STATE_CONNECTED)
	{
		return -1;
	}

	SentMessage sent;

	sent.to = peer;
	memset(&sent.header, 0, sizeof(sent.header));

	if (packet->dataLength >= sizeof(MessageHeader))
	{
		memcpy(&sent.header, packet->data, sizeof(MessageHeader));
		sent.body.assign(packet->data + sizeof(MessageHeader), packet->data + packet->dataLength);
	}

	sentMessages.push_back(sent);
	packet->referenceCount++;
	packetsOnTheirWay.push_back(packet);
	return 0;
}

void enet_host_broadcast(ENetHost *host, uint8_t channel, ENetPacket *packet)
{
	broadcasts++;
	broadcastsWithNoHost += host ? 0 : 1;

	// Real ENet would read through the empty pointer. Here it's counted, and a check above says so.
	if (!host)
	{
		return;
	}

	for (ENetPeer *peer = host->peers; peer < &host->peers[host->peerCount]; ++peer)
	{
		if (peer->state == ENET_PEER_STATE_CONNECTED)
		{
			enet_peer_send(peer, channel, packet);
		}
	}

	if (packet->referenceCount == 0)
	{
		enet_packet_destroy(packet);
	}
}

enet_uint32 enet_crc32(const ENetBuffer *, size_t)
{
	return 0;
}

static int peersReset = 0;

void enet_peer_reset(ENetPeer *peer)
{
	peersReset++;
	peer->state = ENET_PEER_STATE_DISCONNECTED;
}

// The reason the game last gave when it closed a connection gently (the request to join's, networking.h)
static enet_uint32 lastDisconnectData = 0;

void enet_peer_disconnect(ENetPeer *, enet_uint32 data)
{
	closedGently++;
	lastDisconnectData = data;
}

void enet_peer_disconnect_now(ENetPeer *peer, enet_uint32)
{
	closedAtOnce++;
	flushesWhenLastClosed = flushes;
	peer->state = ENET_PEER_STATE_DISCONNECTED;
}

static enet_uint32 enetClock = 0;

// What the address page holds. networking.cpp asks it on a search.
char IPAddressString[16] = "";

enet_uint32 enet_time_get()
{
	return enetClock;
}

// The game's log. networking.cpp writes a few lines there, and the tests read them back.
static std::vector<std::string> logLines;

void LogString(const std::string &line)
{
	logLines.push_back(line);
}

static bool LogHas(const std::string &line)
{
	for (std::string const &said : logLines)
	{
		if (said == line)
		{
			return true;
		}
	}

	return false;
}

// Leaving closes each connection with net_close.cpp, which needs the real ENet and has its own test, tests/net_close_test.cpp. Here
// it's only counted, with how many hosts had been let go when it was asked, so the order can be checked: a host that had already
// been destroyed would have no connections left to close, and its memory would be gone.
static int closesAsked = 0, hostsDestroyedWhenClosed = -1, connectionsThatDontAnswer = 0;

int Net_CloseConnectionsAfterWhatsWaiting(ENetHost *, unsigned int)
{
	closesAsked++;
	hostsDestroyedWhenClosed = hostsDestroyed;
	return connectionsThatDontAnswer;
}

void enet_peer_timeout(ENetPeer *, enet_uint32, enet_uint32, enet_uint32)
{
}

int enet_address_set_host_ip(ENetAddress *address, const char *)
{
	address->host = 0x0100007F;

	return 0;
}

// Writes the address as ENet would, four numbers with dots, so that any line made from it would show it
int enet_address_get_host_ip(const ENetAddress *address, char *hostName, size_t nameLength)
{
	if (nameLength > 0)
	{
		snprintf(hostName, nameLength, "%u.%u.%u.%u", address->host & 0xFF, (address->host >> 8) & 0xFF, (address->host >> 16) & 0xFF, address->host >> 24);
	}

	return 0;
}

ENetSocket enet_socket_create(int)
{
	if (socketCreateFails)
	{
		return ENET_SOCKET_NULL;
	}

	socketsCreated++;
	socketsOpen++;
	return 100 + socketsCreated;
}

int enet_socket_set_option(ENetSocket, int, int)
{
	return 0;
}

int enet_socket_bind(ENetSocket, const ENetAddress *address)
{
	if (socketBindFails)
	{
		return -1;
	}

	socketsBoundTo.push_back(*address);
	return 0;
}

// Hudson's branch, a run's one relay beyond this machine (networking.cpp, LoopbackOnlyAddress): this machine's address towards it, found by
// connecting a UDP socket to it, which sends nothing, and asking the socket where it is. A test says which address that gives; 0: none.
static enet_uint32 routeTowardsTheRelay = 0;
static int socketsConnected = 0;
static ENetAddress lastSocketConnectedTo;

int enet_socket_connect(ENetSocket, const ENetAddress *address)
{
	socketsConnected++;

	if (address)
	{
		lastSocketConnectedTo = *address;
	}

	return routeTowardsTheRelay != 0 ? 0 : -1;
}

int enet_socket_get_address(ENetSocket, ENetAddress *address)
{
	if (!address || routeTowardsTheRelay == 0)
	{
		return -1;
	}

	address->host = routeTowardsTheRelay;
	address->port = 50000;
	return 0;
}

void enet_socket_destroy(ENetSocket socket)
{
	// The real one is harmless with no socket, so this only counts the real ones
	if (socket != ENET_SOCKET_NULL)
	{
		socketsDestroyed++;
		socketsOpen--;
	}
}

// Every datagram the game sent, byte for byte, in order (the relay's tests)
static std::vector<std::vector<uint8_t> > datagramsSentBytes;

// What's arrived at the socket for finding games, for the game to read
struct WaitingDatagram
{
	std::vector<uint8_t> bytes;
	ENetAddress from;
};

static std::deque<WaitingDatagram> datagramsWaiting;

// The last thing the game sent from that socket, byte for byte
static std::vector<uint8_t> lastDatagramSent;

// Added 2026-11-09 by Melanikus for the AvP port (#21): the identity store's stand-in (net_identity_store.h). It finds what the test says,
// and gives back a whole identity, made fresh, where the real store would give the kept one or a new one it has kept. The real store, with
// Windows' sealing and the file, is net_identity_store_test's: here the test sees what networking.cpp does with each answer.
static NetIdentityStoreResult storeFinds = kNetIdentityStoreNoFile;
static bool storeKeepFails = false;
static int storeLoads = 0, storeMakes = 0;

NetIdentityStoreResult NetIdentityStore_Load(NetIdentity *out, const char **why)
{
	storeLoads++;

	if (why)
	{
		*why = storeFinds == kNetIdentityStoreLoaded || storeFinds == kNetIdentityStoreNoFile ? NULL : "the test's stand-in says so";
	}

	if (storeFinds == kNetIdentityStoreLoaded)
	{
		return NetIdentity_Make(out) ? kNetIdentityStoreLoaded : kNetIdentityStoreCantRead;
	}

	return storeFinds;
}

bool NetIdentityStore_MakeAndKeep(NetIdentity *out)
{
	storeMakes++;

	if (storeKeepFails || !NetIdentity_Make(out))
	{
		return false;
	}

	storeFinds = kNetIdentityStoreLoaded;   // kept, so the next run finds it
	return true;
}

const char *NetIdentityStore_Words(NetIdentityStoreResult result)
{
	return result == kNetIdentityStoreNoFile ? "none is kept yet" : "the kept one can't be used";
}

static bool AllZeroBytes(const uint8_t *bytes, size_t size)
{
	for (size_t i = 0; i < size; i++)
	{
		if (bytes[i] != 0)
		{
			return false;
		}
	}

	return true;
}

// Added 2026-11-09 by Melanikus for the AvP port (protocol version 2): the host behind the relay, as far as handshake B goes. A joiner runs
// B over its port on the relay before ENet connects, so a join through the relay needs a host that answers: this is the real host's side
// of B (net_peer_v2.h), with an identity of its own, whose whole id the test's games on the list carry. Whatever the game sends to that
// port as a handshake datagram it reads, and its answers arrive from that port, as through a real relay.
static NetIdentity scriptedHostIdentity;
static NetPeerHostSide scriptedHostSide;
static uint16_t const kScriptedHostPort = 27501;

static void ScriptedHostAnswers(const ENetAddress *to, const uint8_t *bytes, size_t size)
{
	if (!to || to->host != kRelayHost || to->port != kScriptedHostPort || size < 4 || memcmp(bytes, "AVPE", 4) != 0 || !scriptedHostIdentity.made)
	{
		return;
	}

	if (scriptedHostSide.Port() == 0)
	{
		scriptedHostSide.Start("virginia", kScriptedHostPort, &scriptedHostIdentity, enetClock);
	}

	static uint8_t opened[kNetSealedGameMostBytes];
	size_t written = 0;
	scriptedHostSide.Hear(bytes, size, opened, sizeof(opened), &written, enetClock);
	NetPeerSend sends[kNetPeerMostSends];
	size_t const count = scriptedHostSide.TakeSends(sends, kNetPeerMostSends);

	for (size_t i = 0; i < count; i++)
	{
		WaitingDatagram answer;
		answer.bytes.assign(sends[i].bytes, sends[i].bytes + sends[i].size);
		answer.from.host = kRelayHost;
		answer.from.port = kScriptedHostPort;
		datagramsWaiting.push_back(answer);
	}
}

int enet_socket_wait(ENetSocket socket, enet_uint32 *condition, enet_uint32)
{
	if (socket == ENET_SOCKET_NULL)
	{
		waitsOnNoSocket++;
		return -1;
	}

	waitsOnASocket++;
	*condition = datagramsWaiting.empty() ? 0 : ENET_SOCKET_WAIT_RECEIVE;
	return 0;
}

int enet_socket_receive(ENetSocket, ENetAddress *address, ENetBuffer *buffers, size_t)
{
	if (datagramsWaiting.empty())
	{
		return 0;
	}

	WaitingDatagram const waiting = datagramsWaiting.front();

	datagramsWaiting.pop_front();
	*address = waiting.from;
	memcpy(buffers->data, waiting.bytes.data(), waiting.bytes.size());
	return (int)waiting.bytes.size();
}

int enet_socket_send(ENetSocket, const ENetAddress *address, const ENetBuffer *buffers, size_t)
{
	if (datagramSendFails)
	{
		return -1;
	}

	// Modified 2026-11-09 by Melanikus for the AvP port (protocol version 2's draft, step 7): the stand-in channel's own datagrams. A
	// handshake datagram or a cookie ask ('AVPR' 'H' or 'C') is counted apart, so the checks of what the game sends keep their meaning. A
	// sealed message ('AVPR' 'S') is recorded as the message inside it, as the relay sees it once opened, and counted as sealed.
	uint8_t const *bytes = buffers ? (uint8_t const *)buffers->data : NULL;
	size_t size = buffers ? buffers->dataLength : 0;

	if (bytes && size >= 5 && memcmp(bytes, "AVPR", 4) == 0 && (bytes[4] == 'H' || bytes[4] == 'C'))
	{
		channelSends++;
		return static_cast<int>(size);
	}

	if (bytes && size == kNetRelayV2StandInBytes && memcmp(bytes, "AVPR", 4) == 0 && bytes[4] == 'S')
	{
		sealedSends++;
		bytes += 5;
		size = kNetSealedControlMessageBytes;
	}

	// A relay message (net_relay.h) has no game header to look at: its kind is where a header's gap is. Nor has a handshake datagram or a
	// sealed one (protocol version 2, 'AVPE' and 'AVPG'), modified 2026-11-09 by Melanikus for the AvP port.
	bool const endToEnd = bytes && size >= 4 && (memcmp(bytes, "AVPE", 4) == 0 || memcmp(bytes, "AVPG", 4) == 0);

	if (bytes && size >= sizeof(MessageHeader) && !endToEnd && NetRelay_Read(bytes, size, NULL) != kNetRelayIsAMessage)
	{
		LookAtTheGapInAHeader(bytes);
	}

	if (bytes)
	{
		lastDatagramSent.assign(bytes, bytes + size);
		ScriptedHostAnswers(address, bytes, size);
	}

	datagramsSent++;
	datagramsSentTo.push_back(*address);
	datagramsSentBytes.push_back(lastDatagramSent);
	return 1;
}

static std::string ReadSourceFile(char const *name)
{
	// /FC in this test's settings makes __FILE__ the whole path of this file, so everything else can be found from it
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	// Files here have Windows' line ends, and what's looked for is written with plain ones
	std::string text = all.str(), plain;

	for (char const c : text)
	{
		if (c != '\r')
		{
			plain += c;
		}
	}

	return plain;
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

static int CountOf(const std::string &text, const char *what)
{
	int count = 0;

	for (std::string::size_type at = text.find(what); at != std::string::npos; at = text.find(what, at + 1))
	{
		count++;
	}

	return count;
}

// The text from one marker up to the next (source_slice.h). A marker that isn't there is a failed check of its own, named by the marker.
static std::string Between(const std::string &text, const char *from, const char *to)
{
	return SourceSlice(text, from, to, kSliceEndFromTheStart, [](const std::string &why) { Check(false, why.c_str()); });
}

static char playerName[] = "Ripley";
static char sessionName[] = "A game";

static NetResult HostAGame()
{
	AvP.Network = I_No_Network;
	return Net_HostGame(playerName, sessionName, 0, 0, 0);
}

// Modified 2026-11-11 by Melanikus for the AvP port (no plain game): a join is through the game's line's relay, through the stand-in
// channel, as JoinThroughTheChannel does it
static NetResult JoinThroughTheChannel();   // further down, with the channel's own tests

static NetResult JoinAGame()
{
	return JoinThroughTheChannel();
}

// Back to a game with nothing going on. Whether this works is tested below, and this is only for between those tests.
static void EndTheGame()
{
	Net_Disconnect();
	AvP.Network = I_No_Network;
	scriptedHostSide.Reset();   // the host behind the relay's handshake goes with the game too (protocol version 2)
}

// Modified 2026-11-08 by Melanikus for the AvP port: hosting through a relay (net_relay.h, PROTOCOL.md 7014de1), with the list naming one.
// The host times it with echoes from the game's own socket, asks it for a room, listens to it alone, takes its messages off ENet's hands,
// puts the room on the list with its region and round trip, hands the list each newer pass, and closes the room on leaving. With no relay
// answering, the game is open on this network instead; with none named, hosting is as it always was.
static void TellTheHostPlain(enet_uint32 fromHost, uint16_t fromPort, uint8_t *bytes, size_t size)
{
	theHost.receivedAddress.host = fromHost;
	theHost.receivedAddress.port = fromPort;
	theHost.receivedData = bytes;
	theHost.receivedDataLength = size;
}

// Modified 2026-11-09 by Melanikus for the AvP port (protocol version 2's draft, step 7): a relay message from the relay comes
// sealed, as through the channel to it ('AVPR' 'S' and the message, which the stand-in session opens), so the tests from before step 7 mean
// what they meant. TellTheHostPlain gives it as it is, as a version 1 relay would send it, for the checks that it's never read.
static uint8_t sealedForTheHost[kNetRelayV2StandInBytes];

static void TellTheHost(enet_uint32 fromHost, uint16_t fromPort, uint8_t *bytes, size_t size)
{
	if (fromHost == kRelayHost && bytes && size == kNetRelayMessageBytes && NetRelay_Read(bytes, size, NULL) == kNetRelayIsAMessage)
	{
		memcpy(sealedForTheHost, "AVPR", 4);
		sealedForTheHost[4] = 'S';
		memcpy(sealedForTheHost + 5, bytes, size);
		TellTheHostPlain(fromHost, fromPort, sealedForTheHost, sizeof(sealedForTheHost));
		return;
	}

	TellTheHostPlain(fromHost, fromPort, bytes, size);
}

static void TestHostingThroughTheRelay()
{
	printf("Hosting through a relay: the relay's address\n");
	// Modified 2026-11-10 by Melanikus for the AvP port (#175 PR 3a, version 2 only): version 1's host, with its echoes and its fall back into
	// a game open on this network, went with version 1, and the host's way through the relay, sealed, is TestTheRelayChannel's. What's left
	// here is the relay's address: one listed at 0.0.0.0 or 255.255.255.255 is sent nothing, in case the list's own rule ever lets one
	// through, and as the game can't go online sealed without it, it isn't hosted at all (#188).
	relayNamed = true;
	enet_uint32 const neverSentTo[] = { ENET_HOST_ANY, ENET_HOST_BROADCAST };

	for (enet_uint32 const address : neverSentTo)
	{
		relayAddressNamed = address;
		int const sentNever = datagramsSent;
		Check(HostAGame() == NET_FAIL && datagramsSent == sentNever && Net_HostFailedWhy() != NULL && strcmp(Net_HostFailedWhy(), "the server's relay can't be used") == 0,
			address == ENET_HOST_ANY ? "a relay listed at 0.0.0.0: nothing is sent to it, and the online game isn't hosted, saying the relay can't be used"
				: "a relay listed at 255.255.255.255: nothing is sent to it, and the online game isn't hosted, saying the relay can't be used");
		EndTheGame();
	}

	relayAddressNamed = kRelayHost;

	// Modified 2026-11-11 by Melanikus for the AvP port (no plain game, and "No auto choice, they choose")
	hostLineGiven = -1;
	int const sentAfter = datagramsSent;
	Check(HostAGame() == NET_FAIL && datagramsSent == sentAfter && Net_HostFailedWhy() != NULL && strcmp(Net_HostFailedWhy(), "choose a server to host on") == 0,
		"with no server chosen to host on, nothing is hosted, nothing goes to a relay, and the host says to choose one (there's no game at home)");
	hostLineGiven = -2;
	EndTheGame();

	Check(!Has(ReadSourceFile("3dc/networking.cpp"), "enet_address_set_host("),
		"SOURCE: the game's own thread never looks a relay's name up: the list hands its address over (Fourth's cold read, finding 1)");

	Check(!Has(ReadSourceFile("3dc/networking.cpp"), "+ relayRegion"), "SOURCE: no line the game writes holds the list's region: the words are the game's own (Fourth's cold read, finding 3)");
}

// Words into one of a listing's fields, cut to its room, and always ended with a 0, whatever is handed in
template <size_t N> static void PutWords(char (&field)[N], const char *words)
{
	_snprintf(field, N - 1, "%s", words ? words : "");
	field[N - 1] = '\0';
}

// Modified 2026-11-08 by Melanikus for the AvP port: a game from the relays' list goes on the list of sessions (Net_AddRelaySession), as
// one found on this network does, with the same checks; and never past the end of the list
// Modified 2026-11-09 by Melanikus for the AvP port: each field is filled cut to its own room (PutWords), never by strcpy
static RelayListGame AGameOnTheList(const char *code)
{
	RelayListGame game;
	memset(&game, 0, sizeof(game));
	PutWords(game.region, "virginia");
	PutWords(game.code, code);
	PutWords(game.name, "Hunt");
	PutWords(game.level, "Genshd1");
	PutWords(game.mode, "Cooperative");
	game.players = 1;
	game.maxPlayers = 4;
	game.version = kMultiplayerVersion;
	// Added 2026-11-09 by Melanikus for the AvP port (protocol version 2): the host behind the relay's whole id, as a listing carries it
	memcpy(game.hostId, scriptedHostIdentity.id, sizeof(game.hostId));
	return game;
}

static void TestRelayGamesOnTheList()
{
	printf("Games from the relays' list\n");
	NumberOfSessionsFound = 0;
	RelayListGame const game = AGameOnTheList("ABCD2345");
	Check(Net_AddRelaySession(game, 0) && NumberOfSessionsFound == 1 && SessionData[0].throughRelay && memcmp(SessionData[0].relayCode, "ABCD2345", 8) == 0
		&& strcmp(SessionData[0].relayRegion, "virginia") == 0 && SessionData[0].host == 0 && SessionData[0].AllowedToJoin
		&& strcmp(SessionData[0].Name, "Hunt (1/4)") == 0, "a game from the relays' list goes on the list of sessions, marked as a relay one, with no address");
	Check(strcmp(SessionData[0].levelName, "Genshd1") == 0 && SessionData[0].gameStyle == game.gameStyle && SessionData[0].players == 1 && SessionData[0].maxPlayers == 4,
		"with its level's name, its style and its players for the games page's columns");
	Check(!Net_AddRelaySession(game, 0) && NumberOfSessionsFound == 1, "the same game twice is one game");

	RelayListGame badCode = AGameOnTheList("ABCD234O");
	Check(!Net_AddRelaySession(badCode, 0) && NumberOfSessionsFound == 1, "a code with O in it isn't a game's");
	RelayListGame noRegion = AGameOnTheList("ABCD2346");
	noRegion.region[0] = 0;
	Check(!Net_AddRelaySession(noRegion, 0) && NumberOfSessionsFound == 1, "nor a game with no region");
	RelayListGame outOfRange = AGameOnTheList("ABCD2346");
	outOfRange.levelNumber = 101;
	Check(!Net_AddRelaySession(outOfRange, 0) && NumberOfSessionsFound == 1, "nor one whose level number is past 100");

	RelayListGame older = AGameOnTheList("ABCD2347");
	older.version = kMultiplayerVersion - 1;
	Check(Net_AddRelaySession(older, 0) && NumberOfSessionsFound == 2 && !SessionData[1].AllowedToJoin && strstr(SessionData[1].Name, "(V ") != NULL,
		"another version is listed with its version, and can't be joined, as on this network");
	RelayListGame full = AGameOnTheList("ABCD2348");
	full.players = 4;
	Check(Net_AddRelaySession(full, 0) && !SessionData[2].AllowedToJoin, "a full game is listed and can't be joined");

	static const char codes[][9] = { "ABCD2352", "ABCD2353", "ABCD2354", "ABCD2355", "ABCD2356", "ABCD2357", "ABCD2358", "ABCD2359" };
	int added = 0;

	for (int i = 0; i < 8; i++)
	{
		added += Net_AddRelaySession(AGameOnTheList(codes[i]), 0) ? 1 : 0;
	}

	Check(NumberOfSessionsFound == MAX_NO_OF_SESSIONS && added == MAX_NO_OF_SESSIONS - 3, "the list fills to its end");
	Check(!Net_AddRelaySession(AGameOnTheList("ABCD2362"), 0) && NumberOfSessionsFound == MAX_NO_OF_SESSIONS, "and nothing more goes on it");

	// Each one's GUID, from its region and code: the same game the same each time, two games apart (Fourth's cold read, finding 6)
	GUID const first = SessionData[0].Guid;
	Check(first.Data2 == 0x524C && first.Data3 == 1 && memcmp(first.Data4, "ABCD2345", 8) == 0, "a relay game's GUID is its own kind, and holds its code");
	Check(memcmp(&SessionData[0].Guid, &SessionData[1].Guid, sizeof(GUID)) != 0, "two relay games' GUIDs differ");
	NumberOfSessionsFound = 0;
	Net_AddRelaySession(AGameOnTheList("ABCD2345"), 0);
	Check(memcmp(&SessionData[0].Guid, &first, sizeof(GUID)) == 0, "and the same game put on the list again gets the same GUID, so the page's highlight stays on it");
	RelayListGame elsewhere = AGameOnTheList("ABCD2345");
	strcpy(elsewhere.region, "paris");
	Net_AddRelaySession(elsewhere, 0);
	Check(NumberOfSessionsFound == 2 && memcmp(&SessionData[1].Guid, &first, sizeof(GUID)) != 0, "a game with the same code on another region's relay is another game, with another GUID");
	NumberOfSessionsFound = 0;
	memset(SessionData, 0, sizeof(SessionData[0]) * MAX_NO_OF_SESSIONS);
}

// Modified 2026-11-08 by Melanikus for the AvP port: joining a game from the relays' list, through a scripted relay. The relay's answers
// wait at the new host's socket before the join starts: JOIN_PORT from its control port, then READY from the joiner's port. Then each way it
// fails: the relay saying no, the relay silent, and no relay for the game's region.
// The menu's wait hook, as tests set it: how often it was asked, and whether it says to go on
static int waitHookAsked = 0;

static bool GiveUpAtOnce(void)
{
	waitHookAsked++;
	return false;
}

static bool KeepWaiting(void)
{
	waitHookAsked++;
	return true;
}

// Modified 2026-11-09 by Melanikus for the AvP port: a hook that notes the join's step each time it's asked (Net_RelayJoinStep), as the
// menu's does to say where the join has got to
static int stepsSeen[3] = {};

static bool NoteTheStep(void)
{
	int const step = Net_RelayJoinStep();

	if (step >= 0 && step < 3)
	{
		stepsSeen[step]++;
	}

	return true;
}

static void ArrivesFromTheRelayPlain(const NetRelayMessage &m, uint16_t fromPort)
{
	WaitingDatagram waiting;
	waiting.bytes.resize(kNetRelayMessageBytes);
	NetRelay_Write(m, waiting.bytes.data());
	waiting.from.host = kRelayHost;
	waiting.from.port = fromPort;
	datagramsWaiting.push_back(waiting);
}

// Modified 2026-11-09 by Melanikus for the AvP port (protocol version 2's draft, step 7): sealed, as TellTheHost's are
static void ArrivesFromTheRelay(const NetRelayMessage &m, uint16_t fromPort)
{
	WaitingDatagram waiting;
	waiting.bytes.resize(kNetRelayV2StandInBytes);
	memcpy(waiting.bytes.data(), "AVPR", 4);
	waiting.bytes[4] = 'S';
	NetRelay_Write(m, waiting.bytes.data() + 5);
	waiting.from.host = kRelayHost;
	waiting.from.port = fromPort;
	datagramsWaiting.push_back(waiting);
}

static void TestJoiningThroughTheRelay()
{
	printf("Joining through a relay\n");
	relayNamed = true;
	datagramsWaiting.clear();
	NumberOfSessionsFound = 0;
	Check(Net_AddRelaySession(AGameOnTheList("ABCD2345"), 0) && SessionData[0].throughRelay, "a game from the relays' list, to join");

	uint8_t ticket[kNetRelaySecretBytes];
	memset(ticket, 0x6B, sizeof(ticket));
	NetRelayMessage given = NetRelay_Message(kNetRelayJoinPort);
	memcpy(given.secret, ticket, sizeof(ticket));
	memcpy(given.code, "ABCD2345", 8);
	given.port = 27501;
	NetRelayMessage ready = NetRelay_Message(kNetRelayReady);
	memcpy(ready.secret, ticket, sizeof(ticket));
	ready.port = 27501;
	ArrivesFromTheRelay(given, kNetRelayDefaultControlPort);
	ArrivesFromTheRelay(ready, 27501);

	size_t const sentBefore = datagramsSentBytes.size();
	AvP.Network = I_Peer;
	Check(Net_ConnectToSession(0, playerName) == NET_OK, "joining it works");
	Check(datagramsSentBytes.size() > sentBefore && datagramsSentBytes[sentBefore].size() == kNetRelayMessageBytes && datagramsSentBytes[sentBefore][5] == kNetRelayJoin
		&& memcmp(datagramsSentBytes[sentBefore].data() + 24, "ABCD2345", 8) == 0 && datagramsSentTo[datagramsSentTo.size() - (datagramsSentBytes.size() - sentBefore)].port == kNetRelayDefaultControlPort,
		"first it asks the relay's control port to join that game, with its code");
	Check(lastConnectTo.host == kRelayHost && lastConnectTo.port == 27501, "then the game connects to the port the relay gave it, as if that were the host");
	Check(theHost.intercept != NULL, "and a READY sent again later is the relay's: it's taken off ENet's hands");

	// Modified 2026-11-09 by Melanikus for the AvP port: a joiner through a relay hears from the relay alone, as a host does
	uint8_t notTheRelays[40];
	memset(notTheRelays, 0x11, sizeof(notTheRelays));
	TellTheHost(0x0200007F, 27501, notTheRelays, sizeof(notTheRelays));
	Check(theHost.intercept(&theHost, NULL) == 1, "a packet from anywhere but the relay is dropped before ENet reads it");
	TellTheHost(kRelayHost, 27501, notTheRelays, sizeof(notTheRelays));
	// Modified 2026-11-09 by Melanikus for the AvP port (protocol version 2): a plain one from its port too; the game's own come sealed
	Check(theHost.intercept(&theHost, NULL) == 1, "and so is a plain one from its port on the relay: the game's own come sealed");
	{
		// The host behind the relay has the keys too (B5 went), so what it seals opens here, and ENet reads the opened bytes
		uint8_t plain[40];
		memset(plain, 0x5C, sizeof(plain));
		static uint8_t sealed[kNetSealedGameMostBytes];
		size_t sealedSize = 0;
		bool const sealedIt = scriptedHostSide.Ready() && scriptedHostSide.Seal(plain, sizeof(plain), sealed, sizeof(sealed), &sealedSize, enetClock);
		TellTheHost(kRelayHost, 27501, sealed, sealedSize);
		Check(sealedIt && theHost.intercept(&theHost, NULL) == 0 && theHost.receivedDataLength == sizeof(plain) && memcmp(theHost.receivedData, plain, sizeof(plain)) == 0,
			"handshake B ran with the host the list named, and a game datagram it sealed opens here, for ENet to read as it was");
		TellTheHost(kRelayHost, 27501, sealed, sealedSize);
		Check(theHost.intercept(&theHost, NULL) == 1, "the same sealed datagram again is dropped: none is taken twice");

		// The check code (CRYPTO.md section 8): the joiner has the same 6 digits as the host behind the relay, for the players to compare
		char code[kNetRelayCheckCodeBytes];
		bool const shown = Net_RelayCheckCodeNow(code);
		bool allDigits = strlen(code) == 6;

		for (size_t i = 0; i < 6 && allDigits; i++)
		{
			allDigits = isdigit(static_cast<unsigned char>(code[i])) != 0;
		}

		Check(shown && allDigits && strcmp(code, scriptedHostSide.CheckCode()) == 0,
			"the joiner has the check code to show: 6 digits, the same the host behind the relay worked out");
		EndTheGame();
		Check(!Net_RelayCheckCodeNow(code) && code[0] == 0, "and once the game is left, it has none");
	}

	// The relay says no
	NumberOfSessionsFound = 0;
	Net_AddRelaySession(AGameOnTheList("ABCD2345"), 0);
	NetRelayMessage refused = NetRelay_Message(kNetRelayRefused);
	refused.number = kNetRelayRefusedNoGame;
	ArrivesFromTheRelay(refused, kNetRelayDefaultControlPort);
	AvP.Network = I_Peer;
	int const destroyedBefore = hostsDestroyed;
	Check(Net_ConnectToSession(0, playerName) == NET_FAIL && strcmp(Net_RelayJoinFailedWhy(), "the relay knows no game with that code") == 0 && datagramsWaiting.empty(),
		"the relay saying there's no such game: the join fails, and says so in the game's own words");
	Check(hostsDestroyed == destroyedBefore + 1, "and the host it made for the join is let go: nothing is left open");
	EndTheGame();

	// The relay silent: the handshake ends by its own turns, though this test's clock doesn't move
	NumberOfSessionsFound = 0;
	Net_AddRelaySession(AGameOnTheList("ABCD2345"), 0);
	AvP.Network = I_Peer;
	Check(Net_ConnectToSession(0, playerName) == NET_FAIL && strcmp(Net_RelayJoinFailedWhy(), "the relay didn't answer") == 0,
		"a silent relay: the join gives up, even with the clock standing still, and says the relay didn't answer");
	EndTheGame();

	// The player gives up: the menu's hook says so on the first slice, and the join stops at once
	NumberOfSessionsFound = 0;
	Net_AddRelaySession(AGameOnTheList("ABCD2345"), 0);
	waitHookAsked = 0;
	Net_SetRelayJoinWaitHook(GiveUpAtOnce);
	AvP.Network = I_Peer;
	Check(Net_ConnectToSession(0, playerName) == NET_FAIL && strcmp(Net_RelayJoinFailedWhy(), "the join was given up") == 0 && waitHookAsked == 1,
		"the player giving up (Esc, through the menu's hook): the join stops on that slice, and says it was given up");
	EndTheGame();
	Net_SetRelayJoinWaitHook(KeepWaiting);
	waitHookAsked = 0;
	NumberOfSessionsFound = 0;
	Net_AddRelaySession(AGameOnTheList("ABCD2345"), 0);
	AvP.Network = I_Peer;
	Check(Net_ConnectToSession(0, playerName) == NET_FAIL && strcmp(Net_RelayJoinFailedWhy(), "the relay didn't answer") == 0 && waitHookAsked > 100,
		"with a hook that keeps waiting, it's asked each slice, and the wait goes to its limit");
	Net_SetRelayJoinWaitHook(NULL);
	EndTheGame();

	// The join's step, as the menu's hook sees it: reaching the relay, then, once JOIN_PORT has come, the relay introducing the two ends;
	// and none once the join is over. The host's hellos never come here, so READY never does.
	memset(stepsSeen, 0, sizeof(stepsSeen));
	Net_SetRelayJoinWaitHook(NoteTheStep);
	NumberOfSessionsFound = 0;
	Net_AddRelaySession(AGameOnTheList("ABCD2345"), 0);
	ArrivesFromTheRelay(given, kNetRelayDefaultControlPort);
	AvP.Network = I_Peer;
	Check(Net_ConnectToSession(0, playerName) == NET_FAIL && stepsSeen[kNetRelayJoinStepReaching] >= 1 && stepsSeen[kNetRelayJoinStepIntroducing] >= 1
		&& stepsSeen[kNetRelayJoinStepNone] == 0 && Net_RelayJoinStep() == kNetRelayJoinStepNone,
		"the menu's hook sees the join reaching the relay, then the relay introducing the two ends, and no step once it's over");
	Net_SetRelayJoinWaitHook(NULL);
	EndTheGame();
	datagramsWaiting.clear();

	// No relay for its line (modified 2026-11-11: a game's relay is its line's)
	serverInTheFile = 1;
	relayNamed = false;
	NumberOfSessionsFound = 0;
	Net_AddRelaySession(AGameOnTheList("ABCD2345"), 0);
	AvP.Network = I_Peer;
	Check(Net_ConnectToSession(0, playerName) == NET_FAIL && strcmp(Net_RelayJoinFailedWhy(), "no relay could be reached") == 0, "no relay for its line: no join");
	EndTheGame();
	serverInTheFile = -1;
	relayNamed = true;

	NumberOfSessionsFound = 0;
	memset(SessionData, 0, sizeof(SessionData[0]) * MAX_NO_OF_SESSIONS);
	datagramsWaiting.clear();
}

// EACH GAME THROUGH ITS OWN LINE (Fifth's condition, 2026-11-11; check-online-play's rule J): a game's relay and its keys come from the
// servers.cfg line whose server listed it, keyed by that line, never by its region, which is only a word a server sends. Here two lines
// both list a game with the same code in the same region. Added 2026-11-11 by Melanikus for the AvP port.
static void ResetTheStandInChannel();   // further down, with the channel's own tests

static void TestEachGameThroughItsOwnLine()
{
	printf("Each game through its own line\n");
	relayNamed = true;
	linesInTheFile = 2;
	NumberOfSessionsFound = 0;
	memset(SessionData, 0, sizeof(SessionData[0]) * MAX_NO_OF_SESSIONS);

	RelayListGame const same = AGameOnTheList("ABCD2345");
	Check(Net_AddRelaySession(same, 0) && Net_AddRelaySession(same, 1) && NumberOfSessionsFound == 2 && SessionData[0].serverLine == 0
		&& SessionData[1].serverLine == 1, "two lines' games with the same code and the same region are two games, each kept with its own line");
	Check(!Net_AddRelaySession(same, 0) && NumberOfSessionsFound == 2, "and the same line's game twice is still one game");
	Check(!Net_AddRelaySession(same, 2) && !Net_AddRelaySession(same, -1) && NumberOfSessionsFound == 2, "a game from no line of servers.cfg is dropped");
	Check(Net_FindRelaySession(0, same.region, same.code) == 0 && Net_FindRelaySession(1, same.region, same.code) == 1,
		"and each is found by its line too, never by its region and code alone");

	// The second line's game is joined through the second line's relay, with the second line's keys
	int const keysBefore = relayKeysAsked;
	Net_SetRelayJoinWaitHook(GiveUpAtOnce);
	AvP.Network = I_Peer;
	Check(Net_ConnectToSession(1, playerName) == NET_FAIL, "(the second game's join, given up at once, as nothing answers it here)");
	Net_SetRelayJoinWaitHook(NULL);
	Check(relayOfAsked == 1 && relayKeysAsked == keysBefore + 1 && keysAskedOnLine == 1,
		"THROUGH ITS OWN LINE: the second game's join reads the second line's relay and asks the second line for its keys, never the first's, though both say the same region");
	EndTheGame();

	// Keys that come saying they're for another line than the one asked are never taken: the channel ends, and nothing is sent
	ResetTheStandInChannel();
	keysComeForLine = 1;
	hostLineGiven = 0;
	AvP.Network = I_No_Network;
	Check(HostAGame() == NET_OK, "(a host on the first line, whose relay's keys will come saying they're the second line's)");
	Net_ServiceNetwork();
	NetRelayHostState news;
	Net_RelayHostStateNow(&news);
	Check(news.what == kNetRelayHostEnded && news.why != NULL && netRelayV2StandIn.starts == 0,
		"keys that come for another line than the one asked are never taken: no handshake starts with them, and the online game ends, saying why");
	keysComeForLine = -1;
	hostLineGiven = -2;
	EndTheGame();

	linesInTheFile = 1;
	NumberOfSessionsFound = 0;
	memset(SessionData, 0, sizeof(SessionData[0]) * MAX_NO_OF_SESSIONS);
	datagramsWaiting.clear();
}

// Modified 2026-11-09 by Melanikus for the AvP port: THE RELAY ON THE SCREEN (Fourth's online screens). The host's news about its relay,
// once each time it changes, and the line in play saying which relay the game goes through and how far away it is, for a host and for a
// joiner. Asking changes nothing.
// A host that has just started hosting through the relay: the relay answers its first echo, the echoes end, and the relay opens its room.
// Modified 2026-11-09 by Melanikus for the AvP port (step 7): version 2 has no echoes. The channel to the relay opens by
// itself on the first turn (the stand-in list's keys, the cookie round and the stand-in session), and the room is asked for through it.
// The clock goes on as it did, so what comes after keeps its times.
static void HostThroughTheRelayUntilItsRoomIsOpen()
{
	Net_ServiceNetwork();

	enetClock += 100;
	Net_ServiceNetwork();
	enetClock += 100;
	Net_ServiceNetwork();
	enetClock += 1000;
	Net_ServiceNetwork();

	NetRelayMessage opened = NetRelay_Message(kNetRelayHostOpened);
	memset(opened.secret, 0x5A, sizeof(opened.secret));
	memcpy(opened.code, "ABCD2345", 8);
	memset(opened.pass, 0x33, sizeof(opened.pass));
	opened.passTime = 1700000000u;
	uint8_t bytes[kNetRelayMessageBytes];
	NetRelay_Write(opened, bytes);
	TellTheHost(kRelayHost, kNetRelayDefaultControlPort, bytes, sizeof(bytes));
	theHost.intercept(&theHost, NULL);
	Net_ServiceNetwork();
}

static void ResetTheStandInChannel();   // further down, with the channel's own tests

static void TestTheRelayOnTheScreen()
{
	printf("The relay on the screen\n");
	// Modified 2026-11-10 by Melanikus for the AvP port (#175 PR 3a): version 2's host, as every relay game is, through the stand-in channel
	ResetTheStandInChannel();
	relayNamed = true;
	NetRelayHostState news;
	NetRelayHostState again;
	NetRelayPlayState play;

	Net_RelayHostStateNow(&news);
	unsigned int const changesAtFirst = news.changes;
	Net_RelayPlayStateNow(&play);
	Check(news.what == kNetRelayHostNone && !play.throughRelay && play.ms == -1, "with no game, the host has no news, and there's no line in play");

	Check(HostAGame() == NET_OK, "a game hosts with a relay named");
	Net_RelayHostStateNow(&news);
	Net_RelayPlayStateNow(&play);
	Check(news.what == kNetRelayHostFinding && news.changes == changesAtFirst + 1 && news.why == NULL && news.region[0] == 0,
		"the host's news: it's finding the nearest relay");
	Check(!play.throughRelay, "and there's no line in play until its room is open");

	HostThroughTheRelayUntilItsRoomIsOpen();
	Net_RelayHostStateNow(&news);
	Check(news.what == kNetRelayHostListed && news.changes == changesAtFirst + 2 && strcmp(news.region, "virginia") == 0 && news.ms == hostMsListed
		&& news.ms >= 1 && news.why == NULL, "then its room is open and it's going on the list, through the relay's region, at the round trip the list was given");
	Net_RelayHostStateNow(&again);
	Check(again.changes == news.changes && again.what == news.what, "asking again changes nothing");
	Net_RelayPlayStateNow(&play);
	Check(play.throughRelay && play.isHost && strcmp(play.region, "virginia") == 0 && play.ms == hostMsListed,
		"the line in play: hosting through that region, at the channel's round trip until a keep has been answered");

	// A keep, answered 37 ms later: the line's round trip is the keep's
	enetClock += kNetRelayKeepEveryMs;
	Net_ServiceNetwork();
	Check(!netRelayV2StandIn.sealed.empty() && netRelayV2StandIn.sealed.back()[5] == kNetRelayHostKeep, "15 s on, the host keeps its room, sealed, through the channel");
	enetClock += 37;
	NetRelayMessage kept = NetRelay_Message(kNetRelayHostKept);
	memset(kept.secret, 0x5A, sizeof(kept.secret));
	memset(kept.pass, 0x44, sizeof(kept.pass));
	kept.passTime = 1700000015u;
	uint8_t bytes[kNetRelayMessageBytes];
	NetRelay_Write(kept, bytes);
	TellTheHost(kRelayHost, kNetRelayDefaultControlPort, bytes, sizeof(bytes));
	theHost.intercept(&theHost, NULL);
	Net_RelayPlayStateNow(&play);
	Check(play.throughRelay && play.isHost && play.ms == 37, "and once the relay answers the keep, 37 ms later, the line's round trip is 37 ms");
	Net_RelayHostStateNow(&again);
	Check(again.changes == news.changes, "which isn't news: the news is only what the host is doing");

	EndTheGame();
	Net_RelayHostStateNow(&news);
	Net_RelayPlayStateNow(&play);
	Check(news.what == kNetRelayHostNone && news.changes == changesAtFirst + 3 && !play.throughRelay, "leaving the game: no news, and no line");

	// The room lost, with no HOST_KEPT for 45 s: why is in the game's own words
	Check(HostAGame() == NET_OK, "a game hosts through the relay again");
	HostThroughTheRelayUntilItsRoomIsOpen();
	enetClock += kNetRelayKeptWithinMs;
	Net_ServiceNetwork();
	Net_RelayHostStateNow(&news);
	Net_RelayPlayStateNow(&play);
	Check(news.what == kNetRelayHostEnded && news.why != NULL && strcmp(news.why, NetRelay_WhyText(kNetRelayNoKeep)) == 0 && news.region[0] == 0 && !play.throughRelay,
		"its room lost: the news is that the online game has ended, and why, in the game's own words; and there's no line in play");
	EndTheGame();

	// A relay that can't be used (listed at an address that's never sent to): the online game isn't hosted, so there's no news of it. A relay
	// that stops answering ends the game, as the lost room does (TestTheRelayChannel).
	relayAddressNamed = ENET_HOST_ANY;
	Net_RelayHostStateNow(&again);
	Check(HostAGame() == NET_FAIL, "a game isn't hosted with the only relay named at an address that's never sent to");
	Net_RelayHostStateNow(&news);
	Check(news.changes == again.changes, "and there's no news of a game that never started");
	EndTheGame();
	relayAddressNamed = kRelayHost;

	hostLineGiven = -1;
	Net_RelayHostStateNow(&again);
	Check(HostAGame() == NET_FAIL, "a game with no server chosen isn't hosted (there's no game at home)");
	Net_RelayHostStateNow(&news);
	Check(news.what == kNetRelayHostNone && news.changes == again.changes, "and has no relay news at all");
	hostLineGiven = -2;
	EndTheGame();

	// A joiner through a relay: ENet's round trip to the host, once ENet has timed one. Its join goes through B, through the channel to the
	// relay (step 7)
	relayNamed = true;
	datagramsWaiting.clear();
	NumberOfSessionsFound = 0;
	Net_AddRelaySession(AGameOnTheList("ABCD2345"), 0);
	uint8_t ticket[kNetRelaySecretBytes];
	memset(ticket, 0x6B, sizeof(ticket));
	NetRelayMessage given = NetRelay_Message(kNetRelayJoinPort);
	memcpy(given.secret, ticket, sizeof(ticket));
	memcpy(given.code, "ABCD2345", 8);
	given.port = 27501;
	NetRelayMessage ready = NetRelay_Message(kNetRelayReady);
	memcpy(ready.secret, ticket, sizeof(ticket));
	ready.port = 27501;
	ArrivesFromTheRelay(given, kNetRelayDefaultControlPort);
	ArrivesFromTheRelay(ready, 27501);
	thePeer.lastReceiveTime = 0;
	thePeer.roundTripTime = 500;
	AvP.Network = I_Peer;
	Check(Net_ConnectToSession(0, playerName) == NET_OK, "a game joined through a relay");
	Net_RelayPlayStateNow(&play);
	Check(play.throughRelay && !play.isHost && strcmp(play.region, "virginia") == 0 && play.ms == -1,
		"the joiner's line: through its game's relay, with no round trip until ENet has timed one, so never ENet's starting guess of 500 ms");
	thePeer.lastReceiveTime = 1;
	thePeer.roundTripTime = 63;
	Net_RelayPlayStateNow(&play);
	Check(play.throughRelay && play.ms == 63, "then ENet's round trip to the host, through the relay");
	thePeer.roundTripTime = 20000;
	Net_RelayPlayStateNow(&play);
	Check(play.ms == 9999, "never more than 9,999 ms");
	thePeer.state = ENET_PEER_STATE_CONNECTION_PENDING;
	Net_RelayPlayStateNow(&play);
	Check(!play.throughRelay, "and no line while the connection isn't made");
	thePeer.state = ENET_PEER_STATE_CONNECTED;
	EndTheGame();
	Net_RelayPlayStateNow(&play);
	Check(!play.throughRelay, "nor once the game is over");

	thePeer.lastReceiveTime = 0;
	thePeer.roundTripTime = 0;
	relayNamed = true;   // as the tests after this expect (2026-11-11: a line's relay is named)
	NumberOfSessionsFound = 0;
	memset(SessionData, 0, sizeof(SessionData[0]) * MAX_NO_OF_SESSIONS);
	datagramsWaiting.clear();
}

// Hudson's branch, server #14's run through a relay on another machine (2026-11-09). A run with net= uses only this machine's own address,
// or, given one relay beyond it by its address, that relay at that address. Its sockets then go on this machine's address towards that
// relay, never on 0.0.0.0, and each drops whatever comes from anywhere else before ENet reads it. (Modified 2026-11-10 by Melanikus for the
// AvP port, #175: the relay is given by its address alone, a servers.cfg line's, and a name is refused.)
extern bool NetLoopbackOnly;
bool Net_AllowOneRemoteRelay(const char *address);
uint32_t Net_RemoteRelayAddress();

static void TestARunsOneRelayBeyondThisMachine()
{
	printf("A Hudson run with net=, and its one relay beyond this machine\n");

	// Modified 2026-11-10 by Melanikus for the AvP port (#175 PR 3a): version 2, as every relay game is. What's tried here is the run's own
	// rule: its sockets go on this machine's address towards its one relay, hear that relay alone, and leave every other relay out. How a
	// game goes through a relay, sealed, is TestTheRelayChannel's and TestJoiningThroughTheRelay's.
	enet_uint32 const kThisMachine = 0x0100007F;   // 127.0.0.1, as ENet keeps it
	enet_uint32 const kRoute = 0x0A01A8C0;         // 192.168.1.10: this machine's address towards the relay
	enet_uint32 const kElsewhere = 0x0B0C0B0A;     // a machine that isn't the relay
	enet_uint32 const kMovedTo = 0x0E0C0B0A;       // another address the list might give its relay
	NetLoopbackOnly = true;
	relayNamed = true;
	routeTowardsTheRelay = kRoute;
	datagramsWaiting.clear();
	Check(Net_AllowOneRemoteRelay(""), "to start, no relay beyond this machine");

	int const sentBefore = datagramsSent;
	Check(HostAGame() == NET_FAIL && hostsBoundTo.back().host == kThisMachine && datagramsSent == sentBefore,
		"given none, a run's online host is on this machine's own address, leaves the list's relay elsewhere out, and isn't hosted (sealed or nothing)");
	EndTheGame();

	Check(Net_AllowOneRemoteRelay("10.11.12.14"), "given another relay, by its address");
	Check(HostAGame() == NET_FAIL && hostsBoundTo.back().host == kThisMachine && datagramsSent == sentBefore && Net_RemoteRelayAddress() == 0,
		"the list's relay, at another address, is still left out: nothing is sent to it, no address is let through, and the game isn't hosted");
	EndTheGame();

	Check(!Net_AllowOneRemoteRelay("relay.example") && !Net_AllowOneRemoteRelay("10.11.12.013") && !Net_AllowOneRemoteRelay("10.11.12.13 ")
		&& !Net_AllowOneRemoteRelay("0.0.0.0") && !Net_AllowOneRemoteRelay("255.255.255.255") && !Net_AllowOneRemoteRelay(NULL),
		"a name, an address with a leading zero or a space after it, any address, everyone's, or nothing at all, is refused");

	// The test's relay is at 10.11.12.13 (kRelayHost, as ENet keeps it), whatever name the list gives it
	Check(Net_AllowOneRemoteRelay("10.11.12.13"), "given the list's relay, by its address");
	int const connectsBefore = socketsConnected;
	int const openBefore = socketsOpen;
	Check(HostAGame() == NET_OK && hostsBoundTo.back().host == kRoute && hostsBoundTo.back().port == 23513,
		"the host's socket goes on this machine's address towards that relay, never on 0.0.0.0, at the game's port");
	Check(socketsConnected == connectsBefore + 1 && lastSocketConnectedTo.host == kRelayHost && socketsOpen == openBefore,
		"which it found by connecting a socket to the relay, a thing that sends nothing, and that socket is closed again");
	Check(Net_RemoteRelayAddress() == kRelayHost, "the one address let through is the one it was given");
	Net_ServiceNetwork();
	Check(datagramsSent > sentBefore && datagramsSentTo.back().host == kRelayHost, "and the channel to that relay starts, as with one on this machine");
	uint8_t stranger[40];
	memset(stranger, 0x22, sizeof(stranger));
	TellTheHost(kElsewhere, 27501, stranger, sizeof(stranger));
	Check(theHost.intercept != NULL && theHost.intercept(&theHost, NULL) == 1, "a packet from anywhere but the relay is dropped before ENet reads it");

	HostThroughTheRelayUntilItsRoomIsOpen();
	TellTheHost(kElsewhere, 27501, stranger, sizeof(stranger));
	Check(theHost.intercept(&theHost, NULL) == 1, "and with its room open, one from elsewhere still is");

	// The room lost: the online game has ended, so nothing reaches ENet any more, the relay's included (#188)
	enetClock += kNetRelayKeptWithinMs;
	Net_ServiceNetwork();
	TellTheHost(kRelayHost, 27501, stranger, sizeof(stranger));
	Check(theHost.intercept != NULL && theHost.intercept(&theHost, NULL) == 1, "its room lost, the online game has ended: nothing reaches ENet, the relay's included");
	EndTheGame();

	// The list giving its relay another address later: that one is left out (a servers.cfg line's address never changes, but the rule holds)
	relayAddressNamed = kMovedTo;
	int const sentMoved = datagramsSent;
	Check(HostAGame() == NET_FAIL && datagramsSent == sentMoved && Net_RemoteRelayAddress() == kRelayHost,
		"the list giving its relay another address later: that one is sent nothing, the game isn't hosted, and the first stays the only one let through");
	EndTheGame();
	relayAddressNamed = kRelayHost;

	// A joiner through it: its socket on the same address, on any free port, hearing the relay alone, whether or not its join then goes
	NumberOfSessionsFound = 0;
	Net_AddRelaySession(AGameOnTheList("ABCD2345"), 0);
	AvP.Network = I_Peer;
	Net_ConnectToSession(0, playerName);
	Check(hostsBoundTo.back().host == kRoute && hostsBoundTo.back().port == 0, "a joiner through it: its socket is on the same address, on any free port");
	TellTheHost(kElsewhere, 27501, stranger, sizeof(stranger));
	Check(theHost.intercept != NULL && theHost.intercept(&theHost, NULL) == 1, "and it drops whatever isn't the relay's too");
	EndTheGame();
	datagramsWaiting.clear();

	// No way towards the relay found: this machine's own address, as before, so the relay just can't be reached
	Check(Net_AllowOneRemoteRelay("10.11.12.13"), "given the relay again");
	routeTowardsTheRelay = 0;
	HostAGame();
	Check(hostsBoundTo.back().host == kThisMachine, "with no way towards it found, the host stays on this machine's own address");
	EndTheGame();

	Net_AllowOneRemoteRelay("");
	routeTowardsTheRelay = 0;
	NetLoopbackOnly = false;
	relayNamed = true;   // as the tests after this expect (2026-11-11: a line's relay is named)
	NumberOfSessionsFound = 0;
	memset(SessionData, 0, sizeof(SessionData[0]) * MAX_NO_OF_SESSIONS);
	ResetTheStandInChannel();
}

// Added 2026-11-09 by Melanikus for the AvP port (protocol version 2, Primary's read of the wiring, finding 1): a host asks the start limit
// (net_peer_v2.h, NetPeerStartLimit) before it starts each handshake, forgets a port the relay doesn't name any more, and forgets
// everything when its game through the relay ends. The limit itself is net_v2b_test's; this reads where networking.cpp uses it.
static void TestTheStartLimitIsAsked()
{
	printf("A host asks before it starts a handshake (protocol version 2)\n");
	std::string const source = ReadSourceFile("3dc/networking.cpp");
	size_t const sideAt = source.find("static NetPeerHostSide *RelayEndToEndSide(");
	size_t const sideEnd = sideAt == std::string::npos ? std::string::npos : source.find("\n}\n", sideAt);
	std::string const side = sideEnd == std::string::npos ? std::string() : source.substr(sideAt, sideEnd - sideAt);
	size_t const named = side.find("relayHost.NamedJoinerPort(port)");
	size_t const asked = side.find("if (!relayEndToEndStarts.Allow(port, now))\n\t{\n\t\treturn NULL;\n\t}");
	size_t const started = side.find("return unused->Start(");
	Check(named != std::string::npos && asked != std::string::npos && started != std::string::npos && named < asked && asked < started,
		"SOURCE: a host starts a handshake only for a named port, and only if the start limit allows it");
	Check(Has(source, "if (counted != 0 && !relayHost.NamedJoinerPort(counted))\n\t\t{\n\t\t\trelayEndToEndStarts.Forget(counted);"),
		"SOURCE: and each frame forgets a port the relay doesn't name any more, so it starts afresh when it's named again");
	size_t const forgetAt = source.find("static void RelayEndToEndForget()");
	size_t const forgetEnd = forgetAt == std::string::npos ? std::string::npos : source.find("\n}\n", forgetAt);
	Check(forgetEnd != std::string::npos && Has(source.substr(forgetAt, forgetEnd - forgetAt), "relayEndToEndStarts.Reset();"),
		"SOURCE: and a game through the relay that ends forgets every count");
}

// Added 2026-11-10 by Melanikus for the AvP port (#175 PR 3a): the joiner's round trip to its relay, for the ping column and the line in play,
// is the channel's own timing, once a join has made it; before, the games page shows "?". Version 1's timing of every relay with plain
// echoes, from a socket of its own, went with version 1: a version 2 relay answers no plain echo (Primary, beta2-server's RelayCore.java).
static void TestTheJoinersRoundTrip()
{
	printf("The joiner's round trip to its relay\n");
	Check(Net_JoinerMsToRelay("virginia") == -1 && Net_JoinerMsToRelay(NULL) == -1, "before any join, there's no round trip: the games page shows \"?\"");
	std::string const networking = ReadSourceFile("3dc/networking.cpp");
	std::string const ms = Between(networking, "int Net_JoinerMsToRelay(const char *region)\n{", "\n}\n");
	Check(Has(ms, "relayV2Ms == 0 ||") && Has(ms, "relayV2Ms > static_cast<uint32_t>(kRelayMostHostMs) ||")
		&& Has(ms, "strncmp(region, relayV2Region, sizeof(relayV2Region)) != 0") && Has(ms, "return static_cast<int>(relayV2Ms);"),
		"SOURCE: it's the channel's own timing, for the relay of that region alone, and none when the channel hasn't timed it or the time is too long to be one");
	Check(!Has(networking, "joinerPicker") && !Has(networking, "joinerTimingSocket") && !Has(networking, "Net_TimeRelaysForJoining"),
		"SOURCE: and nothing times a relay with plain echoes any more");
}

static void TestStartingTheGame()
{
	Check(Net_GetNextPlayerID() != 0, "the first player ID there is isn't 0, which means nobody");
	Check(Net_Initialise() == NET_OK, "starting the game's networking works");
	Check(socketsCreated == 0, "and it makes no socket, because there's nobody to talk to yet");
	Check(socketsBoundTo.empty(), "and it listens on nothing");
	Check(hostsCreated == 0, "and it's no host");
}

static void TestNobodyToSendTo()
{
	int const id = 1234;

	Check(Net_SendSystemMessage(NET_SETPLAYERID, 1, 0, &id, sizeof(id)) == NET_FAIL, "a message with nobody to send it to fails");
	Check(packetsMade == 0 && packetsToAPeer == 0, "and nothing is made or sent");

	int const linesBefore = errorLines;

	for (int i = 0; i < 100000; i++)
	{
		Net_SendSystemMessage(NET_SETPLAYERID, 1, 0, &id, sizeof(id));
	}

	Check(errorLines - linesBefore < 40, "and the log only says so now and then: " + std::to_string(errorLines - linesBefore) + " lines");
}

// A game message goes to everyone through the host, and with no game running there isn't one
static void TestSendingWithNoGame()
{
	uint8_t message[4] = { 1, 2, 3, 4 };
	int const packetsBefore = packetsMade;

	Check(Net_Send(NET_SETPLAYERID, 1, 0, message, sizeof(message)) == NET_FAIL, "a game message with no game running fails");
	Check(packetsMade == packetsBefore && broadcasts == 0, "and nothing is made or sent");
	Check(broadcastsWithNoHost == 0, "and ENet is never handed an empty host");

	int const linesBefore = errorLines;

	for (int i = 0; i < 100000; i++)
	{
		Net_Send(NET_SETPLAYERID, 1, 0, message, sizeof(message));
	}

	Check(errorLines - linesBefore < 40, "and the log only says so now and then: " + std::to_string(errorLines - linesBefore));
	Check(Net_Send(NET_SETPLAYERID, 1, 0, NULL, 0) == NET_FAIL, "a message of nothing fails as well");

	Check(HostAGame() == NET_OK, "with a game going");

	int const packetsWithAGame = packetsMade;

	// A host doesn't broadcast: it sends to each player it has let in (SendToClients), so with nobody let in yet, it goes to nobody
	Check(Net_Send(NET_SETPLAYERID, 1, 0, message, sizeof(message)) == NET_OK, "a game message goes");
	Check(packetsMade == packetsWithAGame + 1 && broadcastsWithNoHost == 0, "made into one packet for the players the host has let in, and ENet is never handed an empty host");

	EndTheGame();
	broadcasts = 0;
	Check(Net_Send(NET_SETPLAYERID, 1, 0, message, sizeof(message)) == NET_FAIL, "and after the game has ended, it fails again");
	Check(broadcasts == 0, "with nothing sent");
}

static void TestDisconnectWithNoGame()
{
	int const before = packetsToAPeer;
	int const errorsBefore = errorLines;

	AvP.Network = I_No_Network;
	Net_Disconnect();
	Check(true, "the disconnect command with no game going on is safe");
	Check(packetsToAPeer == before, "and it sends nothing");
	Check(errorLines == errorsBefore, "and it isn't an error");

	bool hasTheCommand = false;

	for (std::string const &command : commands)
	{
		hasTheCommand = hasTheCommand || command == "disconnect";
	}

	Check(hasTheCommand, "and it's there as a console command");
}

static void TestHosting()
{
	int const created = socketsCreated;
	int const packets = packetsToAPeer;

	Check(HostAGame() == NET_OK, "hosting works");
	Check(AvP.Network == I_Host && gamesStartedForHost >= 1, "and starts a game");
	// Modified 2026-11-11 by Melanikus for the AvP port (no plain game): a host opens no socket for being found, as nothing on this
	// network is asked about games any more: it's found on its line's list
	Check(socketsOpen == 0 && socketsCreated == created, "and opens no socket for being found: its game is found on the server's list");
	Check(hostsBoundTo.back().port == 23513, "and its ENet host is on the game's port");
	Check(packetsToAPeer == packets, "and sends nothing to anyone");

	// This is the road a host's answers used to take
	int const id = 1234;

	Check(Net_SendSystemMessage(NET_SETPLAYERID, 1, 0, &id, sizeof(id)) == NET_FAIL, "a host has no server, so that fails, and its game goes on");
	Check(packetsToAPeer == packets, "and nothing goes out");

	Net_Disconnect();
	AvP.Network = I_No_Network;

	Check(HostAGame() == NET_OK && socketsCreated == created, "and a second game hosts too, still opening no socket");
	EndTheGame();
}

static void TestHostingWhenItCantBeDone()
{
	hostCreateFails = true;
	Check(HostAGame() == NET_FAIL, "a host that can't have its port fails");
	Check(socketsOpen == 0, "and opens nothing");
	Check(AvP.Network == I_No_Network, "and isn't a host");
	hostCreateFails = false;
}

static void TestSkirmish()
{
	int const hosts = hostsCreated;

	netGameData.skirmishMode = 1;
	Check(HostAGame() == NET_OK, "a game alone against the computer starts");
	Check(hostsCreated == hosts && socketsOpen == 0, "and opens nothing, because it isn't on any network");
	netGameData.skirmishMode = 0;
	EndTheGame();
}

static void TestJoining()
{
	int const sent = packetsToAPeer;

	Check(JoinAGame() == NET_OK, "joining works");
	Check(packetsToAPeer == sent + 1 && lastMessageSent == NET_CREATEPLAYERORGROUP, "and it says who it is");

	int const id = 1;

	Check(Net_SendSystemMessage(NET_DESTROYPLAYERORGROUP, 0, 0, &id, sizeof(id)) == NET_OK, "a client has a server to send to");

	int const goneBefore = packetsToAPeerThatsGone;
	int const sentBeforeLeaving = packetsToAPeer;

	Net_Disconnect();
	Check(packetsToAPeer == sentBeforeLeaving, "leaving sends no message: the connection closing says it");
	Check(hostsDestroyed == hostsCreated, "and lets its ENet host go");
	Check(Net_SendSystemMessage(NET_DESTROYPLAYERORGROUP, 0, 0, &id, sizeof(id)) == NET_FAIL, "and after that there's nobody to send to");
	Check(packetsToAPeerThatsGone == goneBefore, "so nothing is sent to it");

	int const sentNow = packetsToAPeer;
	int const destroyed = hostsDestroyed;

	Net_Disconnect();
	Check(packetsToAPeer == sentNow && hostsDestroyed == destroyed, "and leaving again does nothing");
	AvP.Network = I_No_Network;
}

// Modified 2026-11-09 by Melanikus for the AvP port: a network game goes on while its window isn't the one in front. A host whose window
// wasn't lost its room on the relay (no HOST_KEEP for 45 s): the game waited in Windows' messages for as long as another window was in
// front, and everything that keeps it in the game runs on its own thread. The waiting itself is win_func.cpp's, read here, not run.
static void TestAGameGoesOnBehindAnotherWindow()
{
	printf("A network game behind another window\n");
	Check(!Net_GameGoesOnWhileInactive(), "with no network game, the game waits while its window isn't in front, as it always has");
	Check(HostAGame() == NET_OK && Net_GameGoesOnWhileInactive(), "a host goes on");
	EndTheGame();
	Check(!Net_GameGoesOnWhileInactive(), "and once its game is over, it waits again");
	Check(JoinAGame() == NET_OK && Net_GameGoesOnWhileInactive(), "a joiner goes on too");
	EndTheGame();
	Check(!Net_GameGoesOnWhileInactive(), "until its game is over");

	std::string const messages = ReadSourceFile("3dc/win95/win_func.cpp");
	Check(Has(messages, "if (!bActive && !Net_GameGoesOnWhileInactive())") && Has(messages, "while (!bActive && bRunning && !Net_GameGoesOnWhileInactive());"),
		"SOURCE: Windows' messages hold the game while its window isn't in front only when there's no network game");
	std::string const window = ReadSourceFile("3dc/avp/win95/win_proj.cpp");
	Check(Has(window, "if (raw->header.dwType == RIM_TYPEMOUSE && bActive)"), "SOURCE: the mouse, which Windows gives the game even then, doesn't move the player");
	Check(Has(window, "if (dwSize > sizeof(lpb) || ::GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) == (UINT)-1)"),
		"SOURCE: and the mouse's report is read only when it fits its room");
	std::string const input = ReadSourceFile("win32/di_func.cpp");
	Check(Has(input, "\tif (bActive)\n\t{\n\t\tReadJoysticks();\n\t}"), "SOURCE: nor does a gamepad, which is read whatever window is in front");
	Check(Has(window, "\t\t\tIngameKeyboardInput_ClearBuffer();\n\t\t\treturn 0;"), "SOURCE: and every key and button held is let go as the window stops being in front");
}

static void TestJoiningAgainWhenItCantBeDone()
{
	Check(JoinAGame() == NET_OK, "joining again");

	hostCreateFails = true;
	Check(JoinAGame() == NET_FAIL, "and joining somewhere else, and failing");
	hostCreateFails = false;

	int const id = 1;
	int const gone = packetsToAPeerThatsGone;

	Check(Net_SendSystemMessage(NET_DESTROYPLAYERORGROUP, 0, 0, &id, sizeof(id)) == NET_FAIL, "lets go of the last server, because its ENet host has gone");
	Check(packetsToAPeerThatsGone == gone, "and nothing is sent to it");
	AvP.Network = I_No_Network;
}

static void TestJoinThatGetsNoAnswerThenHosting()
{
	connectGetsAnAnswer = false;
	Check(JoinAGame() == NET_FAIL, "a join that gets no answer fails");
	connectGetsAnAnswer = true;
	Check(hostsDestroyed == hostsCreated, "and lets go of what it opened, so nothing stays open");

	connectCantStart = true;
	Check(JoinAGame() == NET_FAIL, "a join that ENet can't even start fails");
	connectCantStart = false;
	Check(hostsDestroyed == hostsCreated, "and lets go of it too");

	int const id = 1234;
	int const sent = packetsToAPeer;

	Check(HostAGame() == NET_OK, "and hosting straight after works");
	Check(hostsDestroyed == hostsCreated - 1, "and lets the failed join's ENet host go, rather than losing it");
	Check(Net_SendSystemMessage(NET_SETPLAYERID, 1, 0, &id, sizeof(id)) == NET_FAIL && packetsToAPeer == sent, "and doesn't think the old server is still there");
	EndTheGame();
}

static void TestHostingWhileStillInAGame()
{
	Check(JoinAGame() == NET_OK, "someone is in a game");

	int const destroyed = hostsDestroyed;
	int const id = 1;
	int const sent = packetsToAPeer;

	Check(HostAGame() == NET_OK, "and starts hosting");
	Check(hostsDestroyed == destroyed + 1, "which lets go of the old connection, rather than losing it");
	Check(Net_SendSystemMessage(NET_SETPLAYERID, 1, 0, &id, sizeof(id)) == NET_FAIL && packetsToAPeer == sent, "and doesn't think the old server is still there");
	EndTheGame();
}

// A message as another machine would send it, and everything that can be wrong with one
static std::vector<uint8_t> AMessage(uint8_t system, uint32_t type, NetID from, const void *data = NULL, size_t size = 0, int marker = kMarker)
{
	MessageHeader header;

	memset(&header, 0, sizeof(header));
	header.marker = marker;
	header.isSystemMessage = system;
	header.messageType = type;
	header.fromID = from;

	std::vector<uint8_t> all(sizeof(header) + size);

	memcpy(all.data(), &header, sizeof(header));

	if (size)
	{
		memcpy(all.data() + sizeof(header), data, size);
	}

	return all;
}

// What a real client's code sends for a game message. AGameMessage changes only who it's from. See realClientsAsk just below for why.
static std::vector<uint8_t> realGameMessage;

// The bytes a real client's own code sends when it asks to join, taken from what it really sent. A test that fills in its own good message can't see the day the client stops sending one. That happened once: the host's version check refused every real client, and every test passed, because each one filled in the version by hand.
static std::vector<uint8_t> realClientsAsk;

static void ListenToARealClient()
{
	realClientsAsk.clear();
	sentMessages.clear();
	Check(JoinAGame() == NET_OK, "a real client joins, to hear what it says");

	// A game message, the way the game sends every one
	char words[] = "something for the game";

	sentMessages.clear();
	Check(Net_Send(NET_ID_ALLPLAYERS, 1, 0, (uint8_t *)words, sizeof(words)) == NET_OK, "it sends a game message");

	for (size_t i = 0; i < sentMessages.size(); i++)
	{
		if (sentMessages[i].header.isSystemMessage == 0)
		{
			uint8_t const *const headerBytes = (uint8_t const *)&sentMessages[i].header;

			realGameMessage.assign(headerBytes, headerBytes + sizeof(MessageHeader));
			realGameMessage.insert(realGameMessage.end(), sentMessages[i].body.begin(), sentMessages[i].body.end());
		}
	}

	Check(realGameMessage.size() == sizeof(MessageHeader) + sizeof(words), "and here it is");

	// Back to the start, for what it says when it asks to join
	EndTheGame();
	sentMessages.clear();
	Check(JoinAGame() == NET_OK, "it joins again");

	for (size_t i = 0; i < sentMessages.size(); i++)
	{
		if (sentMessages[i].header.isSystemMessage == 1 && sentMessages[i].header.messageType == NET_CREATEPLAYERORGROUP)
		{
			uint8_t const *const headerBytes = (uint8_t const *)&sentMessages[i].header;

			realClientsAsk.assign(headerBytes, headerBytes + sizeof(MessageHeader));
			realClientsAsk.insert(realClientsAsk.end(), sentMessages[i].body.begin(), sentMessages[i].body.end());
		}
	}

	EndTheGame();
	sentMessages.clear();
	Check(realClientsAsk.size() == sizeof(MessageHeader) + sizeof(PlayerDetails), "it asks to join, with its details");
}

// Only the name is changed, from what a real client said
static std::vector<uint8_t> AskingToJoin(const char *name)
{
	std::vector<uint8_t> asking = realClientsAsk;

	if (asking.size() >= sizeof(MessageHeader) + sizeof(PlayerDetails))
	{
		PlayerDetails *const details = (PlayerDetails *)(asking.data() + sizeof(MessageHeader));

		memset(details->name, 0, sizeof(details->name));
		strncpy(details->name, name, sizeof(details->name) - 1);
	}

	return asking;
}

static std::vector<uint8_t> AGameMessage(NetID from)
{
	std::vector<uint8_t> message = realGameMessage;

	if (message.size() >= sizeof(MessageHeader))
	{
		((MessageHeader *)message.data())->fromID = from;
	}

	return message;
}

// The game's buffer holds kMostMessageBytes. The rest is so that a test can see something too big without it doing any harm.
static uint8_t received[kMostMessageBytes + 64];

// What a joiner's CONNECT says: 0, as the game's always does since the v2 draft's commit word went (modified 2026-11-10 by Melanikus for the
// AvP port, #175 PR 3a)
static enet_uint32 connectSays = 0;

static void Connects(int n)
{
	peers[n].state = ENET_PEER_STATE_CONNECTED;

	ENetEvent const event = { ENET_EVENT_TYPE_CONNECT, &peers[n], NULL, connectSays };

	eventsWaiting.push_back(event);
}

static void Closes(int n)
{
	peers[n].state = ENET_PEER_STATE_DISCONNECTED;

	ENetEvent const event = { ENET_EVENT_TYPE_DISCONNECT, &peers[n], NULL };

	eventsWaiting.push_back(event);
}

// Takes in everything waiting, as the game's loop does
static void TakeEverything()
{
	size_t size = 0;

	while (Net_Receive(received, size) == NET_OK)
	{
	}
}

// This message arrives on this connection. Says how much of it was for the game.
static size_t Hears(int n, const std::vector<uint8_t> &message)
{
	ENetEvent const event = { ENET_EVENT_TYPE_RECEIVE, &peers[n], MakePacket(message.data(), message.size()) };

	eventsWaiting.push_back(event);

	size_t size = 99999;

	Net_Receive(received, size);
	return size;
}

static NetID IDIn(const SentMessage &sent)
{
	NetID id = 0;

	if (sent.body.size() == sizeof(id))
	{
		memcpy(&id, sent.body.data(), sizeof(id));
	}

	return id;
}

static NetID IDOf(int n)
{
	return (NetID)(uintptr_t)peers[n].data;
}

// A host with these clients let in. Its first client is on connection 1.
static void HostWithClients(int clients)
{
	playersInGame.clear();
	playersGone.clear();
	Check(HostAGame() == NET_OK, "a host, with " + std::to_string(clients) + " clients to come");

	for (int i = 1; i <= clients; i++)
	{
		Connects(i);
		TakeEverything();
		Hears(i, AskingToJoin("Someone"));
	}

	EverythingArrives();
	sentMessages.clear();
}

// Modified 2026-11-09 by Melanikus for the AvP port: the request to join (network version 114). A host that asks holds a joiner's "let me
// in" until it's answered. Yes lets it in; no, a second joiner meanwhile, and no answer in time each close the connection with their own
// reason; a joiner that leaves takes its request with it; and what needs no question, another version, isn't asked about. Then the
// joiner's side: waiting, then yes, the host's no, or no answer at all.
static void TestTheRequestToJoin()
{
	printf("The request to join\n");
	Net_LetEveryJoinerIn(false);
	char name[kPlayerNameSize];
	NetJoinWho who = {};

	HostWithClients(0);
	Check(!Net_JoinRequestWaiting(name, sizeof(name), &who), "with nobody asking, there's no request");
	Connects(1);
	TakeEverything();
	Hears(1, AskingToJoin("Ripley"));
	Check(playersInGame.empty() && IDOf(1) == 0 && Net_JoinRequestWaiting(name, sizeof(name), &who) && strcmp(name, "Ripley") == 0 && who.connection == 1,
		"a joiner asks: it isn't let in yet, and the host has its request, with its name");
	{
		// Added 2026-11-09 by Melanikus for the AvP port (protocol version 2): a joiner on this network had no handshake, so no check code
		char code[kNetRelayCheckCodeBytes];
		memset(code, 'X', sizeof(code));
		Check(!Net_JoinRequestCheckCode(who, code) && code[0] == 0, "a joiner on this network has no check code to show: it had no handshake");
		std::string const source = ReadSourceFile("3dc/networking.cpp");
		Check(Has(source, "|| !PeerIsThroughRelay(asking))\n\t{\n\t\treturn false;") &&
			Has(source, "NetPeerHostSide *const side = RelayEndToEndSide(asking->address.port, false, enet_time_get());\n\treturn side && side->Ready() && CopyCheckCode(side->CheckCode(), out);"),
			"SOURCE: the host shows a code only for the waiting joiner through the relay, from its own handshake, once that's done");
	}

	int const gentleBefore = closedGently;
	Connects(2);
	TakeEverything();
	Hears(2, AskingToJoin("Hicks"));
	Check(closedGently == gentleBefore + 1 && lastDisconnectData == kNetJoinRefusedBusy && Net_JoinRequestWaiting(name, sizeof(name), &who) && strcmp(name, "Ripley") == 0,
		"a second joiner meanwhile is told the host is busy, and the first is still the one waiting");

	enetClock += 20000;
	Net_ServiceNetwork();
	Check(peers[1].state == ENET_PEER_STATE_CONNECTED && Net_JoinRequestWaiting(name, sizeof(name), &who),
		"20 s on, past the limit on saying who it is, a joiner the host is answering is still waiting");

	NetJoinWho const somebodyElse = { who.connection, who.connectID + 1 };
	Net_AnswerJoinRequest(somebodyElse, true);
	Check(playersInGame.empty() && Net_JoinRequestWaiting(name, sizeof(name), &who), "an answer for somebody else does nothing");

	Net_AnswerJoinRequest(who, true);
	Check(playersInGame.size() == 1 && IDOf(1) != 0 && !Net_JoinRequestWaiting(name, sizeof(name), &who),
		"the host says yes: the joiner is let in, as before, and the request has gone");
	EndTheGame();

	// A name that never ends: the host's screen still gets one that does
	HostWithClients(0);
	Connects(1);
	TakeEverything();
	{
		std::vector<uint8_t> noEnd = AskingToJoin("Drake");

		if (noEnd.size() >= sizeof(MessageHeader) + sizeof(PlayerDetails))
		{
			memset(((PlayerDetails *)(noEnd.data() + sizeof(MessageHeader)))->name, 'D', kPlayerNameSize);
		}

		Hears(1, noEnd);
	}
	{
		char wide[kPlayerNameSize * 4];
		memset(wide, 'X', sizeof(wide));
		Check(Net_JoinRequestWaiting(wide, sizeof(wide), &who) && strlen(wide) == kPlayerNameSize - 1 && wide[0] == 'D',
			"a request whose name never ends still hands over a name that ends, however much room it's given");
	}
	EndTheGame();

	// No
	HostWithClients(0);
	Connects(1);
	TakeEverything();
	Hears(1, AskingToJoin("Bishop"));
	Net_JoinRequestWaiting(name, sizeof(name), &who);
	Net_AnswerJoinRequest(who, false);
	Check(playersInGame.empty() && IDOf(1) == 0 && lastDisconnectData == kNetJoinRefusedNo && !Net_JoinRequestWaiting(name, sizeof(name), &who),
		"the host says no: nothing is made for the joiner, its connection is closed with the host's no, and the request has gone");
	Net_AnswerJoinRequest(who, true);
	Check(playersInGame.empty(), "a second answer to a request that's gone does nothing");
	EndTheGame();

	// No answer in time
	HostWithClients(0);
	Connects(1);
	TakeEverything();
	Hears(1, AskingToJoin("Vasquez"));
	enetClock += kNetJoinAnswerWithinMs;
	Net_ServiceNetwork();
	Check(playersInGame.empty() && lastDisconnectData == kNetJoinRefusedNoAnswer && !Net_JoinRequestWaiting(name, sizeof(name), &who),
		"unanswered for 30 s, the joiner is told no answer came, and the request has gone");
	EndTheGame();

	// The joiner who asked leaves, and somebody new comes to the same place
	HostWithClients(0);
	Connects(1);
	TakeEverything();
	Hears(1, AskingToJoin("Gorman"));
	Closes(1);
	TakeEverything();
	Check(!Net_JoinRequestWaiting(name, sizeof(name), &who), "a joiner who leaves while waiting takes its request with it");
	peers[1].connectID = 7;
	Connects(1);
	TakeEverything();
	Check(!Net_JoinRequestWaiting(name, sizeof(name), &who), "and the next one in that place, who hasn't asked, has no request of the first one's");
	peers[1].connectID = 0;
	EndTheGame();

	// What needs no question isn't asked: another version
	HostWithClients(0);
	Connects(1);
	TakeEverything();
	std::vector<uint8_t> older = AskingToJoin("Apone");

	if (older.size() >= sizeof(MessageHeader) + sizeof(PlayerDetails))
	{
		((PlayerDetails *)(older.data() + sizeof(MessageHeader)))->version = kMultiplayerVersion - 1;
	}

	Hears(1, older);
	Check(!Net_JoinRequestWaiting(name, sizeof(name), &who) && playersInGame.empty(), "a joiner of another version isn't asked about: it's told no at once, as before");
	EndTheGame();

	// The joiner's side
	Check(JoinAGame() == NET_OK && Net_JoinAnswerNow() == kNetJoinAnswerWaiting, "a joiner, once its request has gone, waits for the host's answer");
	NetID const given = 9;
	Hears(0, AMessage(1, NET_SETPLAYERID, 1, &given, sizeof(given)));
	Check(Net_JoinAnswerNow() == kNetJoinAnswerYes, "and the host giving it its player ID is the yes");
	EndTheGame();

	Check(JoinAGame() == NET_OK && Net_JoinAnswerNow() == kNetJoinAnswerWaiting, "(joining again)");
	{
		ENetEvent const closed = { ENET_EVENT_TYPE_DISCONNECT, &peers[0], NULL, kNetJoinRefusedNo };
		peers[0].state = ENET_PEER_STATE_DISCONNECTED;
		eventsWaiting.push_back(closed);
		TakeEverything();
	}
	Check(Net_JoinAnswerNow() == kNetJoinAnswerNo, "the host closing the connection with its no is the no");
	EndTheGame();
	Check(Net_JoinAnswerNow() == kNetJoinAnswerNo, "and the answer stays once the game is left, for the screen to say why");

	Check(JoinAGame() == NET_OK && Net_JoinAnswerNow() == kNetJoinAnswerWaiting, "a new join starts waiting again");
	enetClock += kNetJoinAnswerWithinMs + 5000;
	Check(Net_JoinAnswerNow() == kNetJoinAnswerNoAnswer, "and with no answer at all, past the host's own limit, it's no answer");
	EndTheGame();

	Net_LetEveryJoinerIn(true);
}

static void TestConnectionsThatHoldAPlace()
{
	HostWithClients(1);

	// One that never finishes starting
	int const resetBefore = peersReset;

	peers[2].state = ENET_PEER_STATE_ACKNOWLEDGING_CONNECT;
	Net_ServiceNetwork();
	enetClock += 2999;
	Net_ServiceNetwork();
	Check(peersReset == resetBefore && peers[2].state == ENET_PEER_STATE_ACKNOWLEDGING_CONNECT, "a connection still starting keeps its place for a moment");
	enetClock += 1;
	Net_ServiceNetwork();
	Check(peersReset == resetBefore + 1 && peers[2].state == ENET_PEER_STATE_DISCONNECTED, "and loses it if it never finishes");

	// An older version of the game, which our ENet (patch 006) told no without a place, and counted
	int const linesBefore = errorLines;

	theHost.olderConnectsRefused = 2;
	Net_ServiceNetwork();

	int const linesAfter = errorLines;

	Net_ServiceNetwork();
	Check(linesAfter > linesBefore && errorLines == linesAfter && theHost.olderConnectsRefused == 0, "an older version of the game that ENet told no is in the host's log, and only once");

	// One that was let in, and went quiet
	int const askedBefore = closedGently;

	Hears(1, AGameMessage(IDOf(1)));
	enetClock += 299999;
	Net_ServiceNetwork();
	Check(closedGently == askedBefore, "a player can be quiet for a while");
	Hears(1, AGameMessage(IDOf(1)));
	enetClock += 299999;
	Net_ServiceNetwork();
	Check(closedGently == askedBefore, "and anything it sends starts that again");
	enetClock += 1;
	Net_ServiceNetwork();
	Check(closedGently == askedBefore + 1, "but a player silent for minutes is asked to go, the ordinary way, so the game hears");
	EndTheGame();
}

static void TestTooManyBytes()
{
	HostWithClients(1);

	enetClock += 1000;

	std::vector<uint8_t> big = AGameMessage(IDOf(1));

	// As big as a message can be and still be read at all. 400 of these are more than the limit.
	big.resize(2048, 0);

	int acceptedBytes = 0;
	int refused = 0;

	for (int i = 0; i < 400; i++)
	{
		if (Hears(1, big) == big.size())
		{
			acceptedBytes += (int)big.size();
		}
		else
		{
			refused++;
		}
	}

	Check(acceptedBytes > 400 * 1024, "a client can send a lot of bytes in a second");
	Check(acceptedBytes <= 512 * 1024 && refused > 0, "but only so many");
	enetClock += 1000;
	Check(Hears(1, big) == big.size(), "and it has more the next second");
	EndTheGame();
}

static void TestAnsweringWhileLoading()
{
	int const before = servicesWithNowhereForAnEvent;

	Net_KeepAnsweringWhileLoading();
	Check(servicesWithNowhereForAnEvent == before, "with no game going, a load has nobody to answer");

	HostWithClients(1);
	Net_SayLongestGapWhileLoading();

	int const hosting = servicesWithNowhereForAnEvent;

	Net_KeepAnsweringWhileLoading();
	Check(servicesWithNowhereForAnEvent == hosting + 1, "in a game, a load answers, and with nowhere for an event, so ENet keeps what arrives until the level is ready");
	Net_KeepAnsweringWhileLoading();
	Check(servicesWithNowhereForAnEvent == hosting + 1, "it doesn't answer at every step");
	enetClock += 100;
	Net_KeepAnsweringWhileLoading();
	Check(servicesWithNowhereForAnEvent == hosting + 2, "only every so often");
	EndTheGame();
}

static void TestWhatAClientCantAfford()
{
	HostWithClients(2);

	std::vector<uint8_t> const message = AGameMessage(IDOf(1));
	int const passedOnBefore = packetsToAPeer;

	clientCanAfford = false;
	Check(Hears(1, message) == 0, "the game doesn't read what a client can't afford");
	Check(packetsToAPeer == passedOnBefore, "and nobody else is sent it");
	clientCanAfford = true;
	Check(Hears(1, message) == message.size(), "a message it can afford goes to the game");
	Check(packetsToAPeer > passedOnBefore, "and on to everyone else");
	EndTheGame();
}

static void TestARealClientGetsIn()
{
	HostWithClients(0);
	Connects(1);
	TakeEverything();
	Check(Hears(1, realClientsAsk) == 0, "what a real client says isn't for the game to read");
	Check(playersInGame.size() == 1 && playersInGame[0].name == playerName, "and the host lets it in, by the name it gave");
	EndTheGame();
}

// What the host's screen says when someone connects. Players stream and share pictures of their screens, so the other machine's address
// and port are never shown there, only which connection it is (SCREENIP-1; the user's ruling: no IPs).
static void TestNoAddressOnScreen()
{
	HostWithClients(0);
	screenLines.clear();

	peers[3].address.host = 0x1400A8C0;   // 192.168.0.20, as ENet keeps it: the first of the four numbers in the lowest byte
	peers[3].address.port = 23513;
	Connects(3);
	TakeEverything();

	bool saidWhichConnection = false;
	bool showedTheAddress = false;

	for (size_t i = 0; i < screenLines.size(); i++)
	{
		saidWhichConnection = saidWhichConnection || screenLines[i] == "A player is joining (connection 4)";
		showedTheAddress = showedTheAddress || Has(screenLines[i], "192") || Has(screenLines[i], "168") || Has(screenLines[i], "23513");
	}

	Check(saidWhichConnection, "the host's screen says a player is joining, and as which connection");
	Check(!showedTheAddress, "and never their address or port");
	Check(!Has(ReadSourceFile("3dc/networking.cpp"), "enet_address_get_host_ip("), "and the network code never turns an address into text at all");

	peers[3].address.host = 0;
	peers[3].address.port = 0;
	EndTheGame();
}

static void TestLettingPeopleIn()
{
	playersInGame.clear();
	Check(HostAGame() == NET_OK, "someone hosts");
	Check(connectionsAllowed == 7, "with room for the other seven players, and no more");
	Check(channelsAllowed == 1, "and one channel for each, the only one the game uses");
	Check(theHost.duplicatePeers == kMostClients && theHost.maximumPacketSize == kMostMessageBytes && theHost.maximumWaitingData == 8 * 1024 * 1024, "with the game's limits on places from one address (through the relay, every joiner comes from its one address, so all the game's places), a message's size, and what can wait");
	Check(theHost.checksum == enet_crc32, "and a checksum on every packet");
	Check(netSession.nPlayers == 1, "and the game says it has one player");

	int const hostGoneBefore = hostHasGoneCalls;
	size_t nothing = 0;

	Check(Net_Receive(received, nothing) == NET_NO_MESSAGES && hostHasGoneCalls == hostGoneBefore, "a host has no server, and that doesn't mean it has lost one");

	NetID const hostID = AvPNetID;

	sentMessages.clear();
	peers[1].data = (void *)12345;
	Connects(1);
	TakeEverything();
	Check(sentMessages.empty() && playersInGame.empty(), "a connection isn't a player, and is told nothing");
	Check(IDOf(1) == 0, "and whatever ENet's list had in that place before is forgotten");

	Check(Hears(1, AskingToJoin("Ripley")) == 0, "asking to join is between them and the host, so the game doesn't get it as a message");
	Check(playersInGame.size() == 1 && playersInGame[0].name == "Ripley", "and they're in the game, with their name");
	Check(sentMessages.size() == 1 && sentMessages[0].to == &peers[1], "the host answers on that one connection");
	Check(sentMessages.size() == 1 && sentMessages[0].header.isSystemMessage == 1 && sentMessages[0].header.messageType == NET_SETPLAYERID && sentMessages[0].header.marker == kMarker, "with their ID");

	NetID const ripley = sentMessages.empty() ? 0 : IDIn(sentMessages[0]);

	Check(ripley != 0 && !playersInGame.empty() && ripley == playersInGame[0].id, "the same ID the game has for them");
	Check(ripley != hostID, "which isn't the host's");
	Check(IDOf(1) == ripley, "and the connection remembers it");
	Check(netSession.nPlayers == 2, "and anyone looking for a game now sees two players");

	sentMessages.clear();
	Connects(2);
	TakeEverything();
	Hears(2, AskingToJoin("Hicks"));
	Check(sentMessages.size() == 1 && sentMessages[0].to == &peers[2], "the second client's answer goes only to them");
	Check(playersInGame.size() == 2 && playersInGame[1].name == "Hicks" && !sentMessages.empty() && IDIn(sentMessages[0]) != ripley, "and they have their own ID, though it's the same moment by the clock");

	EverythingArrives();
	Check(packetsAlive == 0, "and nothing was left behind in memory");
	EndTheGame();
}

static void TestWhatAClientCantSay()
{
	HostWithClients(2);

	NetID const one = IDOf(1), two = IDOf(2);
	size_t const playersBefore = playersInGame.size();
	int const askedBefore = playersAsked;
	uint8_t const nPlayersBefore = netSession.nPlayers;

	Connects(3);
	TakeEverything();

	auto const refused = [&](int n, const std::vector<uint8_t> &message, const std::string &what)
	{
		size_t const heard = Hears(n, message);

		Check(heard == 0 && sentMessages.empty() && playersInGame.size() == playersBefore && playersAsked == askedBefore && playersGone.empty() && netSession.nPlayers == nPlayersBefore && IDOf(1) == one && IDOf(2) == two, what);
		sentMessages.clear();
	};

	std::vector<uint8_t> const tooShort(sizeof(MessageHeader) - 1, 0);
	std::vector<uint8_t> asking = AskingToJoin("Burke");
	std::vector<uint8_t> shortAsk(asking.begin(), asking.end() - 1);
	std::vector<uint8_t> longAsk = asking;
	std::vector<uint8_t> const wrongMarker = AMessage(0, NET_ID_ALLPLAYERS, one, "x", 1, 0x12345678);
	std::vector<uint8_t> const wrongMarkerAsk = AMessage(1, NET_CREATEPLAYERORGROUP, 0, asking.data() + sizeof(MessageHeader), sizeof(PlayerDetails), 0x12345678);
	NetID const someoneElse = two;

	longAsk.push_back(0);

	refused(1, std::vector<uint8_t>(), "nothing at all is no message");
	refused(1, tooShort, "too short to have a header is no message");

	std::vector<uint8_t> almostAHeader = AGameMessage(one);

	almostAHeader.resize(sizeof(MessageHeader) - 1);
	refused(1, almostAHeader, "and that's so when all that's missing is the last byte");

	std::vector<uint8_t> tooBig = AGameMessage(one);

	tooBig.resize(kMostMessageBytes + 1);
	refused(1, tooBig, "a message too big for the game's buffer is no message");

	std::vector<uint8_t> asBigAsItGets = AGameMessage(one);

	asBigAsItGets.resize(kMostMessageBytes);
	Check(Hears(1, asBigAsItGets) == kMostMessageBytes, "and one that's exactly the most there can be goes through whole");
	EverythingArrives();
	sentMessages.clear();

	refused(1, wrongMarker, "a message that isn't marked as this game's goes no further");
	refused(3, wrongMarkerAsk, "and nor does a request to join");
	refused(1, asking, "someone who's in can't join again and take another place");
	refused(3, shortAsk, "a request to join that's too short is refused");
	refused(3, longAsk, "and one that's too long");
	refused(1, AMessage(1, NET_DESTROYPLAYERORGROUP, one, &someoneElse, sizeof(someoneElse)), "a client can't say someone else has left, or that anyone has: that's what the connection closing is for");
	refused(1, AMessage(1, NET_HOST, one), "or that it's the host now");
	refused(1, AMessage(1, NET_SETPLAYERID, one, &someoneElse, sizeof(someoneElse)), "or hand out an ID");
	refused(1, AMessage(1, NET_SESSIONLOST, one), "or that the game's over");
	refused(1, AMessage(1, 9999, one), "or anything else that isn't a message");
	refused(1, AMessage(2, 9999, one), "and any number at all counts as saying it's a system message");
	refused(3, AMessage(1, NET_DESTROYPLAYERORGROUP, 0, asking.data() + sizeof(MessageHeader), sizeof(PlayerDetails)), "only asking to be let in lets anyone in, whatever size it is");
	refused(3, AGameMessage(0), "someone who hasn't been let in can't send to the game");
	refused(3, AGameMessage(one), "not even by saying they're someone who has");
	refused(1, AGameMessage(someoneElse), "a client can't send saying it's someone else");
	refused(1, AGameMessage(AvPNetID), "or that it's the host");
	refused(1, AGameMessage(0), "or no one");

	int const linesBefore = errorLines;

	for (int i = 0; i < 20000; i++)
	{
		Hears(1, AGameMessage(someoneElse));
	}

	Check(errorLines - linesBefore < 40, "and however often they try, the log only says so now and then: " + std::to_string(errorLines - linesBefore) + " lines");
	EndTheGame();
}

// A game that says it's another version doesn't get in, and neither does a connection that never says anything
static void TestWhoDoesntGetIn()
{
	HostWithClients(0);

	// Everything is what a real client says, except its version
	std::vector<uint8_t> oldVersion = AskingToJoin("OLD");

	if (oldVersion.size() >= sizeof(MessageHeader) + sizeof(PlayerDetails))
	{
		((PlayerDetails *)(oldVersion.data() + sizeof(MessageHeader)))->version = kMultiplayerVersion - 1;
	}

	Connects(1);
	TakeEverything();
	Hears(1, oldVersion);
	Check(playersInGame.empty(), "a game of another version isn't let in");
	Check(peers[1].state != ENET_PEER_STATE_CONNECTED || peers[1].data == NULL, "and has no player ID");
	EndTheGame();

	HostWithClients(0);
	enetClock = 1000;
	Connects(1);
	TakeEverything();
	enetClock += 14000;
	Net_ServiceNetwork();
	Check(peers[1].state == ENET_PEER_STATE_CONNECTED, "a connection that has said nothing yet stays for a while");
	enetClock += 2000;
	Net_ServiceNetwork();
	Check(peers[1].state != ENET_PEER_STATE_CONNECTED, "and is dropped after that");
	EndTheGame();

	HostWithClients(0);
	enetClock = 50000;
	Connects(1);
	TakeEverything();
	Hears(1, AskingToJoin("FINE"));
	Check(playersInGame.size() == 1, "someone who says who they are gets in");
	enetClock += 100000;
	Net_ServiceNetwork();
	Check(peers[1].state == ENET_PEER_STATE_CONNECTED, "and stays however long they play");
	EndTheGame();
}

// Nobody can use the host to reach everyone else as fast as they like
static void TestTooManyMessages()
{
	HostWithClients(2);

	NetID const one = IDOf(1);

	enetClock = 200000;
	TakeEverything();
	sentMessages.clear();

	for (int i = 0; i < 1500; i++)
	{
		Hears(1, AGameMessage(one));
	}

	Check(sentMessages.size() == 1000, "a client's messages are passed on up to the limit, and no further");
	EndTheGame();
}

static void TestANameWithNoEnd()
{
	HostWithClients(0);

	PlayerDetails details;

	memset(&details, 'A', sizeof(details));
	details.version = kMultiplayerVersion;
	Connects(1);
	TakeEverything();
	Hears(1, AMessage(1, NET_CREATEPLAYERORGROUP, 0, &details, sizeof(details)));
	Check(playersInGame.size() == 1 && playersInGame[0].name.size() == sizeof(details.name) - 1, "a name that never ends gets an end, in the host's copy");
	EndTheGame();

	char from[13];
	char to[13];

	memset(from, 'B', sizeof(from));
	memset(to, 0, sizeof(to));
	NetCopyPlayerName(to, from, sizeof(to));
	Check(strlen(to) == sizeof(to) - 1, "and in a client's copy of the game's description");
	Check(memcmp(to, from, sizeof(to) - 1) == 0, "with everything else in it kept");
}

static void TestPassingThingsOn()
{
	HostWithClients(2);

	NetID const one = IDOf(1);

	Connects(3);
	TakeEverything();
	sentMessages.clear();

	std::vector<uint8_t> const message = AGameMessage(one);

	Check(Hears(1, message) == message.size(), "a message from someone who's in goes to this game");
	Check(memcmp(received, message.data(), message.size()) == 0, "just as it arrived");
	Check(sentMessages.size() == 1 && sentMessages[0].to == &peers[2], "and on to the others, not back to who sent it, and not to a connection that hasn't been let in");
	Check(!sentMessages.empty() && sentMessages[0].header.fromID == one && sentMessages[0].header.marker == kMarker && sentMessages[0].header.isSystemMessage == 0, "still saying who it's from");
	Check(!sentMessages.empty() && sentMessages[0].body.size() == message.size() - sizeof(MessageHeader) && memcmp(sentMessages[0].body.data(), message.data() + sizeof(MessageHeader), sentMessages[0].body.size()) == 0, "with all of it there");

	EverythingArrives();

	int const aliveBefore = packetsAlive;

	Closes(2);
	TakeEverything();
	sentMessages.clear();
	Hears(1, message);
	Check(sentMessages.empty(), "with nobody else in the game it goes no further");
	Check(packetsAlive == aliveBefore, "and the copy made for them is freed");

	// What the host says itself
	HostWithClients(2);
	Connects(3);
	TakeEverything();

	uint8_t words[] = "from the host";
	int const made = packetsMade;

	Check(Net_Send(NET_ID_ALLPLAYERS, AvPNetID, 0, words, sizeof(words)) == NET_OK, "the host can say something to everyone");
	Check(packetsMade == made + 1 && sentMessages.size() == 2 && sentMessages[0].to == &peers[1] && sentMessages[1].to == &peers[2], "and it goes to those let in, not to a connection that hasn't been");
	EverythingArrives();

	int const aliveNow = packetsAlive;

	Closes(1);
	Closes(2);
	TakeEverything();
	sentMessages.clear();
	Check(Net_Send(NET_ID_ALLPLAYERS, AvPNetID, 0, words, sizeof(words)) == NET_OK && sentMessages.empty(), "with nobody to hear it, nothing goes out");
	Check(packetsAlive == aliveNow, "and nothing is left behind in memory");
	EndTheGame();

	Check(Net_Send(NET_ID_ALLPLAYERS, 1, 0, words, sizeof(words)) == NET_FAIL, "a game that has left has nobody to send to, and doesn't try");

	// The game can fill its whole buffer, and the header goes in front
	HostWithClients(1);

	std::vector<uint8_t> full(NET_MESSAGEBUFFERSIZE, 7);

	Check(Net_Send(NET_ID_ALLPLAYERS, AvPNetID, 0, full.data(), full.size()) == NET_OK, "a full buffer can be sent");
	Check(sentMessages.size() == 1 && sentMessages[0].body.size() == full.size(), "and all of it goes, because the check at the other end would refuse a message with its end cut off");
	EverythingArrives();
	EndTheGame();
}

static void TestNoRoom()
{
	// DetermineAvailableCharacterTypes above says four players, and the host is one of them
	HostWithClients(3);
	Check(netSession.nPlayers == 4 && netSession.maxPlayers == 4, "the game has as many players as it said it would take");

	int const gentleBefore = closedGently;
	int const askedBefore = playersAsked;

	Connects(4);
	TakeEverything();
	sentMessages.clear();
	Hears(4, AskingToJoin("Too many"));
	Check(sentMessages.empty() && IDOf(4) == 0, "the next one has no answer and no ID");
	Check(playersAsked == askedBefore, "and the game isn't asked to take them");
	Check(closedGently == gentleBefore + 1, "and their connection is closed");
	EndTheGame();

	// The game has its own list, which can be full for its own reasons
	HostWithClients(0);
	gameHasRoom = false;

	int const gentleNext = closedGently;

	Connects(1);
	TakeEverything();
	sentMessages.clear();
	Hears(1, AskingToJoin("No room"));
	gameHasRoom = true;
	Check(sentMessages.empty() && IDOf(1) == 0 && closedGently == gentleNext + 1 && netSession.nPlayers == 1, "the same if the game has no room");
	EndTheGame();

	// And an answer that can't be sent leaves nobody half in the game
	HostWithClients(0);
	Connects(1);
	TakeEverything();

	int const gentleLast = closedGently;

	packetCreateFails = true;
	Hears(1, AskingToJoin("No answer"));
	packetCreateFails = false;
	Check(playersInGame.empty() && playersGone.size() == 1, "someone who can't be told their ID doesn't stay in the game");
	Check(IDOf(1) == 0 && closedGently == gentleLast + 1 && netSession.nPlayers == 1, "and has no ID, and their connection is closed");
	EndTheGame();

	// The same if ENet won't send it
	HostWithClients(0);
	Connects(1);
	TakeEverything();

	int const alive = packetsAlive;

	peers[1].state = ENET_PEER_STATE_DISCONNECTED;
	Hears(1, AskingToJoin("Gone already"));
	Check(playersInGame.empty() && IDOf(1) == 0, "and the same if the answer can't be sent");
	Check(packetsAlive == alive, "and the answer that couldn't be sent is freed");
	EndTheGame();
}

static void TestSomeoneGoing()
{
	HostWithClients(2);

	NetID const one = IDOf(1), two = IDOf(2);

	Closes(1);
	TakeEverything();
	Check(playersGone.size() == 1 && playersGone[0] == one, "a connection closing, however that came about, means that player has gone");
	Check(IDOf(1) == 0, "and the connection forgets them");
	Check(netSession.nPlayers == 2, "and the game has one fewer");
	Check(IDOf(2) == two, "and the other player's still there");

	playersGone.clear();
	Connects(3);
	Closes(3);
	TakeEverything();
	Check(playersGone.empty() && netSession.nPlayers == 2, "a connection that was never let in leaves nothing to take away");

	// The next one to have that place in ENet's list
	Connects(1);
	TakeEverything();
	Check(Hears(1, AGameMessage(one)) == 0, "whoever gets the same place next can't speak as the one who left");

	sentMessages.clear();
	Hears(1, AskingToJoin("Next"));
	Check(sentMessages.size() == 1 && IDIn(sentMessages[0]) != one && IDIn(sentMessages[0]) != two, "and gets an ID of their own");
	EndTheGame();
}

static void TestTheClientsSide()
{
	AvPNetID = 777;
	int const noAddress = hostsWithNoAddress;
	size_t const bound = hostsBoundTo.size();

	Check(JoinAGame() == NET_OK, "someone joins");
	Check(hostsWithNoAddress == noAddress + 1 && hostsBoundTo.size() == bound, "with no port of their own for anyone to find");
	Check(connectionsAllowed == 1, "and only the one connection");
	Check(channelsAllowed == 1 && channelsAsked == 1, "and one channel, the only one the game uses");
	Check(theHost.maximumPacketSize == kMostMessageBytes && theHost.maximumWaitingData == 8 * 1024 * 1024 && theHost.checksum == enet_crc32, "with the game's limits on a message's size and what can wait, and a checksum on every packet");
	Check(AvPNetID == 0, "the ID from the last game has gone");

	NetID const given = 4242;
	NetID const other = 5555;

	Check(Hears(0, AMessage(1, 9999, 1)) == 0 && AvPNetID == 0, "a system message that isn't their ID changes nothing");
	Check(Hears(0, AMessage(1, 9999, 1, &given, sizeof(given))) == 0 && AvPNetID == 0, "even one the same size as an ID");
	Check(Hears(0, AMessage(1, NET_HOST, 1)) == 0 && AvP.Network == I_Peer, "and that goes for being told they're the host now");
	Check(Hears(0, AMessage(1, NET_SETPLAYERID, 1, &given, sizeof(given) - 1)) == 0 && AvPNetID == 0, "an ID that's too short is no ID");
	Check(Hears(0, AMessage(1, NET_SETPLAYERID, 1, &given, sizeof(given), 0x12345678)) == 0 && AvPNetID == 0, "and nor is one that isn't marked as this game's");

	std::vector<uint8_t> tooLong = AMessage(1, NET_SETPLAYERID, 1, &given, sizeof(given));

	tooLong.push_back(0);
	Check(Hears(0, tooLong) == 0 && AvPNetID == 0, "and nor is one that's too long");

	Check(Hears(0, AMessage(1, NET_SETPLAYERID, 1, &given, sizeof(given))) == 0, "the ID isn't for the game to read as a message");
	Check(AvPNetID == given && thisClientPlayer.ID == given, "and this is their ID now");
	Hears(0, AMessage(1, NET_SETPLAYERID, 1, &other, sizeof(other)));
	Check(AvPNetID == given, "a host can only say so once");

	std::vector<uint8_t> const message = AGameMessage(99);

	Check(Hears(0, message) == message.size(), "a game message from the host goes to the game");
	Check(Hears(0, AMessage(0, NET_ID_ALLPLAYERS, 99, "x", 1, 0x12345678)) == 0, "unless it isn't marked as this game's");
	Check(Hears(0, std::vector<uint8_t>(3, 0)) == 0, "or it's too short to have a header");
	Check(Hears(0, std::vector<uint8_t>()) == 0, "or it's empty");
	EndTheGame();
}

static void TestWaitingToJoin()
{
	Check(JoinAGame() == NET_OK, "someone is joining");
	netGameData.needGameDescription = 1;
	Check(Net_ConnectingToSession() == NET_OK, "and is waiting for the game's description");
	netGameData.needGameDescription = 0;
	Check(Net_ConnectingToSession() == AVPMENU_MULTIPLAYER_CONFIG_JOIN, "and when it comes they can go on");
	netGameData.needGameDescription = 1;

	// The host says no, or goes
	int const destroyed = hostsDestroyed;
	int const gone = hostHasGoneCalls;

	Closes(0);
	TakeEverything();
	Check(hostHasGoneCalls == gone + 1, "the host going tells the game, the next time it looks");
	Check(Net_SendSystemMessage(NET_DESTROYPLAYERORGROUP, 0, 0, &destroyed, sizeof(destroyed)) == NET_FAIL, "and there's nobody to send to any more");
	Check(Net_ConnectingToSession() == NET_FAIL, "and the wait is over");
	Check(AvP.Network == I_No_Network && AvPNetID == 0, "and they're in no game");
	Check(hostsDestroyed == destroyed + 1, "and nothing stays open");

	size_t size = 0;
	int const goneNow = hostHasGoneCalls;

	Check(Net_Receive(received, size) == NET_NO_MESSAGES, "a game with no host has nothing to read, or the game's loop would go round for ever");
	Check(hostHasGoneCalls == goneNow, "and a game that has left asks nothing more about its host");

	// The player gets bored, after the host has given them an ID
	Check(JoinAGame() == NET_OK, "someone is joining again");

	NetID const given = 31;

	Hears(0, AMessage(1, NET_SETPLAYERID, 1, &given, sizeof(given)));
	Check(AvPNetID == given, "and has an ID");

	int const destroyedNext = hostsDestroyed;

	DebouncedKeyboardInput[KEY_ESCAPE] = 1;
	Check(Net_ConnectingToSession() == NET_FAIL, "Escape stops the wait");
	DebouncedKeyboardInput[KEY_ESCAPE] = 0;
	Check(AvP.Network == I_No_Network && hostsDestroyed == destroyedNext + 1, "and that lets go of the connection as well");
	Check(AvPNetID == 0, "and the ID, which was for that game");

	// The console's disconnect command, in the middle of a game
	Check(JoinAGame() == NET_OK, "someone is in a game again");

	int const goneBefore = hostHasGoneCalls;

	Net_Disconnect();
	Check(Net_Receive(received, size) == NET_NO_MESSAGES && hostHasGoneCalls == goneBefore + 1, "a client whose connection the console closed has lost its game too");
	AvP.Network = I_No_Network;
}

static void TestLeavingClosesConnections()
{
	HostWithClients(2);

	Connects(3);
	TakeEverything();

	int const closedBefore = closedAtOnce;
	int const closesBefore = closesAsked;

	Net_Disconnect();
	Check(closesAsked == closesBefore + 1 && hostsDestroyedWhenClosed == hostsDestroyed - 1, "a host leaving closes its connections after what's waiting, before letting its ENet host go");
	Check(closedAtOnce == closedBefore, "and closes none of them at once, which would throw away the last thing it said");
	AvP.Network = I_No_Network;

	Check(JoinAGame() == NET_OK, "and someone joining");
	netGameData.needGameDescription = 1;

	int const closedClient = closedAtOnce;
	int const closesClient = closesAsked;

	sentMessages.clear();
	Net_Disconnect();
	Check(closesAsked == closesClient + 1 && hostsDestroyedWhenClosed == hostsDestroyed - 1 && closedAtOnce == closedClient, "does the same when they leave");
	Check(sentMessages.empty(), "and there's no message saying they've gone for anyone to have to believe");
	AvP.Network = I_No_Network;
}

// A connection whose other end never answers is waited for only so long, and the log says how many there were
static void TestLeavingWithoutAnAnswer()
{
	Check(JoinAGame() == NET_OK, "joining, to leave without an answer");

	int const destroyed = hostsDestroyed;

	logLines.clear();
	connectionsThatDontAnswer = 1;
	Net_Disconnect();
	connectionsThatDontAnswer = 0;
	Check(LogHas("net: left without hearing back from 1 connection(s), which may have gone already"), "leaving without an answer from a connection says so");
	Check(hostsDestroyed == destroyed + 1, "and still lets its ENet host go");
	AvP.Network = I_No_Network;
}

// A client's one connection is the host's, so its closing is the host gone, and the log says so. On a host, a connection closing is
// a client gone, and that isn't this line.
static void TestTheHostsConnectionClosing()
{
	uint8_t message[16];
	size_t size = 0;

	Check(JoinAGame() == NET_OK, "joining, for the host's connection to close");
	logLines.clear();
	eventsWaiting.push_back(ENetEvent{ ENET_EVENT_TYPE_DISCONNECT, &thePeer, NULL });
	Net_Receive(message, size);
	Check(LogHas("net: the host's connection closed"), "a client says when the host's connection closes");
	EndTheGame();

	Check(HostAGame() == NET_OK, "hosting, for a client's connection to close");
	logLines.clear();
	eventsWaiting.push_back(ENetEvent{ ENET_EVENT_TYPE_DISCONNECT, &peers[1], NULL });
	Net_Receive(message, size);
	Check(!LogHas("net: the host's connection closed"), "and a host doesn't say that when a client's connection closes");
	EndTheGame();
}

static void TestPlayerIDs()
{
	std::vector<NetID> ids;

	for (int i = 0; i < 2000; i++)
	{
		ids.push_back(Net_GetNextPlayerID());
	}

	std::vector<NetID> sorted = ids;

	std::sort(sorted.begin(), sorted.end());
	Check(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end(), "no two players get the same ID, though it's all the same moment by the clock");
	Check(sorted.front() != 0, "and none of them is 0, which means nobody");
}

// The game's table of what a message can hold has to agree with the code that reads messages, and that code is too tied to the rest of the game to run here. So this reads both, and compares them for every kind there is.
static void TestTheTableOfMessageParts()
{
	std::string const pldnet = ReadSourceFile("3dc/avp/win95/pldnet.cpp");
	std::string const header = ReadSourceFile("3dc/avp/win95/pldnet.h");
	std::string const table = Between(pldnet, "static constexpr NetPartRule kNetPartRules[] =", "static constexpr bool NetPartRulesAreInOrder()");
	std::string const reading = Between(pldnet, "static void ProcessGameMessage(NetID senderId, uint8_t *msgP, size_t msgSize)\n{", "/* if we have read the message correctly");
	std::string const kinds = Between(header, "typedef enum netmessagetype", "} NETMESSAGETYPE;");

	// Every kind there is, in the order the game numbers them
	std::vector<std::string> names;
	std::stringstream lines(kinds);
	std::string line;

	while (std::getline(lines, line))
	{
		std::string::size_type const at = line.find("NetMT_");

		if (at != std::string::npos && line.find("//") > at)
		{
			// The kind's name is the word that starts there: letters, digits and underscores, up to the comma or the end
			std::string name;

			for (std::string::size_type i = at; i < line.size() && (isalnum((unsigned char)line[i]) || line[i] == '_'); i++)
			{
				name += line[i];
			}

			names.push_back(name);
		}
	}

	Check(names.size() == 65, "there are 65 kinds of part: " + std::to_string(names.size()));

	int inTheTable = 0, sameLength = 0, inOrder = 0;
	std::string::size_type place = 0;
	std::string mismatched;

	for (std::string const &name : names)
	{
		std::string const entry = "{ " + name + ", ";
		std::string::size_type const at = table.find(entry);

		if (at == std::string::npos)
		{
			mismatched += " " + name;
			continue;
		}

		inTheTable++;
		inOrder += (at >= place) ? 1 : 0;
		place = at;

		std::string const rule = table.substr(at + entry.size(), table.find('\n', at) - at - entry.size());
		std::string::size_type const start = reading.find("case (" + name + "):");
		std::string::size_type const next = reading.find("case (", start + 1);
		std::string const reader = (start == std::string::npos) ? std::string() : reading.substr(start, next == std::string::npos ? std::string::npos : next - start);
		std::string::size_type const steps = reader.find("subMessagePtr += sizeof(");
		std::string const steppedBy = (steps == std::string::npos) ? std::string() : reader.substr(steps + 17, reader.find(';', steps) - steps - 17);

		bool agrees;

		if (Has(reader, "subMessagePtr += GetSizeOf"))
		{
			agrees = Has(rule, "_HEADER), true, ");
		}
		else if (Has(reader, "ProcessNetMsg_ChatBroadcast"))
		{
			agrees = rule.compare(0, 9, "2, true, ") == 0;
		}
		else if (steppedBy.empty())
		{
			agrees = !reader.empty() && rule.compare(0, 10, "0, false, ") == 0;
		}
		else
		{
			agrees = rule.compare(0, steppedBy.size() + 9, steppedBy + ", false, ") == 0;
		}

		sameLength += agrees ? 1 : 0;

		if (!agrees)
		{
			mismatched += " " + name;
		}
	}

	Check(inTheTable == 65, "every one of them is in the table");
	Check(inOrder == 65, "in the game's order");
	Check(sameLength == 65, "and each has the length the code that reads it steps by:" + mismatched);
	Check(CountOf(table, "{ NetMT_") == 65, "and there's nothing else in it");

	// What a message can make. The kind and the weapon are its own numbers, and a shape this level didn't load comes back as -1.
	std::string const makeGhost = ReadSourceFile("3dc/avp/win95/pldghost.cpp");

	Check(Has(makeGhost, "// The kind came in a message, so it could be anything. This list is everything a ghost can be.\n\t\t\t\tsbPtr->shapeIndex = -1;"), "a ghost of an unknown kind has no shape");
	Check(Has(makeGhost, "// The number came in a message, so it could be anything\n\t\t\t\t\t\t\tsbPtr->shapeIndex = -1;"), "and so does a dropped weapon of an unknown kind");
	Check(Has(makeGhost, "if (sbPtr->shapeIndex == -1)\n\t\t{\n\t\t\tNet_NoteRefused("), "and one with no shape isn't made");
	Check(CountOf(Between(makeGhost, "STRATEGYBLOCK *CreateNetGhost(", "int UseExtrapolation=1;"), "if (!root_section)") == 3, "a level without a disc's model can't crash on one");

	// The check of a host's level is one function, and it's the one the menu asks
	Check(Has(ReadSourceFile("3dc/avp/win95/frontend/menus.cpp"), "if (!Net_HostsLevelIsOnItsList())"), "the menu asks whether a host's level is on its list");

	// What goes on the screen
	Check(Has(pldnet, "NetMakeNameUnique(netGameData.playerData[freePlayerIndex].name, NET_PLAYERNAMELENGTH,"), "a new player's name has to be free");
	Check(Has(pldnet, "\"%.*s says: %s\""), "a chat line says someone is speaking");
	Check(Has(pldnet, "NetCleanText(OnScreenMessageBuffer, sizeof(OnScreenMessageBuffer));"), "and can't break into lines of its own");

	// What the numbers in a message mean, as an independent read of this code asked
	Check(Has(pldnet, "if (objectPtr->I_SBtype != I_BehaviourInanimateObject)\n\t{\n\t\tNet_NoteRefused("), "only something that can be picked up gets treated as a pick-up");
	Check(Has(pldnet, "if (!GhostKindCanBeMadeByMessage(type))") && Has(pldnet, "if (objectId == GHOST_PLAYEROBJECTID)"), "a message can't make a player's body, or a kind that isn't on the list");
	Check(CountOf(pldnet, "if (sbPtr && !IsAnAlienGhost(sbPtr))") == 3, "the three alien messages only go to an alien's ghost");
	Check(Has(pldnet, "if (sbPtr && !IsAPlayersGhost(sbPtr))"), "and the short player state only to a player's");
	Check(CountOf(pldnet, "if (!NetDamageMultipleIsGood(multiple))") == 3, "all three damage messages check how many times over a hit counts");
	Check(Has(ReadSourceFile("3dc/avp/weapons.cpp"), "while (Integer > 0) {"), "and the loop can't run away anyway");
	Check(Has(pldnet, "if (NetNumberFitsTable(killerType, 3))"), "the computer's aliens don't score past the three species");
	Check(Has(pldnet, "NetThisSecondHasRoom(timeGetTime(), &newGhostsSecond[sender], &newGhostsUsed[sender], kMostNewGhostsEachSecond)"), "only so many new ghosts a second, for each player");
	Check(Has(pldnet, "NetThisSecondHasRoom(timeGetTime(), &droppedGunsSecond[sender], &droppedGunsUsed[sender], kMostDroppedGunsEachSecond)"), "and dropped guns");
	Check(Has(ReadSourceFile("3dc/avp/pvisible.cpp"), "sbPtr = CreateActiveStrategyBlock();\n\n\tif(!sbPtr) return 0;\n\n\tInitialiseSBValues(sbPtr);"), "running out of blocks is checked before one is used");
	Check(Has(makeGhost, "if (lightPtr)\n\t\t\t\t\t\t{\n\t\t\t\t\t\t\tlightPtr->LightBright"), "a flare with no light is fine");
	Check(Has(makeGhost, "// A kind of alien this level didn't load has no model, and the kind came in a message\n\tif (!root_section)"), "a kind of alien that isn't loaded makes no model");
	Check(CountOf(pldnet, "if (PlayerIdInPlayerList(AvPNetID) == NET_IDNOTINPLAYERLIST) {\n\t\tNet_NoteRefused") >= 2, "a game left off the host's list doesn't write before the list");

	// The build itself stops if a message or a level list changes while the version doesn't
	std::string const versionCheck = ReadSourceFile("3dc/avp/win95/net_version_check.h");

	Check(Has(pldnet, "#include \"net_version_check.h\""), "the build checks the version against the sizes");
	Check(Has(versionCheck, "static_assert(kMultiplayerVersion == kVersionTheseSizesBelongTo,"), "the version has to be the one the sizes were written down for");
	Check(Has(versionCheck, "kSizesNow == kSizesAtThatVersion"), "and the sizes have to be those");
	Check(Has(versionCheck, "2 * sizeof(NETMESSAGE_GAMEDESCRIPTION) +"), "the game description is one of them");
	Check(Has(versionCheck, "59 * MAX_NO_OF_MULTIPLAYER_EPISODES +") && Has(versionCheck, "60 * MAX_NO_OF_COOPERATIVE_EPISODES +") && Has(versionCheck, "61 * kLevelListCampaignLevels;"), "and so are the level lists, the campaign's too");
	Check(Has(versionCheck, "58 * (sizeof(kNetPartRules) / sizeof(kNetPartRules[0])) +"), "and how many kinds of message there are");

	// What has to go out before a machine stops playing can't wait for the next turn of the send clock
	Check(Has(pldnet, "static void NetSendMessagesNow(void)\n{\n\tnetGameData.sendTimer = 0;\n\tNetSendMessages();"), "something urgent doesn't wait for the turn of the clock");
	Check(Has(pldnet, "AddNetMsg_PlayerLeaving();\n\t\tNetSendMessagesNow();"), "someone saying he's leaving is one");
	Check(Has(pldnet, "AddNetMsg_StartGame();\n\t\tNetSendMessagesNow();"), "and the start of a game");
	Check(Has(pldnet, "// AddNetMsg_EndGame();\n\t\tNetSendMessagesNow();"), "and the end of one");

	// Someone leaving says so several times over, and every one of those is honest
	Check(Has(pldnet, "kMostLeavingsEachSecond = NET_MESSAGEITERATIONS;") && Has(pldnet, "for (int i = 0; i < NET_MESSAGEITERATIONS; i++)\n\t{\n\t\tAddNetMsg_PlayerLeaving();"), "a goodbye said as many times as a game says it isn't refused");

	// A corpse's message is honest, and isn't counted as a refusal
	Check(Has(pldnet, "if (type == I_BehaviourNetCorpse)\n\t\t{\n\t\t\treturn;\n\t\t}\n\n\t\t// The kind is the sender's word"), "a message about a corpse that's gone is simply ignored");

	// How often a machine sends. A host and a joiner each set this, and left at nothing it would be every frame.
	Check(CountOf(pldnet, "netGameData.sendFrequency = ONE_FIXED / kSendsEachSecond;") == 2, "both a host and a joiner send a fixed number of times a second");
	Check(Has(pldnet, "static const int kSendsEachSecond = 60;"), "and that's 60");

	// Autoguns on the network: only the campaign mode, which plays as single player does, has them. A level's own stay there, run by the host, and in
	// any other network game they're taken away as it starts, and a player can't place one. So an autogun message from any other game is refused by name.
	std::string const agun = ReadSourceFile("3dc/avp/bh_agun.cpp");
	std::string const autogunHandler = Between(pldnet, "static void ProcessNetMsg_PlayerAutoGunState(NETMESSAGE_AGUNSTATE *messagePtr, NetID senderId)\n{", "static void ProcessNetMsg_MakeDecal(");

	Check(Has(Between(agun, "void AutoGunBehaveInit(", "sbPtr->shapeIndex = toolsData->shapenum;"), "if(!SameAsSinglePlayer())\n\t{\n\t\tRemoveBehaviourStrategy(sbPtr);"), "a level's own autoguns are taken away as a network game starts, unless it plays as single player");
	Check(Has(Between(agun, "void CastSentrygun(void) {", "CreateSentrygun(&position, 0);"), "if (AvP.Network!=I_No_Network) {\n\t\tNewOnScreenMessage(\"NO SENTRYGUNS IN MULTIPLAYER MODE\""), "and a player can't place one in any network game");
	Check(Has(Between(reading, "case (NetMT_PlayerAutoGunState):", "subMessagePtr += sizeof(NETMESSAGE_AGUNSTATE);"), "ProcessNetMsg_PlayerAutoGunState((NETMESSAGE_AGUNSTATE *)subMessagePtr, senderId);"), "an autogun message goes to its handler");
	Check(Has(autogunHandler, "if (!SameAsSinglePlayer() || objectId == GHOST_PLAYEROBJECTID") && Has(autogunHandler, "Net_NoteRefused(\"an autogun message named something that isn't an autogun\");"), "which refuses it by name outside the campaign mode, or when it names a player");

	// THE GUARD. A merge of main into the co-op branch once dropped all of the autogun's network code without git marking a conflict, so every part of it
	// is checked to be here: its sending, its handler and that handler's declaration, its ghost's flash and sound, and what the handler uses of them.
	std::string const ghosts = ReadSourceFile("3dc/avp/win95/pldghost.cpp");
	std::string const ghostsHeader = ReadSourceFile("3dc/avp/win95/pldghost.h");

	Check(Has(pldnet, "void AddNetMsg_PlayerAutoGunState(STRATEGYBLOCK *sbPtr)\n{"), "GUARD: the autogun's sending is there");
	Check(Has(pldnet, "static void ProcessNetMsg_PlayerAutoGunState(NETMESSAGE_AGUNSTATE *msgPtr, NetID senderId);") && !autogunHandler.empty(), "GUARD: and its handler, declared and written");
	Check(Has(ghosts, "void HandleGhostAutoGunMuzzleFlash(STRATEGYBLOCK *sbPtr, int firing)\n{") && Has(ghosts, "void HandleGhostAutoGunSound(STRATEGYBLOCK *sbPtr, int firing)\n{")
		&& Has(ghosts, "static void CalculatePosnForGhostAutoGunMuzzleFlash(STRATEGYBLOCK *sbPtr,VECTORCH *position, EULER *orientation)\n{"), "GUARD: and its ghost's flash and sound");
	Check(Has(ghostsHeader, "extern void HandleGhostAutoGunMuzzleFlash(STRATEGYBLOCK *sbPtr, int firing);") && Has(ghostsHeader, "extern void HandleGhostAutoGunSound(STRATEGYBLOCK *sbPtr, int firing);"), "GUARD: declared where the handler can see them");
	Check(Has(autogunHandler, "HandleGhostAutoGunMuzzleFlash(sbPtr, (int)messagePtr->IAmFiring);") && Has(autogunHandler, "HandleGhostAutoGunSound(sbPtr, (int)messagePtr->IAmFiring);"), "GUARD: and the handler uses them");
	Check(Has(pldnet, "#include \"bh_agun.h\"") && Has(ReadSourceFile("3dc/avp/win95/pldnet.h"), "extern void AddNetMsg_PlayerAutoGunState(STRATEGYBLOCK *sbPtr);"), "GUARD: and what they need: the autogun's own header, and its sending declared for the rest of the game");

	// Who may say what. These are the ones whoever hears them just believes.
	std::vector<std::string> const onlyTheHost = { "NetMT_CoopGo", "NetMT_CoopNextLevel", "NetMT_LevelEvent", "NetMT_PlayerAutoGunState", "NetMT_AlienAIState", "NetMT_AlienAISequenceChange", "NetMT_AlienAIKilled", "NetMT_SpotAlienSound", "NetMT_FarAlienPosition", "NetMT_GameDescription", "NetMT_StartGame", "NetMT_AllGameScores", "NetMT_PlayerScores", "NetMT_InanimateObjectDestroyed", "NetMT_PlatformLiftState", "NetMT_RestartNetworkGame", "NetMT_FragmentalObjectsStatus", "NetMT_LastManStanding_Restart", "NetMT_LastManStanding_RestartInfo", "NetMT_LastManStanding_RestartCountDown", "NetMT_LastManStanding_LastMan", "NetMT_PredatorTag_NewPredator", "NetMT_EndGame", "NetMT_RespawnPickups", "NetMT_ScoreChange", "NetMT_StrategySynch", "NetMT_SpeciesScores",
		"NetMT_NpcState", "NetMT_NpcXenoDeltas", "NetMT_NpcKilled", "NetMT_NpcSound", "NetMT_NpcThrownObject", "NetMT_LiftState", "NetMT_CoopCampaignChoice" };
	int rightOnes = 0;

	for (std::string const &name : onlyTheHost)
	{
		std::string::size_type const at = table.find("{ " + name + ", ");

		rightOnes += (at != std::string::npos && Has(table.substr(at, table.find('\n', at) - at), ", true }")) ? 1 : 0;
	}

	Check(rightOnes == 34, "34 kinds can only come from the host");
	Check(CountOf(table, ", true }") == 34, "and no others");

	// Numbers in a message that are used as places in memory. Whichever machine sent them, they could be anything.
	Check(Has(Between(pldnet, "static void ProcessNetMsg_PlayerScores(NETMESSAGE_PLAYERSCORES *messagePtr)\n{", "static void ProcessNetMsg_ScoreChange("), "if (!NetNumberFitsTable(playerId, NET_MAXPLAYERS))\n\t{\n\t\treturn;"), "a player's scores have to name a place in the list");
	Check(Has(Between(pldnet, "static void ProcessNetMsg_ScoreChange(NETMESSAGE_SCORECHANGE *messagePtr)\n{", "static void ProcessNetMsg_SpeciesScores("), "if (!NetNumberFitsTable(messagePtr->victimIndex, NET_MAXPLAYERS) || !NetNumberFitsTable(messagePtr->killerIndex, NET_MAXPLAYERS + 1))\n\t{\n\t\treturn;"), "and so does a change in them, with one past the end for a killer that's an alien the computer plays");
	Check(Has(pldnet, "if (killerIndex != NET_IDNOTINPLAYERLIST && NetNumberFitsTable(messagePtr->AlienType, 3)) {"), "a dead alien has to be a kind there is");
	// A player's class number comes over the network too, and an unknown one is taken as the general class, as the character type is (NET-SUBTYPE-1)
	Check(Has(pldnet, "netGameData.playerData[i].characterSubType = NetNumberFitsTable(messagePtr->players[i].characterSubType, NUM_PC_SUBTYPES) ? (NETGAME_SPECIALISTCHARACTERTYPE)messagePtr->players[i].characterSubType : NGSCT_General;"), "a player's class in the game's description has to be one there is, or it's the general one");
	Check(Has(pldnet, "netGameData.playerData[id].characterSubType = NetNumberFitsTable(messagePtr->characterSubType, NUM_PC_SUBTYPES) ? (NETGAME_SPECIALISTCHARACTERTYPE)messagePtr->characterSubType : NGSCT_General;"), "a player's class in a player's description has to be one there is, or it's the general one");
	Check(Has(pldnet, "netGameData.playerData[playerIndex].characterSubType = NetNumberFitsTable(messagePtr->characterSubType, NUM_PC_SUBTYPES) ? static_cast<NETGAME_SPECIALISTCHARACTERTYPE>(messagePtr->characterSubType) : NGSCT_General;"), "a player's class when they start again has to be one there is, or it's the general one");
	Check(Has(header, "static_assert(NUM_PC_SUBTYPES == NGSCT_M79 + 1,"), "and how many classes there are is checked against the list when the game is built");
	Check(Has(pldnet, "if (myIndex != NET_IDNOTINPLAYERLIST && netGameData.playerData[myIndex].startFlag) {"), "and the end of a game doesn't look before the start of the list when this client isn't in it");
	Check(Has(pldnet, "if (!NetNumberFitsTable((int)messagePtr->DecalID, MAX_NO_OF_DECAL_IDS)) {\n\t\treturn;"), "a decal has to be one there is");
	Check(Has(pldnet, "if (messagePtr->ModuleIndex != -1 && !NetNumberFitsTable(messagePtr->ModuleIndex, ModuleArraySize)) {\n\t\tNet_NoteRefused(\"a decal message named a module this level doesn't have\");\n\t\treturn;"), "and on one of the level's modules, or it's refused with that reason");
	Check(Has(pldnet, "messagePtr->ModuleIndex != -1 && "), "but one on no module, which the game that made it gives as -1, is let in");
	Check(Has(pldnet, "if (!FALLP_AuxLocs || !NetNumberFitsTable((int)messagePtr->targetModuleIndex, ModuleArraySize)) {\n\t\t\tNet_NoteRefused(\"a far alien message named a module with no far places\");\n\t\t\treturn;"), "an alien's far place is checked against the list of far places, which has one entry for each module, not each AI module");
	{
		std::string const frustum = ReadSourceFile("3dc/frustum.cpp");
		Check(Has(frustum, "int QuadWithinFrustum(void)") && !Has(frustum, "int DecalWithinFrustum("), "the unused decal cull, which read a decal's module number as a place in a list, is gone");
	}
	Check(Has(pldnet, "(messagePtr->IOType == IOT_Ammo && !NetNumberFitsTable(messagePtr->subtype, MAX_NO_OF_AMMO_TEMPLATES))"), "an object that can be picked up as ammunition has to be ammunition there is");
	Check(Has(pldnet, "(messagePtr->IOType == IOT_Weapon && !NetNumberFitsTable(messagePtr->subtype, MAX_NO_OF_WEAPON_TEMPLATES))"), "and as a weapon, a weapon there is");
	Check(Has(pldnet, "if (!NetNumberFitsTable(messagePtr->type, MAX_NO_OF_WEAPON_TEMPLATES)) {\n\t\treturn;"), "and so does a weapon left behind");
	Check(Has(pldnet, "if (!NetNumberFitsTable((int)messagePtr->myType, NGCT_AI_Alien) || !NetNumberFitsTable((int)messagePtr->killerType, NGCT_AI_Praetorian + 1)) {\n\t\treturn;"), "a kill has to say a kind of player who died and a kind of killer that exist, because they're places in the score tables");
	Check(Has(pldnet, "if (!NetNumberFitsTable(messagePtr->characterType, NGCT_AI_Alien))\n\t\t{\n\t\t\treturn;"), "a player describing themselves has to be a kind there is");
	Check(Has(pldnet, "if (!NetNumberFitsTable(messagePtr->characterType, NGCT_AI_Alien) || !NetNumberFitsTable(messagePtr->nextCharacterType, NGCT_AI_Alien))\n\t{\n\t\treturn;"), "and so does one saying where they are, now and next");
	Check(Has(pldnet, "NetNumberFitsTable(messagePtr->players[i].characterType, NGCT_AI_Alien) ? (NETGAME_CHARACTERTYPE)messagePtr->players[i].characterType : NGCT_Marine;"), "and in the game's description, one that isn't becomes a marine");
	Check(Has(ReadSourceFile("3dc/avp/bh_ais.cpp"), "if (!retval) {\n\t\tretval=FirstDeath;\n\t}\n\n\treturn(retval);"), "a death code nobody knows still gives a death, because everyone who asks reads what they get");
	Check(Has(pldnet, "static uint8_t msg[kMostMessageBytes];"), "the buffer a message is read into has room for the most a message can be");
	Check(CountOf(pldnet, "NetNumberFitsTable(") >= 3 && Has(pldnet, "!messageProfile && messageHeader->ammo_id != AMMO_FLECHETTE_POSTMAX && !NetNumberFitsTable(messageHeader->ammo_id, MAX_NO_OF_AMMO_TEMPLATES)"), "a damage message naming ammunition that isn't in the table is ignored");
	Check(CountOf(pldnet, "if (!NetNumberFitsTable(damage.Id, MAX_NO_OF_AMMO_TEMPLATES))") == 2, "in the other two damage messages as well");
	Check(Has(pldnet, "NetCheckParts(message + kMessageHeaderSize, size - kMessageHeaderSize, kNetPartRules, sizeof(kNetPartRules) / sizeof(kNetPartRules[0]), fromTheHost, LengthOfANetPart))\n\t{\n\t\tcase kNetPartsGood:\n\t\t\treturn Net_CoopLobbyTakesThis(message, size);"), "the whole message is walked, from after its header");
	Check(Has(pldnet, "if (size < kMessageHeaderSize)\n\t{\n\t\tNet_NoteRefused("), "and one too short to have a header is refused");
	Check(Has(pldnet, "return NetChatPartLength(part, bytesLeft);"), "a chat line has to end inside the message");
	Check(Has(reading, "// Net_GameMessageIsGood has refused a message with a part it doesn't know, so this can't happen. If it ever did, nothing after it could be read.\n\t\t\t\treturn;"), "and a part nobody knows stops the reading");
	Check(Has(ReadSourceFile("3dc/avp/game_commands.cpp"), "if (AvP.Network != I_Host) return;\n\n\tint seed=FastRandom();\n\tAddNetMsg_RestartNetworkGame(seed);"), "only a host can restart a network game, so a client can't be told something it can't do");
}

static void TestTheMessageCheck()
{
	// A host: nothing from a client is the host's word
	HostWithClients(2);

	NetID const one = IDOf(1);
	std::vector<uint8_t> const message = AGameMessage(one);

	sentMessages.clear();
	lastAskedFromTheHost = true;
	Check(Hears(1, message) == message.size() && !lastAskedFromTheHost, "a host asks about a client's message as not being the host's word");
	Check(lastAskedSize == message.size(), "with the whole message");
	Check(sentMessages.size() == 1, "and a good one goes on to the others");

	gameMessagesAreGood = false;
	sentMessages.clear();
	Check(Hears(1, message) == 0, "a bad one doesn't go to the game");
	Check(sentMessages.empty(), "and isn't passed on either, because everyone else would have to believe it");
	gameMessagesAreGood = true;
	EverythingArrives();
	EndTheGame();

	// A client: the host is whoever gave it its ID
	Check(JoinAGame() == NET_OK, "someone joins");

	NetID const given = 4242;
	NetID const theHost = 77;
	NetID const someoneElse = 78;

	lastAskedFromTheHost = true;
	Hears(0, AGameMessage(theHost));
	Check(!lastAskedFromTheHost, "before it has been given an ID, it knows nobody's the host");

	Hears(0, AMessage(1, NET_SETPLAYERID, theHost, &given, sizeof(given)));

	lastAskedFromTheHost = false;
	Hears(0, AGameMessage(theHost));
	Check(lastAskedFromTheHost, "after that the host is whoever gave it the ID");

	lastAskedFromTheHost = true;
	Hears(0, AGameMessage(someoneElse));
	Check(!lastAskedFromTheHost, "and anyone else isn't");

	gameMessagesAreGood = false;
	Check(Hears(0, AGameMessage(theHost)) == 0, "a client doesn't read a bad message from its host either");
	gameMessagesAreGood = true;

	// The next game starts again
	EndTheGame();
	Check(JoinAGame() == NET_OK, "someone joins another game");
	lastAskedFromTheHost = true;
	Hears(0, AGameMessage(theHost));
	Check(!lastAskedFromTheHost, "and the last game's host means nothing in this one");
	EndTheGame();
}

static void TestTheGameUsesThis()
{
	std::string const menus = ReadSourceFile("3dc/avp/win95/frontend/menus.cpp");
	std::string const networking = ReadSourceFile("3dc/networking.cpp");
	std::string const pldnet = ReadSourceFile("3dc/avp/win95/pldnet.cpp");
	std::string const starting = Between(networking, "NetResult Net_Initialise()", "void Net_Deinitialise()");

	Check(!Has(menus, "Net_StopSearchingForSessions") && !Has(networking, "Net_UpdateSessionList") && !Has(networking, "broadcastSocket"),
		"there's no search on this network to stop: the page of games found reads the servers' lists alone (2026-11-11)");
	Check(!Has(starting, "enet_socket_create") && !Has(starting, "enet_socket_bind"), "and starting the game can't make a socket, even if a test above would miss it");
	Check(!Has(networking, "assert(ServerPeer)"), "the function checks for itself, rather than with an assert");
	Check(!Has(networking, "enet_host_create("), "every ENet host the game makes has the game's limits, because they're all made in net_host.cpp");

	// An empty message comes with no bytes, and copying from nothing isn't allowed in C++, so the size is checked first
	size_t const sizeChecked = networking.find("if (eEvent.packet->dataLength < kMessageHeaderSize)");
	size_t const copied = networking.find("memcpy(messageData, eEvent.packet->data, eEvent.packet->dataLength);");
	Check(sizeChecked != std::string::npos && copied != std::string::npos && sizeChecked < copied, "a message too short to have a header is refused before it's copied");
	// Each reader checks for itself that a message is long enough to have a header before it copies the header out (ReadTheHeader), so
	// either is safe whoever hands it a message. The check above refuses a message that short before either reader sees it, so nothing
	// sent from outside reaches this one today, and a test that sends a short message can't tell whether it's there. So it's read here.
	Check(Has(networking, "static bool ReadTheHeader(const uint8_t *data, size_t size, MessageHeader &header)\n{\n\tif (size < sizeof(MessageHeader))\n\t{\n\t\tLogDroppedMessage(\"it's too short to have a header\");\n\t\treturn false;\n\t}\n\n\tmemcpy(&header, data, sizeof(MessageHeader));"), "and each reader checks again before it copies the header, whoever hands it the message");

	// Everything about who's who is tested above, in the real file, and these are where the rest of the game joins on
	Check(!Has(pldnet, "ProcessSystemMessage") && !Has(pldnet, "isSystemMessage"), "the rest of the game has no way to act on a system message, so only what's been checked reaches it");
	Check(CountOf(pldnet, "ProcessGameMessage(fromID, msg, msgSize);") == 2, "and both places that collect messages hand on game messages only");
	Check(CountOf(pldnet, "if (msgSize >= sizeof(MessageHeader))") == 2, "and only if there's a message: what was only for the host comes out with no size");

	std::string const adding = Between(pldnet, "bool AddPlayerToGame(NetID id, const char *name)", "// A client's connection has closed");

	Check(Has(adding, "if (freePlayerIndex == NET_NOEMPTYSLOTINPLAYERLIST) {\n\t\treturn false;"), "the game says when it has no room");
	Check(Has(adding, "Inform_PlayerHasConnected(id);\n\treturn true;"), "and when it has taken someone");
	Check(Has(pldnet, "void Net_PlayerHasGone(NetID id)\n{\n\tif (PlayerIdInPlayerList(id) != NET_IDNOTINPLAYERLIST)\n\t{\n\t\tProcessNetMsg_PlayerLeaving(id);"), "a player whose connection closes leaves the way one who says so does, but not twice");

	std::string const hostGone = Between(pldnet, "void Net_HostHasGone(void)", "\n}\n");

	Check(Has(hostGone, "netGameData.myGameState = NGS_Error_HostLost;"), "a client with no host has no game");
	Check(Has(hostGone, "if (!inTheMenus)\n\t{\n\t\tAvP.MainLoopRunning = 0;"), "and a level being played ends");
	Check(Has(menus, "if (netGameData.myGameState == NGS_Error_HostLost)\n\t\t{\n\t\t\t// The host has gone, so there's no game to join. Back to the main menus.\n\t\t\tNet_Disconnect();\n\t\t\tAvP.Network = I_No_Network;"), "and in the menus it goes back to the start");

	Check(Has(pldnet, "NetCopyPlayerName(messagePtr->players[i].name, netGameData.playerData[i].name, NET_PLAYERNAMELENGTH);"), "the host's description of the game has each player's name");
	Check(Has(pldnet, "NetCopyPlayerName(netGameData.playerData[i].name, messagePtr->players[i].name, NET_PLAYERNAMELENGTH);"), "and that's where a client gets it, with an end");
	Check(Has(ReadSourceFile("3dc/avp/win95/pldnet.h"), "unsigned char startFlag;\n\tchar name[NET_PLAYERNAMELENGTH];\n} GAMEDESCRIPTION_PLAYERDATA;"), "there's room for it in the message");
	// (116 since #175 PR 3a, 00ddb399, which left this line at 114: modified 2026-11-10 by Melanikus for the AvP port)
	Check(Has(ReadSourceFile("3dc/networking.h"), "const int kMultiplayerVersion = 116;"), "and an older game can tell this one isn't what it speaks");

	// The log lines a tester's log needs from pldnet.cpp. A message left out for want of room says so from every place it can be left out.
	Check(Has(pldnet, "if (numBytesReqd > numBytesLeft)\n\t\t{\n\t\t\tNoteSendBufferFull(numBytesReqd);") && !Has(pldnet, "if (numBytesReqd > numBytesLeft)\n\t\t{\n\t\t\tLOCALASSERT(1 == 0);"), "SOURCE: a message left out of a full frame is in the log, from every room check");
	Check(Has(pldnet, "if (headerSize + size > left)\n\t{\n\t\tNoteSendBufferFull(headerSize + size);"), "SOURCE: and from the one the NPCs' messages use");
	Check(Has(pldnet, "\t\tif (NetLevelCount_One(switchSyncsNotMatched))\n\t\t{\n\t\t\tLogString(\"net: the host's switches, doors and lifts can't be matched"), "SOURCE: the host's switches that can't be matched are in the log, once a level");
	Check(Has(pldnet, "void EndAVPNetGame(void)\n{\n\tSwitchSyncs_LevelIsOver();"), "SOURCE: and how many, when the game ends");
	Check(Has(pldnet, "\tnetGameData.myGameState = NGS_Joining;\n\tSwitchSyncs_LevelIsOver();"), "SOURCE: or the next level starts");
}

// Added 2026-11-09 by Melanikus for the AvP port: THE CHANNEL TO THE RELAY (protocol version 2's draft, step 7; AvP-docs/engine/
// v2-step7-design.md, agreed with Primary). Since #175 PR 3a, in every game:
// - a relay is used only once its keys have come from the list, which hands over only keys the server's servers.cfg line has proved
//   (modified 2026-11-10, #175), and until then it's sent nothing at all;
// - every version 1 message to the relay goes sealed, through the session, and only messages the session opened are read;
// - a plain version 1 message from the relay, a READY sent again among them, is dropped and never read, and so is a sealed one that
//   doesn't open;
// - what the relay sends is sorted by its first four bytes;
// - a channel that can't go on ends the room or the join, with the player told why.
// The session is this test's stand-in (net_start_test_stubs/net_relay_v2.h): the real one is net_v2a_test's.
static size_t SentToTheRelayFrom(size_t first)
{
	size_t count = 0;

	for (size_t i = first; i < datagramsSentTo.size(); i++)
	{
		count += datagramsSentTo[i].host == kRelayHost ? 1 : 0;
	}

	return count;
}

// The channel as every test but this one has it: it opens by itself (net_start_test_stubs/net_relay_v2.h)
static void ResetTheStandInChannel()
{
	netRelayV2StandIn = NetRelayV2StandIn();
	netRelayV2StandIn.goReady = true;
	netRelayV2StandIn.pickerMs = 41;
	relayKeysGive = kRelayKeysCame;
}

static NetRelayMessage TheRoomOpened()
{
	NetRelayMessage opened = NetRelay_Message(kNetRelayHostOpened);
	memset(opened.secret, 0x5A, sizeof(opened.secret));
	memcpy(opened.code, "ABCD2345", 8);
	memset(opened.pass, 0x33, sizeof(opened.pass));
	opened.passTime = 1700000000u;
	return opened;
}

static bool LogMentions(const char *words);   // further down

// Added 2026-11-09 by Melanikus for the AvP port (Secondary's review of step 7): a join through the relay, its channel
// opening first, as the joiner's checks in TestTheRelayChannel have it, with the relay's answers waiting before it starts. Gives back
// what Net_ConnectToSession gave.
static NetResult JoinThroughTheChannel()
{
	ResetTheStandInChannel();
	datagramsWaiting.clear();
	NumberOfSessionsFound = 0;
	Net_AddRelaySession(AGameOnTheList("ABCD2345"), 0);
	NetRelayMessage given = NetRelay_Message(kNetRelayJoinPort);
	memset(given.secret, 0x6B, sizeof(given.secret));
	memcpy(given.code, "ABCD2345", 8);
	given.port = 27501;
	NetRelayMessage ready = NetRelay_Message(kNetRelayReady);
	memset(ready.secret, 0x6B, sizeof(ready.secret));
	ready.port = 27501;
	ArrivesFromTheRelay(given, kNetRelayDefaultControlPort);
	ArrivesFromTheRelay(ready, 27501);
	AvP.Network = I_Peer;
	return Net_ConnectToSession(0, playerName);
}

// Whether a log line holding words was written from logLines[from] on (added 2026-11-09 by Melanikus for the AvP port), so a check sees its
// own step's lines and not an earlier test's
static bool LoggedSince(size_t from, const char *words)
{
	for (size_t i = from; i < logLines.size(); i++)
	{
		if (logLines[i].find(words) != std::string::npos)
		{
			return true;
		}
	}

	return false;
}

// The number a game through a relay writes in the log as it ends, of the packets it dropped as not the game's (Net_Disconnect), looked for
// from logLines[from] on. 0 when there's no such line, as none is written for 0.
static int DroppedFromElsewhereLogged(size_t from)
{
	std::string const before = "net: while going through a relay, ";
	std::string const after = " packet(s) from elsewhere were dropped";

	for (size_t i = from; i < logLines.size(); i++)
	{
		std::string const &line = logLines[i];
		size_t const at = line.find(before);

		if (at == std::string::npos || line.find(after) == std::string::npos)
		{
			continue;
		}

		int count = 0;

		for (size_t c = at + before.size(); c < line.size() && isdigit(static_cast<unsigned char>(line[c])); c++)
		{
			count = count * 10 + (line[c] - '0');
		}

		return count;
	}

	return 0;
}

static void TestTheRelayChannel()
{
	printf("The channel to the relay (step 7)\n");
	NetRelayHostState news;
	NetRelayPlayState play;
	relayNamed = true;

	// Keys before anything. Modified 2026-11-10 by Melanikus for the AvP port (#175): the keys come proved by the server's servers.cfg line
	// (relay_list.h), so there's no RelayKey to hold them to here: the host asks for them, and sends nothing until they've come.
	int const asksAtFirst = relayKeysAsked;
	size_t sentFirst = datagramsSentTo.size();
	int channelFirst = channelSends;

	// Keys that haven't come, then keys the list won't give
	ResetTheStandInChannel();
	relayKeysGive = kRelayKeysWaiting;
	sentFirst = datagramsSentTo.size();
	channelFirst = channelSends;
	Check(HostAGame() == NET_OK && relayKeysAsked == asksAtFirst + 1, "a game hosts, and asks the list for its relay's keys, which are still to come");
	Net_ServiceNetwork();
	enetClock += 1000;
	Net_ServiceNetwork();
	Check(SentToTheRelayFrom(sentFirst) == 0 && channelSends == channelFirst && netRelayV2StandIn.pickerStarts == 0, "while the keys haven't come, nothing is sent to the relay");
	relayKeysGive = kRelayKeysRefused;
	int const socketsBeforeTheKeys = socketsOpen;
	Net_ServiceNetwork();
	Net_RelayHostStateNow(&news);
	// Modified 2026-11-10 by Melanikus for the AvP port (#188): sealed or nothing, so the online game ends, and never opens on this network
	Check(news.what == kNetRelayHostEnded && news.why != NULL && SentToTheRelayFrom(sentFirst) == 0 && socketsOpen == socketsBeforeTheKeys,
		"and keys the list won't give: nothing is sent, and the online game ends, saying why, with no search port opened on this network instead");
	EndTheGame();

	// The right keys: the cookie round, then handshake A with this run's identity, then the room through the sealed channel
	ResetTheStandInChannel();
	netRelayV2StandIn.goReady = false;
	sentFirst = datagramsSentTo.size();
	channelFirst = channelSends;
	int const sealedFirst = sealedSends;
	int const roomsFirst = roomsListed;
	int const unlistedFirst = roomsUnlisted;
	Check(HostAGame() == NET_OK, "a game hosts, with its relay's right keys to come");
	Net_ServiceNetwork();
	Check(netRelayV2StandIn.pickerStarts == 1 && netRelayV2StandIn.pickerPort == kNetRelayDefaultControlPort && netRelayV2StandIn.starts == 1
		&& netRelayV2StandIn.region == "virginia" && memcmp(&netRelayV2StandIn.keys, &testRelayKeys, sizeof(testRelayKeys)) == 0
		&& netRelayV2StandIn.controlPort == kNetRelayDefaultControlPort && netRelayV2StandIn.drawsGiven,
		"keys that match: the relay is timed with the cookie round, then handshake A starts with those keys, at its control port, in its region");
	Check(netRelayV2StandIn.playerP384 != NULL && netRelayV2StandIn.playerStatics != NULL && netRelayV2StandIn.playerKemPrivate != NULL,
		"and with this run's identity, which the session borrows: its P-384 key, its public halves and its ML-KEM private key");
	Check(channelSends == channelFirst + 2 && SentToTheRelayFrom(sentFirst) == 0 && sealedSends == sealedFirst,
		"what has gone to the relay is the cookie ask and the handshake's first datagram: no version 1 message yet, plain or sealed");

	netRelayV2StandIn.goReady = true;
	enetClock += 10;
	Net_ServiceNetwork();
	Check(!netRelayV2StandIn.sealed.empty() && netRelayV2StandIn.sealed[0][5] == kNetRelayHostOpen && netRelayV2StandIn.sealedTo[0] == kNetRelayDefaultControlPort,
		"once the channel is open, the room is asked for through it: HOST_OPEN, sealed, to the relay's control port");
	Check(sealedSends > sealedFirst && SentToTheRelayFrom(sentFirst) == static_cast<size_t>(sealedSends - sealedFirst),
		"and nothing goes to the relay plain: every version 1 message to it was sealed");

	// The relay's answer, HOST_OPENED: plain, it's dropped and never read; sealed and not opening, the same; sealed, it opens the room
	uint8_t plain[kNetRelayMessageBytes];
	NetRelay_Write(TheRoomOpened(), plain);
	TellTheHostPlain(kRelayHost, kNetRelayDefaultControlPort, plain, sizeof(plain));
	Check(theHost.intercept(&theHost, NULL) == 1, "a plain version 1 message from the relay is taken off ENet's hands");
	Net_ServiceNetwork();
	Net_RelayPlayStateNow(&play);
	Check(!play.throughRelay && roomsListed == roomsFirst, "a plain version 1 message from the relay is dropped and never read: a plain HOST_OPENED opens no room");
	uint8_t notOpening[kNetRelayV2StandInBytes];
	memcpy(notOpening, "AVPR", 4);
	notOpening[4] = 'X';
	memcpy(notOpening + 5, plain, sizeof(plain));
	TellTheHostPlain(kRelayHost, kNetRelayDefaultControlPort, notOpening, sizeof(notOpening));
	Check(theHost.intercept(&theHost, NULL) == 1, "a sealed message that doesn't open is taken off ENet's hands too");
	Net_ServiceNetwork();
	Net_RelayPlayStateNow(&play);
	Check(!play.throughRelay && roomsListed == roomsFirst, "and is dropped: a HOST_OPENED that doesn't open opens no room");
	TellTheHost(kRelayHost, kNetRelayDefaultControlPort, plain, sizeof(plain));
	Check(theHost.intercept(&theHost, NULL) == 1 && netRelayV2StandIn.heardFrom.back() == kNetRelayDefaultControlPort, "the same HOST_OPENED, sealed, goes to the channel");
	Net_ServiceNetwork();
	Net_RelayPlayStateNow(&play);
	Check(play.throughRelay && roomsListed == roomsFirst + 1 && hostMsListed == 41,
		"which opens it, and the room opens and goes on the list, at the cookie round's round trip");

	// Sorting by the first four bytes
	size_t const heardBefore = netRelayV2StandIn.heardFrom.size();
	uint8_t junk[40];
	memset(junk, 0x47, sizeof(junk));
	TellTheHostPlain(kRelayHost, 27777, junk, sizeof(junk));
	Check(theHost.intercept(&theHost, NULL) == 1 && netRelayV2StandIn.heardFrom.size() == heardBefore,
		"anything from the relay that's neither the channel's ('AVPR') nor the end to end's ('AVPE', 'AVPG') is dropped");
	// Added 2026-11-09 by Melanikus for the AvP port: the same from a joiner's port, as a game packet plain would come (version 1's
	// TestHostingThroughTheRelay had ENet read it): it never reaches ENet
	uint8_t plainGame[40];
	memset(plainGame, 0x11, sizeof(plainGame));
	TellTheHostPlain(kRelayHost, 27501, plainGame, sizeof(plainGame));
	Check(theHost.intercept(&theHost, NULL) == 1 && netRelayV2StandIn.heardFrom.size() == heardBefore,
		"a plain packet from a joiner's port on the relay is dropped before ENet reads it: only a sealed one that opens is the game's");
	TellTheHostPlain(0x04030201u, kNetRelayDefaultControlPort, sealedForTheHost, sizeof(sealedForTheHost));
	Check(theHost.intercept(&theHost, NULL) == 1 && netRelayV2StandIn.heardFrom.size() == heardBefore, "and from anywhere but the relay, even an 'AVPR' is never heard");

	// Added 2026-11-09 by Melanikus for the AvP port (Secondary's finding 4.1): a player in the game who came through the relay, and a
	// connection from somewhere else. While hosting through a relay nothing else is heard, so the second can't really be there, but what
	// ends with the relay must be only the relay's.
	ENetPeer &throughTheRelay = peers[3];
	throughTheRelay.state = ENET_PEER_STATE_CONNECTED;
	throughTheRelay.address.host = kRelayHost;
	throughTheRelay.address.port = 27501;
	throughTheRelay.data = reinterpret_cast<void *>(static_cast<uintptr_t>(4242));
	// Each is a player (data is their id): a connection with none is closed by the game's own sweep once its time to say who it is has
	// gone (ForgetConnectionsNotLetIn), which this test's clock has long passed, and that would hide what's tried here
	ENetPeer &fromElsewhere = peers[5];
	fromElsewhere.state = ENET_PEER_STATE_CONNECTED;
	fromElsewhere.address.host = 0x0200007F;
	fromElsewhere.address.port = 27502;
	fromElsewhere.data = reinterpret_cast<void *>(static_cast<uintptr_t>(4343));
	FakePlayer hicks;
	hicks.id = 4242;
	hicks.name = "Hicks";
	playersInGame.push_back(hicks);
	netSession.nPlayers++;
	int const playersThen = netSession.nPlayers;
	int const resetThen = peersReset;
	size_t const goneThen = playersGone.size();

	// The channel ending ends the room, with its words
	int const wipesBefore = netRelayV2StandIn.wipes;
	int const socketsWhileOpen = socketsOpen;
	size_t const giveUpLogFrom = logLines.size();
	netRelayV2StandIn.failWith = kNetRelayV2NoAnswer;
	enetClock += 10;
	Net_ServiceNetwork();
	Net_RelayHostStateNow(&news);
	Net_RelayPlayStateNow(&play);
	Check(news.what == kNetRelayHostEnded && news.why != NULL && strstr(news.why, "ask for access on the online page") != NULL && !play.throughRelay
		&& roomsUnlisted == unlistedFirst + 1 && netRelayV2StandIn.wipes > wipesBefore,
		"the channel ending ends the online game: it comes off the list, the news says why in the game's own words (pointing at the access request), and the session's keys are wiped");
	// Added 2026-11-10 by Melanikus for the AvP port (#188): the game has ended, so nothing reaches ENet any more, the relay included, and no
	// search port opens on this network instead
	TellTheHostPlain(kRelayHost, 27501, plainGame, sizeof(plainGame));
	int const takenAfter = theHost.intercept != NULL ? theHost.intercept(&theHost, NULL) : 0;
	TellTheHostPlain(0x0200007F, 27502, plainGame, sizeof(plainGame));
	int const takenAtHome = theHost.intercept != NULL ? theHost.intercept(&theHost, NULL) : 0;
	Check(takenAfter == 1 && takenAtHome == 1 && socketsOpen == socketsWhileOpen,
		"and the game has ended: nothing reaches ENet any more, from the relay or from this network, and no search port opens (sealed or nothing)");
	Check(throughTheRelay.state == ENET_PEER_STATE_DISCONNECTED && throughTheRelay.data == NULL && peersReset == resetThen + 1,
		"and every connection through the relay ends with it, at once: nothing from the relay's address reaches ENet unsealed after it");
	Check(playersGone.size() == goneThen + 1 && playersGone.back() == 4242 && netSession.nPlayers == playersThen - 1,
		"the player who came through it has left the game, as when a connection closes, and their seat is free");
	Check(fromElsewhere.data == reinterpret_cast<void *>(static_cast<uintptr_t>(4343)),
		"a connection from anywhere else isn't touched: it keeps its player, and the counts above are the relay's one alone");
	fromElsewhere.state = ENET_PEER_STATE_DISCONNECTED;
	fromElsewhere.data = NULL;
	// Added 2026-11-09 by Melanikus for the AvP port: a host that loses its relay (as a revoke ends a room: the relay just stops answering)
	// says what went sealed until then, once
	Check(LoggedSince(giveUpLogFrom, "net: through the relay, ") && LoggedSince(giveUpLogFrom, " datagram(s) went out sealed, and "),
		"a host that loses its relay says, as it gives it up, how many datagrams went out sealed and came in and opened: counts only");
	size_t const hostEndFrom = logLines.size();
	EndTheGame();
	Check(!LoggedSince(hostEndFrom, "net: through the relay, "), "and leaving after that doesn't say them twice");


	// The joiner: the channel opens first, then the join goes through it
	ResetTheStandInChannel();
	datagramsWaiting.clear();
	NumberOfSessionsFound = 0;
	Net_AddRelaySession(AGameOnTheList("ABCD2345"), 0);
	uint8_t ticket[kNetRelaySecretBytes];
	memset(ticket, 0x6B, sizeof(ticket));
	NetRelayMessage given = NetRelay_Message(kNetRelayJoinPort);
	memcpy(given.secret, ticket, sizeof(ticket));
	memcpy(given.code, "ABCD2345", 8);
	given.port = 27501;
	NetRelayMessage ready = NetRelay_Message(kNetRelayReady);
	memcpy(ready.secret, ticket, sizeof(ticket));
	ready.port = 27501;
	ArrivesFromTheRelay(given, kNetRelayDefaultControlPort);
	ArrivesFromTheRelay(ready, 27501);
	AvP.Network = I_Peer;
	size_t const joinLogFrom = logLines.size();
	Check(Net_ConnectToSession(0, playerName) == NET_OK, "a game joined through the relay");
	// Added 2026-11-09 by Melanikus for the AvP port: the joiner's log proves its own steps, in fixed words with no key, code or address
	Check(LoggedSince(joinLogFrom, "net: the joiner's sealed channel to the relay is open")
		&& LoggedSince(joinLogFrom, "net: handshake B with the host is done: from here the game is sealed end to end"),
		"the joiner's log says its sealed channel to the relay is open, and that handshake B with the host is done");
	Check(netRelayV2StandIn.starts == 1 && netRelayV2StandIn.pickerStarts == 0 && !netRelayV2StandIn.sealed.empty() && netRelayV2StandIn.sealed[0][5] == kNetRelayJoin
		&& netRelayV2StandIn.sealedTo[0] == kNetRelayDefaultControlPort,
		"the joiner opens the channel first (handshake A, with no timing: only a host needs that), then asks to join through it, sealed");
	uint8_t readyPlain[kNetRelayMessageBytes];
	NetRelay_Write(ready, readyPlain);
	// Modified 2026-11-09 by Melanikus for the AvP port: a plain one starts 'AVPR' as the channel's own datagrams do, so the channel hears it
	// too, and opens nothing of it (Secondary's read of step 7, section 2). What reaches the relay's machines is only what it opened.
	size_t const joinerHeard = netRelayV2StandIn.heardFrom.size();
	int const joinerOpened = netRelayV2StandIn.openedGiven;
	TellTheHostPlain(kRelayHost, 27501, readyPlain, sizeof(readyPlain));
	Check(theHost.intercept(&theHost, NULL) == 1 && netRelayV2StandIn.heardFrom.size() == joinerHeard + 1 && netRelayV2StandIn.openedGiven == joinerOpened,
		"a READY sent again plain is dropped and never read: the channel hears it, as it starts 'AVPR', and opens nothing of it");
	TellTheHost(kRelayHost, 27501, readyPlain, sizeof(readyPlain));
	Check(theHost.intercept(&theHost, NULL) == 1 && netRelayV2StandIn.heardFrom.size() == joinerHeard + 2 && netRelayV2StandIn.heardFrom.back() == 27501
		&& netRelayV2StandIn.openedGiven == joinerOpened + 1,
		"sealed, from the joiner's own port, the channel opens it and hands it on: it hears the relay on every port");
	size_t const joinEndFrom = logLines.size();
	EndTheGame();
	Check(LoggedSince(joinEndFrom, "net: through the relay, ") && LoggedSince(joinEndFrom, " datagram(s) went out sealed, and ")
		&& LoggedSince(joinEndFrom, " came in and opened"),
		"and as the game ends, its log says how many datagrams went out sealed through the relay and how many came in and opened: counts only");

	// A joiner whose relay's keys the list won't give (modified 2026-11-10 by Melanikus for the AvP port, #175: in place of a region with no
	// RelayKey, which is gone): no join, and nothing sent
	ResetTheStandInChannel();
	relayKeysGive = kRelayKeysRefused;
	datagramsWaiting.clear();
	NumberOfSessionsFound = 0;
	Net_AddRelaySession(AGameOnTheList("ABCD2345"), 0);
	AvP.Network = I_Peer;
	sentFirst = datagramsSentTo.size();
	channelFirst = channelSends;
	int const joinAsks = relayKeysAsked;
	Check(Net_ConnectToSession(0, playerName) == NET_FAIL && SentToTheRelayFrom(sentFirst) == 0 && channelSends == channelFirst && relayKeysAsked == joinAsks + 1
		&& strstr(Net_RelayJoinFailedWhy(), "keys") != NULL,
		"a join through a relay whose keys the list won't give: they're asked for, nothing is sent to the relay, and the player is told why");
	EndTheGame();

	// Added 2026-11-09 by Melanikus for the AvP port (Secondary's finding 2.1): ONCE A JOINER'S CHANNEL HAS ENDED, its game goes on over B's
	// sealed datagrams, and the relay is still heard sealed-only until that game ends. Nothing shows that from outside but a count: what's
	// dropped as not the game's goes in the log as the game ends. So the same game is played twice, once with nothing more from the relay
	// after its channel has ended, and once with three datagrams then: a plain READY and a sealed-looking one, which must both be dropped
	// (there's no channel to open the second), and one of B's ('AVPG'), which must still go to B. B opens nothing of it, so ENet gets
	// nothing, and it isn't counted as dropped.
	int droppedWithNothing = 0;
	int droppedWithThree = 0;

	for (int run = 0; run < 2; run++)
	{
		Check(JoinThroughTheChannel() == NET_OK, run == 0 ? "a game joined through the relay, whose channel will end" : "and the same game again");
		size_t const logFrom = logLines.size();
		netRelayV2StandIn.failWith = kNetRelayV2NoAnswer;
		enetClock += 10;
		Net_ServiceNetwork();
		Net_RelayPlayStateNow(&play);
		Check(LogMentions("net: the joiner's channel to the relay has ended") && play.throughRelay,
			"its channel to the relay ends, the log says so, and the game goes on through the relay");

		if (run == 1)
		{
			uint8_t lateReady[kNetRelayMessageBytes];
			NetRelay_Write(ready, lateReady);
			size_t const heardThen = netRelayV2StandIn.heardFrom.size();
			int const openedThen = netRelayV2StandIn.openedGiven;
			TellTheHostPlain(kRelayHost, 27501, lateReady, sizeof(lateReady));
			int const plainTaken = theHost.intercept(&theHost, NULL);
			TellTheHost(kRelayHost, 27501, lateReady, sizeof(lateReady));
			int const sealedTaken = theHost.intercept(&theHost, NULL);
			uint8_t gameDatagram[60];
			memset(gameDatagram, 0x21, sizeof(gameDatagram));
			memcpy(gameDatagram, "AVPG", 4);
			TellTheHostPlain(kRelayHost, 27501, gameDatagram, sizeof(gameDatagram));
			int const gameTaken = theHost.intercept(&theHost, NULL);
			Check(plainTaken == 1 && sealedTaken == 1 && gameTaken == 1 && netRelayV2StandIn.heardFrom.size() == heardThen && netRelayV2StandIn.openedGiven == openedThen,
				"after the channel has ended, nothing from the relay reaches ENet as it came, and the channel, which has gone, hears none of it");
		}

		EndTheGame();
		(run == 0 ? droppedWithNothing : droppedWithThree) = DroppedFromElsewhereLogged(logFrom);
	}

	Check(droppedWithThree == droppedWithNothing + 2,
		"the plain READY and the sealed-looking one are dropped, never read by the relay's machines, and B's datagram still goes to B: the game goes on over B");

	// Added 2026-11-09 by Melanikus for the AvP port (Secondary's finding 4.2): EVERY WAY A JOIN THROUGH THE RELAY FAILS FORGETS AND WIPES:
	// when ENet can't start the connection, and when the request to join can't be sent. The channel that was going ends, its keys wiped,
	// and the ENet host is let go of, so nothing goes on talking to the relay after the player has been told the join failed.
	const char *const failures[] = { "ENet can't start the connection", "the request to join can't be sent" };

	for (int failure = 0; failure < 2; failure++)
	{
		int const createdBefore = hostsCreated;
		int const destroyedBefore = hostsDestroyed;
		connectCantStart = failure == 0;
		packetCreateFails = failure == 1;
		NetResult const joined = JoinThroughTheChannel();
		connectCantStart = false;
		packetCreateFails = false;
		size_t const sentAfter = datagramsSentTo.size();
		int const channelAfter = channelSends;
		enetClock += 1000;
		Net_ServiceNetwork();
		Net_RelayPlayStateNow(&play);
		Check(joined == NET_FAIL && netRelayV2StandIn.liveWipes == 1 && hostsDestroyed - destroyedBefore == hostsCreated - createdBefore
			&& SentToTheRelayFrom(sentAfter) == 0 && channelSends == channelAfter && !play.throughRelay,
			std::string("a join through the relay that fails as ") + failures[failure] + ": the channel's keys are wiped, the host is let go of, and nothing more goes to the relay");
		EndTheGame();
	}

	// And in the code, every way out of a join that fails once it has begun with the relay, those two and the three before them: each forgets
	// and wipes, and lets the host go. Each is a block of its own, so what's looked at is from the brace that opens it to its return.
	std::string const joinCode = Between(ReadSourceFile("3dc/networking.cpp"), "NetResult Net_ConnectToSession(int sessionNumber, char *playerName)\n{", "\n}\n");
	size_t const throughFrom = joinCode.find("if (SessionData[sessionNumber].throughRelay)");
	int failurePaths = 0;
	int failuresThatForget = 0;

	for (size_t at = joinCode.find("return NET_FAIL;", throughFrom == std::string::npos ? joinCode.size() : throughFrom); at != std::string::npos;
		at = joinCode.find("return NET_FAIL;", at + 1))
	{
		size_t const opens = joinCode.rfind('{', at);
		std::string const block = opens == std::string::npos ? std::string() : joinCode.substr(opens, at - opens);
		failurePaths++;
		failuresThatForget += Has(block, "RelayEndToEndForget();") && Has(block, "enet_host_destroy(host);") && Has(block, "host = NULL;") ? 1 : 0;
	}

	Check(failurePaths == 5 && failuresThatForget == failurePaths,
		"SOURCE: each of a join's 5 ways to fail once it has begun with the relay forgets and wipes, and lets the host go");

	// A join that starts while the last one through the relay is still open (Net_ConnectToSession lets go of that one's host first): the
	// last one's channel ends then too, so nothing of it goes on with the new host (added 2026-11-09 by Melanikus for the AvP port)
	Check(JoinThroughTheChannel() == NET_OK, "a game joined through the relay, and not left");
	// Modified 2026-11-11 by Melanikus for the AvP port (no plain game): the second join is of a plain game, which is refused, and the
	// first one's channel ends all the same, before anything else
	SessionData[0].throughRelay = false;
	size_t const sentStale = datagramsSentTo.size();
	int const channelStale = channelSends;
	Check(Net_ConnectToSession(0, playerName) == NET_FAIL && netRelayV2StandIn.liveWipes == 1,
		"then a join of a plain game, without leaving the first: it's refused, and the first one's channel to the relay ends, its keys wiped");
	enetClock += 1000;
	Net_ServiceNetwork();
	Check(SentToTheRelayFrom(sentStale) == 0 && channelSends == channelStale, "and nothing more goes to the relay");
	EndTheGame();

	// What can't be seen from outside, read in the code: the relay's plain messages are never read anywhere the game reads
	// the relay (the intercept, a joiner's handshake with the relay, and B's join), and only what the channel opened reaches net_relay.h's
	// machines
	std::string const networking = ReadSourceFile("3dc/networking.cpp");
	// Modified 2026-11-09 by Melanikus for the AvP port (Secondary's finding 2.1): and after the channel has ended, until the game ends
	std::string const sorting = Between(networking, "if (channelOn || relayV2Used)", "return RelayEndToEndHeard(on, now);\n\t}");
	Check(!sorting.empty() && !Has(sorting, "RelayControlDatagram(") && Has(sorting, "if (channelOn)\n\t\t\t{\n\t\t\t\tRelayV2Hear(on->receivedAddress.port"),
		"SOURCE: with the channel in use, or once it has been, the intercept hands the relay's messages to the channel alone, never to the relay's machines plain");
	// Added 2026-11-10 by Melanikus for the AvP port (#175 PR 3a): and without the channel, nothing from a relay is the game's. Only Hudson's
	// socket on the way to its relay beyond this machine gets there, before the channel opens, which no stand-in here can hold still, so
	// it's read in the code.
	std::string const interceptEnd = Between(networking, "return RelayEndToEndHeard(on, now);\n\t}", "\n}\n");
	Check(Has(interceptEnd, "\n\trelayStrangersDropped++;\n\treturn 1;") && !Has(interceptEnd, "RelayControlDatagram(") && !Has(interceptEnd, "return 0;"),
		"SOURCE: without the channel, in use or used, the intercept drops whatever a relay sends: version 1's plain path is gone");
	std::string const beginning = Between(networking, "static bool RelayV2Begin(const ENetAddress &relay, const char *region, bool times, const char **why)\n{", "\n}\n");
	std::string const forgetting = Between(networking, "static void RelayEndToEndForget()\n{", "\n}\n");
	Check(Has(beginning, "relayV2Step = kRelayV2Keys;\n\trelayV2Used = true;\n\treturn true;") && Has(forgetting, "relayV2Used = false;"),
		"SOURCE: a game that has begun a channel is heard that way until its end forgets it");

	// Added 2026-11-09 by Melanikus for the AvP port (Secondary's findings 5.1 and 4.3): the copies of the relay's messages networking.cpp
	// makes on its stack, before sealing and once opened, are wiped, and so are version 1's two machines, with the messages still waiting
	// in them, when a game through the relay ends. None of that can be seen from outside, so it's read in the code.
	std::string const sendingAll = Between(networking, "static void RelayControlSendAll(ENetHost *on, End &end)\n{", "\n}\n");
	std::string const takingOpened = Between(networking, "static void RelayV2TakeOpened(uint32_t now)\n{", "\n}\n");
	Check(Has(sendingAll, "NetCrypto_Wipe(sends, sizeof(sends));\n\tRelayV2SendAll(on);") && Has(takingOpened, "\n\tNetCrypto_Wipe(opened, sizeof(opened));"),
		"SOURCE: the version 1 messages held on the stack, plain, are wiped once they've been sealed or read");

	// Added 2026-11-09 by Melanikus for the AvP port: what this test's stand-ins can't show (no scripted joiner finishes B with a host here, and
	// the stand-in ENet sends nothing through the hook), read in the code: the host's line for a joiner's B, and the two counts behind the
	// end-of-game line
	std::string const endToEndHearing = Between(networking, "static int RelayEndToEndHeard(ENetHost *on, uint32_t now)\n{", "\n}\n");
	std::string const endToEndSealing = Between(networking, "static int ENET_CALLBACK RelaySealedSend(ENetHost *from, const ENetAddress *address, const ENetBuffer *buffers, size_t bufferCount)\n{", "\n}\n");
	Check(Has(endToEndHearing, "bool const wasReady = side->Ready();")
		&& Has(endToEndHearing, "if (!wasReady && side->Ready())\n\t\t{\n\t\t\tLogString(\"net: a joiner's handshake B is done: from here its game is sealed end to end\");")
		&& Has(endToEndHearing, "relayEndToEndOpened++;\n\ton->receivedData = relayOpened;")
		&& Has(endToEndSealing, "int const sent = enet_socket_send(from->socket, address, &buffer, 1);\n\trelayEndToEndSealed += sent < 0 ? 0 : 1;"),
		"SOURCE: the host logs once when a joiner's handshake B is done, and every datagram sealed out or opened in is counted for the end-of-game line");
	std::string const relayMachines = ReadSourceFile("3dc/net_relay.h");
	Check(Has(forgetting, "relayHost.Reset();\n\trelayJoiner.Reset();")
		&& Has(relayMachines, "\t\tmemset(hellos, 0, sizeof(hellos));\n\t\tsendCount = 0;\n\t\tmemset(sends, 0, sizeof(sends));")
		&& Has(relayMachines, "\t\tstartedAt = askedAt = portAt = helloAt = 0;\n\t\tsendCount = 0;\n\t\tmemset(sends, 0, sizeof(sends));"),
		"SOURCE: a game through the relay that ends wipes the room's token and pass, a joiner's ticket, and the messages still waiting with them");
	std::string const handshake = Between(networking, "static uint16_t RelayHandshake(const char *region, const char *code)", "relayJoinStep = kNetRelayJoinStepNone;\n\n\tif (!relayJoiner.Ready())");
	Check(Has(handshake, "if (from.host == relayAddress.host && HasMagic(packet, static_cast<size_t>(got), \"AVPR\"))")
		&& !Has(handshake, "RelayControlDatagram(") && !Has(handshake, "else if (from.host == relayAddress.host"),
		"SOURCE: a joiner's handshake with the relay hears only the channel, and never reads the relay plain");
	std::string const bJoin = Between(networking, "static const char *RelayEndToEndJoin(", "\n}\n");
	Check(!bJoin.empty() && !Has(bJoin, "RelayControlDatagram(") && Has(bJoin, "if (HasMagic(packet, static_cast<size_t>(got), \"AVPR\"))"),
		"SOURCE: B's join hears the relay's own messages through the channel alone");
	std::string const hearing = Between(networking, "static void RelayV2Hear(uint16_t fromPort, const uint8_t *data, size_t size, uint32_t now)\n{", "\n}\n");
	Check(Has(hearing, "relayV2Session.Hear(fromPort, data, size, now);\n\t\tRelayV2TakeOpened(now);") && !Has(hearing, "RelayControlDatagram("),
		"SOURCE: only the messages the channel opened go on to the relay's machines");

	// As the tests after this expect
	ResetTheStandInChannel();
	relayNamed = true;   // as the tests after this expect (2026-11-11: a line's relay is named)
	NumberOfSessionsFound = 0;
	memset(SessionData, 0, sizeof(SessionData[0]) * MAX_NO_OF_SESSIONS);
	datagramsWaiting.clear();
}

// Added 2026-11-09 by Melanikus for the AvP port (#21): THIS RUN'S IDENTITY, KEPT BETWEEN RUNS (net_identity_run.h). The store's stand-in
// says what it found. On the first run, one is made and kept. On the next, the kept one is used. A kept one that can't be used is never
// replaced without the player's yes, and until then nothing goes to a relay. The player's yes is refused while a game is open.
static void RestartTheNetwork()
{
	EndTheGame();
	Net_Deinitialise();
	logLines.clear();
	Net_Initialise();
}

static size_t SentToTheRelaySince(size_t first)
{
	size_t count = 0;

	for (size_t i = first; i < datagramsSentTo.size(); i++)
	{
		count += datagramsSentTo[i].host == kRelayHost ? 1 : 0;
	}

	return count;
}

static void TestTheKeptIdentity()
{
	printf("This run's identity, kept between runs (#21)\n");

	// Its check that a kept identity that can't be used sends nothing to the relay relies on a host through the relay asking for its identity
	// first, as every one does since version 2 became every game's protocol (#175 PR 3a; it was the v2 draft's, Fifth, 2026-11-09).
	uint8_t statics[kNetIdentityStaticBytes];
	uint8_t fingerprint[kNetIdentityFingerprintBytes];
	uint8_t worked[kNetIdentityFingerprintBytes];

	// The first run: none is kept, so one is made and kept, once
	storeFinds = kNetIdentityStoreNoFile;
	int const loadsAtFirst = storeLoads;
	int const makesAtFirst = storeMakes;
	RestartTheNetwork();
	Check(storeLoads == loadsAtFirst && storeMakes == makesAtFirst, "starting the network doesn't look for the identity: only a relay's first use, or the page, does");
	Check(Net_KeptIdentityResult() == kNetIdentityStoreNoFile && storeLoads == loadsAtFirst + 1 && storeMakes == makesAtFirst + 1
		&& LogHas("net: a new identity was made for this player and kept"), "the first run: none is kept, so one is made and kept, and the log says so");
	memset(statics, 0xA5, sizeof(statics));
	memset(fingerprint, 0xA5, sizeof(fingerprint));
	Check(Net_OurIdentity(statics, fingerprint) && storeLoads == loadsAtFirst + 1 && storeMakes == makesAtFirst + 1,
		"the page gets this run's identity, without looking again");
	Check(NetIdentity_Fingerprint(statics, statics + kNetCryptoP384PublicBytes, worked) && memcmp(worked, fingerprint, sizeof(worked)) == 0,
		"and the fingerprint it gets is the one its public halves make");
	Check(!Net_OurIdentity(NULL, fingerprint) && !Net_OurIdentity(statics, NULL), "nowhere to put them is refused");

	// The next run: the kept one, and nothing new made
	RestartTheNetwork();
	Check(Net_KeptIdentityResult() == kNetIdentityStoreLoaded && storeMakes == makesAtFirst + 1 && LogHas("net: this player's identity is the one kept from earlier runs"),
		"the next run uses the kept one, and makes none");

	// A kept one that can't be used: none is made, and nothing goes to a relay, until the player's yes
	NetIdentityStoreResult const unusable[] = { kNetIdentityStoreRefused, kNetIdentityStoreCantUnseal, kNetIdentityStoreCantRead };

	for (NetIdentityStoreResult const found : unusable)
	{
		std::string const which = " (" + std::string(found == kNetIdentityStoreRefused ? "refused" : found == kNetIdentityStoreCantUnseal ? "not opened for this user" : "not read") + ")";
		storeFinds = found;
		RestartTheNetwork();
		int const loads = storeLoads;
		int const makes = storeMakes;
		memset(statics, 0xA5, sizeof(statics));
		memset(fingerprint, 0xA5, sizeof(fingerprint));
		Check(!Net_OurIdentity(statics, fingerprint) && AllZeroBytes(statics, sizeof(statics)) && AllZeroBytes(fingerprint, sizeof(fingerprint))
			&& Net_KeptIdentityResult() == found && storeLoads == loads + 1 && storeMakes == makes,
			"a kept identity that can't be used" + which + ": none is made, the page gets only 0s, and it's looked for once");
		Check(LogHas("net: the kept identity: the kept one can't be used (the test's stand-in says so)")
			&& LogHas("net: so there's no identity this run until the player chooses to make a new one, and nothing goes through a relay"),
			"and the log says what was found, and what it means" + which);
		relayNamed = true;
		size_t const sentBefore = datagramsSentTo.size();
		// Modified 2026-11-10 by Melanikus for the AvP port (#188): sealed or nothing, so an online game with no identity isn't hosted at all
		Check(HostAGame() == NET_FAIL && SentToTheRelaySince(sentBefore) == 0 && Net_HostFailedWhy() != NULL && strstr(Net_HostFailedWhy(), "identity") != NULL,
			"an online game then isn't hosted at all, sends nothing to the relay, and says it's the identity" + which);
		EndTheGame();
		relayNamed = true;   // as the tests after this expect (2026-11-11)
	}

	// The player's yes: refused while a game is open, then a new one made and kept, and nothing left to ask
	Check(HostAGame() == NET_OK, "a game hosted through its line's relay");
	int const makesBeforeYes = storeMakes;
	Check(!Net_MakeNewIdentity() && storeMakes == makesBeforeYes && LogHas("net: a new identity isn't made while a network game is open"),
		"the player's yes is refused while a network game is open, and nothing is made");
	EndTheGame();
	Check(Net_MakeNewIdentity() && storeMakes == makesBeforeYes + 1 && Net_KeptIdentityResult() == kNetIdentityStoreLoaded && Net_OurIdentity(statics, fingerprint),
		"once it's over, the player's yes makes a new one and keeps it, and there's nothing left to ask");

	// A yes whose new identity can't be kept: none this run
	storeKeepFails = true;
	Check(!Net_MakeNewIdentity() && !Net_OurIdentity(statics, fingerprint) && AllZeroBytes(fingerprint, sizeof(fingerprint)),
		"a yes whose new identity can't be kept leaves none this run");

	// The first run, with an identity that can't be kept: none this run, and it's tried once
	storeFinds = kNetIdentityStoreNoFile;
	RestartTheNetwork();
	int const makesUnkept = storeMakes;
	Check(!Net_OurIdentity(statics, fingerprint) && !Net_OurIdentity(statics, fingerprint) && storeMakes == makesUnkept + 1
		&& LogHas("net: a new identity couldn't be made and kept, so there's none this run, and nothing goes through a relay"),
		"an identity that can't be kept on the first run: none this run, tried once, and the log says so");
	storeKeepFails = false;

	// As the tests after this expect: the network started, with an identity to use
	storeFinds = kNetIdentityStoreLoaded;
	RestartTheNetwork();
	Check(Net_OurIdentity(statics, fingerprint), "the network started again, with the kept identity");
}

// (Removed 2026-11-10 by Melanikus for the AvP port, #175 PR 3a: the v2 draft's own test, TestTheV2Draft, and its helpers, as the draft's
// switch went: version 2 is every game's protocol, at 116.)

static bool LogMentions(const char *words)
{
	for (std::string const &line : logLines)
	{
		if (line.find(words) != std::string::npos)
		{
			return true;
		}
	}

	return false;
}

// The settings file's network numbers. It's the player's own file, but it can hold any number. A number the game can't use is put back
// to the game's own, and the log says so with the number, so a tester's log shows why a host isn't where it was set to be.
// The three numbers the settings file holds, one per line, as a player's file has them
static void StartAgainWith(int port, int incoming, int outgoing)
{
	EndTheGame();
	Net_Deinitialise();
	settingsFile.clear();
	settingsFile["PortNumber"] = port;
	settingsFile["IncomingBandwidth"] = incoming;
	settingsFile["OutgoingBandwidth"] = outgoing;
	logLines.clear();
	Net_Initialise();
}

// Added 2026-11-10 by Melanikus for the AvP port (#188, sealed or nothing; networking.h says the rule): a game beyond the home network is
// sealed, through the relay, or it doesn't happen, and a plain game is only ever between addresses on the home network. The online host's
// own way through the relay is TestTheRelayChannel's.
static void TestSealedOrNothing()
{
	// Modified 2026-11-11 by Melanikus for the AvP port (the owner's rulings for beta.2: there's no plain game, at home or anywhere, and "No
	// auto choice, they choose"): what was the home game's rule is gone with it, and what's left is that nothing is hosted but on the line
	// the player chose, after its yes and its answer, and nothing plain is joined or answered.
	printf("Sealed or nothing (#188), and no plain game\n");
	const char *why = "not asked";

	// NOTHING HOSTS WITHOUT A CHOICE: with no line chosen to host on, no host is made, nothing is sent or asked, and the host says to choose
	relayNamed = true;
	hostLineGiven = -1;
	int const hostsBeforeTheChoice = hostsCreated;
	size_t const sentBeforeTheChoice = datagramsSentTo.size();
	int const keysBeforeTheChoice = relayKeysAsked;
	Check(HostAGame() == NET_FAIL && Net_HostFailedWhy() != NULL && strcmp(Net_HostFailedWhy(), "choose a server to host on") == 0
		&& hostsCreated == hostsBeforeTheChoice && datagramsSentTo.size() == sentBeforeTheChoice && relayKeysAsked == keysBeforeTheChoice,
		"NOTHING HOSTS WITHOUT A CHOICE: with no server chosen, no host is made, nothing is sent or asked, and the host says to choose a server");
	Check(Net_ServerState(&why) == kNetServerNone, "and the host's state says no server is chosen");
	hostLineGiven = -2;
	EndTheGame();

	// The chosen line's server hasn't answered yet: not hosted, nothing made, and the screen told to wait for it
	serverStateGiven = kRelayServerAsking;
	int const hostsBeforeTheAnswer = hostsCreated;
	Check(HostAGame() == NET_FAIL && Net_HostFailedWhy() != NULL && strcmp(Net_HostFailedWhy(), "the server hasn't answered yet") == 0
		&& Net_ServerState(&why) == kNetServerAsking && hostsCreated == hostsBeforeTheAnswer,
		"an online host whose chosen server hasn't answered yet isn't hosted, makes nothing, and the screen is told to wait for it");

	// One whose server can't be used: not hosted, with the list's own words
	serverStateGiven = kRelayServerCantUse;
	serverStateWhy = "the relay server couldn't be reached";
	Check(HostAGame() == NET_FAIL && Net_HostFailedWhy() != NULL && strcmp(Net_HostFailedWhy(), "the relay server couldn't be reached") == 0
		&& Net_ServerState(&why) == kNetServerCantUse && why != NULL && strcmp(why, "the relay server couldn't be reached") == 0 && hostsCreated == hostsBeforeTheAnswer,
		"and one whose server can't be reached isn't hosted either, saying the list's words, and makes nothing");

	// One whose line the player hasn't said yes to: nothing is made, and nothing is sent anywhere
	serverStateGiven = kRelayServerNeedsYes;
	serverStateWhy = NULL;
	size_t const sentBeforeTheYes = datagramsSentTo.size();
	Check(HostAGame() == NET_FAIL && Net_HostFailedWhy() != NULL && strcmp(Net_HostFailedWhy(), "you haven't said yes to the server in servers.cfg yet") == 0
		&& Net_ServerState(&why) == kNetServerNeedsYes && hostsCreated == hostsBeforeTheAnswer && datagramsSentTo.size() == sentBeforeTheYes,
		"before the yes, no host is made and nothing is sent anywhere");
	serverStateGiven = -1;
	serverStateWhy = NULL;
	EndTheGame();

	// A plain game, from anywhere, isn't joined: refused before anything is opened, saying why
	int const hostsBefore = hostsCreated;
	AvP.Network = I_Peer;
	NumberOfSessionsFound = 1;
	memset(&SessionData[0], 0, sizeof(SessionData[0]));
	SessionData[0].throughRelay = false;
	SessionData[0].host = 0x1400A8C0;   // 192.168.0.20, on a home network: there's no game at home any more either
	SessionData[0].port = 23513;
	Check(Net_ConnectToSession(0, playerName) == NET_FAIL && Net_JoinRefusedWhy() != NULL && strstr(Net_JoinRefusedWhy(), "relay") != NULL
		&& hostsCreated == hostsBefore,
		"a plain game, even one on a home network, is refused before anything opens, and the refusal says why");
	EndTheGame();

	// And nothing listens or answers on this network: a host opens no socket for being found
	int const socketsBefore = socketsCreated;
	Check(HostAGame() == NET_OK && socketsCreated == socketsBefore, "a host through its line's relay opens no socket for being found");
	Net_ServiceNetwork();
	Check(socketsCreated == socketsBefore, "and serving the network opens none either");
	EndTheGame();

	serverInTheFile = -1;
	relayNamed = true;
	NumberOfSessionsFound = 0;
	memset(SessionData, 0, sizeof(SessionData[0]) * MAX_NO_OF_SESSIONS);
}

static void TestTheSettingsFile()
{
	StartAgainWith(0, -5, -1);
	Check(LogHas("net: the settings file's PortNumber is 0, which the game can't use, so it uses 23513"), "a port of 0 isn't used, and the log says so");
	Check(LogHas("net: the settings file's IncomingBandwidth is -5, which the game can't use, so it uses 0"), "nor a speed below nothing coming in");
	Check(LogHas("net: the settings file's OutgoingBandwidth is -1, which the game can't use, so it uses 0"), "nor going out");
	Check(HostAGame() == NET_OK && hostsBoundTo.back().port == 23513, "and a host plays on the game's own port");
	Check(hostIncomingBandwidth == 0 && hostOutgoingBandwidth == 0, "with as much speed as there is, not four thousand million bytes a second");

	StartAgainWith(23514, 0, 0);
	Check(LogHas("net: the settings file's PortNumber is 23514, which the game can't use, so it uses 23513"), "the port a search uses isn't used for the game as well");
	Check(HostAGame() == NET_OK && hostsBoundTo.back().port == 23513, "so the host still opens both");

	StartAgainWith(65536, 0, 0);
	Check(LogHas("net: the settings file's PortNumber is 65536, which the game can't use, so it uses 23513"), "nor a port past the last there is");

	StartAgainWith(30000, 20000, 10000);
	Check(std::none_of(logLines.begin(), logLines.end(), [](std::string const &line) { return line.rfind("net: the settings file's", 0) == 0; }), "a port and speeds the game can use are used, with nothing in the log");
	Check(HostAGame() == NET_OK && hostsBoundTo.back().port == 30000, "and the host plays on it");
	Check(hostIncomingBandwidth == 20000 && hostOutgoingBandwidth == 10000, "with the speeds the file says");
	EndTheGame();
}

// Called last: every message from every test above has been looked at by now, and each of them had a stack under it
static void TestNoHeaderLeftAnything()
{
	Check(headersLookedAt > 20, "the tests sent plenty of messages");
	Check(headersWithSomethingInTheGap == 0, "and none had anything in the gap in its header");
}

int main()
{
	// The channel to the relay opens by itself, with the test's own relay keys (step 7, added 2026-11-09 by Melanikus for the AvP port)
	MakeTheTestRelayKeys();
	ResetTheStandInChannel();

	// The tests written before the request to join (version 114) let their joiners in at once, as a test's host does. TestTheRequestToJoin
	// is the one that asks.
	Net_LetEveryJoinerIn(true);

	// The host behind the relay's identity (protocol version 2), made once, as the game makes one each run
	Check(NetIdentity_Make(&scriptedHostIdentity), "the host behind the test's relay has an identity, for handshake B");

	TestStartingTheGame();
	TestNobodyToSendTo();
	TestSendingWithNoGame();
	ListenToARealClient();
	TestDisconnectWithNoGame();
	TestHosting();
	TestHostingThroughTheRelay();
	TestRelayGamesOnTheList();
	TestJoiningThroughTheRelay();
	TestEachGameThroughItsOwnLine();
	TestTheRelayOnTheScreen();
	TestARunsOneRelayBeyondThisMachine();
	TestTheJoinersRoundTrip();
	TestTheStartLimitIsAsked();
	TestTheRequestToJoin();
	TestHostingWhenItCantBeDone();
	TestSkirmish();
	DirtyTheStack();
	TestJoining();
	TestAGameGoesOnBehindAnotherWindow();
	TestJoiningAgainWhenItCantBeDone();
	TestJoinThatGetsNoAnswerThenHosting();
	TestHostingWhileStillInAGame();
	DirtyTheStack();
	TestARealClientGetsIn();
	TestNoAddressOnScreen();
	TestWhatAClientCantAfford();
	TestAnsweringWhileLoading();
	TestConnectionsThatHoldAPlace();
	TestTooManyBytes();
	TestLettingPeopleIn();
	TestWhatAClientCantSay();
	TestWhoDoesntGetIn();
	TestTooManyMessages();
	TestANameWithNoEnd();
	DirtyTheStack();
	TestPassingThingsOn();
	TestNoRoom();
	TestSomeoneGoing();
	DirtyTheStack();
	TestTheClientsSide();
	TestWaitingToJoin();
	TestLeavingClosesConnections();
	TestLeavingWithoutAnAnswer();
	TestTheHostsConnectionClosing();
	TestPlayerIDs();
	TestTheMessageCheck();
	TestTheTableOfMessageParts();
	TestTheGameUsesThis();
	TestTheKeptIdentity();
	TestTheRelayChannel();
	TestSealedOrNothing();
	TestTheSettingsFile();

	TestNoHeaderLeftAnything();

	Check(packetsDestroyedThatWerentHeld == 0, "ENet is never asked to destroy a packet it doesn't hold, or the same one twice");

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
