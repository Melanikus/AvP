// Tests for two things in 3dc/networking.cpp: what the game has open on the network, and what it does with a message it has nobody to send to. Written 2026-11-06 by Melanikus for the AvP port.
//
// This runs the real file. Only ENet and the rest of the game are stand-ins, in tests/net_start_test_stubs, and they open nothing: they count what was asked for, and the clock is the test's.
//
// WHY THESE TWO: only a client has a server to send to, so anything else asking to send has to be told no. And a game that isn't doing anything on a network has no reason to have anything open on one.
//
// Modified 2026-11-07 by Melanikus for the AvP port: and how it leaves. What's waiting goes first and each connection is closed after
// it, while the ENet host is still there (3dc/net_close.h), and a client says in the log when the host's connection closes.

#include "enet/enet.h"
#include "networking.h"

#include <fstream>
#include <vector>

// menus.cpp says this for itself, and it isn't in networking.h
extern bool Net_UpdateSessionList(int &SelectedItem);

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

static int hostsCreated = 0, hostsDestroyed = 0;
static std::vector<ENetAddress> hostsBoundTo;
static bool connectGetsAnAnswer = true;

static ENetHost theHost;
static ENetPeer thePeer;
static ENetPacket thePacket;
static bool peerIsLive = false;
static int packetsMade = 0, packetsToAPeer = 0, packetsToAPeerThatsGone = 0;
static uint32_t lastMessageSent = 0xFFFFFFFF;

int enet_initialize()
{
	return 0;
}

void enet_deinitialize()
{
}

ENetHost *enet_host_create(const ENetAddress *address, size_t, size_t, enet_uint32, enet_uint32)
{
	if (hostCreateFails)
	{
		return NULL;
	}

	hostsCreated++;
	hostsBoundTo.push_back(*address);
	return &theHost;
}

void enet_host_destroy(ENetHost *)
{
	hostsDestroyed++;

	// A host owns its peers, so this one is gone. The real one's memory is freed here, so nothing should touch it again.
	peerIsLive = false;
	thePeer.state = ENET_PEER_STATE_DISCONNECTED;
}

ENetPeer *enet_host_connect(ENetHost *, const ENetAddress *, size_t, enet_uint32)
{
	peerIsLive = true;
	thePeer.state = ENET_PEER_STATE_CONNECTED;
	return &thePeer;
}

int enet_host_service(ENetHost *, ENetEvent *event, enet_uint32)
{
	if (event && connectGetsAnAnswer)
	{
		event->type = ENET_EVENT_TYPE_CONNECT;
		event->peer = &thePeer;
		return 1;
	}

	return 0;
}

// The next look at what has happened finds that the connection to thePeer has closed
static bool aConnectionCloses = false;

int enet_host_check_events(ENetHost *, ENetEvent *event)
{
	if (event && aConnectionCloses)
	{
		aConnectionCloses = false;
		event->type = ENET_EVENT_TYPE_DISCONNECT;
		event->peer = &thePeer;
		event->packet = NULL;
		return 1;
	}

	return 0;
}

void enet_host_broadcast(ENetHost *, uint8_t, ENetPacket *)
{
}

void enet_host_flush(ENetHost *)
{
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

	return &thePacket;
}

void enet_packet_destroy(ENetPacket *)
{
}

int enet_peer_send(ENetPeer *, uint8_t, ENetPacket *)
{
	packetsToAPeer++;
	packetsToAPeerThatsGone += peerIsLive ? 0 : 1;
	return 0;
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

	// What a host does when it has something to tell a client: see NET_CREATEPLAYERORGROUP in pldnet.cpp
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

	int const hostClosesBefore = closesAsked;

	Net_Disconnect();
	Check(socketsOpen == 0, "the game ending closes it");
	Check(closesAsked == hostClosesBefore + 1 && hostsDestroyedWhenClosed == hostsDestroyed - 1, "and a host closes its connections after what's waiting, before letting its ENet host go");
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

	int const closesBefore = closesAsked;

	lastMessageSent = 0xFFFFFFFF;
	Net_Disconnect();
	Check(lastMessageSent == NET_DESTROYPLAYERORGROUP && packetsToAPeer == sentBeforeLeaving + 1, "leaving says so");
	Check(hostsDestroyed == hostsCreated, "and lets its ENet host go");
	Check(closesAsked == closesBefore + 1 && hostsDestroyedWhenClosed == hostsDestroyed - 1, "and first closes its connection after what's waiting, its goodbye among it");
	Check(Net_SendSystemMessage(NET_DESTROYPLAYERORGROUP, 0, 0, &id, sizeof(id)) == NET_FAIL, "and after that there's nobody to send to");
	Check(packetsToAPeerThatsGone == goneBefore, "so nothing is sent to it");

	int const sentNow = packetsToAPeer;
	int const destroyed = hostsDestroyed;

	Net_Disconnect();
	Check(packetsToAPeer == sentNow && hostsDestroyed == destroyed, "and leaving again does nothing");
	Check(closesAsked == closesBefore + 1, "and has nothing left to close");
	AvP.Network = I_No_Network;
}

static void TestLeavingWhenTheGoodbyeCantBeSent()
{
	Check(JoinAGame() == NET_OK, "joining again");

	int const destroyed = hostsDestroyed;

	packetCreateFails = true;
	Net_Disconnect();
	packetCreateFails = false;
	Check(hostsDestroyed == destroyed + 1, "a goodbye that can't be sent doesn't stop it leaving");

	int const id = 1;

	Check(Net_SendSystemMessage(NET_DESTROYPLAYERORGROUP, 0, 0, &id, sizeof(id)) == NET_FAIL, "and it's still forgotten the server");
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

	int const id = 1234;
	int const sent = packetsToAPeer;

	Check(HostAGame() == NET_OK, "and hosting straight after works");
	Check(hostsDestroyed == hostsCreated - 1, "and lets the failed join's ENet host go, rather than losing it");
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
	aConnectionCloses = true;
	Net_Receive(message, size);
	Check(LogHas("net: the host's connection closed"), "a client says when the host's connection closes");
	EndTheGame();

	Check(HostAGame() == NET_OK, "hosting, for a client's connection to close");
	logLines.clear();
	aConnectionCloses = true;
	Net_Receive(message, size);
	Check(!LogHas("net: the host's connection closed"), "and a host doesn't say that when a client's connection closes");
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
	Check(Has(pldnet, "Net_SendSystemMessage(NET_SETPLAYERID, AvPNetID, 0, &newPlayer.ID, sizeof(newPlayer.ID));"), "the host's reply goes through the function tested above");
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
	TestLeavingWhenTheGoodbyeCantBeSent();
	TestJoiningAgainWhenItCantBeDone();
	TestJoinThatGetsNoAnswerThenHosting();
	TestJoiningWhileAPageIsSearching();
	TestLeavingWithoutAnAnswer();
	TestTheHostsConnectionClosing();
	TestTheGameUsesThis();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
