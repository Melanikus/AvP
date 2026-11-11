// Copyright (C) 2010 Barry Duncan. All Rights Reserved.
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
//   notes that Net_Receive's buffer must hold NET_MESSAGEBUFFERSIZE bytes
// Modified 2026-11-06 by Melanikus for the AvP port:
//   the build now stops if a message changes while the multiplayer version does not: see net_version_check.h
//   the page of games found can stop the search, so that nothing stays open on the network
//   a message's header says whether it's a system message in a byte, because any number can arrive from another machine
//   a level load answers other machines all the way through
//   the multiplayer version is 103, and the build now stops if a message changes without it: see net_version_check.h
// Modified 2026-11-07 by Melanikus for the AvP port:
//   the list of versions has 105 and 106, which raised the number without a line
//   Net_DescribeConnections, each connection's number and round trip for a tester's mark
// Modified 2026-11-08 by Melanikus for the AvP port:
//   the multiplayer version is 113: a player's state says whether his shoulder lamp is on. The message keeps its size, so only the number tells
//   an older build it can't read that bit
//   Net_NoteFrame and Net_NoteFigureWait, for the report's lines on frames and figures

#ifndef _NETWORKING_H_
#define _NETWORKING_H_

#include "module.h"
#include "strategy_def.h"
#include "equipment.h"
#include "pldnet.h"

enum NetResult
{
	NET_OK,
	NET_FAIL,
	NET_NO_MESSAGES,
	NET_ERR_BUSY,
	NET_ERR_CONNECTIONLOST,
	NET_ERR_INVALIDPARAMS,
	NET_ERR_INVALIDPLAYER,
	NET_ERR_NOTLOGGEDIN,
	NET_ERR_SENDTOOBIG,
	NET_ERR_BUFFERTOOSMALL
};

NetResult Net_Initialise();
void      Net_Deinitialise();
void      Net_Disconnect();
NetID     Net_HostPlayerID();     // who the host is, as far as this machine has been told; 0 before that, and on the host itself
void      Net_ConnectToAddress();
void      Net_FindAvPSessions();
void      Net_StopSearchingForSessions();
int       Net_ConnectingToSession();
NetID     Net_GetNextPlayerID();
NetResult Net_Send(uint32_t messageType, NetID fromID, NetID toID, uint8_t *messageData, size_t dataSize);
NetResult Net_Receive(uint8_t *messageData, size_t &dataSize); // messageData must hold kMostMessageBytes bytes. Only game messages come out of this: see WHO IS WHO in networking.cpp
NetResult Net_SendSystemMessage(uint32_t messageType, NetID fromID, NetID toID, const void *messageData, size_t dataSize);
int       Net_InitLobbiedGame();
void      Net_ServiceNetwork();
bool      Net_GameGoesOnWhileInactive(void);   // a network game is going, so it goes on behind another window (win_func.cpp)

// Called all the way through a level load. It answers the other machines, and leaves what they sent waiting for when the level is ready.
void      Net_KeepAnsweringWhileLoading();

// At the end of a load: says in the log how long the longest gap between answers was, because that's what the patience below has to be longer than.
void      Net_SayLongestGapWhileLoading();

uint32_t  Net_JoinGame();
NetResult Net_ConnectToSession(int sessionNumber, char *playerName);
NetResult Net_ConnectToThisMachine(int level, char *playerName);   // a Hudson run with net= only
NetResult Net_HostGame(char *playerName, char *sessionName, int species, uint16_t gameStyle, uint16_t level);

// SEALED OR NOTHING (#188). Added 2026-11-10 by Melanikus for the AvP port, for the owner's boundary (2026-11-09 night): beyond the home
// network a game is sealed, through the relay, or it doesn't happen; a plain game, as LAN always was, is only ever between addresses on the
// home network (net_checks.h's NetAddress_IsThisNetwork: 10/8, 172.16/12, 192.168/16, 127/8 and 169.254/16). For every game (#175 PR 3a).
//   - Net_HostGame, with a server in servers.cfg (Relay_ServerName, relay_list.h), hosts online: through that server's relay, sealed, or
//     not at all. It never opens on this network instead, and never opens the search port. With no server, it's a home game, as below.
//   - Net_HostAtHome hosts a home game, whatever servers.cfg says: plain, found by a search on this network, and deaf to every address
//     that isn't on the home network, so a forwarded router port can't bring in a joiner from the internet.
//   - Net_HostFailedWhy: why the last of the two gave NET_FAIL for one of online play's reasons, in the game's own fixed words. NULL if it
//     didn't fail, or failed for one of the game's older reasons (networking not started, the session or player not made), which the
//     screen says in its own words. When it's only that the server hasn't answered yet, Net_ServerState says kNetServerAsking: the screen
//     waits for it, then hosts again.
NetResult Net_HostAtHome(char *playerName, char *sessionName, int species, uint16_t gameStyle, uint16_t level);
const char *Net_HostFailedWhy(void);

// Where the online server stands, for a host's screen before it hosts, and while it waits. It only reads, so a screen can ask every
// frame. Net_HostGame asks the server when it hasn't been asked; so does the games page (RelayList_FindRelays).
int const kNetServerNone = 0;      // no server in servers.cfg: a host's game is a home game
int const kNetServerAsking = 1;    // the server is being asked, or hasn't been yet, and hasn't answered: wait, then host
int const kNetServerReady = 2;     // the server answered, and it's the one its line names: a host goes online through it
int const kNetServerCantUse = 3;   // the server can't be used just now: why says why (it couldn't be reached, said no, or its keys don't
                                   // match its line), in the game's own fixed words
int Net_ServerState(const char **why);

// Why the last Net_ConnectToSession gave NET_FAIL before it opened anything: a game on this network whose host isn't on the home network
// (from a typed address, or a search's answer). The game's own fixed words, or NULL if it wasn't refused that way.
const char *Net_JoinRefusedWhy(void);

// Whether the last search left the typed address unasked, as it isn't on the home network: the words for the games page, or NULL if it
// was asked or there was none
const char *Net_TypedAddressRefusedWhy(void);

// What a search for games is told about this one, for a game from the co-op campaign page, whose level changes as the host changes its rows.
void Net_SayWhatTheGameIs(uint16_t level, const char *levelName, const char *sessionName, uint8_t coopLobby);

// A host takes this player out of the game, and their address can't come back until the game closes. See coop_lobby.h.
void Net_RemoveThisPlayer(NetID id);

// Was this machine told, as it was let go, that the host had removed it?
bool Net_WasRemovedByTheHost();
int       Net_ConnectingToLobbiedGame(char *playerName);

// For a tester's mark (mark_key.cpp): each connection this machine has, by number, with its round trip, such as
// "connection 1 at 45 ms, connection 2 at 120 ms". Never an address. words has room for size characters and always ends with a 0;
// it's empty out of a network game.
void      Net_DescribeConnections(char *words, size_t size);

// For the 10-second report's two lines on frames and figures (net_watch.h). pldnet.cpp tells it each frame's time, in the game's own frame
// time (65536 to a second), and pldghost.cpp tells it each time a figure waited more than 100 ms for news, with how far it then moved.
void      Net_NoteFrame(int frameTime);
void      Net_NoteFigureWait(unsigned int waitedMs, unsigned int jumpMm, bool aPlayer);

extern NetID AvPNetID;

static const int kMarker = 'xPvA'; // AvPx

struct MessageHeader
{
	int marker;
	uint8_t isSystemMessage; // 1 for a system message. A byte rather than a bool, because any number can arrive here from another machine
	uint32_t messageType;
	NetID fromID;
	NetID toID;
};

/*
Version 0 - Original multiplayer + save patch
Version 100 - Added pistol,skeeter (and new levels)
Version 102 - A host answers each client itself, and the game description has the players' names
Version 103 - Two builds made apart from each other both used 103, for different things:
              on the co-op branch, the mode that plays like single player: its own messages and level lists;
              on main, every packet has a checksum, and ENet refuses the kinds of message the game never sends
Version 104 - The host says what each NPC is doing, for that mode (the co-op branch)
Version 105 - An NPC's voice is heard on every machine, for that mode (the co-op branch)
Version 106 - The host says where each platform lift's car is, for that mode (the co-op branch)
Version 107 - Used by builds of the co-op lobby, before it landed. Not this code, and never to be used again
Version 108 - Used by builds of the in-game pages, before they landed. Not this code, and never to be used again
Version 109 - Used by rehearsal builds of the co-op landing, before it landed: the co-op branch with main's network fixes. Never to be used again
Version 110 - 109, and a search's question as big as its answer, so an answer is never bigger than its question
Version 111 - 110, and the co-op campaign lobby: the host's choice, a joiner's answer about the map, and a byte in a game's description
Version 112 - 111, and the in-game co-op pages: who the host is waiting for, in a game's description
Version 113 - 112, and whether a marine player's shoulder lamp is on, in a spare bit after landingNoise in a player's state, for the mode that plays like single player
Version 114 - 113, and a joiner asks the host first: its request to join (its "let me in", with its name) is held until the host answers yes, or closes it with no, busy or no answer
Version 115 - a -dev v2 draft (-dev -netv2), for good: two 115 copies play only if they're the same build (JBLD); never a players' number. Players' v2 will take the next free number (116 or later).
Version 116 - version 2 for every game (#175 PR 3a): through a relay, only its sealed channel, handshakes A and B and sealed datagrams, the listing carrying the host's 48-byte id; no plain game beyond the home network; no same-commit word in the CONNECT

A number any build has ever used, a local test copy included, is never used again for a different protocol. A change takes
the next free number when it lands, not when it's written.
*/
const int kMultiplayerVersion = 116; // bjd - my version, not directplay compatible

// Modified 2026-11-10 by Melanikus for the AvP port (#175 PR 3a): protocol version 2's draft, which only -dev -netv2 turned on and which spoke
// 115, is now every game's protocol, at 116 (above). Its switch (Net_V2DraftOn, NetV2DraftAsked, -netv2) is gone, and with it beta.1's
// version 1 relay path and the draft's same-commit rule (JBLD): the version is what two builds must share.

struct MessageHeader; // forward declare the structure
extern const uint32_t kMessageHeaderSize;

// The game fills a buffer of NET_MESSAGEBUFFERSIZE, and the header goes in front of that, so this is the most a message can be.
const size_t kMostMessageBytes = NET_MESSAGEBUFFERSIZE + sizeof(MessageHeader);

enum NetMessageTypes
{
	NET_SYSTEM_MESSAGE,
	NET_GAME_MESSAGE
};

enum NetSystemMessages
{
	NETSYS_REQUEST_SESSION_INFO,
	NETSYS_SESSION_INFO,
	NET_CREATEPLAYERORGROUP,

	NET_SETPLAYERID,

	NET_DESTROYPLAYERORGROUP,
	NET_ADDPLAYERTOGROUP,
	NET_DELETEPLAYERFROMGROUP,
	NET_SESSIONLOST,
	NET_HOST,
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
	NET_PLAYERTYPE_PLAYER,
//	NET_RECEIVE_ALL,
	NET_ID_ALLPLAYERS
//	NET_ID_SERVERPLAYER,
};

const int kPlayerNameSize  = 40;
const int kLevelNameSize   = 40;
const int kSessionNameSize = 40;

#pragma pack(1)

struct SessionDescription
{
	GUID      guidInstance;
	GUID      guidApplication;
	uint32_t  host;
	uint16_t  port;
	uint32_t  version;
	uint16_t  gameStyle;
	uint16_t  level;
	uint8_t   nPlayers;
	uint8_t   maxPlayers;
	char      levelName[kLevelNameSize];
	char      sessionName[kSessionNameSize];
	uint8_t   coopLobby;   // kCoopLobbyNone, Open or UnderWay: see coop_lobby.h
};

struct PlayerDetails
{
	NetID	ID;
	uint8_t	type;
	char	name[kPlayerNameSize];
	int		version;   // kMultiplayerVersion: a host won't let in a game that doesn't speak what it speaks
};

#pragma pack()

extern PlayerDetails thisClientPlayer;

// Modified 2026-11-08 by Melanikus for the AvP port: why the last join through the relay failed, in words for the menu's page. Always one of
// the game's own fixed sentences (NetRelay_WhyText, net_relay.h), never text from the network.
const char *Net_RelayJoinFailedWhy();

// Modified 2026-11-08 by Melanikus for the AvP port: the join through a relay waits up to about 26 s, in 50 ms slices. The hook is asked once
// each slice. It keeps the game's window answering Windows, so it isn't marked "Not responding", and gives back false when the player has
// given up (Esc): the join stops at once, and Net_RelayJoinFailedWhy says "the join was given up". The menus set it (Fourth's cold read,
// finding 2). With none set, the wait goes on to its limit.
typedef bool (*NetRelayJoinWaitHook)(void);
void Net_SetRelayJoinWaitHook(NetRelayJoinWaitHook hook);

// Where a join through a relay has got to, for the menu's wait hook to say (Fourth's online screens): reaching the relay (asking it, and
// its cookie round), or the relay introducing the two ends (their hellos, until READY). None when no join is under way.
int const kNetRelayJoinStepNone = 0;
int const kNetRelayJoinStepReaching = 1;
int const kNetRelayJoinStepIntroducing = 2;
int Net_RelayJoinStep();

// The joiner's round trip to its relay, for the ping column and the line in play: in ms, from the channel's own timing once a join has made
// it, for that relay's region; -1 before, or for any other region (modified 2026-11-10 by Melanikus for the AvP port, #175 PR 3a: version
// 1's timing of every relay with plain echoes, before a join, went with version 1). Only reads.
int Net_JoinerMsToRelay(const char *region);

// A game from the relays' list (relay_list_text.h's RelayListGame) goes on the list of sessions, to be joined with Net_ConnectToSession like
// one found on this network. False if it isn't one this game can join, or it's on the list already, or the list is full (MAX_NO_OF_SESSIONS):
// nothing is ever written past it. Only on the game's main thread, like the list of sessions.
struct RelayListGame;
bool Net_AddRelaySession(const RelayListGame &game);

// Where a game from the relays' list is on the list of sessions, by its region and its code's 8 letters; -1 if it isn't there
int Net_FindRelaySession(const char *region, const char *code);

// THE REQUEST TO JOIN (version 114; networking.cpp says how it works). Every joiner asks the host first, and a person at the host answers.
// The host's side: the oldest request waiting, if there is one, with the joiner's name (as the joiner gave it: hostile, so the screen makes it
// safe to draw) and who it is; and the answer, which does nothing if that request has gone. A test's host lets everyone in without asking.
uint32_t const kNetJoinAnswerWithinMs = 30000;   // unanswered this long, a request counts as no answer
struct NetJoinWho
{
	int connection;
	unsigned int connectID;
};
bool Net_JoinRequestWaiting(char *nameOut, size_t nameRoom, NetJoinWho *who);
void Net_AnswerJoinRequest(NetJoinWho who, bool yes);
void Net_LetEveryJoinerIn(bool yes);

// Why a host closed a joiner's connection before letting it in: ENet's disconnect carries one of these, as kCoopRemovedByTheHost (coop_lobby.h)
unsigned int const kNetJoinRefusedNo = 0x4A4E4F2Eu;         // "JNO.": the host said no
unsigned int const kNetJoinRefusedBusy = 0x4A425359u;       // "JBSY": the host was answering another joiner
unsigned int const kNetJoinRefusedNoAnswer = 0x4A4E4141u;   // "JNAA": nobody answered in time

// The joiner's side: where its request has got to
int const kNetJoinAnswerNone = 0;       // no request: not joining, or not yet connected
int const kNetJoinAnswerWaiting = 1;
int const kNetJoinAnswerYes = 2;        // let in: the host gave it its player ID
int const kNetJoinAnswerNo = 3;
int const kNetJoinAnswerBusy = 4;
int const kNetJoinAnswerNoAnswer = 5;   // the host's limit passed, or the connection closed with no answer
int Net_JoinAnswerNow();

// THE CHECK CODE. Added 2026-11-09 by Melanikus for the AvP port (protocol version 2, CRYPTO.md section 8: "always shown on the join
// screen"). Through a relay, the joiner and its host each work out the same 6 digits from their handshake. The two players read them to
// each other: if they match, nobody sat in the middle. Both calls only read, so a screen can ask every frame.
//   - Net_RelayCheckCodeNow: a joiner's own code, once its handshake with the host is done. False, with "" written, before, after, or
//     in a game that didn't come through a relay.
//   - Net_JoinRequestCheckCode: the host's code for the joiner whose request to join waits (who, as Net_JoinRequestWaiting gave it).
//     False, with "" written, for a joiner on this network (it has no handshake), or a request that's gone or somebody else's.
// out has room for the 6 digits and the 0 that ends them.
size_t const kNetRelayCheckCodeBytes = 7;
bool Net_RelayCheckCodeNow(char out[kNetRelayCheckCodeBytes]);
bool Net_JoinRequestCheckCode(NetJoinWho who, char out[kNetRelayCheckCodeBytes]);

// THE RELAY ON THE SCREEN. Added 2026-11-09 by Melanikus for the AvP port, for Fourth's online screens: the host's news about its relay,
// and the line in play saying which relay the game goes through and how far away it is. Both are read only: asking changes nothing, so a
// screen can ask every frame. A region is the list's text, so the screen makes it safe (OnlineScreens_SafeName) before drawing it.
size_t const kNetRelayRegionLetters = 16;   // relay_list_text.h's kRelayListRegionLetters (networking.cpp checks they're the same)

// What a host going through a relay is doing, for a line on the host's screen each time it changes
int const kNetRelayHostNone = 0;       // not hosting through a relay
int const kNetRelayHostFinding = 1;    // timing the relays the list named, to open the game's room on the nearest
int const kNetRelayHostListed = 2;     // the room is open, and the game is going on the list: region and ms say through which relay, and how far
int const kNetRelayHostNoRelay = 3;    // no relay answered, or none could be used: the game is open on this network only
int const kNetRelayHostRoomGone = 4;   // the room on the relay has gone (why says why), so the game is open on this network instead
// Added 2026-11-10 by Melanikus for the AvP port (#188): an online game that loses its relay can't go on sealed, so it has ended (why says
// why). Nothing reaches the host any more, and nothing on this network can find or join it: the screen ends the game.
int const kNetRelayHostEnded = 5;
struct NetRelayHostState
{
	unsigned int changes;                     // goes up by one each time what changes, so the screen shows each change once
	int what;                                 // one of kNetRelayHost...
	char region[kNetRelayRegionLetters + 1];  // for Listed: the relay's region; "" otherwise
	int ms;                                   // for Listed: the host's round trip to that relay, from its echoes; 0 otherwise
	const char *why;                          // for RoomGone and Ended: why, in the game's own fixed words (NetRelay_WhyText); NULL otherwise
};
void Net_RelayHostStateNow(NetRelayHostState *state);

// The relay this game is playing through now, and how far away it is, for the line kept on the screen in play
struct NetRelayPlayState
{
	bool throughRelay;                        // false: not playing through a relay, so no line, and the rest is empty
	bool isHost;
	char region[kNetRelayRegionLetters + 1];  // the relay's region
	int ms;   // -1 while nothing is measured. The host: its round trip to its relay, from the last keep and its answer (one every 15 s), or
	          // from its echoes until the first. A joiner: ENet's round trip to the host, which goes through the relay, so it's what the player
	          // feels. That's -1 until ENet has timed its first acknowledgement, so ENet's starting guess (500 ms) is never shown.
};
void Net_RelayPlayStateNow(NetRelayPlayState *state);

#endif // #ifndef _NETWORKING_
