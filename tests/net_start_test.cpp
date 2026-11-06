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

int enet_host_service(ENetHost *, ENetEvent *event, enet_uint32)
{
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

int enet_socket_wait(ENetSocket socket, enet_uint32 *condition, enet_uint32)
{
	if (socket == ENET_SOCKET_NULL)
	{
		waitsOnNoSocket++;
		return -1;
	}

	waitsOnASocket++;
	*condition = 0;
	return 0;
}

int enet_socket_receive(ENetSocket, ENetAddress *, ENetBuffer *, size_t)
{
	return 0;
}

int enet_socket_send(ENetSocket, const ENetAddress *address, const ENetBuffer *, size_t)
{
	if (datagramSendFails)
	{
		return -1;
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

static char playerName[] = "Hudson";
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
	Check(socketsCreated == created + 3 && datagramsSent == sent + 3, "and the next search doesn't think one is still going on");

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

static std::vector<uint8_t> AskingToJoin(const char *name)
{
	PlayerDetails details;

	memset(&details, 0, sizeof(details));
	strncpy(details.name, name, sizeof(details.name) - 1);
	details.type = NET_PLAYERTYPE_PLAYER;

	return AMessage(1, NET_CREATEPLAYERORGROUP, 0, &details, sizeof(details));
}

static std::vector<uint8_t> AGameMessage(NetID from)
{
	char const words[] = "something for the game";

	return AMessage(0, NET_ID_ALLPLAYERS, from, words, sizeof(words));
}

// The game's buffer holds NET_MESSAGEBUFFERSIZE. The rest is so that a test can see something too big without it doing any harm.
static uint8_t received[NET_MESSAGEBUFFERSIZE + 64];

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

	tooBig.resize(NET_MESSAGEBUFFERSIZE + 1);
	refused(1, tooBig, "a message too big for the game's buffer is no message");

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

static void TestANameWithNoEnd()
{
	HostWithClients(0);

	PlayerDetails details;

	memset(&details, 'A', sizeof(details));
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
	Check(Has(ReadSourceFile("3dc/networking.h"), "const int kMultiplayerVersion = 102;"), "and an older game can tell this one isn't what it speaks");
}

int main()
{
	TestStartingTheGame();
	TestNobodyToSendTo();
	TestDisconnectWithNoGame();
	TestLookingForGames();
	TestWhenASocketCantBeHad();
	TestHosting();
	TestHostingWhenASocketCantBeHad();
	TestHostingWhenItCantBeDone();
	TestSkirmish();
	TestJoining();
	TestJoiningAgainWhenItCantBeDone();
	TestJoinThatGetsNoAnswerThenHosting();
	TestHostingWhileStillInAGame();
	TestJoiningWhileAPageIsSearching();
	TestLettingPeopleIn();
	TestWhatAClientCantSay();
	TestANameWithNoEnd();
	TestPassingThingsOn();
	TestNoRoom();
	TestSomeoneGoing();
	TestTheClientsSide();
	TestWaitingToJoin();
	TestLeavingClosesConnections();
	TestPlayerIDs();
	TestTheGameUsesThis();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
