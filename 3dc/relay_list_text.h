// The words that go to and from the relay server's list: the bodies the game sends, and the list of games it reads back. Written
// 2026-11-08 by Melanikus for the AvP port.
//
// The list is a page on the relay server, over HTTPS (relay_shared.h says what the relay is). A host puts its game on it, and a player
// reads it to pick a game. PROTOCOL.md in the server's repository, section 4, is the whole of what goes back and forth, and this is
// written to it. relay_list.cpp makes the calls. This part only turns things into text and back, so that a test can try all of it.
//
// What comes back is from the network, so it's treated as hostile (AVP Tao 3). This reads exactly what the protocol says, and nothing
// else: one object holding "games", a list of at most 100 games, each with exactly its eight fields, each within its limits. Anything
// else, anywhere in the answer, refuses the whole of it, with the reason, rather than keeping the part that looked right.
//
// tests/relay_list_test.cpp tries every refusal, and the bodies.

#ifndef RELAY_LIST_TEXT_H
#define RELAY_LIST_TEXT_H

#include <stddef.h>
#include <stdint.h>

#include "relay_shared.h"

enum
{
	kRelayListMostGames = 100,           // the server sends at most this many
	kRelayListTextLetters = 32,          // a name, a level or a mode: 1 to 32 printable ASCII characters
	kRelayListMostVersion = 65535,       // the game's network version, 1 to 65535
	kRelayListMostSeconds = 31536000,    // how long a game has been listed: no game is listed for more than a year
	kRelayListMostAnswerBytes = 32768,   // 100 games at every limit come to about 25 KB
	kRelayListMostBodyBytes = 1024       // the server takes a body of at most 1 KB
};

// One game on the list, as the Find games page shows it. Every text is ended by a 0.
struct RelayListGame
{
	char code[kRelayCodeLetters + 1];
	char name[kRelayListTextLetters + 1];
	char level[kRelayListTextLetters + 1];
	char mode[kRelayListTextLetters + 1];
	int players;
	int maxPlayers;
	int version;
	long seconds;
};

// Why a list was refused. Each has its own words, for the log and for the tests.
enum RelayListRefusal
{
	kRelayListAccepted,
	kRelayListTooBig,          // more bytes than any honest list
	kRelayListHasAZero,        // a 0 byte, which no JSON text holds
	kRelayListNotAnObject,     // it doesn't start with {
	kRelayListNotGames,        // its one field isn't "games"
	kRelayListNotAList,        // "games" isn't a list
	kRelayListTooManyGames,    // more than 100
	kRelayListNotAGame,        // something in the list isn't an object
	kRelayListUnknownField,    // a field the protocol doesn't have
	kRelayListFieldTwice,      // a field given twice in one game
	kRelayListFieldMissing,    // a game without all eight
	kRelayListBadEscape,       // a \ that isn't \" or \\, the only two the server writes
	kRelayListBadText,         // a name, level or mode that isn't 1 to 32 printable ASCII characters
	kRelayListBadCode,         // a code that isn't 8 code letters
	kRelayListBadNumber,       // a number that isn't a whole number in its range
	kRelayListMorePlayersThanRoom, // players over maxPlayers
	kRelayListOtherVersion,    // a game for another network version than the one asked for
	kRelayListSameGameTwice,   // two games with one code
	kRelayListEndsTooSoon,     // the text stops before the list is closed
	kRelayListNotWhatComesNext, // something else where a , : } or ] has to be
	kRelayListMoreAfterTheEnd  // more after the closing }
};

// The words for a refusal: "the list holds a game with a field the protocol doesn't have", and so on
const char *RelayList_RefusalWords(RelayListRefusal why);

// Reads a GET /v1/games answer. games has room for kRelayListMostGames. ourVersion is the version that was asked for. On a refusal,
// count is 0 and nothing in games is to be used.
RelayListRefusal RelayList_ReadGames(const char *body, size_t bytes, int ourVersion, RelayListGame *games, int *count);

// The body of a POST or PUT /v1/games. False, with nothing to send, if any of it is outside the protocol's limits: the game checks its
// own words before they go, so the server never has to refuse them. room is the space in body, a 0 included.
bool RelayList_ListingBody(const uint8_t token[kRelayTokenBytes], const char *name, const char *level, const char *mode, int players,
	int maxPlayers, int version, char *body, size_t room);

// The body of a DELETE /v1/games
bool RelayList_EndingBody(const uint8_t token[kRelayTokenBytes], char *body, size_t room);

// Makes the game's own words fit a name, level or mode: a character that isn't printable ASCII becomes ?, at most 32 are kept, and nothing
// at all becomes fallback. to has room for kRelayListTextLetters + 1.
void RelayList_FitText(const char *from, const char *fallback, char *to);

#endif
