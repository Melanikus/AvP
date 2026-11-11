// The list of servers the game may play online through: servers.cfg, in the game's folder beside bin. Written 2026-11-10 by Melanikus for
// the AvP port (#175). This file only reads its text. Finding the file and using what it says is relay_list.cpp's.
//
// WHY A FILE: the owner's rulings, 2026-11-09 and 2026-11-10. "We'll ship a servers.cfg with plaintext IP so community can share 'add this
// IP' for new servers", because "this has to be picked up years from now, may run in someone's box, we can't rely on real certificate".
// One file only: "10 years from now someone may deploy it and say 'Ok replace the original v1.x config with new server ips'". And short:
// "Must be something simple we can easily instruct ppl to switch/replace as there is no server discovery". So a server is one short line
// of plain text that a person can paste: where the server is, one number that proves it's the right one, and its name. Nothing in it
// depends on a name service or a certificate authority still being there in twenty years, and whoever has the game can change the file
// with Notepad.
//
// ONE LINE, ONE SERVER, three fields with any run of spaces or tabs between them (chat programs turn tabs into spaces):
//   address  fingerprint  name
// For example:
//   203.0.113.7  0123...cdef  New York
//   1. address: one plain IPv4 address. The server's list answers HTTPS on port 443 there, and its relay takes the game's first message
//      on port 27500, unless the line says both ports: address:listport:relayport (each from 1 to 65535). One address only, so a line
//      can never send the game anywhere but the one machine it names.
//   2. fingerprint: 64 small hex digits. The SHA-256 of the server's two keys, the list's and the relay's, worked out the one way the
//      server's CRYPTO.md says ("the servers.cfg fingerprint"). The game checks it against what the server shows it, before it sends that
//      server anything that matters (relay_list.cpp). One number covers both keys.
//   3. name: the rest of the line, as people see it: 1 to 31 printable characters. Spaces inside it are fine: each run of spaces or tabs
//      in it counts as one space, so a name pasted through a chat program is still the same name. It can't start with #.
// A line that starts with # is a note for people, and the game skips it. So does an empty line.
//
// AN ADDRESS is four numbers from 0 to 255 with dots, no leading zeros, and never a name. A name is an address whoever holds the name
// chooses on the day, so a lapsed name would send every player who has the line somewhere else (the owner's ruling, 2026-11-09). A server
// that moves gets a new line. Some addresses can never be a server's: "this network" (0/8), broadcast, multicast, the reserved block
// (240/4), and a carrier's own network (100.64/10), which isn't the player's network. The player's own machine (127/8) and their own
// network (10/8, 172.16/12, 192.168/16, 169.254/16) may be, for a server run at home, and the game is to say so before it uses the line
// (#175's yes-screen). The file a release ships is to have none of those: tools/check-online-play.ps1, with #175's last part, refuses them.
//
// READ AS HOSTILE (AVP Tao 3): anyone can write this file, and it passes through a download. So:
//   - a file over 16 KB, or of more than 64 lines, is refused whole: nothing in it is used;
//   - the lines are split at each newline and nowhere else, so nothing inside a line can move where a line starts or ends. A line may
//     end with a carriage return before its newline (Windows' own line end), UTF-8's mark at the very start of the file (which some
//     editors write) is skipped, and spaces and tabs at either end of a line don't count;
//   - a line holding any other byte than a printable character, a space or a tab (an accent, a lone carriage return, a 0) is refused,
//     note or not, and the other lines still count. So one bad line never leaves a player with no servers at all;
//   - a line with a field missing, or a field of the wrong shape, is refused whole, and the other lines still count;
//   - two lines that share a name (capitals and small letters counting as the same) or an address are both refused. One server can't be
//     passed off as another, and neither of the two is guessed at;
//   - every refusal is told with the line's number and a reason in the game's own words, never what the line said, which could be
//     anything at all (ServersCfg_LineWords), in the log and on the screen, so someone following an instruction can see what to fix;
//   - reading a line contacts nothing. Until #175's yes-screen, where the player says yes to a line before it's used, only a game started
//     with -dev uses one at all (relay_list.cpp's Relay_ServerName), so a player's game reaches no server.
// Nothing in a line is ever run, followed or used for anything but what's written above.

#ifndef SERVERS_CFG_H
#define SERVERS_CFG_H

#include <stddef.h>
#include <stdint.h>

// The file's name, as the log and the screens give it
#define SERVERS_CFG_FILE_NAME "servers.cfg"

// The most the file may be: its size in bytes, and how many lines it has, notes and empty lines counted
int const kServersCfgMostBytes = 16 * 1024;
int const kServersCfgMostLines = 64;

// The ports a line means when it names none
int const kServersCfgListPort = 443;
int const kServersCfgRelayPort = 27500;

// A line's name, and its fingerprint's size
int const kServersCfgNameLetters = 31;
int const kServersCfgFingerprintBytes = 32;   // SHA-256

// Where a server is, for the words the game shows before a player uses it: on the internet, on this PC, or on the player's own network
enum ServersCfgPlace
{
	kServersCfgOnTheInternet,
	kServersCfgOnThisPC,          // 127/8
	kServersCfgOnTheOwnNetwork    // 10/8, 172.16/12, 192.168/16 and 169.254/16
};

// One server, from a line that passed every rule
struct ServersCfgServer
{
	char name[kServersCfgNameLetters + 1];   // as the game shows it: each run of spaces in the line's name is one space here
	uint32_t address;                        // its first number in the highest byte (192.168.0.1 is 0xC0A80001)
	ServersCfgPlace place;
	int listPort;
	int relayPort;
	uint8_t fingerprint[kServersCfgFingerprintBytes];
	int lineNumber;                          // counted from 1, as an editor shows it
};

// Why the whole file was refused, or that it wasn't
enum ServersCfgFileRefusal
{
	kServersCfgFileRead,
	kServersCfgFileTooBig,         // over kServersCfgMostBytes
	kServersCfgFileTooManyLines,   // over kServersCfgMostLines
	kServersCfgFileNoText          // a size with no text: never read
};

// Why one line was refused
enum ServersCfgLineRefusal
{
	kServersCfgLineGood,
	kServersCfgBadByte,            // a byte that isn't a printable character, a space or a tab (a note's too)
	kServersCfgFieldMissing,       // fewer than the three fields
	kServersCfgBadAddress,         // not one plain IPv4 address, alone or with both ports
	kServersCfgNeverAServer,       // 0/8, broadcast, multicast, 240/4 or 100.64/10
	kServersCfgBadFingerprint,     // not 64 small hex digits
	kServersCfgBadName,            // over 31 letters, or starting with #
	kServersCfgNameShared,         // another good line has the same name
	kServersCfgAddressShared       // another good line has the same address
};

// What the file said: the servers from its good lines, and each refusal, both in the file's order. A file refused whole has no
// servers and no line refusals, only fileRefusal.
struct ServersCfgLineNote
{
	int lineNumber;
	ServersCfgLineRefusal why;
};

struct ServersCfgFileRead
{
	ServersCfgFileRefusal fileRefusal;
	ServersCfgServer servers[kServersCfgMostLines];
	int serverCount;
	ServersCfgLineNote refused[kServersCfgMostLines];
	int refusedCount;
};

// Reads the file's text: bytes of it at text, which needs no 0 at its end (a 0 inside it refuses its line). Everything in out is
// written, whatever the text, and nothing else is.
void ServersCfg_Read(const char *text, size_t bytes, ServersCfgFileRead *out);

// The words for a refusal, for the log, in the game's own words: never anything from the line. Every refusal has words.
const char *ServersCfg_FileWords(ServersCfgFileRefusal why);
const char *ServersCfg_LineWords(ServersCfgLineRefusal why);

#endif
