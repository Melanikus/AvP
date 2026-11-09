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

// Called all the way through a level load. It answers the other machines, and leaves what they sent waiting for when the level is ready.
void      Net_KeepAnsweringWhileLoading();

// At the end of a load: says in the log how long the longest gap between answers was, because that's what the patience below has to be longer than.
void      Net_SayLongestGapWhileLoading();

uint32_t  Net_JoinGame();
NetResult Net_ConnectToSession(int sessionNumber, char *playerName);
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

A number any build has ever used, a local test copy included, is never used again for a different protocol. A change takes
the next free number when it lands, not when it's written.
*/
const int kMultiplayerVersion = 113; // bjd - my version, not directplay compatible

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

#endif // #ifndef _NETWORKING_
