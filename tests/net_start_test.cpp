// Tests for three things in 3dc/networking.cpp: what the game has open on the network, what it does with a message it has nobody to send to, and who gets into a game and what they can say once they're in. Written 2026-11-06 by Melanikus for the AvP port.
//
// This runs the real file. Only ENet and the rest of the game are stand-ins, in tests/net_start_test_stubs, and they open nothing: they count what was asked for, and the clock is the test's.
//
// WHY THESE THREE: only a client has a server to send to, so anything else asking to send has to be told no. A game that isn't doing anything on a network has no reason to have anything open on one. And anyone who can reach a host can connect and send anything at all, so it believes nothing about who sent what except which connection it came on.

#include "enet/enet.h"
#include "networking.h"
#include "net_checks.h"

#include <algorithm>
#include <deque>
#include <fstream>
#include <vector>

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

void Con_PrintDebugMessage(const std::string &)
{
}

int Config_GetInt(const std::string &, const std::string &, int defaultValue)
{
	return defaultValue;
}

void NewOnScreenMessage(char *)
{
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
static bool socketCreateFails = false, socketBindFails = false, datagramSendFails = false, hostCreateFails = false, packetCreateFails = false;
static std::vector<ENetAddress> socketsBoundTo, datagramsSentTo;

static int hostsCreated = 0, hostsDestroyed = 0, hostsWithNoAddress = 0;
static size_t connectionsAllowed = 0;
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

static ENetPacket *MakePacket(const void *data, size_t dataLength)
{
	ENetPacket *packet = new ENetPacket;

	packet->data = new uint8_t[dataLength ? dataLength : 1];
	packet->dataLength = dataLength;
	packet->referenceCount = 0;

	if (data && dataLength)
	{
		memcpy(packet->data, data, dataLength);
	}

	packetsAlive++;
	return packet;
}

void enet_packet_destroy(ENetPacket *packet)
{
	delete[] packet->data;
	delete packet;
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

ENetHost *enet_host_create(const ENetAddress *address, size_t peerCount, size_t, enet_uint32, enet_uint32)
{
	if (hostCreateFails)
	{
		return NULL;
	}

	hostsCreated++;
	connectionsAllowed = peerCount;

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

ENetPeer *enet_host_connect(ENetHost *, const ENetAddress *, size_t, enet_uint32)
{
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

void enet_peer_disconnect(ENetPeer *, enet_uint32)
{
	closedGently++;
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

void LogString(const std::string &)
{
}

void enet_peer_timeout(ENetPeer *, enet_uint32, enet_uint32, enet_uint32)
{
}

int enet_address_set_host_ip(ENetAddress *address, const char *)
{
	address->host = 0x0100007F;

	return 0;
}

int enet_address_get_host_ip(const ENetAddress *, char *hostName, size_t nameLength)
{
	if (nameLength > 0)
	{
		hostName[0] = 0;
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

void enet_socket_destroy(ENetSocket socket)
{
	// The real one is harmless with no socket, so this only counts the real ones
	if (socket != ENET_SOCKET_NULL)
	{
		socketsDestroyed++;
		socketsOpen--;
	}
}

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
	if (datagramSendFails)
	{
		return -1;
	}

	if (buffers && buffers->data && buffers->dataLength >= sizeof(MessageHeader))
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

static std::string Between(const std::string &text, const char *from, const char *to)
{
	std::string::size_type const start = text.find(from);
	std::string::size_type const end = start == std::string::npos ? start : text.find(to, start);

	return (start == std::string::npos || end == std::string::npos) ? std::string() : text.substr(start, end - start);
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

	Check(question.size() == sizeof(MessageHeader), "a search asks with a header and nothing else");
	fakeTime += 5000;
	Net_UpdateSessionList(selection);

	Check(HostAGame() == NET_OK, "a real host");
	Arrives(question, 0x0200007F);
	Net_ServiceNetwork();

	std::vector<uint8_t> const answer = lastDatagramSent;

	Check(answer.size() == sizeof(MessageHeader) + sizeof(SessionDescription), "answers it");
	Check(datagramsSentTo.back().host == 0x0200007F, "to whoever asked");
	EndTheGame();

	Net_FindAvPSessions();
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

static void TestLettingPeopleIn()
{
	playersInGame.clear();
	Check(HostAGame() == NET_OK, "someone hosts");
	Check(connectionsAllowed == 7, "with room for the other seven players, and no more");
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
	int const flushesBefore = flushes;

	Net_Disconnect();
	Check(flushes == flushesBefore + 1, "a host leaving sends what it has first, because closing the connections throws away what's still waiting");
	Check(closedAtOnce == closedBefore + 3, "and tells everyone connected straight away");
	Check(flushesWhenLastClosed == flushes, "in that order");
	AvP.Network = I_No_Network;

	Check(JoinAGame() == NET_OK, "and someone joining");
	netGameData.needGameDescription = 1;

	int const closedClient = closedAtOnce;
	int const flushesClient = flushes;

	sentMessages.clear();
	Net_Disconnect();
	Check(flushes == flushesClient + 1 && closedAtOnce == closedClient + 1, "does the same when they leave");
	Check(sentMessages.empty(), "and there's no message saying they've gone for anyone to have to believe");
	AvP.Network = I_No_Network;
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
		std::string::size_type const comma = line.find(',', at);

		if (at != std::string::npos && line.find("//") > at)
		{
			names.push_back(line.substr(at, (comma == std::string::npos ? line.find_last_not_of(" \t") + 1 : comma) - at));
		}
	}

	Check(names.size() == 52, "there are 52 kinds of part: " + std::to_string(names.size()));

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

	Check(inTheTable == 52, "every one of them is in the table");
	Check(inOrder == 52, "in the game's order");
	Check(sameLength == 52, "and each has the length the code that reads it steps by:" + mismatched);
	Check(CountOf(table, "{ NetMT_") == 52, "and there's nothing else in it");

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
	Check(Has(versionCheck, "57 * MAX_NO_OF_MULTIPLAYER_EPISODES +") && Has(versionCheck, "58 * MAX_NO_OF_COOPERATIVE_EPISODES;"), "and so are the level lists");
	Check(Has(versionCheck, "56 * (sizeof(kNetPartRules) / sizeof(kNetPartRules[0])) +"), "and how many kinds of message there are");

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

	// No network game has an autogun, so a message about one is refused by name, and nothing in the game makes or sends one
	Check(Has(Between(reading, "case (NetMT_PlayerAutoGunState):", "subMessagePtr += sizeof(NETMESSAGE_AGUNSTATE);"), "Net_NoteRefused("), "an autogun message is refused");
	Check(Has(Between(ReadSourceFile("3dc/avp/bh_agun.cpp"), "void AutoGunBehaveInit(", "sbPtr->shapeIndex = toolsData->shapenum;"), "if(AvP.Network != I_No_Network)\n\t{\n\t\tRemoveBehaviourStrategy(sbPtr);"), "a level's own autoguns are taken away in a network game");
	Check(Has(Between(ReadSourceFile("3dc/avp/bh_agun.cpp"), "void CastSentrygun(void) {", "CreateSentrygun(&position, 0);"), "if (AvP.Network!=I_No_Network) {\n\t\tNewOnScreenMessage(\"NO SENTRYGUNS IN MULTIPLAYER MODE\");\n\t\treturn;"), "and a player can't place one in a network game");
	Check(!Has(pldnet, "AddNetMsg_PlayerAutoGunState"), "so nothing sends one");

	// Who may say what. These are the ones whoever hears them just believes.
	std::vector<std::string> const onlyTheHost = { "NetMT_PlayerAutoGunState", "NetMT_AlienAIState", "NetMT_AlienAISequenceChange", "NetMT_AlienAIKilled", "NetMT_SpotAlienSound", "NetMT_FarAlienPosition", "NetMT_GameDescription", "NetMT_StartGame", "NetMT_AllGameScores", "NetMT_PlayerScores", "NetMT_InanimateObjectDestroyed", "NetMT_PlatformLiftState", "NetMT_RestartNetworkGame", "NetMT_FragmentalObjectsStatus", "NetMT_LastManStanding_Restart", "NetMT_LastManStanding_RestartInfo", "NetMT_LastManStanding_RestartCountDown", "NetMT_LastManStanding_LastMan", "NetMT_PredatorTag_NewPredator", "NetMT_EndGame", "NetMT_RespawnPickups", "NetMT_ScoreChange", "NetMT_StrategySynch", "NetMT_SpeciesScores" };
	int rightOnes = 0;

	for (std::string const &name : onlyTheHost)
	{
		std::string::size_type const at = table.find("{ " + name + ", ");

		rightOnes += (at != std::string::npos && Has(table.substr(at, table.find('\n', at) - at), ", true }")) ? 1 : 0;
	}

	Check(rightOnes == 24, "24 kinds can only come from the host");
	Check(CountOf(table, ", true }") == 24, "and no others");

	// Numbers in a message that are used as places in memory. Whichever machine sent them, they could be anything.
	Check(Has(Between(pldnet, "static void ProcessNetMsg_PlayerScores(NETMESSAGE_PLAYERSCORES *messagePtr)\n{", "static void ProcessNetMsg_ScoreChange("), "if (!NetNumberFitsTable(playerId, NET_MAXPLAYERS))\n\t{\n\t\treturn;"), "a player's scores have to name a place in the list");
	Check(Has(Between(pldnet, "static void ProcessNetMsg_ScoreChange(NETMESSAGE_SCORECHANGE *messagePtr)\n{", "static void ProcessNetMsg_SpeciesScores("), "if (!NetNumberFitsTable(messagePtr->victimIndex, NET_MAXPLAYERS) || !NetNumberFitsTable(messagePtr->killerIndex, NET_MAXPLAYERS + 1))\n\t{\n\t\treturn;"), "and so does a change in them, with one past the end for a killer that's an alien the computer plays");
	Check(Has(pldnet, "if (killerIndex != NET_IDNOTINPLAYERLIST && NetNumberFitsTable(messagePtr->AlienType, 3)) {"), "a dead alien has to be a kind there is");
	Check(Has(pldnet, "if (myIndex != NET_IDNOTINPLAYERLIST && netGameData.playerData[myIndex].startFlag) {"), "and the end of a game doesn't look before the start of the list when this client isn't in it");
	Check(Has(pldnet, "if (!NetNumberFitsTable((int)messagePtr->DecalID, MAX_NO_OF_DECAL_IDS)) {\n\t\treturn;"), "a decal has to be one there is");
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
	Check(Has(pldnet, "NetCheckParts(message + kMessageHeaderSize, size - kMessageHeaderSize, kNetPartRules, sizeof(kNetPartRules) / sizeof(kNetPartRules[0]), fromTheHost, LengthOfANetPart))\n\t{\n\t\tcase kNetPartsGood:\n\t\t\treturn true;"), "the whole message is walked, from after its header");
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
	Check(Has(ReadSourceFile("3dc/networking.h"), "const int kMultiplayerVersion = 103;"), "and an older game can tell this one isn't what it speaks");
}

// Called last: every message from every test above has been looked at by now, and each of them had a stack under it
static void TestNoHeaderLeftAnything()
{
	Check(headersLookedAt > 20, "the tests sent plenty of messages");
	Check(headersWithSomethingInTheGap == 0, "and none had anything in the gap in its header");
}

int main()
{
	TestStartingTheGame();
	TestNobodyToSendTo();
	ListenToARealClient();
	TestDisconnectWithNoGame();
	TestLookingForGames();
	TestNothingOfTheStackGoesOut();
	TestASearchAndItsAnswers();
	TestWhenASocketCantBeHad();
	TestHosting();
	TestHostingWhenASocketCantBeHad();
	TestHostingWhenItCantBeDone();
	TestSkirmish();
	DirtyTheStack();
	TestJoining();
	TestJoiningAgainWhenItCantBeDone();
	TestJoinThatGetsNoAnswerThenHosting();
	TestHostingWhileStillInAGame();
	TestJoiningWhileAPageIsSearching();
	DirtyTheStack();
	TestARealClientGetsIn();
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
	TestPlayerIDs();
	TestTheMessageCheck();
	TestTheTableOfMessageParts();
	TestTheGameUsesThis();

	TestNoHeaderLeftAnything();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
