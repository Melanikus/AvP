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
//   a connection that never says who it is is dropped, answers to anyone asking about a game are limited, a second game found in one look is read properly, and a joiner must speak this version
//   a system message with no server to send it to fails, and leaving a game forgets the server
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

#include "3dc.h"
#include "enet\enet.h"
#include "console.h"
#include "configFile.h"
#include "logString.h"
#include "menus.h"
#include "mp_config.h"
#include "networking.h"
#include "net_checks.h"
#include "net_host.h"
#include <assert.h>
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

// How long a connection has to say who it is. ENet's own handshake has finished by then, so a real client only takes a moment. Without this, seven silent connections would fill the game for good.
static const unsigned int kMostMillisecondsToBeLetIn = 15000;

// When each of the host's connections was made
static unsigned int connectionStartedAt[kMostClients + 1];

// A real client answers ENet's first reply straight away. Someone starting connections from addresses that aren't theirs never does, and each one holds a place until this runs out. ENet's own wait is much longer.
static const unsigned int kMostMillisecondsHalfOpen = 3000;
static unsigned int halfOpenSince[kMostClients + 1];

// A player sends something many times a second, even waiting in a menu, and a level loads in much less than this. So a player silent for this long has gone, and is asked to go.
static const unsigned int kMostMillisecondsSilent = 300000;
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

extern void MinimalNetCollectMessages(void);
extern void InitAVPNetGameForHost(int species, int gamestyle, int level);
extern void InitAVPNetGameForJoin(void);
extern int DetermineAvailableCharacterTypes(bool ConsiderUsedCharacters);
extern char *GetCustomMultiplayerLevelName(int index, int gameType);
extern bool AddPlayerToGame(NetID id, const char *name);
extern void Net_PlayerHasGone(NetID id);
extern void Net_HostHasGone();
extern bool Net_GameMessageIsGood(const uint8_t *message, size_t size, bool fromTheHost);
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

	// bounded: a long session name used to overflow Name (40 bytes) into host/port
	_snprintf(name, nameSize - 1, "%s (%u/%u)", session.sessionName, session.nPlayers, session.maxPlayers);
	name[nameSize - 1] = '\0';

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
		if (local_index < 0)
		{
			// we don't have the level, so ignore this session
			return;
		}

		SessionData[NumberOfSessionsFound].levelIndex = local_index;
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

			// hack - I think we'll receive our own broadcast?
			if (len == sizeof(MessageHeader)) {
				OutputDebugString("We received our request back\n");
			}

			else if (len == sizeof(header) + sizeof(SessionDescription)) 
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

NetResult Net_Initialise()
{
	Con_PrintMessage("Initialising ENet networking");

	if (enet_initialize() != 0)
	{
		Con_PrintError("Failed to initialise ENet networking");
		return NET_FAIL;
	}

	// get user config data
	netPortNumber = Config_GetInt("[Networking]", "PortNumber", netPortNumber);
//	broadcastPort = Config_GetInt("[Networking]", "BroadcastPort", broadcastPort);
	incomingBandwidth = Config_GetInt("[Networking]", "IncomingBandwidth", 0);
	outgoingBandwidth = Config_GetInt("[Networking]", "OutgoingBandwidth", 0);

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
			OpenDiscoverySocket();

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

void Net_Disconnect()
{
	OutputDebugString("Net_Disconnect\n");

	// This also runs from the console's disconnect command, in a game that isn't on any network
	if (host)
	{
		// What's been sent so far goes now, because closing the connections throws away anything still waiting
		enet_host_flush(host);

		// Tell whoever's at the other end straight away, instead of leaving them to work it out when nothing arrives for a while. Nobody has to believe a message saying who has left, because it's the connection itself that closes.
		for (ENetPeer *peer = host->peers; peer < &host->peers[host->peerCount]; ++peer)
		{
			if (peer->state == ENET_PEER_STATE_CONNECTED)
			{
				enet_peer_disconnect_now(peer, 0);
			}
		}

		enet_host_destroy(host);
		host = NULL;
	}

	// The ENet host owned this, and has just freed it
	ServerPeer = NULL;

	sessionCheckTimer = 0;
	CloseDiscoverySocket();
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
		}
		if (len == sizeof(MessageHeader))
		{
			MessageHeader header;
			memcpy(&header, packetBuffer, sizeof(MessageHeader));

			// Anyone can send this, from an address that isn't theirs, and the answer is bigger than the question. So it has to look like ours, and this game only answers so many.
			static unsigned int secondStarted = 0;
			static int answersThisSecond = 0;

			if (header.marker != kMarker)
			{
				continue;
			}

			if (NETSYS_REQUEST_SESSION_INFO == header.messageType && NetAddressHasRoom(sessionAskers, sizeof(sessionAskers) / sizeof(sessionAskers[0]), addr.host, enet_time_get(), kMostSessionAnswersToOneAddressEachSecond) && NetThisSecondHasRoom(enet_time_get(), &secondStarted, &answersThisSecond, kMostSessionAnswersEachSecond))
			{
				MemoryWriteStream ws(packetBuffer, kPacketBufferSize);

				header.marker = kMarker;
				header.isSystemMessage = true;
				header.messageType = NETSYS_SESSION_INFO;
				ws.PutBytes(&header, sizeof(header));
				ws.PutBytes(&netSession, sizeof(SessionDescription));

				buf.dataLength = ws.GetBytesWritten();
				buf.data = packetBuffer;

					int ret = enet_socket_send(broadcastSocket, &addr, &buf, 1);
					if (0 == ret) {
						// blocking - wait 1 second then try again (for a maximum of 5 seconds)
//						Sleep(1000);
						return;
					}
					else if (0 > ret) {
						Con_PrintError("Couldn't send session details");
						return;
					}
					else {
						Con_PrintDebugMessage("Sent session details");
						return;
					}
//				}
			}
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

static void ForgetConnectionsNotLetIn()
{
	unsigned int const now = enet_time_get();

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

		if (peer->state == ENET_PEER_STATE_CONNECTED && peer->data != NULL && now - lastHeardFrom[i] >= kMostMillisecondsSilent)
		{
			// The ordinary way out, so that the game hears this player has gone
			LogDroppedMessage("a player has sent nothing for minutes");
			lastHeardFrom[i] = now;
			enet_peer_disconnect(peer, 0);
			continue;
		}

		if (peer->state == ENET_PEER_STATE_CONNECTED && peer->data == NULL && now - connectionStartedAt[i] >= kMostMillisecondsToBeLetIn)
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

		// also check for session data requests
		if (AvP.Network == I_Host) {
			Net_CheckForSessionRequest();
			ForgetConnectionsNotLetIn();
		}
	}
}

// WHO IS WHO. A client only ever talks to the host, and the host passes on what's meant for the others. Anyone who can reach the host's port can connect and send anything at all, and a message can say it's from anyone. So the host doesn't go by that.
// Each connection can be let in once, and gets one player ID, which is kept with the connection. After that, whatever arrives on the connection has to carry that ID. What arrives is checked here, before the rest of the game sees it, and the
// only messages the rest of the game ever sees are game messages, whole ones.
// On a client, who the host is: whoever gave it its ID. A host passes on what the others say, each saying who it's from, and only the host's own word counts as the host's.
static NetID hostPlayerID = 0;

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
			LetClientIn(from, data + sizeof(MessageHeader));
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

	if (connection <= kMostClients)
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

					char ipaddr[32];
					enet_address_get_host_ip(&eEvent.peer->address, ipaddr, sizeof(ipaddr));

					std::stringstream sstream;
					sstream << "Enet got a connection from " << ipaddr << ":" << eEvent.peer->address.port;
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

	// A game that has already left has nobody to send to
	if (!host)
	{
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

	ENetBuffer buf;
	buf.data = &newHeader;
	buf.dataLength = sizeof(MessageHeader);

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

NetResult Net_UpdateSessionDescForLobbiedGame(uint16_t gameStyle, uint16_t level)
{
	return NET_OK;
}
