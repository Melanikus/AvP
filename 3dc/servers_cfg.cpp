// Reads servers.cfg's text (servers_cfg.h says what it is, and every rule). Written 2026-11-10 by Melanikus for the AvP port (#175). It
// reads text it's given and nothing else: no file, no network, no settings. relay_list.cpp reads the file.
//
// PASTE-SAFE: people pass these lines to each other through chat programs, which turn tabs into spaces, and save them with editors that
// add Windows' line ends or a mark at the start. So a run of spaces or tabs is one gap between fields, a line may end with a carriage
// return before its newline, spaces at either end of a line don't count, and UTF-8's mark at the very start of the file is skipped. A
// line holding anything else is refused on its own, and the rest of the file still counts.

#include "servers_cfg.h"

#include "relay_list_text.h"

#include <string.h>

namespace
{
	// UTF-8's byte order mark, which some editors put at the start of every file they save
	unsigned char const kByteOrderMark[3] = { 0xEF, 0xBB, 0xBF };

	// The longest an address can be when it's right: "255.255.255.255". One longer than that is wrong however it goes on, so it's never
	// copied anywhere.
	size_t const kMostIPv4Letters = 15;

	// A piece of the text: where it starts, and how long it is. Nothing here needs a 0 at the end of a piece.
	struct Piece
	{
		const char *at;
		size_t length;
	};

	bool IsGap(char letter)
	{
		return letter == ' ' || letter == '\t';
	}

	// The bytes a line may hold: the printable ones, from the space to the ~, and a tab
	bool IsLineByte(unsigned char byte)
	{
		return (byte >= 0x20 && byte <= 0x7E) || byte == '\t';
	}

	// A piece without the spaces and tabs at either end
	Piece Trimmed(Piece piece)
	{
		while (piece.length > 0 && IsGap(piece.at[0]))
		{
			piece.at++;
			piece.length--;
		}

		while (piece.length > 0 && IsGap(piece.at[piece.length - 1]))
		{
			piece.length--;
		}

		return piece;
	}

	// The next field of a line, from where *rest starts: the letters up to the next gap. *rest moves past it and the gap after it. An
	// empty piece when there's nothing left.
	Piece NextField(Piece *rest)
	{
		Piece field = { rest->at, 0 };

		while (field.length < rest->length && !IsGap(rest->at[field.length]))
		{
			field.length++;
		}

		size_t skip = field.length;

		while (skip < rest->length && IsGap(rest->at[skip]))
		{
			skip++;
		}

		rest->at += skip;
		rest->length -= skip;
		return field;
	}

	// A small hex digit's value, or -1 for anything else. Capitals are refused: the file is written one way only, so a fingerprint can't
	// be written two ways that look different and mean the same.
	int SmallHexValue(char letter)
	{
		if (letter >= '0' && letter <= '9')
		{
			return letter - '0';
		}

		if (letter >= 'a' && letter <= 'f')
		{
			return letter - 'a' + 10;
		}

		return -1;
	}

	// Exactly 2 * size small hex digits, into bytes. False, with bytes left as they were, for anything else.
	bool ReadSmallHex(Piece piece, uint8_t *bytes, size_t size)
	{
		if (piece.length != 2 * size)
		{
			return false;
		}

		for (size_t i = 0; i < piece.length; i++)
		{
			if (SmallHexValue(piece.at[i]) < 0)
			{
				return false;
			}
		}

		for (size_t i = 0; i < size; i++)
		{
			bytes[i] = static_cast<uint8_t>((SmallHexValue(piece.at[2 * i]) << 4) | SmallHexValue(piece.at[2 * i + 1]));
		}

		return true;
	}

	// A port: 1 to 5 digits, no leading zero, from 1 to 65535. False for anything else.
	bool ReadPort(Piece piece, int *port)
	{
		if (piece.length < 1 || piece.length > 5 || piece.at[0] == '0')
		{
			return false;
		}

		int value = 0;

		for (size_t i = 0; i < piece.length; i++)
		{
			if (piece.at[i] < '0' || piece.at[i] > '9')
			{
				return false;
			}

			value = value * 10 + (piece.at[i] - '0');
		}

		if (value > 65535)
		{
			return false;
		}

		*port = value;
		return true;
	}

	// The address field: one plain IPv4 address alone (relay_list_text.h's RelayList_IPv4Refusal, the game's one reader of an address),
	// meaning the usual two ports, or with both ports after it, address:listport:relayport. Then where the address is: an address no
	// server can have is refused.
	ServersCfgLineRefusal ReadAddress(Piece piece, ServersCfgServer *server)
	{
		// No colon, or two. One alone would leave which port it is to a guess. A third makes the relay's port a port with a colon in
		// it, which ReadPort refuses.
		const char *const firstColon = static_cast<const char *>(memchr(piece.at, ':', piece.length));
		size_t const addressLetters = firstColon ? static_cast<size_t>(firstColon - piece.at) : piece.length;
		int listPort = kServersCfgListPort;
		int relayPort = kServersCfgRelayPort;

		if (firstColon)
		{
			Piece const ports = { firstColon + 1, piece.length - addressLetters - 1 };
			const char *const secondColon = static_cast<const char *>(memchr(ports.at, ':', ports.length));

			if (!secondColon)
			{
				return kServersCfgBadAddress;
			}

			Piece const listPiece = { ports.at, static_cast<size_t>(secondColon - ports.at) };
			Piece const relayPiece = { secondColon + 1, ports.length - listPiece.length - 1 };

			if (!ReadPort(listPiece, &listPort) || !ReadPort(relayPiece, &relayPort))
			{
				return kServersCfgBadAddress;
			}
		}

		// The copy below has room for the longest address and its 0. An empty one is refused by the reader of an address.
		if (addressLetters > kMostIPv4Letters)
		{
			return kServersCfgBadAddress;
		}

		// The address on its own, with a 0 after it, for the reader of an address
		char address[kMostIPv4Letters + 1];
		memcpy(address, piece.at, addressLetters);
		address[addressLetters] = '\0';

		uint32_t value = 0;

		if (RelayList_IPv4Refusal(address, &value) != kRelayIPv4Read)
		{
			return kServersCfgBadAddress;
		}

		// Where it is. RelayList_AddressRefusal's private block holds 100.64/10 too, a carrier's network, which isn't the player's own
		// network: that one is never a server's here.
		bool const carriersNetwork = (value & 0xFFC00000u) == 0x64400000u;
		ServersCfgPlace place = kServersCfgOnTheInternet;

		switch (RelayList_AddressRefusal(value))
		{
			case kRelayAddressUsable:
				place = kServersCfgOnTheInternet;
				break;

			case kRelayAddressLoopback:
				place = kServersCfgOnThisPC;
				break;

			case kRelayAddressPrivate:
				if (carriersNetwork)
				{
					return kServersCfgNeverAServer;
				}

				place = kServersCfgOnTheOwnNetwork;
				break;

			default:
				// "This network", broadcast, multicast and the reserved block: never one machine to send to
				return kServersCfgNeverAServer;
		}

		server->address = value;
		server->place = place;
		server->listPort = listPort;
		server->relayPort = relayPort;
		return kServersCfgLineGood;
	}

	// The name: what's left of the line after its other fields, already trimmed. Each run of spaces or tabs inside it becomes one space,
	// so a name pasted through a chat program is the same name. False if it's longer than kServersCfgNameLetters after that, or starts
	// with #, which would look like a note.
	bool ReadName(Piece piece, char name[kServersCfgNameLetters + 1])
	{
		if (piece.length == 0 || piece.at[0] == '#')
		{
			return false;
		}

		size_t letters = 0;

		for (size_t i = 0; i < piece.length; i++)
		{
			bool const gap = IsGap(piece.at[i]);

			if (gap && i > 0 && IsGap(piece.at[i - 1]))
			{
				continue;
			}

			if (letters == static_cast<size_t>(kServersCfgNameLetters))
			{
				return false;
			}

			name[letters++] = gap ? ' ' : piece.at[i];
		}

		name[letters] = '\0';
		return true;
	}

	// A small letter for a capital, and every other letter as it is, so two names that differ only in capitals count as the same
	char Lower(char letter)
	{
		return letter >= 'A' && letter <= 'Z' ? static_cast<char>(letter - 'A' + 'a') : letter;
	}

	bool SameName(const char *a, const char *b)
	{
		for (size_t i = 0;; i++)
		{
			if (Lower(a[i]) != Lower(b[i]))
			{
				return false;
			}

			if (a[i] == '\0')
			{
				return true;
			}
		}
	}

	// ---- THE LINE'S LAYOUT: address, fingerprint, then the name, which is the rest of the line and so may hold spaces
	ServersCfgLineRefusal ReadLine(Piece line, ServersCfgServer *server)
	{
		Piece rest = line;
		Piece const addressField = NextField(&rest);
		Piece const fingerprintField = NextField(&rest);

		// The name is what's left: nothing left means a field is missing
		if (fingerprintField.length == 0 || rest.length == 0)
		{
			return kServersCfgFieldMissing;
		}

		ServersCfgLineRefusal const address = ReadAddress(addressField, server);

		if (address != kServersCfgLineGood)
		{
			return address;
		}

		if (!ReadSmallHex(fingerprintField, server->fingerprint, kServersCfgFingerprintBytes))
		{
			return kServersCfgBadFingerprint;
		}

		if (!ReadName(rest, server->name))
		{
			return kServersCfgBadName;
		}

		return kServersCfgLineGood;
	}

	// Whether a note is one a line that passed on its own can have: good, or refused only for sharing with another
	bool PassedOnItsOwn(ServersCfgLineRefusal why)
	{
		return why == kServersCfgLineGood || why == kServersCfgNameShared || why == kServersCfgAddressShared;
	}
}

void ServersCfg_Read(const char *text, size_t bytes, ServersCfgFileRead *out)
{
	memset(out, 0, sizeof(*out));
	out->fileRefusal = kServersCfgFileRead;

	if (bytes == 0)
	{
		return;
	}

	if (!text)
	{
		out->fileRefusal = kServersCfgFileNoText;
		return;
	}

	if (bytes > static_cast<size_t>(kServersCfgMostBytes))
	{
		out->fileRefusal = kServersCfgFileTooBig;
		return;
	}

	// UTF-8's mark at the very start is skipped. Anywhere else, its bytes refuse their line.
	size_t first = 0;

	if (bytes >= sizeof(kByteOrderMark) && memcmp(text, kByteOrderMark, sizeof(kByteOrderMark)) == 0)
	{
		first = sizeof(kByteOrderMark);
	}

	// The lines are counted before any is read, at the newlines and nowhere else. The last line needs no newline after it.
	int lines = 0;

	for (size_t i = first; i < bytes; i++)
	{
		if (text[i] == '\n')
		{
			lines++;
		}
	}

	if (bytes > first && text[bytes - 1] != '\n')
	{
		lines++;
	}

	if (lines > kServersCfgMostLines)
	{
		out->fileRefusal = kServersCfgFileTooManyLines;
		return;
	}

	// Each line in turn, held to the layout on its own. There are at most kServersCfgMostLines, so these always have room.
	ServersCfgServer candidates[kServersCfgMostLines];
	ServersCfgLineNote notes[kServersCfgMostLines];
	int count = 0;
	size_t start = first;
	int lineNumber = 0;

	while (start < bytes)
	{
		const char *const newline = static_cast<const char *>(memchr(text + start, '\n', bytes - start));
		size_t end = newline ? static_cast<size_t>(newline - text) : bytes;
		size_t const next = end + 1;
		lineNumber++;

		// Windows' line end: one carriage return right before the newline, or at the very end of the file
		if (end > start && text[end - 1] == '\r')
		{
			end--;
		}

		Piece const raw = { text + start, end - start };
		start = next;

		bool clean = true;

		for (size_t i = 0; i < raw.length; i++)
		{
			clean = clean && IsLineByte(static_cast<unsigned char>(raw.at[i]));
		}

		memset(&candidates[count], 0, sizeof(candidates[count]));
		candidates[count].lineNumber = lineNumber;
		notes[count].lineNumber = lineNumber;

		if (!clean)
		{
			notes[count].why = kServersCfgBadByte;
			count++;
			continue;
		}

		// An empty line, and a note for people
		Piece const line = Trimmed(raw);

		if (line.length == 0 || line.at[0] == '#')
		{
			continue;
		}

		notes[count].why = ReadLine(line, &candidates[count]);
		count++;
	}

	// Two lines that share a name or an address are both refused: neither is the real one by coming first
	for (int i = 0; i < count; i++)
	{
		for (int j = i + 1; j < count; j++)
		{
			if (!PassedOnItsOwn(notes[i].why) || !PassedOnItsOwn(notes[j].why))
			{
				continue;
			}

			if (SameName(candidates[i].name, candidates[j].name))
			{
				notes[i].why = notes[j].why = kServersCfgNameShared;
			}
			else if (candidates[i].address == candidates[j].address)
			{
				notes[i].why = notes[i].why == kServersCfgLineGood ? kServersCfgAddressShared : notes[i].why;
				notes[j].why = notes[j].why == kServersCfgLineGood ? kServersCfgAddressShared : notes[j].why;
			}
		}
	}

	for (int i = 0; i < count; i++)
	{
		if (notes[i].why == kServersCfgLineGood)
		{
			out->servers[out->serverCount++] = candidates[i];
		}
		else
		{
			out->refused[out->refusedCount++] = notes[i];
		}
	}
}

const char *ServersCfg_FileWords(ServersCfgFileRefusal why)
{
	switch (why)
	{
		case kServersCfgFileRead: return "it was read";
		case kServersCfgFileTooBig: return "it's over 16 KB, so none of it is used";
		case kServersCfgFileTooManyLines: return "it has more than 64 lines, so none of it is used";
		case kServersCfgFileNoText: return "it couldn't be read";
	}

	return "it was refused for a reason this game has no words for";
}

const char *ServersCfg_LineWords(ServersCfgLineRefusal why)
{
	switch (why)
	{
		case kServersCfgLineGood: return "it's good";
		case kServersCfgBadByte: return "it has a letter that isn't plain English text (an accent, or a hidden character)";
		case kServersCfgFieldMissing: return "it needs an address, a fingerprint and a name, with spaces between them";
		case kServersCfgBadAddress: return "its address isn't four numbers with dots, such as 203.0.113.7, alone or with :listport:relayport";
		case kServersCfgNeverAServer: return "its address can't be a server's";
		case kServersCfgBadFingerprint: return "its fingerprint isn't 64 small hex digits (0 to 9 and a to f)";
		case kServersCfgBadName: return "its name is over 31 letters, or starts with #";
		case kServersCfgNameShared: return "another line has the same name, so neither is used";
		case kServersCfgAddressShared: return "another line has the same address, so neither is used";
	}

	return "it was refused for a reason this game has no words for";
}
