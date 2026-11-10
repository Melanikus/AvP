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

A number any build has ever used, a local test copy included, is never used again for a different protocol. A change takes
the next free number when it lands, not when it's written.
*/
const int kMultiplayerVersion = 114; // bjd - my version, not directplay compatible

// Added 2026-11-09 by Melanikus for the AvP port: PROTOCOL VERSION 2'S DRAFT, for tests only (Secondary's ruling, accepted by PO). Version 2
// (the relays' handshakes and sealed datagrams) is written but not finished, so a player's game never uses it: it's on only in a copy
// started with -dev -netv2, made by tools/build-and-package.ps1 (so it knows the commit it was made from). That copy speaks 115, and every
// other speaks kMultiplayerVersion, the players' number, exactly as before: with the draft off, nothing of version 2 runs at all. Two 115
// copies play only if they were made from the same commit, because drafts change what they send: the joiner puts its commit in ENet's
// CONNECT, and the host closes any other at once with kNetJoinRefusedOtherBuild.
int const kNetV2DraftVersion = 115;
extern bool NetV2DraftAsked;   // the command line had -netv2 (winmain.cpp). It counts only with -dev, in a build that knows its commit.
bool Net_V2DraftOn(void);      // whether this copy runs the draft: -dev, -netv2, and a build that knows its commit
int Net_OurVersion(void);      // kNetV2DraftVersion with the draft on, else kMultiplayerVersion: the version this copy says and asks for

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

// The joiner's round trip to each relay, for the games page's ping column (PROTOCOL.md: 3 echoes to each relay the list names, the lowest
// kept; a game's estimate is this and its hostMs). Net_TimeRelaysForJoining goes each frame the page is up, and starts once the list has
// named its relays; Net_ForgetRelayTimes, when the page opens, so they're timed again. Net_JoinerMsToRelay gives a region's time in ms, or
// -1 until it's timed, or if its relay didn't answer.
void Net_TimeRelaysForJoining();
void Net_ForgetRelayTimes();
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
unsigned int const kNetJoinRefusedOtherBuild = 0x4A424C44u; // "JBLD": a v2 draft made from another commit (added 2026-11-09 by Melanikus for the AvP port)

// The joiner's side: where its request has got to
int const kNetJoinAnswerNone = 0;       // no request: not joining, or not yet connected
int const kNetJoinAnswerWaiting = 1;
int const kNetJoinAnswerYes = 2;        // let in: the host gave it its player ID
int const kNetJoinAnswerNo = 3;
int const kNetJoinAnswerBusy = 4;
int const kNetJoinAnswerNoAnswer = 5;   // the host's limit passed, or the connection closed with no answer
int const kNetJoinAnswerOtherBuild = 6; // the host is a v2 draft made from another commit (JBLD), so the two can't play (added 2026-11-09 by Melanikus for the AvP port)
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
struct NetRelayHostState
{
	unsigned int changes;                     // goes up by one each time what changes, so the screen shows each change once
	int what;                                 // one of kNetRelayHost...
	char region[kNetRelayRegionLetters + 1];  // for Listed: the relay's region; "" otherwise
	int ms;                                   // for Listed: the host's round trip to that relay, from its echoes; 0 otherwise
	const char *why;                          // for RoomGone: why, in the game's own fixed words (NetRelay_WhyText); NULL otherwise
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
