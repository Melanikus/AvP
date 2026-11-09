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
extern bool Net_UpdateSessionList(int &SelectedItem);

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
static bool relayNamed = false;
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
static int datagramsTried = 0;   // sent or not: a check of a refused send needs to know one was tried
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

ENetPeer *enet_host_connect(ENetHost *, const ENetAddress *address, size_t channelCount, enet_uint32)
{
	channelsAsked = channelCount;

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
	datagramsTried++;

	if (datagramSendFails)
	{
		return -1;
	}

	// A relay message (net_relay.h) has no game header to look at: its kind is where a header's gap is
	if (buffers && buffers->data && buffers->dataLength >= sizeof(MessageHeader)
		&& NetRelay_Read((const uint8_t *)buffers->data, buffers->dataLength, NULL) != kNetRelayIsAMessage)
	{
		LookAtTheGapInAHeader(buffers->data);
	}

	if (buffers && buffers->data)
	{
		uint8_t const *const bytes = (uint8_t const *)buffers->data;

		lastDatagramSent.assign(bytes, bytes + buffers->dataLength);
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

static NetResult JoinAGame()
{
	AvP.Network = I_Peer;
	SessionData[0].host = 0x0100007F;
	SessionData[0].port = 23513;
	return Net_ConnectToSession(0, playerName);
}

// Back to a game with nothing going on. Whether this works is tested below, and this is only for between those tests.
static void EndTheGame()
{
	Net_Disconnect();
	AvP.Network = I_No_Network;
}

// Modified 2026-11-08 by Melanikus for the AvP port: hosting through a relay (net_relay.h, PROTOCOL.md 7014de1), with the list naming one.
// The host times it with echoes from the game's own socket, asks it for a room, listens to it alone, takes its messages off ENet's hands,
// puts the room on the list with its region and round trip, hands the list each newer pass, and closes the room on leaving. With no relay
// answering, the game is open on this network instead; with none named, hosting is as it always was.
static void TellTheHost(enet_uint32 fromHost, uint16_t fromPort, uint8_t *bytes, size_t size)
{
	theHost.receivedAddress.host = fromHost;
	theHost.receivedAddress.port = fromPort;
	theHost.receivedData = bytes;
	theHost.receivedDataLength = size;
}

static void TestHostingThroughTheRelay()
{
	printf("Hosting through a relay\n");
	relayNamed = true;
	int const openBefore = socketsOpen;
	int const sentBefore = datagramsSent;
	Check(HostAGame() == NET_OK, "a game hosts with a relay named");
	Check(datagramsSent == sentBefore + 1 && datagramsSentTo.back().host == kRelayHost && datagramsSentTo.back().port == kNetRelayDefaultControlPort
		&& lastDatagramSent.size() == kNetRelayMessageBytes && lastDatagramSent[5] == kNetRelayEcho && lastDatagramSent[34] == 1,
		"first it times the relay: one 64-byte ECHO, the first, to its control port");
	Check(socketsOpen == openBefore, "and opens no socket for being found on this network");
	Check(theHost.intercept != NULL && theHost.duplicatePeers == 7, "the relays' messages are taken off its socket, and its places from one address are the room's");

	// The relay answers the first echo with its bytes; the clock moves on past the other two and the second they have to be answered in
	uint8_t back[kNetRelayMessageBytes];
	memcpy(back, lastDatagramSent.data(), sizeof(back));
	back[5] = kNetRelayEchoBack;
	TellTheHost(kRelayHost, kNetRelayDefaultControlPort, back, sizeof(back));
	Check(theHost.intercept(&theHost, NULL) == 1, "the echo's answer is taken off ENet's hands");
	enetClock += 100;
	Net_ServiceNetwork();
	enetClock += 100;
	Net_ServiceNetwork();
	Check(datagramsSent == sentBefore + 3 && lastDatagramSent[5] == kNetRelayEcho && lastDatagramSent[34] == 3, "the second and third echoes, 100 ms apart");
	enetClock += 1000;
	Net_ServiceNetwork();
	Check(datagramsSent == sentBefore + 4 && lastDatagramSent[5] == kNetRelayHostOpen && datagramsSentTo.back().host == kRelayHost,
		"a second after the last echo, it asks the relay that answered for a room");

	NetRelayMessage opened = NetRelay_Message(kNetRelayHostOpened);
	memset(opened.secret, 0x5A, sizeof(opened.secret));
	memcpy(opened.code, "ABCD2345", 8);
	memset(opened.pass, 0x33, sizeof(opened.pass));
	opened.passTime = 1700000000u;
	uint8_t bytes[kNetRelayMessageBytes];
	NetRelay_Write(opened, bytes);
	TellTheHost(kRelayHost, kNetRelayDefaultControlPort, bytes, sizeof(bytes));
	Check(theHost.intercept(&theHost, NULL) == 1, "HOST_OPENED from the relay's control port is taken off ENet's hands");

	int const listedBefore = roomsListed;
	Net_ServiceNetwork();
	Check(roomsListed == listedBefore + 1 && regionListed == "virginia" && hostMsListed >= 1 && passTimeListed == 1700000000u,
		"and the next frame puts the room on the list, with the relay's region, its round trip and the pass's time");
	Net_ServiceNetwork();
	Check(roomsListed == listedBefore + 1, "once");

	NetRelayMessage kept = NetRelay_Message(kNetRelayHostKept);
	memset(kept.secret, 0x5A, sizeof(kept.secret));
	memset(kept.pass, 0x44, sizeof(kept.pass));
	kept.passTime = 1700000015u;
	NetRelay_Write(kept, bytes);
	TellTheHost(kRelayHost, kNetRelayDefaultControlPort, bytes, sizeof(bytes));
	int const passesBefore = passesListed;
	Check(theHost.intercept(&theHost, NULL) == 1, "HOST_KEPT is taken off ENet's hands too");
	Net_ServiceNetwork();
	Check(passesListed == passesBefore + 1 && passTimeListed == 1700000015u, "and its newer pass goes to the list");
	Net_ServiceNetwork();
	Check(passesListed == passesBefore + 1, "once");

	uint8_t gamePacket[40];
	memset(gamePacket, 0x11, sizeof(gamePacket));
	TellTheHost(kRelayHost, 27501, gamePacket, sizeof(gamePacket));
	Check(theHost.intercept(&theHost, NULL) == 0, "a joiner's packet, through its port on the relay, is the game's: ENet reads it");
	TellTheHost(0x0200007F, 27501, gamePacket, sizeof(gamePacket));
	Check(theHost.intercept(&theHost, NULL) == 1, "and anything from anywhere but the relay is dropped");

	int const unlistedBefore = roomsUnlisted;
	EndTheGame();
	Check(lastDatagramSent.size() == kNetRelayMessageBytes && lastDatagramSent[5] == kNetRelayHostClose && datagramsSentTo.back().host == kRelayHost,
		"leaving tells the relay HOST_CLOSE");
	Check(roomsUnlisted == unlistedBefore + 1, "and takes the game off the list");

	// A room that's lost (no HOST_KEPT for 45 s): off the list, and open on this network instead (Fourth's cold read, finding 4)
	Check(HostAGame() == NET_OK, "a game hosts through the relay again");
	memcpy(back, lastDatagramSent.data(), sizeof(back));
	back[5] = kNetRelayEchoBack;
	TellTheHost(kRelayHost, kNetRelayDefaultControlPort, back, sizeof(back));
	theHost.intercept(&theHost, NULL);
	enetClock += 100;
	Net_ServiceNetwork();
	enetClock += 100;
	Net_ServiceNetwork();
	enetClock += 1000;
	Net_ServiceNetwork();
	NetRelay_Write(opened, bytes);
	TellTheHost(kRelayHost, kNetRelayDefaultControlPort, bytes, sizeof(bytes));
	theHost.intercept(&theHost, NULL);
	Net_ServiceNetwork();
	int const openWhileOpen = socketsOpen;
	int const unlistedWhileOpen = roomsUnlisted;
	enetClock += kNetRelayKeptWithinMs;
	Net_ServiceNetwork();
	Check(roomsUnlisted == unlistedWhileOpen + 1 && socketsOpen == openWhileOpen + 1 && theHost.intercept == NULL && theHost.duplicatePeers == 4,
		"with no HOST_KEPT for 45 s the room is lost: the game comes off the list, and is open on this network instead, as with no relay");
	EndTheGame();
	Check(roomsUnlisted == unlistedWhileOpen + 1, "and leaving after that doesn't take it off the list twice");

	// A relay named, and nobody answering: open on this network instead
	int const sentQuietly = datagramsSent;
	int const openQuietly = socketsOpen;
	Check(HostAGame() == NET_OK && datagramsSent == sentQuietly + 1, "with the relay silent, the first echo goes");
	enetClock += 100;
	Net_ServiceNetwork();
	enetClock += 100;
	Net_ServiceNetwork();
	enetClock += 1000;
	Net_ServiceNetwork();
	Check(socketsOpen == openQuietly + 1 && theHost.intercept == NULL && theHost.duplicatePeers == 4 && lastDatagramSent[5] == kNetRelayEcho,
		"and a second after the last, with no answer, the game is open on this network as it always was, and nothing asked a relay for a room");
	EndTheGame();

	// A relay listed with an address that must never be sent to, in case the list's own rule ever lets one through: hosting is as it
	// always was, and nothing is sent to it
	enet_uint32 const neverSentTo[] = { ENET_HOST_ANY, ENET_HOST_BROADCAST };

	for (enet_uint32 const address : neverSentTo)
	{
		relayAddressNamed = address;
		int const sentNever = datagramsSent;
		Check(HostAGame() == NET_OK && datagramsSent == sentNever && theHost.intercept == NULL && theHost.duplicatePeers == 4,
			address == ENET_HOST_ANY ? "a relay listed at 0.0.0.0: nothing is sent to it, and hosting is as it always was"
				: "a relay listed at 255.255.255.255: nothing is sent to it, and hosting is as it always was");
		EndTheGame();
	}

	relayAddressNamed = kRelayHost;

	relayNamed = false;
	int const sentAfter = datagramsSent;
	Check(HostAGame() == NET_OK && datagramsSent == sentAfter && theHost.intercept == NULL && theHost.duplicatePeers == 4,
		"with no relay named, hosting is as it always was: nothing goes to a relay, and 4 places from one address");
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
	return game;
}

static void TestRelayGamesOnTheList()
{
	printf("Games from the relays' list\n");
	NumberOfSessionsFound = 0;
	RelayListGame const game = AGameOnTheList("ABCD2345");
	Check(Net_AddRelaySession(game) && NumberOfSessionsFound == 1 && SessionData[0].throughRelay && memcmp(SessionData[0].relayCode, "ABCD2345", 8) == 0
		&& strcmp(SessionData[0].relayRegion, "virginia") == 0 && SessionData[0].host == 0 && SessionData[0].AllowedToJoin
		&& strcmp(SessionData[0].Name, "Hunt (1/4)") == 0, "a game from the relays' list goes on the list of sessions, marked as a relay one, with no address");
	Check(strcmp(SessionData[0].levelName, "Genshd1") == 0 && SessionData[0].gameStyle == game.gameStyle && SessionData[0].players == 1 && SessionData[0].maxPlayers == 4,
		"with its level's name, its style and its players for the games page's columns");
	Check(!Net_AddRelaySession(game) && NumberOfSessionsFound == 1, "the same game twice is one game");

	RelayListGame badCode = AGameOnTheList("ABCD234O");
	Check(!Net_AddRelaySession(badCode) && NumberOfSessionsFound == 1, "a code with O in it isn't a game's");
	RelayListGame noRegion = AGameOnTheList("ABCD2346");
	noRegion.region[0] = 0;
	Check(!Net_AddRelaySession(noRegion) && NumberOfSessionsFound == 1, "nor a game with no region");
	RelayListGame outOfRange = AGameOnTheList("ABCD2346");
	outOfRange.levelNumber = 101;
	Check(!Net_AddRelaySession(outOfRange) && NumberOfSessionsFound == 1, "nor one whose level number is past 100");

	RelayListGame older = AGameOnTheList("ABCD2347");
	older.version = kMultiplayerVersion - 1;
	Check(Net_AddRelaySession(older) && NumberOfSessionsFound == 2 && !SessionData[1].AllowedToJoin && strstr(SessionData[1].Name, "(V ") != NULL,
		"another version is listed with its version, and can't be joined, as on this network");
	RelayListGame full = AGameOnTheList("ABCD2348");
	full.players = 4;
	Check(Net_AddRelaySession(full) && !SessionData[2].AllowedToJoin, "a full game is listed and can't be joined");

	static const char codes[][9] = { "ABCD2352", "ABCD2353", "ABCD2354", "ABCD2355", "ABCD2356", "ABCD2357", "ABCD2358", "ABCD2359" };
	int added = 0;

	for (int i = 0; i < 8; i++)
	{
		added += Net_AddRelaySession(AGameOnTheList(codes[i])) ? 1 : 0;
	}

	Check(NumberOfSessionsFound == MAX_NO_OF_SESSIONS && added == MAX_NO_OF_SESSIONS - 3, "the list fills to its end");
	Check(!Net_AddRelaySession(AGameOnTheList("ABCD2362")) && NumberOfSessionsFound == MAX_NO_OF_SESSIONS, "and nothing more goes on it");

	// Each one's GUID, from its region and code: the same game the same each time, two games apart (Fourth's cold read, finding 6)
	GUID const first = SessionData[0].Guid;
	Check(first.Data2 == 0x524C && first.Data3 == 1 && memcmp(first.Data4, "ABCD2345", 8) == 0, "a relay game's GUID is its own kind, and holds its code");
	Check(memcmp(&SessionData[0].Guid, &SessionData[1].Guid, sizeof(GUID)) != 0, "two relay games' GUIDs differ");
	NumberOfSessionsFound = 0;
	Net_AddRelaySession(AGameOnTheList("ABCD2345"));
	Check(memcmp(&SessionData[0].Guid, &first, sizeof(GUID)) == 0, "and the same game put on the list again gets the same GUID, so the page's highlight stays on it");
	RelayListGame elsewhere = AGameOnTheList("ABCD2345");
	strcpy(elsewhere.region, "paris");
	Net_AddRelaySession(elsewhere);
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

static void ArrivesFromTheRelay(const NetRelayMessage &m, uint16_t fromPort)
{
	WaitingDatagram waiting;
	waiting.bytes.resize(kNetRelayMessageBytes);
	NetRelay_Write(m, waiting.bytes.data());
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
	Check(Net_AddRelaySession(AGameOnTheList("ABCD2345")) && SessionData[0].throughRelay, "a game from the relays' list, to join");

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
	Check(theHost.intercept(&theHost, NULL) == 0, "while the game's own, from its port on the relay, are ENet's to read");
	EndTheGame();

	// The relay says no
	NumberOfSessionsFound = 0;
	Net_AddRelaySession(AGameOnTheList("ABCD2345"));
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
	Net_AddRelaySession(AGameOnTheList("ABCD2345"));
	AvP.Network = I_Peer;
	Check(Net_ConnectToSession(0, playerName) == NET_FAIL && strcmp(Net_RelayJoinFailedWhy(), "the relay didn't answer") == 0,
		"a silent relay: the join gives up, even with the clock standing still, and says the relay didn't answer");
	EndTheGame();

	// The player gives up: the menu's hook says so on the first slice, and the join stops at once
	NumberOfSessionsFound = 0;
	Net_AddRelaySession(AGameOnTheList("ABCD2345"));
	waitHookAsked = 0;
	Net_SetRelayJoinWaitHook(GiveUpAtOnce);
	AvP.Network = I_Peer;
	Check(Net_ConnectToSession(0, playerName) == NET_FAIL && strcmp(Net_RelayJoinFailedWhy(), "the join was given up") == 0 && waitHookAsked == 1,
		"the player giving up (Esc, through the menu's hook): the join stops on that slice, and says it was given up");
	EndTheGame();
	Net_SetRelayJoinWaitHook(KeepWaiting);
	waitHookAsked = 0;
	NumberOfSessionsFound = 0;
	Net_AddRelaySession(AGameOnTheList("ABCD2345"));
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
	Net_AddRelaySession(AGameOnTheList("ABCD2345"));
	ArrivesFromTheRelay(given, kNetRelayDefaultControlPort);
	AvP.Network = I_Peer;
	Check(Net_ConnectToSession(0, playerName) == NET_FAIL && stepsSeen[kNetRelayJoinStepReaching] >= 1 && stepsSeen[kNetRelayJoinStepIntroducing] >= 1
		&& stepsSeen[kNetRelayJoinStepNone] == 0 && Net_RelayJoinStep() == kNetRelayJoinStepNone,
		"the menu's hook sees the join reaching the relay, then the relay introducing the two ends, and no step once it's over");
	Net_SetRelayJoinWaitHook(NULL);
	EndTheGame();
	datagramsWaiting.clear();

	// No relay for its region
	relayNamed = false;
	NumberOfSessionsFound = 0;
	Net_AddRelaySession(AGameOnTheList("ABCD2345"));
	AvP.Network = I_Peer;
	Check(Net_ConnectToSession(0, playerName) == NET_FAIL && strcmp(Net_RelayJoinFailedWhy(), "no relay could be reached") == 0, "no relay named for its region: no join");
	EndTheGame();

	NumberOfSessionsFound = 0;
	memset(SessionData, 0, sizeof(SessionData[0]) * MAX_NO_OF_SESSIONS);
	datagramsWaiting.clear();
}

// Modified 2026-11-09 by Melanikus for the AvP port: THE RELAY ON THE SCREEN (Fourth's online screens). The host's news about its relay,
// once each time it changes, and the line in play saying which relay the game goes through and how far away it is, for a host and for a
// joiner. Asking changes nothing.
// A host that has just started hosting through the relay: the relay answers its first echo, the echoes end, and the relay opens its room
static void HostThroughTheRelayUntilItsRoomIsOpen()
{
	uint8_t back[kNetRelayMessageBytes];
	memcpy(back, lastDatagramSent.data(), sizeof(back));
	back[5] = kNetRelayEchoBack;
	TellTheHost(kRelayHost, kNetRelayDefaultControlPort, back, sizeof(back));
	theHost.intercept(&theHost, NULL);
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

static void TestTheRelayOnTheScreen()
{
	printf("The relay on the screen\n");
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
		"the line in play: hosting through that region, at the echoes' round trip until a keep has been answered");

	// A keep, answered 37 ms later: the line's round trip is the keep's
	enetClock += kNetRelayKeepEveryMs;
	Net_ServiceNetwork();
	Check(lastDatagramSent.size() == kNetRelayMessageBytes && lastDatagramSent[5] == kNetRelayHostKeep, "15 s on, the host keeps its room");
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
	Check(news.what == kNetRelayHostRoomGone && news.why != NULL && strcmp(news.why, NetRelay_WhyText(kNetRelayNoKeep)) == 0 && news.region[0] == 0 && !play.throughRelay,
		"its room lost: the news is that the room has gone, and why, in the game's own words; and there's no line in play");
	EndTheGame();

	// No relay answering, or none the list named that can be used: open on this network only
	Check(HostAGame() == NET_OK, "a game hosts with the relay silent");
	enetClock += 100;
	Net_ServiceNetwork();
	enetClock += 100;
	Net_ServiceNetwork();
	enetClock += 1000;
	Net_ServiceNetwork();
	Net_RelayHostStateNow(&news);
	Check(news.what == kNetRelayHostNoRelay && news.why == NULL && news.region[0] == 0, "no relay answered: the news is that the game is open on this network only");
	EndTheGame();
	relayAddressNamed = ENET_HOST_ANY;
	Check(HostAGame() == NET_OK, "a game hosts with the only relay named at an address that's never sent to");
	Net_RelayHostStateNow(&news);
	Check(news.what == kNetRelayHostNoRelay, "and the news is the same");
	EndTheGame();
	relayAddressNamed = kRelayHost;

	relayNamed = false;
	Net_RelayHostStateNow(&again);
	Check(HostAGame() == NET_OK, "a game hosts with no relay named");
	Net_RelayHostStateNow(&news);
	Check(news.what == kNetRelayHostNone && news.changes == again.changes, "and has no relay news at all");
	EndTheGame();

	// A joiner through a relay: ENet's round trip to the host, once ENet has timed one
	relayNamed = true;
	datagramsWaiting.clear();
	NumberOfSessionsFound = 0;
	Net_AddRelaySession(AGameOnTheList("ABCD2345"));
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

	// A game joined on this network has no relay line
	datagramsWaiting.clear();
	NumberOfSessionsFound = 0;
	memset(SessionData, 0, sizeof(SessionData[0]) * MAX_NO_OF_SESSIONS);
	Check(JoinAGame() == NET_OK, "a game joined on this network");
	Net_RelayPlayStateNow(&play);
	Check(!play.throughRelay, "has no relay line in play");
	EndTheGame();

	thePeer.lastReceiveTime = 0;
	thePeer.roundTripTime = 0;
	relayNamed = false;
	NumberOfSessionsFound = 0;
	memset(SessionData, 0, sizeof(SessionData[0]) * MAX_NO_OF_SESSIONS);
	datagramsWaiting.clear();
}

// Added 2026-11-09 by Melanikus for the AvP port: THE LINE IN PLAY'S COUNTS (Net_RelayCountsNow, net_traffic.h). Everything through the
// game's socket since its host was made: what ENet counts, and the relay's own messages, which go past ENet, so the game counts them where
// it sends and reads them. A new host starts again from 0, the counts come in each time the network is served, and the last second's
// figures once a second has gone. The stand-in ENet's totals are only ever raised here, so the log's numbers in later tests aren't upset.
static void TestTheRelayCountsOnTheScreen()
{
	printf("The line in play's counts\n");
	NetRelayCounts counts;
	memset(&counts, 0xA5, sizeof(counts));
	Net_RelayCountsNow(&counts);
	Check(!counts.throughRelay && counts.packetsIn == 0 && counts.packetsOut == 0 && counts.bytesIn == 0 && counts.bytesOut == 0
		&& counts.packetsInLastSecond == 0 && counts.bytesOutLastSecond == 0, "with no game, every count is 0");
	Net_RelayCountsNow(NULL);
	Check(true, "and asking with nowhere to put them does nothing");

	// Whatever ENet had counted before the host was made isn't this game's
	theHost.totalReceivedPackets += 1000;
	theHost.totalSentPackets += 2000;
	theHost.totalReceivedData += 100000;
	theHost.totalSentData += 200000;
	relayNamed = true;
	int const sentAtFirst = datagramsSent;
	Check(HostAGame() == NET_OK, "a game hosts through the relay");
	Net_RelayCountsNow(&counts);
	Check(!counts.throughRelay && counts.packetsIn == 0 && counts.bytesIn == 0 && counts.packetsOut >= 1
		&& counts.packetsOut == static_cast<unsigned long long>(datagramsSent - sentAtFirst) && counts.bytesOut == counts.packetsOut * kNetRelayMessageBytes,
		"a new host starts from 0, whatever ENet had counted, and its first echo to the relay, which goes past ENet, is counted at its size");

	HostThroughTheRelayUntilItsRoomIsOpen();
	Net_RelayCountsNow(&counts);
	Check(counts.throughRelay && counts.packetsOut == static_cast<unsigned long long>(datagramsSent - sentAtFirst)
		&& counts.bytesOut == counts.packetsOut * kNetRelayMessageBytes, "its room open, it goes through the relay, and every message to the relay is counted");

	// What ENet counts comes in when the network is served, not when the screen asks
	unsigned long long const outBefore = counts.packetsOut;
	unsigned long long const bytesOutBefore = counts.bytesOut;
	theHost.totalReceivedPackets += 30;
	theHost.totalReceivedData += 3000;
	theHost.totalSentPackets += 20;
	theHost.totalSentData += 2000;
	Net_RelayCountsNow(&counts);
	Check(counts.packetsIn == 0 && counts.packetsOut == outBefore, "asking changes nothing");
	int const sentBeforeServing = datagramsSent;
	Net_ServiceNetwork();
	unsigned long long const sentWhileServing = static_cast<unsigned long long>(datagramsSent - sentBeforeServing);
	Net_RelayCountsNow(&counts);
	Check(counts.packetsIn == 30 && counts.bytesIn == 3000 && counts.packetsOut == outBefore + 20 + sentWhileServing
		&& counts.bytesOut == bytesOutBefore + 2000 + sentWhileServing * kNetRelayMessageBytes,
		"served, ENet's packets and bytes each way are added to the relay's own messages");

	// While a level loads, the network is answered from the loading, and the counts come in there too
	Net_SayLongestGapWhileLoading();
	theHost.totalReceivedPackets += 5;
	theHost.totalReceivedData += 500;
	Net_KeepAnsweringWhileLoading();
	Net_RelayCountsNow(&counts);
	Check(counts.packetsIn == 35 && counts.bytesIn == 3500, "and while a level loads, they come in each time it answers the network");

	// The last second: a window closes once it has run a second, and its figures stay until the next has
	enetClock += 1000;
	Net_ServiceNetwork();
	NetRelayCounts closed;
	Net_RelayCountsNow(&closed);
	theHost.totalReceivedPackets += 50;
	theHost.totalReceivedData += 5000;
	enetClock += 999;
	Net_ServiceNetwork();
	Net_RelayCountsNow(&counts);
	Check(counts.packetsIn == 85 && counts.packetsInLastSecond == closed.packetsInLastSecond && counts.bytesInLastSecond == closed.bytesInLastSecond,
		"999 ms into a window, the totals are up to date, and the last second's figures are still the last window's");
	enetClock += 1;
	Net_ServiceNetwork();
	Net_RelayCountsNow(&counts);
	Check(counts.packetsInLastSecond == 50 && counts.bytesInLastSecond == 5000, "a second in, they're that second's: 50 packets and 5,000 bytes in");

	// A message the socket refused went nowhere, so it isn't counted: the keep, 15 s on
	int const triedBefore = datagramsTried;
	unsigned long long const outBeforeRefused = counts.packetsOut;
	datagramSendFails = true;
	enetClock += kNetRelayKeepEveryMs;
	Net_ServiceNetwork();
	datagramSendFails = false;
	Net_RelayCountsNow(&counts);
	Check(datagramsTried > triedBefore && counts.packetsOut == outBeforeRefused, "a message to the relay that the socket refused isn't counted");

	EndTheGame();
	Net_RelayCountsNow(&counts);
	Check(!counts.throughRelay && counts.packetsIn == 0 && counts.packetsOut == 0 && counts.bytesIn == 0 && counts.packetsInLastSecond == 0,
		"leaving the game: every count is 0 again");

	// A joiner through a relay: its handshake with the relay goes past ENet both ways, before ENet connects, and is counted from the start
	datagramsWaiting.clear();
	NumberOfSessionsFound = 0;
	Net_AddRelaySession(AGameOnTheList("ABCD2345"));
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
	int const joinerSentBefore = datagramsSent;
	Check(Net_ConnectToSession(0, playerName) == NET_OK, "a game joined through a relay");
	Net_RelayCountsNow(&counts);
	Check(counts.throughRelay && counts.packetsIn == 2 && counts.bytesIn == 2 * kNetRelayMessageBytes && counts.packetsOut >= 1
		&& counts.packetsOut == static_cast<unsigned long long>(datagramsSent - joinerSentBefore) && counts.bytesOut == counts.packetsOut * kNetRelayMessageBytes,
		"the joiner's counts start with the relay's handshake: the 2 messages it read and every one it sent");
	theHost.totalReceivedPackets += 7;
	theHost.totalReceivedData += 700;
	Net_ServiceNetwork();
	Net_RelayCountsNow(&counts);
	Check(counts.packetsIn == 9 && counts.bytesIn == 2 * kNetRelayMessageBytes + 700, "then what ENet counts is added to them");
	EndTheGame();

	// A game joined on this network is counted too, for any screen that wants it, with no relay
	datagramsWaiting.clear();
	NumberOfSessionsFound = 0;
	memset(SessionData, 0, sizeof(SessionData[0]) * MAX_NO_OF_SESSIONS);
	Check(JoinAGame() == NET_OK, "a game joined on this network");
	theHost.totalSentPackets += 4;
	theHost.totalSentData += 400;
	Net_ServiceNetwork();
	Net_RelayCountsNow(&counts);
	Check(!counts.throughRelay && counts.packetsIn == 0 && counts.packetsOut == 4 && counts.bytesOut == 400, "is counted from 0 too, and isn't through a relay");
	EndTheGame();

	relayNamed = false;
	NumberOfSessionsFound = 0;
	memset(SessionData, 0, sizeof(SessionData[0]) * MAX_NO_OF_SESSIONS);
	datagramsWaiting.clear();
}

// Hudson's branch, server #14's run through a relay on another machine (2026-11-09). A run with net= uses only this machine's own address,
// or, given one relay beyond it by name, that relay at the one address its name gave. Its sockets then go on this machine's address towards
// that relay, never on 0.0.0.0, and each drops whatever comes from anywhere else before ENet reads it.
extern bool NetLoopbackOnly;
bool Net_AllowOneRemoteRelay(const char *name);
uint32_t Net_RemoteRelayAddress();

static void TestARunsOneRelayBeyondThisMachine()
{
	printf("A Hudson run with net=, and its one relay beyond this machine\n");
	enet_uint32 const kThisMachine = 0x0100007F;   // 127.0.0.1, as ENet keeps it
	enet_uint32 const kRoute = 0x0A01A8C0;         // 192.168.1.10: this machine's address towards the relay
	enet_uint32 const kElsewhere = 0x0B0C0B0A;     // a machine that isn't the relay
	enet_uint32 const kMovedTo = 0x0E0C0B0A;       // where the relay's name might point later
	NetLoopbackOnly = true;
	relayNamed = true;
	routeTowardsTheRelay = kRoute;
	datagramsWaiting.clear();
	Check(Net_AllowOneRemoteRelay(""), "to start, no relay beyond this machine");

	int const sentBefore = datagramsSent;
	Check(HostAGame() == NET_OK && hostsBoundTo.back().host == kThisMachine && datagramsSent == sentBefore && theHost.intercept == NULL,
		"given none, a run's host is on this machine's own address, and the list's relay elsewhere is sent nothing");
	EndTheGame();

	Check(Net_AllowOneRemoteRelay("other.example"), "given another relay, by its name");
	Check(HostAGame() == NET_OK && hostsBoundTo.back().host == kThisMachine && datagramsSent == sentBefore && Net_RemoteRelayAddress() == 0,
		"the list's relay, which has another name, is still left out, and no address is let through");
	EndTheGame();

	Check(!Net_AllowOneRemoteRelay("not a name") && !Net_AllowOneRemoteRelay(NULL), "what isn't a server's name is refused");

	Check(Net_AllowOneRemoteRelay("relay.example"), "given the list's relay, by its name");
	int const connectsBefore = socketsConnected;
	int const openBefore = socketsOpen;
	Check(HostAGame() == NET_OK && hostsBoundTo.back().host == kRoute && hostsBoundTo.back().port == 23513,
		"the host's socket goes on this machine's address towards that relay, never on 0.0.0.0, at the game's port");
	Check(socketsConnected == connectsBefore + 1 && lastSocketConnectedTo.host == kRelayHost && socketsOpen == openBefore,
		"which it found by connecting a socket to the relay, a thing that sends nothing, and that socket is closed again");
	Check(Net_RemoteRelayAddress() == kRelayHost, "the one address let through is the one the relay's name gave");
	Check(datagramsSent == sentBefore + 1 && datagramsSentTo.back().host == kRelayHost, "and the relay is timed, as one on this machine would be");
	uint8_t stranger[40];
	memset(stranger, 0x22, sizeof(stranger));
	TellTheHost(kElsewhere, 27501, stranger, sizeof(stranger));
	Check(theHost.intercept != NULL && theHost.intercept(&theHost, NULL) == 1, "a packet from anywhere but the relay is dropped before ENet reads it");

	HostThroughTheRelayUntilItsRoomIsOpen();
	TellTheHost(kRelayHost, 27501, stranger, sizeof(stranger));
	Check(theHost.intercept(&theHost, NULL) == 0, "with its room open, a joiner's packet, through its port on the relay, is ENet's to read");
	TellTheHost(kElsewhere, 27501, stranger, sizeof(stranger));
	Check(theHost.intercept(&theHost, NULL) == 1, "and one from elsewhere still isn't");

	// The room lost: the game falls back to this network as always, but its socket is still on the way to the relay, so it still hears
	// the relay alone
	enetClock += kNetRelayKeptWithinMs;
	Net_ServiceNetwork();
	TellTheHost(kElsewhere, 27501, stranger, sizeof(stranger));
	Check(theHost.intercept != NULL && theHost.intercept(&theHost, NULL) == 1, "its room lost, the host still drops whatever isn't the relay's");
	EndTheGame();

	// The relay's name giving another address later: that one is left out
	relayAddressNamed = kMovedTo;
	int const sentMoved = datagramsSent;
	Check(HostAGame() == NET_OK && datagramsSent == sentMoved && Net_RemoteRelayAddress() == kRelayHost,
		"the relay's name giving another address later: that one is sent nothing, and the first stays the only one let through");
	TellTheHost(kMovedTo, kNetRelayDefaultControlPort, stranger, sizeof(stranger));
	Check(theHost.intercept != NULL && theHost.intercept(&theHost, NULL) == 1, "and what comes from the new one is dropped");
	EndTheGame();
	relayAddressNamed = kRelayHost;

	// A joiner through it: on the same address, on any free port, hearing the relay alone
	NumberOfSessionsFound = 0;
	Net_AddRelaySession(AGameOnTheList("ABCD2345"));
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
	Check(Net_ConnectToSession(0, playerName) == NET_OK && hostsBoundTo.back().host == kRoute && hostsBoundTo.back().port == 0,
		"a joiner through it: its socket is on the same address, on any free port");
	TellTheHost(kElsewhere, 27501, stranger, sizeof(stranger));
	Check(theHost.intercept != NULL && theHost.intercept(&theHost, NULL) == 1, "and it drops whatever isn't the relay's too");
	EndTheGame();
	datagramsWaiting.clear();

	// Modified 2026-11-09 by Melanikus for the AvP port: given the relay by its address (-dev's test server, relay_list.h), the run knows it by
	// that address, whatever name the list gives it. The test's relay is at 10.11.12.13 (kRelayHost, as ENet keeps it).
	Check(Net_AllowOneRemoteRelay("10.11.12.13"), "given the relay by its address");
	int const sentByAddress = datagramsSent;
	Check(HostAGame() == NET_OK && hostsBoundTo.back().host == kRoute && datagramsSent == sentByAddress + 1 && datagramsSentTo.back().host == kRelayHost &&
		Net_RemoteRelayAddress() == kRelayHost, "the list's relay at that address is the one, whatever its name");
	EndTheGame();
	Check(Net_AllowOneRemoteRelay("10.11.12.14"), "given another address");
	int const sentElsewhere = datagramsSent;
	Check(HostAGame() == NET_OK && hostsBoundTo.back().host == kThisMachine && datagramsSent == sentElsewhere && Net_RemoteRelayAddress() == 0,
		"the list's relay, at another, is left out, and nothing is let through");
	EndTheGame();

	// No way towards the relay found: this machine's own address, as before, so the relay just can't be reached
	Check(Net_AllowOneRemoteRelay("relay.example"), "given the relay again");
	routeTowardsTheRelay = 0;
	Check(HostAGame() == NET_OK && hostsBoundTo.back().host == kThisMachine, "with no way towards it found, the host stays on this machine's own address");
	EndTheGame();

	Net_AllowOneRemoteRelay("");
	routeTowardsTheRelay = 0;
	NetLoopbackOnly = false;
	relayNamed = false;
	NumberOfSessionsFound = 0;
	memset(SessionData, 0, sizeof(SessionData[0]) * MAX_NO_OF_SESSIONS);
}

// Modified 2026-11-09 by Melanikus for the AvP port: the joiner's round trip to each relay, for the games page's ping column, against a
// scripted relay. Nothing is timed until the list names relays; the echoes go to the relay's control port from a socket of their own; an
// answer from anywhere else is dropped, though it holds the same bytes; and the socket is closed once the timing is done.
static void TestTimingTheRelaysForJoining()
{
	printf("Timing the relays for the games page\n");
	datagramsWaiting.clear();
	relayNamed = false;
	Net_ForgetRelayTimes();
	int const openBefore = socketsOpen;
	int const sentNothing = datagramsSent;
	Net_TimeRelaysForJoining();
	Check(Net_JoinerMsToRelay("virginia") == -1 && socketsOpen == openBefore && datagramsSent == sentNothing,
		"with no relays from the list yet, nothing is timed, opened or sent");

	relayNamed = true;
	int const sentBefore = datagramsSent;
	Net_TimeRelaysForJoining();
	Check(datagramsSent == sentBefore + 1 && datagramsSentTo.back().host == kRelayHost && datagramsSentTo.back().port == kNetRelayDefaultControlPort
		&& lastDatagramSent.size() == kNetRelayMessageBytes && lastDatagramSent[5] == kNetRelayEcho && socketsOpen == openBefore + 1,
		"once the list has its relays: the first echo goes to the relay's control port, from a socket of its own");

	NetRelayMessage echo;
	Check(NetRelay_Read(lastDatagramSent.data(), lastDatagramSent.size(), &echo) == kNetRelayIsAMessage, "(the echo reads back as one)");
	NetRelayMessage back = echo;
	back.type = kNetRelayEchoBack;

	// 10 ms on, a stranger answers with the very same bytes; 40 ms on, the relay does
	WaitingDatagram stranger;
	stranger.bytes.resize(kNetRelayMessageBytes);
	NetRelay_Write(back, stranger.bytes.data());
	stranger.from.host = 0x0E0E0E0E;
	stranger.from.port = kNetRelayDefaultControlPort;
	enetClock += 10;
	datagramsWaiting.push_back(stranger);
	Net_TimeRelaysForJoining();
	enetClock += 30;
	ArrivesFromTheRelay(back, kNetRelayDefaultControlPort);
	Net_TimeRelaysForJoining();
	Check(Net_JoinerMsToRelay("virginia") == -1, "no time is given before the timing is done");

	// The other two echoes, unanswered, then a second for any late answer
	enetClock += 100;
	Net_TimeRelaysForJoining();
	enetClock += 100;
	Net_TimeRelaysForJoining();
	enetClock += kNetRelayEchoAnswerWithinMs;
	Net_TimeRelaysForJoining();
	Check(Net_JoinerMsToRelay("virginia") == 40, "then the relay's time is its answer's, 40 ms: the stranger's earlier one wasn't heard");
	Check(Net_JoinerMsToRelay("paris") == -1 && Net_JoinerMsToRelay(NULL) == -1, "a region with no timed relay, or none at all: no time");
	Check(socketsOpen == openBefore, "and the socket is closed once the timing is done");

	int const sentAfter = datagramsSent;
	Net_TimeRelaysForJoining();
	Check(datagramsSent == sentAfter && Net_JoinerMsToRelay("virginia") == 40, "after that, each frame's call does nothing, and the time stays");

	// Timed again, when the page opens again, and this time the relay never answers
	Net_ForgetRelayTimes();
	Check(Net_JoinerMsToRelay("virginia") == -1, "forgotten when the page opens again");
	Net_TimeRelaysForJoining();
	enetClock += 100;
	Net_TimeRelaysForJoining();
	enetClock += 100;
	Net_TimeRelaysForJoining();
	enetClock += kNetRelayEchoAnswerWithinMs;
	Net_TimeRelaysForJoining();
	Check(Net_JoinerMsToRelay("virginia") == -1 && socketsOpen == openBefore, "a relay that never answers has no time, and the socket is still closed");

	Net_ForgetRelayTimes();
	datagramsWaiting.clear();

	// Modified 2026-11-09 by Melanikus for the AvP port: the list names no relay again, as every relay test leaves it. This one didn't, so every
	// test after it hosted through the relay without saying so, and the one that checks a host's own limits (TestLettingPeopleIn) saw the
	// relay's.
	relayNamed = false;
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

// The whole of a search, with real bytes: what a real search asks, and what a real host answers. Then the same answer changed, as anyone on the network could change it.
static void Arrives(std::vector<uint8_t> const &bytes, enet_uint32 fromHost)
{
	WaitingDatagram waiting;

	waiting.bytes = bytes;
	waiting.from.host = fromHost;
	waiting.from.port = 23514;
	datagramsWaiting.push_back(waiting);
}

static void TestASearchAndItsAnswers()
{
	int selection = 0;

	Net_FindAvPSessions();

	std::vector<uint8_t> const question = lastDatagramSent;

	Check(question.size() == sizeof(MessageHeader) + sizeof(SessionDescription), "a search asks with as many bytes as the answer it wants, so no answer is bigger than its question");
	Check(question.size() > sizeof(MessageHeader) && std::all_of(question.begin() + sizeof(MessageHeader), question.end(), [](uint8_t b) { return b == 0; }), "and after its header, nothing, so nothing left in memory goes with it");
	fakeTime += 5000;
	Net_UpdateSessionList(selection);

	Check(HostAGame() == NET_OK, "a real host");
	Arrives(question, 0x0200007F);
	Net_ServiceNetwork();

	std::vector<uint8_t> const answer = lastDatagramSent;

	Check(answer.size() == sizeof(MessageHeader) + sizeof(SessionDescription), "answers it");
	Check(datagramsSentTo.back().host == 0x0200007F, "to whoever asked");
	Check(answer.size() <= question.size(), "with no more than it was asked with");

	// A question smaller than the answer gets nothing: an older game's, a header alone, and one a byte short. Each from its own address, so the
	// limit on answers to one address isn't what stops it.
	{
		int const sentBefore = datagramsSent;
		std::vector<uint8_t> const headerAlone(question.begin(), question.begin() + sizeof(MessageHeader));
		std::vector<uint8_t> const aByteShort(question.begin(), question.end() - 1);

		Arrives(headerAlone, 0x0A00007F);
		Net_ServiceNetwork();
		Check(datagramsSent == sentBefore, "a question that's only a header gets no answer");
		Arrives(aByteShort, 0x0B00007F);
		Net_ServiceNetwork();
		Check(datagramsSent == sentBefore, "nor one a byte short of the answer");
	}
	EndTheGame();

	Net_FindAvPSessions();
	Arrives(question, 0x0400007F);
	Net_UpdateSessionList(selection);
	Check(NumberOfSessionsFound == 0, "a search's own question coming back, as big as an answer, isn't taken for a game");
	Arrives(answer, 0x0300007F);
	Net_UpdateSessionList(selection);
	Check(NumberOfSessionsFound == 1, "a search takes a real host's answer");
	Check(NumberOfSessionsFound == 1 && SessionData[0].host == 0x0300007F, "and goes by the address it really came from, whatever the answer says");

	Arrives(answer, 0x0300007F);
	Net_UpdateSessionList(selection);
	Check(NumberOfSessionsFound == 1, "a game that answers twice is one game");

	// Starting again each time, because a new search starts with an empty list
	auto const freshSearch = [&selection]()
	{
		fakeTime += 5000;
		Net_UpdateSessionList(selection);
		Net_FindAvPSessions();
	};

	freshSearch();

	std::vector<uint8_t> notThisGame = answer;

	((MessageHeader *)notThisGame.data())->marker ^= 1;
	Arrives(notThisGame, 0x0400007F);
	Net_UpdateSessionList(selection);
	Check(NumberOfSessionsFound == 0, "an answer not marked as this game's is ignored");

	std::vector<uint8_t> anotherGame = answer;

	((SessionDescription *)(anotherGame.data() + sizeof(MessageHeader)))->guidApplication.Data1 ^= 1;
	Arrives(anotherGame, 0x0400007F);
	Net_UpdateSessionList(selection);
	Check(NumberOfSessionsFound == 0, "and one saying it's some other program");

	std::vector<uint8_t> tooLong = answer;

	tooLong.push_back(0);
	Arrives(tooLong, 0x0400007F);
	Net_UpdateSessionList(selection);
	Check(NumberOfSessionsFound == 0, "and one that's too long");

	std::vector<uint8_t> tooShort = answer;

	tooShort.pop_back();
	Arrives(tooShort, 0x0400007F);
	Net_UpdateSessionList(selection);
	Check(NumberOfSessionsFound == 0, "and one that's too short");

	// Names that never end
	std::vector<uint8_t> noEnds = answer;
	SessionDescription *const described = (SessionDescription *)(noEnds.data() + sizeof(MessageHeader));

	memset(described->sessionName, 'A', sizeof(described->sessionName));
	memset(described->levelName, 'B', sizeof(described->levelName));
	Arrives(noEnds, 0x0500007F);
	Net_UpdateSessionList(selection);
	Check(NumberOfSessionsFound == 1 && strnlen(SessionData[0].Name, sizeof(SessionData[0].Name)) < sizeof(SessionData[0].Name), "names with no end still make a name that ends");
	Check(strnlen(SessionData[0].levelName, sizeof(SessionData[0].levelName)) < sizeof(SessionData[0].levelName) && SessionData[0].levelName[0] == 'B'
		&& SessionData[0].gameStyle == described->gameStyle && SessionData[0].players == described->nPlayers && SessionData[0].maxPlayers == described->maxPlayers,
		"and the games page's columns: the level's name, ended, the style and the players, as the host said them");
	// A name with no end reads the same as one with its end, once it has been cut to fit the list, so the answer can't show this. The line has to be seen.
	Check(Has(ReadSourceFile("3dc/networking.cpp"), "session.sessionName[kSessionNameSize - 1] = '\\0';"), "a game's name always gets an end before it's read");

	// One address and many games. Each has its own number, so they're all different games.
	freshSearch();

	for (int i = 0; i < 5; i++)
	{
		std::vector<uint8_t> another = answer;

		((SessionDescription *)(another.data() + sizeof(MessageHeader)))->guidInstance.Data1 += i + 1;
		Arrives(another, 0x0600007F);
	}

	Net_UpdateSessionList(selection);
	Check(NumberOfSessionsFound == 2, "one address can't fill the list");

	// Many addresses and many games
	freshSearch();

	for (int i = 0; i < MAX_NO_OF_SESSIONS + 5; i++)
	{
		std::vector<uint8_t> another = answer;

		((SessionDescription *)(another.data() + sizeof(MessageHeader)))->guidInstance.Data1 += i + 1;
		Arrives(another, 0x0700007F + ((enet_uint32)i << 24));
	}

	Net_UpdateSessionList(selection);
	Check(NumberOfSessionsFound == MAX_NO_OF_SESSIONS, "and a full list stays within its room");

	fakeTime += 5000;
	Net_UpdateSessionList(selection);
	datagramsWaiting.clear();
}

static void TestNothingOfTheStackGoesOut()
{
	// The gap has to exist for this test to mean anything
	Check(offsetof(MessageHeader, messageType) > offsetof(MessageHeader, isSystemMessage) + 1, "a header has a gap in it");

	int const before = headersLookedAt;
	int const dirtyBefore = headersWithSomethingInTheGap;

	// The search for games, to the whole network
	DirtyTheStack();
	Net_FindAvPSessions();
	Check(headersLookedAt > before, "a search's question was looked at");
	Check(headersWithSomethingInTheGap == dirtyBefore, "and has nothing in the gap");

	int selection = 0;
	fakeTime += 5000;
	Net_UpdateSessionList(selection);
}

static void TestLookingForGames()
{
	int const created = socketsCreated;
	int const sent = datagramsSent;
	int selection = 0;

	Net_FindAvPSessions();
	Check(socketsCreated == created + 1 && socketsOpen == 1, "looking for games on the network opens the socket");
	Check(socketsBoundTo.back().port == 23514 && socketsBoundTo.back().host == ENET_HOST_ANY, "on the port they answer to");
	Check(datagramsSent == sent + 1 && datagramsSentTo.back().host == ENET_HOST_BROADCAST && datagramsSentTo.back().port == 23514, "and asks once");

	Net_FindAvPSessions();
	Check(socketsCreated == created + 1 && datagramsSent == sent + 1, "asking again during the search changes nothing");

	fakeTime += 4999;
	Net_UpdateSessionList(selection);
	Check(socketsOpen == 1, "it's open until its time is up");

	fakeTime += 1;
	Net_UpdateSessionList(selection);
	Check(socketsOpen == 0, "and closes when it is");

	int const destroyed = socketsDestroyed;

	Net_UpdateSessionList(selection);
	Check(socketsDestroyed == destroyed, "and it's only closed once, however long the page stays up");

	int const waits = waitsOnNoSocket;

	Net_UpdateSessionList(selection);
	Check(waitsOnNoSocket == waits, "with no search going on, nothing looks at a socket that isn't there");

	// A machine that has only just started has a clock near 0, which looks like a search that's still going on if 0 isn't taken to mean there's no search
	uint32_t const theTime = fakeTime;

	fakeTime = 100;
	Net_UpdateSessionList(selection);
	Check(waitsOnNoSocket == waits && waitsOnASocket >= 0, "and that's so just after the machine has started");
	fakeTime = theTime;

	Net_FindAvPSessions();
	Check(socketsCreated == created + 2 && socketsOpen == 1, "looking again opens it again");

	Net_StopSearchingForSessions();
	Check(socketsOpen == 0, "leaving the page closes it straight away, because nothing else would until the next search");

	Net_FindAvPSessions();
	Check(socketsCreated == created + 3 && datagramsSent >= sent + 3, "and the next search doesn't think one is still going on");

	Net_StopSearchingForSessions();
	Net_StopSearchingForSessions();
	Check(socketsOpen == 0, "and leaving twice is safe");
}

static void TestWhenASocketCantBeHad()
{
	int const errorsBefore = errorLines;
	int const sent = datagramsSent;
	int const waits = waitsOnNoSocket;
	int selection = 0;

	socketBindFails = true;
	Net_FindAvPSessions();
	Check(socketsOpen == 0, "a socket that can't listen is closed, not left open");
	Check(datagramsSent == sent, "and nothing is sent");
	Check(errorLines > errorsBefore, "and the console says so");

	Net_UpdateSessionList(selection);
	Check(waitsOnNoSocket == waits, "and no search is going on, so nothing looks at it");
	socketBindFails = false;

	socketCreateFails = true;

	int const errorsNext = errorLines;

	Net_FindAvPSessions();
	Check(socketsOpen == 0 && datagramsSent == sent && errorLines > errorsNext, "the same when one can't even be made");
	socketCreateFails = false;

	int const errorsLast = errorLines;

	datagramSendFails = true;
	Net_FindAvPSessions();
	Check(socketsOpen == 0 && errorLines > errorsLast, "and a search that can't ask is over, and closes it");
	datagramSendFails = false;
}

static void TestHosting()
{
	int const created = socketsCreated;
	int const packets = packetsToAPeer;

	Check(HostAGame() == NET_OK, "hosting works");
	Check(AvP.Network == I_Host && gamesStartedForHost >= 1, "and starts a game");
	Check(socketsOpen == 1 && socketsCreated == created + 1, "and opens the socket, so that others can find it");
	Check(hostsBoundTo.back().port == 23513, "as well as its game port");
	Check(packetsToAPeer == packets, "and sends nothing to anyone");

	// This is the road a host's answers used to take
	int const id = 1234;

	Check(Net_SendSystemMessage(NET_SETPLAYERID, 1, 0, &id, sizeof(id)) == NET_FAIL, "a host has no server, so that fails, and its game goes on");
	Check(packetsToAPeer == packets, "and nothing goes out");

	int const waits = waitsOnASocket;

	Net_ServiceNetwork();
	Check(waitsOnASocket == waits + 1, "a host listens for anyone looking for a game");

	int selection = 0;

	Net_StopSearchingForSessions();
	Check(socketsOpen == 1, "and stopping a search doesn't stop that");

	Net_FindAvPSessions();
	fakeTime += 5000;
	Net_UpdateSessionList(selection);
	Check(socketsOpen == 1, "and nor does a search's time being up");

	Net_Disconnect();
	Check(socketsOpen == 0, "the game ending closes it");
	AvP.Network = I_No_Network;

	int const destroyed = socketsDestroyed;

	Net_Disconnect();
	Check(socketsDestroyed == destroyed, "and it's only closed once");

	Check(HostAGame() == NET_OK && socketsOpen == 1 && socketsCreated == created + 2, "and a second game opens a new one");
	EndTheGame();
	Check(socketsOpen == 0, "and closes that too");
}

static void TestHostingWhenASocketCantBeHad()
{
	socketBindFails = true;

	int const waits = waitsOnNoSocket;

	Check(HostAGame() == NET_OK, "a host with no socket for being found still has a game, for anyone who has its address");
	Check(socketsOpen == 0, "and doesn't leave that socket open");

	Net_ServiceNetwork();
	Check(waitsOnNoSocket == waits, "and doesn't look at it");

	socketBindFails = false;
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

static void TestJoiningWhileAPageIsSearching()
{
	Net_FindAvPSessions();
	Check(socketsOpen == 1, "a search is going on");

	Check(JoinAGame() == NET_OK, "joining a game it found");
	Net_Disconnect();
	Check(socketsOpen == 0, "and leaving that game closes the socket, whatever state it was in");

	int const created = socketsCreated;

	AvP.Network = I_No_Network;
	Net_FindAvPSessions();
	Check(socketsCreated == created + 1 && socketsOpen == 1, "and the next search doesn't think the old one is still going on");
	Net_StopSearchingForSessions();
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

static void Connects(int n)
{
	peers[n].state = ENET_PEER_STATE_CONNECTED;

	ENetEvent const event = { ENET_EVENT_TYPE_CONNECT, &peers[n], NULL };

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
	Check(theHost.duplicatePeers == 4 && theHost.maximumPacketSize == kMostMessageBytes && theHost.maximumWaitingData == 8 * 1024 * 1024, "with the game's limits on places from one address, a message's size, and what can wait");
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

	Check(Has(menus, "if (previousMenuID == AVPMENU_MULTIPLAYERSELECTSESSION && menuID != AVPMENU_MULTIPLAYERSELECTSESSION)\n\t{\n\t\tNet_StopSearchingForSessions();"), "leaving the page of games found stops the search, which is the only way off that page");
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
	Check(Has(ReadSourceFile("3dc/networking.h"), "const int kMultiplayerVersion = 114;"), "and an older game can tell this one isn't what it speaks");

	// The log lines a tester's log needs from pldnet.cpp. A message left out for want of room says so from every place it can be left out.
	Check(Has(pldnet, "if (numBytesReqd > numBytesLeft)\n\t\t{\n\t\t\tNoteSendBufferFull(numBytesReqd);") && !Has(pldnet, "if (numBytesReqd > numBytesLeft)\n\t\t{\n\t\t\tLOCALASSERT(1 == 0);"), "SOURCE: a message left out of a full frame is in the log, from every room check");
	Check(Has(pldnet, "if (headerSize + size > left)\n\t{\n\t\tNoteSendBufferFull(headerSize + size);"), "SOURCE: and from the one the NPCs' messages use");
	Check(Has(pldnet, "\t\tif (NetLevelCount_One(switchSyncsNotMatched))\n\t\t{\n\t\t\tLogString(\"net: the host's switches, doors and lifts can't be matched"), "SOURCE: the host's switches that can't be matched are in the log, once a level");
	Check(Has(pldnet, "void EndAVPNetGame(void)\n{\n\tSwitchSyncs_LevelIsOver();"), "SOURCE: and how many, when the game ends");
	Check(Has(pldnet, "\tnetGameData.myGameState = NGS_Joining;\n\tSwitchSyncs_LevelIsOver();"), "SOURCE: or the next level starts");
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
	// The tests written before the request to join (version 114) let their joiners in at once, as a test's host does. TestTheRequestToJoin
	// is the one that asks.
	Net_LetEveryJoinerIn(true);

	TestStartingTheGame();
	TestNobodyToSendTo();
	TestSendingWithNoGame();
	ListenToARealClient();
	TestDisconnectWithNoGame();
	TestLookingForGames();
	TestNothingOfTheStackGoesOut();
	TestASearchAndItsAnswers();
	TestWhenASocketCantBeHad();
	TestHosting();
	TestHostingThroughTheRelay();
	TestRelayGamesOnTheList();
	TestJoiningThroughTheRelay();
	TestTheRelayOnTheScreen();
	TestTheRelayCountsOnTheScreen();
	TestARunsOneRelayBeyondThisMachine();
	TestTimingTheRelaysForJoining();
	TestTheRequestToJoin();
	TestHostingWhenASocketCantBeHad();
	TestHostingWhenItCantBeDone();
	TestSkirmish();
	DirtyTheStack();
	TestJoining();
	TestAGameGoesOnBehindAnotherWindow();
	TestJoiningAgainWhenItCantBeDone();
	TestJoinThatGetsNoAnswerThenHosting();
	TestHostingWhileStillInAGame();
	TestJoiningWhileAPageIsSearching();
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
	TestTheSettingsFile();

	TestNoHeaderLeftAnything();

	Check(packetsDestroyedThatWerentHeld == 0, "ENet is never asked to destroy a packet it doesn't hold, or the same one twice");

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
