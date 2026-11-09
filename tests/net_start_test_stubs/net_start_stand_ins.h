// What 3dc/networking.cpp needs from the rest of the game, so that tests/net_start_test.cpp can run the real file with nothing else. Written 2026-11-06 by Melanikus for the AvP port.
// tests/net_start_test.args puts this at the top of every file it compiles, and it sets the markers the game's own console.h, ConfigFile.h and logString.h look for, so that those stay out.

#ifndef NET_START_STAND_INS_H
#define NET_START_STAND_INS_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sstream>
#include <string>

#define _console_h_
#define _ConfigFile_h_
#define LOGSTRING_H
void LogString(const std::string &logString);

typedef uint32_t NetID;

typedef struct _GUID
{
	unsigned long Data1;
	unsigned short Data2;
	unsigned short Data3;
	unsigned char Data4[8];
} GUID;

inline bool IsEqualGUID(const GUID &a, const GUID &b)
{
	return memcmp(&a, &b, sizeof(GUID)) == 0;
}

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

inline void Sleep(unsigned long)
{
}

inline void OutputDebugString(const char *)
{
}

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
} SESSION_DESC;

#define MAX_NO_OF_SESSIONS 10
extern SESSION_DESC SessionData[];
extern uint32_t NumberOfSessionsFound;

// mp_config.h
bool IsCustomLevel(int index, int gameType);
int GetLocalMultiplayerLevelIndex(int index, char *levelName, int gameType);

#endif
