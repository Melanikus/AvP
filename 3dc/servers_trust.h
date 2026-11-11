// The player's yes to a servers.cfg line, kept between runs (#175's last part). Written 2026-11-10 by Melanikus for the AvP port. This
// file only says what a yes is the hash of, and makes and reads the kept file's text. Keeping that file on the disk is
// net_identity_store.cpp's; asking the player, and holding the list to the answer, is relay_list.cpp's.
//
// WHY. The owner's ruling (#175, 2026-11-10): a servers.cfg line is used only after the player's yes. servers.cfg is a file anyone can be
// told to replace ("put this line in your servers.cfg"), so before the game sends anything to a line's server, it shows the player
// where that server is and asks. Only then does anything go to it. A Hudson run is the one exception, as nobody is there to answer
// (relay_list.cpp says how it's bound to Hudson really running).
//
// WHAT A YES IS. SHA-256 over 68 bytes (ServersTrust_HashInput):
//   kServersTrustWords ("AVPx servers-trusted line 1", 27 letters), a 0, the line's address (4 bytes, its first number first), its list
//   port and its relay port (2 bytes each, the higher byte first), and its fingerprint (32 bytes).
// The name isn't in it: it's only what the game shows, so a server renamed in the file is the same server. Another address, port or
// fingerprint is another server, and it's asked about again.
//
// THE FILE, servers-trusted.cfg. Notes (a line starting with #) and yeses, each line "yes " and the hash as 64 small hex digits, every line
// ending with a newline. Nothing else. At most kServersTrustMostYeses yeses, and kServersTrustMostBytes bytes. It holds hashes only, so
// it can never add a server (a hash names none), and it doesn't tell anyone which servers this player uses unless they already have the
// line.
//
// READ AS HOSTILE (AVP Tao 3). The game writes this file, so anything in it that the game wouldn't write was put there by something else:
//   - a line that isn't a note or a yes in exactly that shape, a yes twice, too many yeses, or too many bytes, refuses the whole file. No
//     yes at all is taken from it (the player is asked again), and the next yes writes the file whole again;
//   - the lines are split at each newline and nowhere else. A carriage return just before a newline is taken, as Notepad writes one;
//     anywhere else it's a bad line, and so is any byte that isn't a printable character;
//   - the last line must end with its newline, as the game writes it, so a file cut short is never half read.
//
// THE PICK, servers-picked.cfg (#175 2b; added 2026-11-10 by Melanikus for the AvP port). Which servers.cfg line the player picked, for
// the next start of the game: the same hash as that line's yes, kept in a file of its own, so a pick can never stand in for a yes, and a
// pick file someone spoiled costs only the pick. Notes and at most one line "pick " and the hash, with the same rules as the yeses' file;
// a second pick refuses the file. A file refused, or none, means no pick, and the game uses the first good line, as it did before there
// was a pick. A pick no good line has now (the line was changed or taken out) is the same as none. It holds a hash only, so it can never
// add a server, and it's never an address.

#ifndef SERVERS_TRUST_H
#define SERVERS_TRUST_H

#include <stddef.h>
#include <stdint.h>

#define SERVERS_TRUST_FILE_NAME "servers-trusted.cfg"

char const kServersTrustWords[] = "AVPx servers-trusted line 1";
size_t const kServersTrustHashBytes = 32;                                           // SHA-256
size_t const kServersTrustFingerprintBytes = 32;                                    // a servers.cfg line's (servers_cfg.h)
size_t const kServersTrustInputBytes = sizeof(kServersTrustWords) - 1 + 1 + 4 + 2 + 2 + kServersTrustFingerprintBytes;   // 68
int const kServersTrustMostYeses = 64;
int const kServersTrustMostBytes = 16 * 1024;

// The yeses read from the file, or to be written to it, in the file's order
struct ServersTrustYeses
{
	uint8_t hashes[kServersTrustMostYeses][kServersTrustHashBytes];
	int count;
};

// Why the whole file was refused, or that it wasn't
enum ServersTrustRefusal
{
	kServersTrustRead,
	kServersTrustTooBig,        // over kServersTrustMostBytes
	kServersTrustBadLine,       // a line that isn't a note or a yes in its shape, or a last line with no newline
	kServersTrustTooMany,       // over kServersTrustMostYeses
	kServersTrustTwice,         // the same yes twice, which the game never writes
	kServersTrustTwoPicks       // servers-picked.cfg: more than one pick, which the game never writes (added 2026-11-10, #175 2b)
};

// What a yes is the hash of, for one line: its address (its first number in the highest byte, as servers_cfg.h keeps it), its two ports
// (1 to 65535) and its fingerprint. False, with out wiped, for a port out of range or nothing to read.
bool ServersTrust_HashInput(uint32_t address, int listPort, int relayPort, const uint8_t fingerprint[kServersTrustFingerprintBytes],
	uint8_t out[kServersTrustInputBytes]);

// Reads the file's text: bytes of it at text, which needs no 0 at its end (a 0 is a bad line). out is whole only for kServersTrustRead;
// for every refusal it holds no yes. badLine, if given, is the refused line's number, counted from 1, or 0.
ServersTrustRefusal ServersTrust_Read(const char *text, size_t bytes, ServersTrustYeses *out, int *badLine);

// Whether the yeses hold this hash
bool ServersTrust_Has(const ServersTrustYeses &yeses, const uint8_t hash[kServersTrustHashBytes]);

// Adds a yes: true when it's there afterwards (added, or there already), false when there's no room for another
bool ServersTrust_Add(ServersTrustYeses *yeses, const uint8_t hash[kServersTrustHashBytes]);

// The file's whole text, as the game writes it, into out, which has room for room bytes and gets no 0 at the end. Its size, or 0 if room
// is too small for all of it (then nothing should be written).
size_t ServersTrust_Write(const ServersTrustYeses &yeses, char *out, size_t room);

// The words for a refusal, for the log: the game's own, never anything from the file
const char *ServersTrust_Words(ServersTrustRefusal why);

// ---- The pick (#175 2b; THE PICK, above). Added 2026-11-10 by Melanikus for the AvP port.

#define SERVERS_PICK_FILE_NAME "servers-picked.cfg"

struct ServersTrustPick
{
	bool there;                              // false: no pick, so the first good line
	uint8_t hash[kServersTrustHashBytes];   // the picked line's hash, when there is one
};

// Reads servers-picked.cfg's text, as ServersTrust_Read does the yeses'. out is a pick only for kServersTrustRead with a pick line in the
// file; for every refusal it holds none. A second pick line is kServersTrustTwoPicks.
ServersTrustRefusal ServersTrust_ReadPick(const char *text, size_t bytes, ServersTrustPick *out, int *badLine);

// The pick file's whole text for this hash, as the game writes it, into out (room bytes, no 0 at the end). Its size, or 0 if room is too
// small or there's no hash (then nothing should be written).
size_t ServersTrust_WritePick(const uint8_t hash[kServersTrustHashBytes], char *out, size_t room);

// Which good line a run uses, from 0: the one whose hash is the pick, or else the first. -1 when there are none. hashes holds each good
// line's hash, in the file's order, and hashed says whether it could be worked out: a line whose hash couldn't be is never the pick's.
int ServersTrust_LineToUse(const uint8_t (*hashes)[kServersTrustHashBytes], const bool *hashed, int count, const ServersTrustPick &pick);

#endif
