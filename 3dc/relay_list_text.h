// The words that go to and from the relay server's list: the bodies the game sends, and the answers it reads back. Written 2026-11-08 by
// Melanikus for the AvP port.
//
// The list is a page on the relay server, over HTTPS (relay_shared.h says what the relay is). A host puts its game on it, and a player
// reads it to pick a game. It also says where each region's relay is. PROTOCOL.md in the server's repository, section 4, is the whole of
// what goes back and forth, and this is written to it (at 7014de1). relay_list.cpp makes the calls. This part only turns things into text
// and back, so that a test can try all of it.
//
// What comes back is from the network, so it's treated as hostile (AVP Tao 3). This reads exactly what the protocol says, and nothing
// else: one object holding one field, a list of at most so many objects, each with exactly its fields, each within its limits. Anything
// else, anywhere in an answer, refuses the whole of it, with the reason, rather than keeping the part that looked right.
//
// tests/relay_list_test.cpp tries every refusal, and the bodies.
//
// Modified 2026-11-09 by Melanikus for the AvP port: every game on the list carries its host's fingerprint, the short name of the identity
// that host proves in protocol version 2's handshake B (net_identity.h), so a joiner knows which host to expect before it says a word. A
// host sends its own with its listing. On this version of the game every listing has one: an answer with a game that hasn't is refused.

#ifndef RELAY_LIST_TEXT_H
#define RELAY_LIST_TEXT_H

#include <stddef.h>
#include <stdint.h>

#include "relay_shared.h"
#include "net_identity.h"   // a fingerprint's size, and the one rule for its 32 hex digits, which the joining code uses too

enum
{
	kRelayListMostGames = 100,           // the list sends at most this many games
	kRelayListMostRelays = 16,           // and at most this many relays
	kRelayListTextLetters = 32,          // a name or a mode: 1 to 32 printable ASCII characters
	kRelayListLevelLetters = 39,         // a level's name: 1 to 39, the longest the game's own go (kLevelNameSize - 1, networking.h)
	kRelayListRegionLetters = 16,        // a region: 1 to 16 small letters, digits and -
	kRelayListHostLetters = 63,          // a relay's DNS name: 1 to 63 letters, digits, - and .
	kRelayListSecretBytes = 16,          // a pass and an owner: 16 bytes, written as 32 hex digits, small letters
	kRelayListMostVersion = 65535,       // the game's network version, 1 to 65535
	kRelayListMostSeconds = 31536000,    // how long a game has been listed: a year at most
	kRelayListMostHostMs = 9999,         // the host's round trip to its relay, in ms; 0 if it wasn't measured
	kRelayListMostLevelNumber = 100,     // 0 to 99 for a stock level, 100 for a custom one, found by its name
	kRelayListMostGamesBytes = 49152,    // an answer to GET /v1/games: 48 KB at most (the protocol's worst case is 46,011 bytes)
	kRelayListMostRelaysBytes = 4096,    // an answer to GET /v1/relays: 4 KB at most (its worst case is 1,868 bytes)
	kRelayListMostOwnerBytes = 1024,     // an answer to POST: {"owner"}, far less than this
	kRelayListMostBodyBytes = 1024       // the list takes a body of at most 1 KB
};

// The most a passTime can be: the most a JSON number holds exactly, 2 to the 53rd, less 1
static const uint64_t kRelayListMostPassTime = 9007199254740991ull;

// One game on the list, as the Find games page shows it. Every text is ended by a 0.
struct RelayListGame
{
	char region[kRelayListRegionLetters + 1];
	char code[kRelayCodeLetters + 1];
	char name[kRelayListTextLetters + 1];
	char level[kRelayListLevelLetters + 1];
	char mode[kRelayListTextLetters + 1];
	int gameStyle;
	int levelNumber;
	int coopLobby;
	int players;
	int maxPlayers;
	int version;
	int hostMs;
	long seconds;
	uint8_t fingerprint[kNetIdentityFingerprintBytes];   // the host's identity for this run, which handshake B must find it holds
};

// One region's relay: where a game of that region is joined. networking.cpp reads these (RelayList_Relays).
struct RelayListRelay
{
	char region[kRelayListRegionLetters + 1];
	char host[kRelayListHostLetters + 1];
	int port;
	uint32_t address;   // the host's IPv4 address, as ENetAddress.host keeps it (network byte order), looked up by the list's thread
};

// Why the address a relay's name looked up to isn't used. A relay is somewhere out on the internet, so an address that can't be one isn't
// sent anything: a wrong or hostile name could otherwise aim the game's messages at this network's own devices, or at many at once.
enum RelayAddressRefusal
{
	kRelayAddressUsable,
	kRelayAddressThisNetwork,   // 0.0.0.0/8, "this network", which holds ANY: never a machine to send to
	kRelayAddressBroadcast,     // 255.255.255.255: everyone on this network at once
	kRelayAddressMulticast,     // 224.0.0.0/4: many machines at once
	kRelayAddressReserved,      // 240.0.0.0/4: set aside, never used on the internet
	kRelayAddressPrivate,       // 10/8, 172.16/12, 192.168/16, 169.254/16 and 100.64/10: inside a home's or a carrier's network
	kRelayAddressLoopback       // 127/8: this machine itself
};

// The rule for an address, given with its first number in the highest byte (192.168.0.1 is 0xC0A80001). Private and loopback addresses
// are refused unless developer is true (-dev), so that a relay on this machine or this network can be tested.
RelayAddressRefusal RelayList_AddressRefusal(uint32_t address, bool developer);

// The words for a refusal of an address, for the log: never the address itself
const char *RelayList_AddressWords(RelayAddressRefusal why);

// What a host puts on the list. The texts are the game's own, already fitted (RelayList_FitText).
struct RelayListListing
{
	const char *region;
	const char *code;                        // the 8 letters, with or without a 0 after them
	uint8_t pass[kRelayListSecretBytes];     // from the newest HOST_OPENED or HOST_KEPT
	uint64_t passTime;                       // from that same message
	const char *name;
	const char *level;
	const char *mode;
	int gameStyle;
	int levelNumber;
	int coopLobby;
	int players;
	int maxPlayers;
	int version;
	int hostMs;
	const uint8_t *fingerprint;                // this run's identity's fingerprint (net_identity.h): there has to be one, and not all 0
};

// Why an answer was refused. Each has its own words, for the log and for the tests.
enum RelayListRefusal
{
	kRelayListAccepted,
	kRelayListTooBig,          // more bytes than any honest answer
	kRelayListHasAZero,        // a 0 byte, which no JSON text holds
	kRelayListNotAnObject,     // it doesn't start with {
	kRelayListNotItsField,     // its one field isn't the one the protocol says ("games", "relays" or "owner")
	kRelayListNotAList,        // that field isn't a list
	kRelayListTooMany,         // more than 100 games, or 16 relays
	kRelayListNotAnEntry,      // something in the list isn't an object
	kRelayListUnknownField,    // a field the protocol doesn't have
	kRelayListFieldTwice,      // a field given twice in one entry
	kRelayListFieldMissing,    // an entry without all its fields
	kRelayListBadEscape,       // a \ that isn't \" or \\, the only two the list writes
	kRelayListBadText,         // a name, level or mode that isn't printable ASCII of its length
	kRelayListBadCode,         // a code that isn't 8 code letters
	kRelayListBadRegion,       // a region that isn't 1 to 16 small letters, digits and -
	kRelayListBadHost,         // a relay's host that isn't a DNS name, or is an address instead of one
	kRelayListBadSecret,       // an owner that isn't 32 hex digits, small letters
	kRelayListBadNumber,       // a number that isn't a whole number in its range
	kRelayListMorePlayersThanRoom, // players over maxPlayers
	kRelayListOtherVersion,    // a game for another network version than the one asked for
	kRelayListTwice,           // two games with one region and code, or two relays for one region
	kRelayListEndsTooSoon,     // the text stops before its end
	kRelayListNotWhatComesNext, // something else where a , : } or ] has to be
	kRelayListMoreAfterTheEnd, // more after the closing }
	kRelayListBadFingerprint   // a game's fingerprint that isn't 32 hex digits, small letters
};

// The words for a refusal: "a game has a field the protocol doesn't have", and so on
const char *RelayList_RefusalWords(RelayListRefusal why);

// Reads a GET /v1/games answer. games has room for kRelayListMostGames. ourVersion is the version that was asked for. On a refusal,
// count is 0 and nothing in games is to be used.
// Modified 2026-11-09 by Melanikus for the AvP port: fingerprints says whether the games carry their host's fingerprint, which only a v2
// draft's do (networking.h, Net_V2DraftOn). Then a game without one is refused. Without it, the answer is read as version 1's, with no
// fingerprint field, and each game's fingerprint is left all 0.
RelayListRefusal RelayList_ReadGames(const char *body, size_t bytes, int ourVersion, bool fingerprints, RelayListGame *games, int *count);

// Reads a GET /v1/relays answer. relays has room for kRelayListMostRelays.
RelayListRefusal RelayList_ReadRelays(const char *body, size_t bytes, RelayListRelay *relays, int *count);

// Reads a POST's answer, {"owner"}: the 16 bytes a host proves its listing with afterwards
RelayListRefusal RelayList_ReadOwner(const char *body, size_t bytes, uint8_t owner[kRelayListSecretBytes]);

// Added 2026-11-09 by Melanikus for the AvP port (protocol version 2's draft, step 7): a relay's static public keys, the answer to
// GET /v1/relay-keys?region=<region> (CRYPTO.md 12.1): {"region", "p384", "mlkem"}, the P-384 key's 97 bytes and the ML-KEM-1024 key's
// 1,568, each as exactly twice as many hex digits in small letters. Read as hostile, as every answer is.
// - Only the relay asked about is taken: an answer about another region is refused.
// - On a refusal, p384 and kem are left all 0, and nothing of the answer is kept.
// - Only the keys' form is checked here. Whether they're the relay's real keys is checked after, against the fingerprint the -dev settings
//   give (net_relay_keys.h); until then, nothing is sent to that relay.
size_t const kRelayKeysMostAnswerBytes = 8192;   // the answer is about 3,370 bytes
RelayListRefusal RelayList_ReadRelayKeys(const char *body, size_t bytes, const char *region, uint8_t p384[kNetCryptoP384PublicBytes],
	uint8_t kem[kNetPqKemPublicBytes]);

// The body of a POST /v1/games (owner NULL) or a PUT (owner given). False, with nothing to send, if any of it is outside the protocol's
// limits: the game checks its own words before they go, so the list never has to refuse them. room is the space in body, its 0 included.
// With withFingerprint (a v2 draft's listing, Net_V2DraftOn), a listing with no fingerprint, or one that's all 0 (no identity was made for
// this run), is one of those. Without it (modified 2026-11-09 by Melanikus for the AvP port), the body is version 1's, with no fingerprint
// field at all, whatever the listing holds.
bool RelayList_ListingBody(const RelayListListing &listing, bool withFingerprint, const uint8_t *owner, char *body, size_t room);

// The body of a DELETE /v1/games: the game, by its region and code, and the owner's proof
bool RelayList_EndingBody(const char *region, const char *code, const uint8_t owner[kRelayListSecretBytes], char *body, size_t room);

// Makes the game's own words fit a text on the list: a character that isn't printable ASCII becomes ?, at most `most` are kept, and nothing
// at all becomes fallback. to has room for most + 1.
void RelayList_FitText(const char *from, const char *fallback, char *to, size_t most);

// The words the list shows for a game's style, the game's own number for it (NETGAME_TYPE in pldnet.h, in that order: relay_list.cpp
// checks they still match). In English, whatever language the host plays in, so everyone reading the list reads the same words.
const char *RelayList_ModeWords(int gameStyle, bool coopCampaign);

// Whether a name is one to ask Windows to find: 1 to 253 characters, in parts between dots of 1 to 63 letters, digits and -, none
// starting or ending with -. For the settings' server name, which isn't trusted to be a name just because it's in the file (Tao 3).
bool Relay_IsServerName(const char *name);

// A region: 1 to 16 small letters, digits and -
bool RelayList_IsRegion(const char *region);

// A relay's host from the list: a DNS name of 1 to 63 characters, and never an address: a name whose every part is digits is one
bool RelayList_IsRelayHost(const char *host);

// ---- Reaching a test server with no name and no certificate Windows trusts (-dev only; relay_list.cpp). Added 2026-11-09 by Melanikus for
// the AvP port, so a test can reach the relay on another machine with nothing changed on the tester's computer: no hosts line, nothing
// added to Windows' trusted certificates.

// Whether text is exactly one IPv4 address written plainly: four whole numbers from 0 to 255 with a dot between each, no leading zeros
// (a leading zero means octal to some readers), and nothing else, not even a space. The address goes into hostOrder with its first
// number in the highest byte (192.168.0.1 is 0xC0A80001). Anything else is refused, with nothing written, and the refusal says why.
enum RelayIPv4Refusal
{
	kRelayIPv4Read,
	kRelayIPv4NoText,        // no text, or nowhere to put the address
	kRelayIPv4NotANumber,    // a part that's empty, or isn't digits: a space, a sign, a letter
	kRelayIPv4LeadingZero,   // a part of two or three digits starting with 0
	kRelayIPv4TooBig,        // a part past 255, or of more than three digits
	kRelayIPv4WrongShape     // not four parts with a dot between each and nothing after
};
RelayIPv4Refusal RelayList_IPv4Refusal(const char *text, uint32_t *hostOrder);
bool RelayList_ReadIPv4(const char *text, uint32_t *hostOrder);   // RelayList_IPv4Refusal(...) == kRelayIPv4Read

// The SHA-256 of the list's own certificate, as the settings give it (RelayListPin): exactly 64 hex digits, small letters or capitals,
// and nothing else. Anything else is refused, with nothing written, and the refusal says why.
int const kRelayListPinBytes = 32;
enum RelayPinRefusal
{
	kRelayPinRead,
	kRelayPinNoText,         // no text, or nowhere to put the pin
	kRelayPinWrongLength,    // not 64 characters: a pin written with colons or spaces between its pairs is longer
	kRelayPinNotHex          // a character that isn't 0-9, a-f or A-F
};
RelayPinRefusal RelayList_PinRefusal(const char *text, uint8_t pin[kRelayListPinBytes]);
bool RelayList_ReadPin(const char *text, uint8_t pin[kRelayListPinBytes]);   // RelayList_PinRefusal(...) == kRelayPinRead

// Whether a certificate's SHA-256 is the pin's, comparing every byte whatever the first difference (it takes the same time either way)
bool RelayList_PinMatches(const uint8_t digest[kRelayListPinBytes], const uint8_t pin[kRelayListPinBytes]);

// The name people see for a test server (RelayServerName): 1 to 32 characters, each a letter, a digit, a space or one of ' ( ) - . ,
int const kRelayListShownNameLetters = 32;
enum RelayShownNameRefusal
{
	kRelayShownNameGood,
	kRelayShownNameNoText,       // no text, or an empty one
	kRelayShownNameTooLong,      // more than 32 characters
	kRelayShownNameBadCharacter  // a character outside the set: a quote, a newline, a tab, a %, a slash, a letter beyond ASCII
};
RelayShownNameRefusal RelayList_ShownNameRefusal(const char *name);
bool RelayList_IsShownName(const char *name);   // RelayList_ShownNameRefusal(...) == kRelayShownNameGood

#endif
