// What 3dc/networking.cpp needs from the rest of the game, so that tests/net_start_test.cpp can run the real file with nothing else. Written 2026-11-06 by Melanikus for the AvP port.
// tests/net_start_test.args puts this at the top of every file it compiles, and it sets the markers the game's own console.h, ConfigFile.h and logString.h look for, so that those stay out.

#ifndef NET_START_STAND_INS_H
#define NET_START_STAND_INS_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sstream>
#include <string>

// Modified 2026-11-09 by Melanikus for the AvP port (protocol version 2): the test now builds the game's real cryptography too
// (net_crypto_cng.cpp, pq_key_exchange.cpp), which needs Windows' own headers. So GUID and IsEqualGUID are Windows' own now, from the
// smallest windows.h (no COM, no multimedia, no min and max), and the stand-ins below that share a name with a Windows function are
// renamed after it, so the game's calls reach the stand-ins and nothing clashes.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define _console_h_
#define _ConfigFile_h_
#define LOGSTRING_H
void LogString(const std::string &logString);

typedef uint32_t NetID;

// Not in the smallest windows.h: the stand-in's own
inline long CoCreateGuid(GUID *guid)
{
	memset(guid, 0, sizeof(GUID));
	return 0;
}

// The clock is the test's, so that a five-second wait takes no time
extern uint32_t fakeTime;

inline uint32_t timeGetTime()
{
	return fakeTime;
}

inline void NetStartTestSleep(unsigned long)
{
}

inline void NetStartTestOutputDebugString(const char *)
{
}

#undef Sleep
#define Sleep NetStartTestSleep
#undef OutputDebugString
#define OutputDebugString NetStartTestOutputDebugString

// console.h
typedef void (*funcPointer)(void);
void Con_AddCommand(char *commandName, funcPointer function);
void Con_PrintError(const std::string &errorString);
void Con_PrintMessage(const std::string &messageString);
void Con_PrintDebugMessage(const std::string &messageString);

// ConfigFile.h
int Config_GetInt(const std::string &heading, const std::string &variable, int defaultValue);

// gamedef.h and pldnet.h
enum I_NETWORK
{
	I_No_Network,
	I_Host,
	I_Peer
};

struct FakeAvP
{
	I_NETWORK Network;
};

extern FakeAvP AvP;

enum
{
	JOINNETGAME_WAITFORDESC = 1,
	AVPMENU_MULTIPLAYER_CONFIG_JOIN = 1234,
	KEY_ESCAPE = 0,
	LobbiedGame_Server = 1,
	LobbiedGame_Client = 2
};

#define NET_MESSAGEBUFFERSIZE 3072

struct FakeNetGameData
{
	int skirmishMode;
	int needGameDescription;
	int levelNumber;
	int joiningGameStatus;
};

extern FakeNetGameData netGameData;
extern int LobbiedGame;

// menus.h. Modified 2026-11-08 by Melanikus for the AvP port: a game from the relays' list, as menus.h has it (relay_list_text.h's sizes)
// Modified 2026-11-09 by Melanikus for the AvP port: the games page's columns, as menus.h has them (Fourth's 357ba55a)
#include "relay_list_text.h"

typedef struct
{
	char Name[40];
	int levelIndex;
	char coopLobby;
	uint32_t host;
	uint16_t port;
	GUID Guid;
	bool AllowedToJoin;
	bool throughRelay;
	char relayCode[kRelayCodeLetters];
	char relayRegion[kRelayListRegionLetters + 1];
	char levelName[kRelayListLevelLetters + 1];
	uint16_t gameStyle;
	uint8_t players;
	uint8_t maxPlayers;
	uint8_t relayHostId[kNetIdentityIdBytes];   // added 2026-11-09 by Melanikus for the AvP port: as menus.h has it (protocol version 2; the whole id since item 6)
	int serverLine;   // added 2026-11-11 by Melanikus for the AvP port: which good servers.cfg line listed the game, as menus.h has it (one model for every line)
} SESSION_DESC;

#define MAX_NO_OF_SESSIONS 10
extern SESSION_DESC SessionData[];
extern uint32_t NumberOfSessionsFound;

// mp_config.h
bool IsCustomLevel(int index, int gameType);
int GetLocalMultiplayerLevelIndex(int index, char *levelName, int gameType);

// Added 2026-11-09 by Melanikus for the AvP port (step 7): THE STAND-IN FOR THE CHANNEL TO THE RELAY, IN EVERY FILE THIS TEST BUILDS.
// networking.cpp asks for "net_relay_v2.h", and the compiler looks for a name in quotes first in the folder of the file that asks: 3dc,
// where the real session is. So networking.cpp got the real one while this test got the stand-in (net_start_test_stubs), which then
// couldn't link (handshake A's functions aren't in this test), and would have been two different sessions in one program if it had.
// Asked for here, from this folder, the stand-in is found first; it has the real one's guard (NET_RELAY_V2_H), so 3dc's is never
// read after it, in any file. GitHub's test run found it, as that link error.
#include "net_relay_v2.h"

#endif
