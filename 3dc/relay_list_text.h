// The words that go to and from the relay server's list: the bodies the game sends, and the answers it reads back. Written 2026-11-08 by
// Melanikus for the AvP port.
//
// The list is a page on the relay server, over HTTPS (relay_shared.h says what the relay is). A host puts its game on it, and a player
// reads it to pick a game. It also gives its own relay's keys. PROTOCOL.md in the server's repository, section 4, is the whole of
// what goes back and forth, and this is written to it (at 7014de1). relay_list.cpp makes the calls. This part only turns things into text
// and back, so that a test can try all of it.
//
// What comes back is from the network, so it's treated as hostile (AVP Tao 3). This reads exactly what the protocol says, and nothing
// else: one object holding one field, a list of at most so many objects, each with exactly its fields, each within its limits. Anything
// else, anywhere in an answer, refuses the whole of it, with the reason, rather than keeping the part that looked right.
//
// tests/relay_list_test.cpp tries every refusal, and the bodies.
//
// Modified 2026-11-09 by Melanikus for the AvP port: every game on the list carries its host's id, the whole name of the identity that
// host proves in protocol version 2's handshake B (net_identity.h), so a joiner knows which host to expect before it says a word. A host
// sends its own with its listing. On this version of the game every listing has one: an answer with a game that hasn't is refused.
// (Modified 2026-11-10 by Melanikus for the AvP port, SERVER-BLOCKERS item 6: the field is "id", the 48-byte id as 96 hex digits, in
// place of "fingerprint", its first 16 bytes. The list refuses the old field.)
//
// Modified 2026-11-10 by Melanikus for the AvP port (#175): the server is a servers.cfg line, with one relay at its own address, so the
// relays' list (GET /v1/relays) isn't read any more, and a relay's keys come with their region from the list's own relay. The readers only
// the -dev settings used are gone too: the pin and the shown name.

#ifndef RELAY_LIST_TEXT_H
#define RELAY_LIST_TEXT_H

#include <stddef.h>
#include <stdint.h>

#include "relay_shared.h"
#include "net_identity.h"   // an id's size, and the one rule for its 96 hex digits, which the joining code uses too

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
	kRelayListMostGamesBytes = 57344,    // an answer to GET /v1/games: 56 KB at most (the protocol's worst case is 51,511 bytes, with ids)
	kRelayListMostOwnerBytes = 1024,     // an answer to POST: {"owner"}, far less than this
	kRelayListMostBodyBytes = 1024       // the list takes a body of at most 1 KB
};

// Asking for online access (server #24): the sizes PROTOCOL.md gives for POST /v1/access-requests. Added 2026-11-09 by Melanikus for the
// AvP port.
enum
{
	kRelayAccessNameMostBytes = 15,      // a name: 1 to 15 bytes, the most a profile's name can be (MAX_SIZE_OF_USERS_NAME, user_profile.h)
	kRelayAccessKeyLetters = 2220,       // the public halves of an identity, 1,665 bytes, in base64: always exactly this many letters
	kRelayAccessCodeLetters = 16,        // the access code: 16 code letters
	kRelayAccessCodeTextBytes = 20,      // written XXXX-XXXX-XXXX-XXXX, with the 0 at the end
	kRelayAccessMostBodyBytes = 4096,    // the list takes a request of at most 4 KB, its own limit (the other calls keep 1 KB)
	kRelayAccessMostAnswerBytes = 512    // and answers it with far less than this
};

static_assert(kNetIdentityStaticBytes % 3 == 0 && kNetIdentityStaticBytes / 3 * 4 == kRelayAccessKeyLetters,
	"the public halves are a whole number of threes, so their base64 has no = at the end, and is always the same length");

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
	uint8_t hostId[kNetIdentityIdBytes];   // the host's whole id for this run, which handshake B must find it holds
};

// One region's relay: where a game of that region is joined. networking.cpp reads these (RelayList_Relays).
struct RelayListRelay
{
	char region[kRelayListRegionLetters + 1];
	char host[kRelayListHostLetters + 1];
	int port;
	uint32_t address;   // the host's IPv4 address, as ENetAddress.host keeps it (network byte order), looked up by the list's thread
};

// What kind of address an address is, for the servers.cfg reader's rule of which can be a server's (servers_cfg.h). Some never are: a
// wrong or hostile line could otherwise aim the game's messages at many machines at once. Modified 2026-11-10 by Melanikus for the AvP
// port (#175): this was the rule for the address a relay's name looked up to, until the server became a servers.cfg line with no names.
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

// The kind of an address, given with its first number in the highest byte (192.168.0.1 is 0xC0A80001). (Modified 2026-11-10 by Melanikus
// for the AvP port, #175: -dev's way, which took private and loopback addresses as usable, is gone with the -dev settings it was for.)
RelayAddressRefusal RelayList_AddressRefusal(uint32_t address);

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
	const uint8_t *hostId;                     // this run's identity's whole id (net_identity.h): there has to be one, and not all 0
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
	kRelayListBadSecret,       // an owner that isn't 32 hex digits, small letters
	kRelayListBadNumber,       // a number that isn't a whole number in its range
	kRelayListMorePlayersThanRoom, // players over maxPlayers
	kRelayListOtherVersion,    // a game for another network version than the one asked for
	kRelayListTwice,           // two games with one region and code, or two relays for one region
	kRelayListEndsTooSoon,     // the text stops before its end
	kRelayListNotWhatComesNext, // something else where a , : } or ] has to be
	kRelayListMoreAfterTheEnd, // more after the closing }
	kRelayListBadFingerprint,  // an access answer's fingerprint that isn't 32 hex digits, small letters
	kRelayListBadHostId,       // a game's host id that isn't 96 hex digits, small letters (added 2026-11-10, SERVER-BLOCKERS item 6)
	kRelayListBadStatus,       // an access request's answer with an HTTP status the protocol never gives it
	kRelayListBadWord,         // a state or an error that isn't one of the protocol's words for its status
	kRelayListNotOurKey        // an answer to an access request that isn't about this PC's own key
};

// The words for a refusal: "a game has a field the protocol doesn't have", and so on
const char *RelayList_RefusalWords(RelayListRefusal why);

// Reads a GET /v1/games answer. games has room for kRelayListMostGames. ourVersion is the version that was asked for. On a refusal,
// count is 0 and nothing in games is to be used.
// Modified 2026-11-09 by Melanikus for the AvP port: every game carries its host's id, and a game without one is refused. (Modified
// 2026-11-10, #175 PR 3a: version 1's answer, with no id field, went with version 1.)
RelayListRefusal RelayList_ReadGames(const char *body, size_t bytes, int ourVersion, RelayListGame *games, int *count);

// Reads a POST's answer, {"owner"}: the 16 bytes a host proves its listing with afterwards
RelayListRefusal RelayList_ReadOwner(const char *body, size_t bytes, uint8_t owner[kRelayListSecretBytes]);

// Added 2026-11-09 by Melanikus for the AvP port (protocol version 2's draft, step 7): a relay's static public keys, the answer to
// GET /v1/relay-keys (CRYPTO.md 12.1): {"region", "p384", "mlkem"}, the P-384 key's 97 bytes and the ML-KEM-1024 key's 1,568, each as
// exactly twice as many hex digits in small letters. Read as hostile, as every answer is.
// Modified 2026-11-10 by Melanikus for the AvP port (#175): the game asks with no region (the keys of the relay on the list's own machine,
// the one the servers.cfg line names), so the answer's region is given back in region, held to a region's own rule, instead of being
// compared with one asked about.
// - On a refusal, region is empty, p384 and kem are left all 0, and nothing of the answer is kept.
// - Only the keys' form is checked here. Whether they're the server's real keys is checked after, with the list's own key, against the
//   servers.cfg line's fingerprint (net_server_fingerprint.h); until then, nothing more is sent to that server.
size_t const kRelayKeysMostAnswerBytes = 8192;   // the answer is about 3,370 bytes
RelayListRefusal RelayList_ReadRelayKeys(const char *body, size_t bytes, char region[kRelayListRegionLetters + 1],
	uint8_t p384[kNetCryptoP384PublicBytes], uint8_t kem[kNetPqKemPublicBytes]);

// The body of a POST /v1/games (owner NULL) or a PUT (owner given). False, with nothing to send, if any of it is outside the protocol's
// limits: the game checks its own words before they go, so the list never has to refuse them. room is the space in body, its 0 included.
// A listing with no host id, or one that's all 0 (no identity was made for this run), is one of those. (Modified 2026-11-10 by Melanikus for
// the AvP port, #175 PR 3a: version 1's body, with no id field, went with version 1.)
bool RelayList_ListingBody(const RelayListListing &listing, const uint8_t *owner, char *body, size_t room);

// The body of a DELETE /v1/games: the game, by its region and code, and the owner's proof
bool RelayList_EndingBody(const char *region, const char *code, const uint8_t owner[kRelayListSecretBytes], char *body, size_t room);

// Makes the game's own words fit a text on the list: a character that isn't printable ASCII becomes ?, at most `most` are kept, and nothing
// at all becomes fallback. to has room for most + 1.
void RelayList_FitText(const char *from, const char *fallback, char *to, size_t most);

// The words the list shows for a game's style, the game's own number for it (NETGAME_TYPE in pldnet.h, in that order: relay_list.cpp
// checks they still match). In English, whatever language the host plays in, so everyone reading the list reads the same words.
const char *RelayList_ModeWords(int gameStyle, bool coopCampaign);

// A region: 1 to 16 small letters, digits and -
bool RelayList_IsRegion(const char *region);

// ---- Asking for online access (server #24). Added 2026-11-09 by Melanikus for the AvP port.
//
// A player asks the list's owner to let this PC play online. The game sends the name the player chose and the public halves of this PC's
// identity (net_identity.h), with POST /v1/access-requests, and only when the player presses Request access: never by itself. A person
// looks at each request. To approve one, the owner types the access code the player gave him, which is made from the identity's
// fingerprint and shown only on this PC's screen. That code is what makes the request safe: a request whose key was swapped on the way
// has another fingerprint, so another code, and is never approved. PROTOCOL.md, in the server's repository, is the whole of the call.

// Why a name can't be sent. The list refuses exactly the same names, and never changes one to fit.
enum RelayAccessNameRefusal
{
	kRelayAccessNameGood,
	kRelayAccessNameEmpty,
	kRelayAccessNameTooLong,       // more than 15 bytes
	kRelayAccessNameBadLetter,     // a byte outside 32 to 126, or a " or a \, which would need escaping in JSON and in the owner's window
	kRelayAccessNameSpaceAtAnEnd   // a space first or last, which nobody reading it could see
};

// The page shows its own words for each (access_page_rules.h, AccessPage_NameText, from language.cpp's table)
RelayAccessNameRefusal RelayAccess_NameRefusal(const char *name);

// The name the request page starts with: the profile's (at most profileBytes of it), kept to the rule. A letter the rule doesn't take is
// left out, spaces at either end are taken off, and it's cut to 15 bytes. Empty if nothing is left, and then the player types one. to has
// room for kRelayAccessNameMostBytes + 1.
void RelayAccess_NameFromProfile(const char *profile, size_t profileBytes, char *to);

// The access code for an identity's fingerprint. It's the fingerprint's first 80 bits, highest first, as 16 code letters of 5 bits each
// (the relay codes' letters, kNetRelayCodeAlphabet), in groups of four: XXXX-XXXX-XXXX-XXXX. 80 bits can't be matched by making keys until
// one fits. The server's approve command makes the same code from the same fingerprint, and the two sides' tests share examples.
void RelayAccess_Code(const uint8_t fingerprint[kNetIdentityFingerprintBytes], char out[kRelayAccessCodeTextBytes]);

// The body of a POST /v1/access-requests: {"v":1,"name":"...","key":"..."}. False, with nothing to send, if the name breaks the rule,
// or the public halves aren't an identity's (they start with the P-384 key's 04), or there's no room.
bool RelayList_AccessBody(const char *name, const uint8_t statics[kNetIdentityStaticBytes], char *body, size_t room);

// What the list said about a request
enum RelayAccessAnswer
{
	kRelayAccessWaiting,   // 200, "waiting": a person will look at it when they can
	kRelayAccessApproved,  // 200, "approved"
	kRelayAccessRefused,   // 200, "refused": it stays so; a new identity is the way to ask again
	kRelayAccessRevoked,   // 200, "revoked": it was approved, and isn't any more; it stays so too
	kRelayAccessBadName,   // 400, "name": the list wouldn't take the name
	kRelayAccessBadKey,    // 400, "key": nor the key
	kRelayAccessBadShape,  // 400, "shape": nor the request
	kRelayAccessBusy,      // 429, "busy": too many requests from this address just now, or too many waiting; try again in a minute
	kRelayAccessClosed     // 503, "closed": the list isn't taking requests just now
};

// The words for an answer, for the log
const char *RelayAccess_AnswerWords(RelayAccessAnswer answer);

// Reads the answer to a POST /v1/access-requests. status is the answer's HTTP status: 200 is {"v":1,"fp":"...","state":"..."}, whose fp
// has to be ours, this PC's own fingerprint; 400, 429 and 503 are {"v":1,"error":"..."}, each with its own words. Anything else is refused.
// answer is set only when the whole answer is good.
RelayListRefusal RelayList_ReadAccess(int status, const char *body, size_t bytes, const uint8_t ours[kNetIdentityFingerprintBytes],
	RelayAccessAnswer *answer);
// ---- An address written plainly. Added 2026-11-09 by Melanikus for the AvP port, for -dev's test server; modified 2026-11-10 (#175): the
// game's one reader of an address, for a servers.cfg line (servers_cfg.cpp) and a Hudson run's relay beyond this machine (networking.cpp).
// The -dev settings it was first written for (RelayServer, RelayListPin, RelayServerName) are gone, with the pin's and the shown name's
// readers.

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

// The most letters of a server's name the online screens make room for: a servers.cfg line's name is at most 31 (servers_cfg.h)
int const kRelayListShownNameLetters = 32;

// ---- A servers.cfg line's address, as a person reads it on the yes page and the server lists. Added 2026-11-10 by Melanikus for the AvP
// port (#175's last part). "104.131.35.41", or with both ports when either isn't the usual one (servers_cfg.h's kServersCfgListPort and
// kServersCfgRelayPort): "203.0.113.7:8443:27500". The longest, "255.255.255.255:65535:65535", is 27 letters, so kRelayListShownAddressBytes
// holds any address with its 0. address has its first number in the highest byte, as servers_cfg.h keeps it.
// It's written whole or not at all: false, with out empty, if room is too small or a port isn't 1 to 65535, so a screen never shows an
// address cut short. (The yes page's first address was 24 bytes, and an address with both ports written out could be cut.)
size_t const kRelayListShownAddressBytes = 28;
bool RelayList_ShownAddress(uint32_t address, int listPort, int relayPort, char *out, size_t room);

#endif
