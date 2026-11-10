// The player's yes to a servers.cfg line: what it's the hash of, and the kept file's text. Written 2026-11-10 by Melanikus for the AvP
// port (#175's last part). servers_trust.h says why, and what the file may hold.

#include "servers_trust.h"

#include <string.h>

namespace
{
	// The note the game writes at the top of the file, for anyone who opens it
	char const kTheNote[] = "# servers-trusted.cfg: the servers.cfg lines this player said yes to, each kept as a hash and never as an address. The game writes it.\n";

	// And at the top of the pick's file (added 2026-11-10, #175 2b)
	char const kThePickNote[] = "# servers-picked.cfg: the servers.cfg line this player picked for the next start, kept as a hash and never as an address. The game writes it.\n";

	char const kHexDigits[] = "0123456789abcdef";

	// A small hex digit's value, or -1 for anything else (capitals included: the game writes small ones only)
	int HexValue(char c)
	{
		if (c >= '0' && c <= '9')
		{
			return c - '0';
		}

		if (c >= 'a' && c <= 'f')
		{
			return c - 'a' + 10;
		}

		return -1;
	}

	// One line's text, its line end taken off (and a carriage return just before it): a note, a hash line into hash, or neither. word is
	// what a hash line starts with: "yes " in the yeses' file, "pick " in the pick's (modified 2026-11-10, #175 2b), so neither file's line
	// is ever taken in the other.
	enum LineKind
	{
		kLineNote,
		kLineYes,
		kLineBad
	};

	LineKind ReadLine(const char *line, size_t size, const char *word, uint8_t hash[kServersTrustHashBytes])
	{
		// Every byte printable, or a tab in a note. A carriage return here is one that wasn't just before the newline.
		for (size_t i = 0; i < size; i++)
		{
			unsigned char const c = static_cast<unsigned char>(line[i]);

			if (!((c >= 32 && c <= 126) || (c == '\t' && size > 0 && line[0] == '#')))
			{
				return kLineBad;
			}
		}

		if (size > 0 && line[0] == '#')
		{
			return kLineNote;
		}

		size_t const wordLetters = strlen(word);   // "yes " or "pick "

		if (size != wordLetters + 2 * kServersTrustHashBytes || memcmp(line, word, wordLetters) != 0)
		{
			return kLineBad;
		}

		for (size_t i = 0; i < kServersTrustHashBytes; i++)
		{
			int const high = HexValue(line[wordLetters + 2 * i]);
			int const low = HexValue(line[wordLetters + 2 * i + 1]);

			if (high < 0 || low < 0)
			{
				return kLineBad;
			}

			hash[i] = static_cast<uint8_t>((high << 4) | low);
		}

		return kLineYes;
	}

	// The same hash, every byte compared: hashes aren't secrets, but it costs nothing
	bool Same(const uint8_t *a, const uint8_t *b)
	{
		uint8_t differences = 0;

		for (size_t i = 0; i < kServersTrustHashBytes; i++)
		{
			differences |= static_cast<uint8_t>(a[i] ^ b[i]);
		}

		return differences == 0;
	}
}

bool ServersTrust_HashInput(uint32_t address, int listPort, int relayPort, const uint8_t fingerprint[kServersTrustFingerprintBytes],
	uint8_t out[kServersTrustInputBytes])
{
	if (!out)
	{
		return false;
	}

	if (!fingerprint || listPort < 1 || listPort > 65535 || relayPort < 1 || relayPort > 65535)
	{
		memset(out, 0, kServersTrustInputBytes);
		return false;
	}

	size_t at = 0;
	memcpy(out + at, kServersTrustWords, sizeof(kServersTrustWords) - 1);
	at += sizeof(kServersTrustWords) - 1;
	out[at++] = 0;
	out[at++] = static_cast<uint8_t>(address >> 24);
	out[at++] = static_cast<uint8_t>(address >> 16);
	out[at++] = static_cast<uint8_t>(address >> 8);
	out[at++] = static_cast<uint8_t>(address);
	out[at++] = static_cast<uint8_t>(listPort >> 8);
	out[at++] = static_cast<uint8_t>(listPort);
	out[at++] = static_cast<uint8_t>(relayPort >> 8);
	out[at++] = static_cast<uint8_t>(relayPort);
	memcpy(out + at, fingerprint, kServersTrustFingerprintBytes);
	return true;
}

// Reads either file's text: notes, and lines of word and a hash, at most most of them, into out (modified 2026-11-10 by Melanikus for the
// AvP port, #175 2b: it was ServersTrust_Read's own, and the pick's file is read the same way). servers_trust.h says what's refused.
static ServersTrustRefusal ReadHashes(const char *text, size_t bytes, const char *word, int most, ServersTrustYeses *out, int *badLine)
{
	if (badLine)
	{
		*badLine = 0;
	}

	if (!out)
	{
		return kServersTrustBadLine;
	}

	out->count = 0;

	if (bytes > static_cast<size_t>(kServersTrustMostBytes))
	{
		return kServersTrustTooBig;
	}

	if (bytes > 0 && !text)
	{
		return kServersTrustBadLine;
	}

	ServersTrustYeses read;
	read.count = 0;
	size_t start = 0;
	int lineNumber = 0;

	while (start < bytes)
	{
		lineNumber++;
		size_t end = start;

		while (end < bytes && text[end] != '\n')
		{
			end++;
		}

		// The last line must end with its newline, as the game writes it
		if (end == bytes)
		{
			if (badLine) { *badLine = lineNumber; }
			return kServersTrustBadLine;
		}

		size_t size = end - start;

		if (size > 0 && text[start + size - 1] == '\r')
		{
			size--;
		}

		uint8_t hash[kServersTrustHashBytes];
		LineKind const kind = ReadLine(text + start, size, word, hash);

		if (kind == kLineBad)
		{
			if (badLine) { *badLine = lineNumber; }
			return kServersTrustBadLine;
		}

		if (kind == kLineYes)
		{
			if (ServersTrust_Has(read, hash))
			{
				if (badLine) { *badLine = lineNumber; }
				return kServersTrustTwice;
			}

			// (never more than the yeses' room, whatever most says)
			if (read.count >= most || read.count >= kServersTrustMostYeses)
			{
				if (badLine) { *badLine = lineNumber; }
				return kServersTrustTooMany;
			}

			memcpy(read.hashes[read.count], hash, kServersTrustHashBytes);
			read.count++;
		}

		start = end + 1;
	}

	*out = read;
	return kServersTrustRead;
}

ServersTrustRefusal ServersTrust_Read(const char *text, size_t bytes, ServersTrustYeses *out, int *badLine)
{
	return ReadHashes(text, bytes, "yes ", kServersTrustMostYeses, out, badLine);
}

// ---- The pick (#175 2b). Added 2026-11-10 by Melanikus for the AvP port: see servers_trust.h.

ServersTrustRefusal ServersTrust_ReadPick(const char *text, size_t bytes, ServersTrustPick *out, int *badLine)
{
	if (!out)
	{
		if (badLine) { *badLine = 0; }
		return kServersTrustBadLine;
	}

	out->there = false;
	memset(out->hash, 0, sizeof(out->hash));

	// At most one pick: a second, the same or another, refuses the file. Off the stack for its size; only the main thread reads it.
	static ServersTrustYeses picks;
	ServersTrustRefusal const read = ReadHashes(text, bytes, "pick ", 1, &picks, badLine);

	if (read == kServersTrustTooMany || read == kServersTrustTwice)
	{
		return kServersTrustTwoPicks;
	}

	if (read != kServersTrustRead)
	{
		return read;
	}

	if (picks.count == 1)
	{
		out->there = true;
		memcpy(out->hash, picks.hashes[0], kServersTrustHashBytes);
	}

	return kServersTrustRead;
}

size_t ServersTrust_WritePick(const uint8_t hash[kServersTrustHashBytes], char *out, size_t room)
{
	size_t const noteBytes = sizeof(kThePickNote) - 1;
	size_t const need = noteBytes + 5 + 2 * kServersTrustHashBytes + 1;   // the note, "pick ", the digits, the newline

	if (!hash || !out || room < need)
	{
		return 0;
	}

	memcpy(out, kThePickNote, noteBytes);
	size_t at = noteBytes;
	memcpy(out + at, "pick ", 5);
	at += 5;

	for (size_t b = 0; b < kServersTrustHashBytes; b++)
	{
		out[at++] = kHexDigits[hash[b] >> 4];
		out[at++] = kHexDigits[hash[b] & 0x0F];
	}

	out[at++] = '\n';
	return at;
}

int ServersTrust_LineToUse(const uint8_t (*hashes)[kServersTrustHashBytes], const bool *hashed, int count, const ServersTrustPick &pick)
{
	if (!hashes || !hashed || count <= 0)
	{
		return -1;
	}

	// Two good lines never have the same hash: it's over the address, and servers.cfg refuses two lines with one address (servers_cfg.h)
	for (int i = 0; pick.there && i < count; i++)
	{
		if (hashed[i] && Same(hashes[i], pick.hash))
		{
			return i;
		}
	}

	return 0;
}

bool ServersTrust_Has(const ServersTrustYeses &yeses, const uint8_t hash[kServersTrustHashBytes])
{
	if (!hash)
	{
		return false;
	}

	bool found = false;

	for (int i = 0; i < yeses.count && i < kServersTrustMostYeses; i++)
	{
		found = Same(yeses.hashes[i], hash) || found;
	}

	return found;
}

bool ServersTrust_Add(ServersTrustYeses *yeses, const uint8_t hash[kServersTrustHashBytes])
{
	if (!yeses || !hash || yeses->count < 0)
	{
		return false;
	}

	if (ServersTrust_Has(*yeses, hash))
	{
		return true;
	}

	if (yeses->count >= kServersTrustMostYeses)
	{
		return false;
	}

	memcpy(yeses->hashes[yeses->count], hash, kServersTrustHashBytes);
	yeses->count++;
	return true;
}

size_t ServersTrust_Write(const ServersTrustYeses &yeses, char *out, size_t room)
{
	size_t const noteBytes = sizeof(kTheNote) - 1;
	size_t const lineBytes = 4 + 2 * kServersTrustHashBytes + 1;   // "yes ", the digits, the newline
	int const count = yeses.count < 0 ? 0 : (yeses.count > kServersTrustMostYeses ? kServersTrustMostYeses : yeses.count);
	size_t const need = noteBytes + static_cast<size_t>(count) * lineBytes;

	if (!out || room < need)
	{
		return 0;
	}

	memcpy(out, kTheNote, noteBytes);
	size_t at = noteBytes;

	for (int i = 0; i < count; i++)
	{
		memcpy(out + at, "yes ", 4);
		at += 4;

		for (size_t b = 0; b < kServersTrustHashBytes; b++)
		{
			out[at++] = kHexDigits[yeses.hashes[i][b] >> 4];
			out[at++] = kHexDigits[yeses.hashes[i][b] & 0x0F];
		}

		out[at++] = '\n';
	}

	return at;
}

const char *ServersTrust_Words(ServersTrustRefusal why)
{
	switch (why)
	{
	case kServersTrustRead:
		return "it was read";
	case kServersTrustTooBig:
		return "it's bigger than the game ever writes it";
	case kServersTrustBadLine:
		return "a line in it isn't one the game writes";
	case kServersTrustTooMany:
		return "it holds more yeses than the game ever keeps";
	case kServersTrustTwice:
		return "it holds the same yes twice, which the game never writes";
	case kServersTrustTwoPicks:
		return "it holds more than one pick, which the game never writes";
	}

	return "it couldn't be read";
}
