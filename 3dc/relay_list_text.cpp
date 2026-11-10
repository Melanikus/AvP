// The words that go to and from the relay server's list. relay_list_text.h says what this is for, and what it refuses.
// Written 2026-11-08 by Melanikus for the AvP port.

#include "relay_list_text.h"

#include <stdio.h>
#include <string.h>

namespace
{
	// ---- Reading an answer

	// Where the reading has got to in an answer, and where the answer ends. Every step looks at the end before it reads.
	struct Reader
	{
		const char *at;
		const char *end;
	};

	bool IsSpace(char c)
	{
		return c == ' ' || c == '\t' || c == '\r' || c == '\n';
	}

	// The next character that isn't a space, without moving past it. 0 at the end of the answer: an answer with a 0 in it is refused
	// before any reading starts, so a 0 here can only mean the end.
	char Next(Reader &r)
	{
		while (r.at < r.end && IsSpace(*r.at))
		{
			r.at++;
		}

		return r.at < r.end ? *r.at : 0;
	}

	// The next character that isn't a space has to be this one, and the reading moves past it
	RelayListRefusal Expect(Reader &r, char wanted)
	{
		char const c = Next(r);

		if (c == 0)
		{
			return kRelayListEndsTooSoon;
		}

		if (c != wanted)
		{
			return kRelayListNotWhatComesNext;
		}

		r.at++;
		return kRelayListAccepted;
	}

	// A string, "like this". The list writes only printable ASCII, and the only escapes it ever writes for that are \" and \\, so any
	// other escape is refused, and so is any character outside printable ASCII. At most `most` characters, into out, which has room for
	// most + 1, the 0 at the end included. More than that is kRelayListBadText.
	RelayListRefusal ReadString(Reader &r, char *out, size_t most, size_t *length)
	{
		RelayListRefusal const why = Expect(r, '"');

		if (why != kRelayListAccepted)
		{
			return why;
		}

		size_t n = 0;

		for (;;)
		{
			if (r.at >= r.end)
			{
				return kRelayListEndsTooSoon;
			}

			char c = *r.at++;

			if (c == '"')
			{
				break;
			}

			if (c == '\\')
			{
				if (r.at >= r.end)
				{
					return kRelayListEndsTooSoon;
				}

				c = *r.at++;

				if (c != '"' && c != '\\')
				{
					return kRelayListBadEscape;
				}
			}
			else
			{
				// The game is built with /J, so a char is already 0 to 255, but this says so rather than relying on it
				unsigned char const byte = static_cast<unsigned char>(c);

				if (byte < 0x20 || byte > 0x7E)
				{
					return kRelayListBadText;
				}
			}

			if (n >= most)
			{
				return kRelayListBadText;
			}

			out[n++] = c;
		}

		out[n] = '\0';
		*length = n;
		return kRelayListAccepted;
	}

	// A whole number from least to most. Only digits: no sign, no point, no exponent, and no 0 in front of another digit, which is what
	// the list writes. Nine digits at most, which is past every limit here and can't overflow.
	RelayListRefusal ReadNumber(Reader &r, long least, long most, long *value)
	{
		if (Next(r) == 0)
		{
			return kRelayListEndsTooSoon;
		}

		const char *const start = r.at;
		long v = 0;

		while (r.at < r.end && *r.at >= '0' && *r.at <= '9')
		{
			if (r.at - start >= 9)
			{
				return kRelayListBadNumber;
			}

			v = v * 10 + (*r.at - '0');
			r.at++;
		}

		size_t const digits = static_cast<size_t>(r.at - start);
		char const after = r.at < r.end ? *r.at : 0;

		if (digits == 0 || (digits > 1 && *start == '0') || after == '.' || after == 'e' || after == 'E' || after == '-' || after == '+')
		{
			return kRelayListBadNumber;
		}

		if (v < least || v > most)
		{
			return kRelayListBadNumber;
		}

		*value = v;
		return kRelayListAccepted;
	}

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

	// An owner: 32 hex digits, small letters, as 16 bytes
	RelayListRefusal ReadSecret(Reader &r, uint8_t *secret)
	{
		char hex[kRelayListSecretBytes * 2 + 1];
		size_t length = 0;
		RelayListRefusal const why = ReadString(r, hex, sizeof(hex) - 1, &length);

		if (why == kRelayListBadText)
		{
			return kRelayListBadSecret;
		}

		if (why != kRelayListAccepted)
		{
			return why;
		}

		if (length != sizeof(hex) - 1)
		{
			return kRelayListBadSecret;
		}

		for (size_t i = 0; i < kRelayListSecretBytes; i++)
		{
			int const high = HexValue(hex[i * 2]);
			int const low = HexValue(hex[i * 2 + 1]);

			if (high < 0 || low < 0)
			{
				return kRelayListBadSecret;
			}

			secret[i] = static_cast<uint8_t>(high * 16 + low);
		}

		return kRelayListAccepted;
	}

	// What kind of thing a field holds
	enum Kind
	{
		kKindText,     // printable ASCII, 1 to mostLetters characters
		kKindRegion,
		kKindCode,
		kKindHost,
		kKindNumber,   // a whole number from least to most
		kKindFingerprint,  // an identity's fingerprint: 32 hex digits, small letters, kept as its 16 bytes (an access answer's "fp")
		kKindHostId,       // a host's whole id: 96 hex digits, small letters, kept as its 48 bytes (a game's "id", added 2026-11-10)
		kKindHexBytes      // exactly mostLetters hex digits, small letters, kept as half as many bytes (a relay's keys; added 2026-11-09)
	};

	// The longest hex a field can be: an ML-KEM-1024 public key's 1,568 bytes (added 2026-11-09 by Melanikus for the AvP port)
	size_t const kMostHexLetters = 2 * kNetPqKemPublicBytes;

	// Whether text is exactly letters hex digits in small letters. Only then is it written into bytes, half as many, so a text that's
	// refused leaves bytes as it was (added 2026-11-09 by Melanikus for the AvP port)
	bool HexInto(const char *text, size_t letters, uint8_t *bytes)
	{
		if (letters % 2 != 0)
		{
			return false;
		}

		for (size_t i = 0; i < letters; i++)
		{
			char const c = text[i];

			if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
			{
				return false;
			}
		}

		for (size_t i = 0; i < letters; i += 2)
		{
			char const high = text[i];
			char const low = text[i + 1];
			int const h = high <= '9' ? high - '0' : high - 'a' + 10;
			int const l = low <= '9' ? low - '0' : low - 'a' + 10;
			bytes[i / 2] = static_cast<uint8_t>((h << 4) | l);
		}

		return true;
	}

	// One field an entry has to have: its name, its kind, its limits, and where what it holds goes
	struct FieldRule
	{
		const char *name;
		Kind kind;
		size_t mostLetters;
		long least;
		long most;
		char *text;
		long *number;
		uint8_t *bytes;   // a fingerprint's 16 bytes, or a host id's 48; every other kind leaves it out, which makes it NULL
	};

	// The longest field names are 11 (levelNumber). A name longer than this is no field the protocol has.
	const size_t kMostFieldNameLetters = 16;
	const int kMostFields = 16;

	RelayListRefusal ReadValue(Reader &r, const FieldRule &rule)
	{
		size_t length = 0;
		RelayListRefusal why = kRelayListAccepted;

		switch (rule.kind)
		{
			case kKindText:
				why = ReadString(r, rule.text, rule.mostLetters, &length);
				return (why == kRelayListAccepted && length == 0) ? kRelayListBadText : why;

			case kKindRegion:
				why = ReadString(r, rule.text, kRelayListRegionLetters, &length);
				return (why == kRelayListBadText || (why == kRelayListAccepted && !RelayList_IsRegion(rule.text))) ? kRelayListBadRegion : why;

			case kKindCode:
				why = ReadString(r, rule.text, kRelayCodeLetters, &length);
				return (why == kRelayListBadText || (why == kRelayListAccepted && !Relay_IsCode(rule.text, length))) ? kRelayListBadCode : why;

			case kKindHost:
				why = ReadString(r, rule.text, kRelayListHostLetters, &length);
				return (why == kRelayListBadText || (why == kRelayListAccepted && !RelayList_IsRelayHost(rule.text))) ? kRelayListBadHost : why;

			case kKindNumber:
				return ReadNumber(r, rule.least, rule.most, rule.number);

			case kKindFingerprint:
			{
				// Read as a text first, then taken by net_identity.h's own rule, the one the joining code uses, so the two can never disagree:
				// exactly 32 hex digits, small letters, and nothing after them
				char hex[kNetIdentityFingerprintHexBytes];
				why = ReadString(r, hex, kNetIdentityFingerprintHexBytes - 1, &length);

				if (why == kRelayListBadText || (why == kRelayListAccepted && !NetIdentity_FingerprintFromHex(hex, rule.bytes)))
				{
					return kRelayListBadFingerprint;
				}

				return why;
			}

			case kKindHostId:
			{
				// As a fingerprint is, by net_identity.h's own rule for an id: exactly 96 hex digits, small letters, and nothing after them
				char hex[kNetIdentityIdHexBytes];
				why = ReadString(r, hex, kNetIdentityIdHexBytes - 1, &length);

				if (why == kRelayListBadText || (why == kRelayListAccepted && !NetIdentity_IdFromHex(hex, rule.bytes)))
				{
					return kRelayListBadHostId;
				}

				return why;
			}

			case kKindHexBytes:
			{
				// Added 2026-11-09 by Melanikus for the AvP port: exactly mostLetters hex digits, small letters, as the server writes them. A
				// text one letter longer than that is read as too long, and anything not exactly right is refused, with nothing of it kept.
				char hex[kMostHexLetters + 1];

				if (rule.mostLetters > kMostHexLetters)
				{
					return kRelayListBadText;
				}

				why = ReadString(r, hex, rule.mostLetters, &length);

				if (why == kRelayListAccepted && (length != rule.mostLetters || !HexInto(hex, length, rule.bytes)))
				{
					return kRelayListBadText;
				}

				return why;
			}
		}

		return kRelayListUnknownField;
	}

	// One entry, { ... }, with exactly the fields in rules, in any order
	RelayListRefusal ReadEntry(Reader &r, const FieldRule *rules, int ruleCount)
	{
		char const first = Next(r);

		if (first == 0)
		{
			return kRelayListEndsTooSoon;
		}

		if (first != '{')
		{
			return kRelayListNotAnEntry;
		}

		r.at++;

		if (Next(r) == '}')
		{
			return kRelayListFieldMissing;
		}

		bool seen[kMostFields] = {};

		for (;;)
		{
			char name[kMostFieldNameLetters + 1];
			size_t nameLength = 0;
			RelayListRefusal why = ReadString(r, name, kMostFieldNameLetters, &nameLength);

			// A name too long, or with something odd in it, is no field the protocol has
			if (why == kRelayListBadText)
			{
				return kRelayListUnknownField;
			}

			if (why != kRelayListAccepted)
			{
				return why;
			}

			int field = -1;

			for (int f = 0; f < ruleCount; f++)
			{
				if (strcmp(name, rules[f].name) == 0)
				{
					field = f;
				}
			}

			if (field < 0)
			{
				return kRelayListUnknownField;
			}

			if (seen[field])
			{
				return kRelayListFieldTwice;
			}

			seen[field] = true;

			why = Expect(r, ':');

			if (why == kRelayListAccepted)
			{
				why = ReadValue(r, rules[field]);
			}

			if (why != kRelayListAccepted)
			{
				return why;
			}

			char const c = Next(r);

			if (c == ',')
			{
				r.at++;
				continue;
			}

			if (c == '}')
			{
				r.at++;
				break;
			}

			return c == 0 ? kRelayListEndsTooSoon : kRelayListNotWhatComesNext;
		}

		for (int f = 0; f < ruleCount; f++)
		{
			if (!seen[f])
			{
				return kRelayListFieldMissing;
			}
		}

		return kRelayListAccepted;
	}

	// What's in an answer's one field: a list read entry by entry, or an owner. Each kind reads it with its own function.
	typedef RelayListRefusal (*ValueReader)(Reader &r, void *into, int *count);

	// The whole answer, { "field": ... }, and nothing after it
	RelayListRefusal ReadAnswer(const char *body, size_t bytes, size_t mostBytes, const char *field, ValueReader readValue, void *into, int *count)
	{
		if (bytes > mostBytes)
		{
			return kRelayListTooBig;
		}

		if (!body)
		{
			return kRelayListEndsTooSoon;
		}

		if (bytes > 0 && memchr(body, 0, bytes) != NULL)
		{
			return kRelayListHasAZero;
		}

		Reader r = { body, body + bytes };
		char const first = Next(r);

		if (first == 0)
		{
			return kRelayListEndsTooSoon;
		}

		if (first != '{')
		{
			return kRelayListNotAnObject;
		}

		r.at++;

		if (Next(r) == '}')
		{
			return kRelayListNotItsField;
		}

		char name[kMostFieldNameLetters + 1];
		size_t nameLength = 0;
		RelayListRefusal why = ReadString(r, name, kMostFieldNameLetters, &nameLength);

		if (why == kRelayListBadText || (why == kRelayListAccepted && strcmp(name, field) != 0))
		{
			return kRelayListNotItsField;
		}

		if (why == kRelayListAccepted)
		{
			why = Expect(r, ':');
		}

		if (why == kRelayListAccepted)
		{
			why = readValue(r, into, count);
		}

		if (why != kRelayListAccepted)
		{
			return why;
		}

		// A second field after the first means the first wasn't the only one
		if (Next(r) == ',')
		{
			return kRelayListNotItsField;
		}

		why = Expect(r, '}');

		if (why != kRelayListAccepted)
		{
			return why;
		}

		return Next(r) == 0 ? kRelayListAccepted : kRelayListMoreAfterTheEnd;
	}

	// A list, [ entry, entry ... ], of at most `most` entries, each read by readEntry, which gets its place on the list
	typedef RelayListRefusal (*EntryReader)(Reader &r, void *into, int place);

	RelayListRefusal ReadList(Reader &r, int most, EntryReader readEntry, void *into, int *count)
	{
		char const open = Next(r);

		if (open == 0)
		{
			return kRelayListEndsTooSoon;
		}

		if (open != '[')
		{
			return kRelayListNotAList;
		}

		r.at++;
		int n = 0;

		if (Next(r) == ']')
		{
			r.at++;
			*count = 0;
			return kRelayListAccepted;
		}

		for (;;)
		{
			if (n >= most)
			{
				return kRelayListTooMany;
			}

			RelayListRefusal const why = readEntry(r, into, n);

			if (why != kRelayListAccepted)
			{
				return why;
			}

			n++;

			char const c = Next(r);

			if (c == ',')
			{
				r.at++;
				continue;
			}

			if (c == ']')
			{
				r.at++;
				break;
			}

			return c == 0 ? kRelayListEndsTooSoon : kRelayListNotWhatComesNext;
		}

		*count = n;
		return kRelayListAccepted;
	}

	// ---- The games

	struct GamesInto
	{
		RelayListGame *games;
		int ourVersion;
		bool hostIds;   // whether each game carries its host's id: only a v2 draft's do (RelayList_ReadGames)
	};

	RelayListRefusal ReadOneGame(Reader &r, void *into, int place)
	{
		GamesInto const &g = *static_cast<GamesInto *>(into);
		RelayListGame &game = g.games[place];
		long gameStyle = 0, levelNumber = 0, coopLobby = 0, players = 0, maxPlayers = 0, version = 0, hostMs = 0, seconds = 0;

		// Every game carries its host's id (net_identity.h), so a joiner knows which host to expect: a game without one is refused,
		// as a game missing any of its fields is, and the whole answer with it
		FieldRule const rules[] =
		{
			{ "region", kKindRegion, 0, 0, 0, game.region, NULL },
			{ "code", kKindCode, 0, 0, 0, game.code, NULL },
			{ "name", kKindText, kRelayListTextLetters, 0, 0, game.name, NULL },
			{ "level", kKindText, kRelayListLevelLetters, 0, 0, game.level, NULL },
			{ "mode", kKindText, kRelayListTextLetters, 0, 0, game.mode, NULL },
			{ "gameStyle", kKindNumber, 0, 0, 255, NULL, &gameStyle },
			{ "levelNumber", kKindNumber, 0, 0, kRelayListMostLevelNumber, NULL, &levelNumber },
			{ "coopLobby", kKindNumber, 0, 0, 255, NULL, &coopLobby },
			{ "players", kKindNumber, 0, 1, kRelayMostPlayers, NULL, &players },
			{ "maxPlayers", kKindNumber, 0, 1, kRelayMostPlayers, NULL, &maxPlayers },
			{ "version", kKindNumber, 0, 1, kRelayListMostVersion, NULL, &version },
			{ "hostMs", kKindNumber, 0, 0, kRelayListMostHostMs, NULL, &hostMs },
			{ "seconds", kKindNumber, 0, 0, kRelayListMostSeconds, NULL, &seconds },
			{ "id", kKindHostId, 0, 0, 0, NULL, NULL, game.hostId }
		};

		// Modified 2026-11-09 by Melanikus for the AvP port: the host's id is the last rule, and only a v2 draft's games have it. Without,
		// the rules are version 1's, so a game is read exactly as version 1 reads it, and its id is left all 0.
		int const ruleCount = static_cast<int>(sizeof(rules) / sizeof(rules[0])) - (g.hostIds ? 0 : 1);

		if (!g.hostIds)
		{
			memset(game.hostId, 0, sizeof(game.hostId));
		}

		RelayListRefusal const why = ReadEntry(r, rules, ruleCount);

		if (why != kRelayListAccepted)
		{
			return why;
		}

		game.gameStyle = static_cast<int>(gameStyle);
		game.levelNumber = static_cast<int>(levelNumber);
		game.coopLobby = static_cast<int>(coopLobby);
		game.players = static_cast<int>(players);
		game.maxPlayers = static_cast<int>(maxPlayers);
		game.version = static_cast<int>(version);
		game.hostMs = static_cast<int>(hostMs);
		game.seconds = seconds;

		if (game.players > game.maxPlayers)
		{
			return kRelayListMorePlayersThanRoom;
		}

		if (game.version != g.ourVersion)
		{
			return kRelayListOtherVersion;
		}

		// A game is known by its region and code together
		for (int other = 0; other < place; other++)
		{
			if (strcmp(g.games[other].code, game.code) == 0 && strcmp(g.games[other].region, game.region) == 0)
			{
				return kRelayListTwice;
			}
		}

		return kRelayListAccepted;
	}

	RelayListRefusal ReadTheGames(Reader &r, void *into, int *count)
	{
		return ReadList(r, kRelayListMostGames, ReadOneGame, into, count);
	}

	// ---- The relays

	RelayListRefusal ReadOneRelay(Reader &r, void *into, int place)
	{
		RelayListRelay *const relays = static_cast<RelayListRelay *>(into);
		RelayListRelay &relay = relays[place];
		long port = 0;

		FieldRule const rules[] =
		{
			{ "region", kKindRegion, 0, 0, 0, relay.region, NULL },
			{ "host", kKindHost, 0, 0, 0, relay.host, NULL },
			{ "port", kKindNumber, 0, 1, 65535, NULL, &port }
		};

		RelayListRefusal const why = ReadEntry(r, rules, static_cast<int>(sizeof(rules) / sizeof(rules[0])));

		if (why != kRelayListAccepted)
		{
			return why;
		}

		relay.port = static_cast<int>(port);

		// One relay for each region
		for (int other = 0; other < place; other++)
		{
			if (strcmp(relays[other].region, relay.region) == 0)
			{
				return kRelayListTwice;
			}
		}

		return kRelayListAccepted;
	}

	RelayListRefusal ReadTheRelays(Reader &r, void *into, int *count)
	{
		return ReadList(r, kRelayListMostRelays, ReadOneRelay, into, count);
	}

	RelayListRefusal ReadTheOwner(Reader &r, void *into, int *)
	{
		return ReadSecret(r, static_cast<uint8_t *>(into));
	}

	// ---- Writing the bodies

	// Adds to a body, as long as there's room: once something doesn't fit, nothing more is added, and the body is refused at the end
	struct Writer
	{
		char *body;
		size_t room;
		size_t used;
		bool fits;
	};

	void AddChar(Writer &w, char c)
	{
		if (!w.fits || w.used + 1 >= w.room)
		{
			w.fits = false;
			return;
		}

		w.body[w.used++] = c;
		w.body[w.used] = '\0';
	}

	void Add(Writer &w, const char *text)
	{
		for (size_t i = 0; text[i]; i++)
		{
			AddChar(w, text[i]);
		}
	}

	// A text in quotes, with " and \ escaped, the only two characters in printable ASCII that need it
	void AddText(Writer &w, const char *text)
	{
		AddChar(w, '"');

		for (size_t i = 0; text[i]; i++)
		{
			if (text[i] == '"' || text[i] == '\\')
			{
				AddChar(w, '\\');
			}

			AddChar(w, text[i]);
		}

		AddChar(w, '"');
	}

	void AddNumber(Writer &w, unsigned long long value)
	{
		char digits[24];
		snprintf(digits, sizeof(digits), "%llu", value);
		Add(w, digits);
	}

	// A pass or an owner, as the list wants it: 32 hex digits, small letters
	void AddSecret(Writer &w, const uint8_t secret[kRelayListSecretBytes])
	{
		static const char kHex[] = "0123456789abcdef";

		AddChar(w, '"');

		for (int i = 0; i < kRelayListSecretBytes; i++)
		{
			AddChar(w, kHex[secret[i] >> 4]);
			AddChar(w, kHex[secret[i] & 15]);
		}

		AddChar(w, '"');
	}

	// A field's name and the : after it. The first field opens the body with {, and each after it starts with a ,
	void AddName(Writer &w, const char *name)
	{
		AddChar(w, w.used == 0 ? '{' : ',');
		AddChar(w, '"');
		Add(w, name);
		Add(w, "\":");
	}

	// 1 to most printable ASCII characters
	bool IsListText(const char *text, size_t most)
	{
		if (!text)
		{
			return false;
		}

		size_t n = 0;

		for (; text[n]; n++)
		{
			unsigned char const byte = static_cast<unsigned char>(text[n]);

			if (byte < 0x20 || byte > 0x7E || n >= most)
			{
				return false;
			}
		}

		return n >= 1;
	}

	// The end of a body: false, with an empty body, if it didn't fit or is past the most bytes the list takes for that call
	bool Finish(Writer &w, size_t most)
	{
		if (!w.fits || w.used > most)
		{
			if (w.room > 0)
			{
				w.body[0] = '\0';
			}

			return false;
		}

		return true;
	}

	size_t FitInto(const char *from, char *to, size_t most)
	{
		size_t n = 0;

		for (; from && from[n] && n < most; n++)
		{
			unsigned char const byte = static_cast<unsigned char>(from[n]);
			to[n] = (byte < 0x20 || byte > 0x7E) ? '?' : from[n];
		}

		to[n] = '\0';
		return n;
	}

	// The 8 letters of a code, which may or may not have a 0 after them
	bool IsCodeText(const char *code)
	{
		return code && Relay_IsCode(code, strnlen(code, kRelayCodeLetters + 1));
	}

	// A host id a host can list its game with: one was given, and it isn't all 0, which is what one never made looks like. A real one is a
	// SHA-384 sum, which is all 0 so rarely that it never happens.
	bool IsAHostId(const uint8_t *hostId)
	{
		if (!hostId)
		{
			return false;
		}

		for (size_t i = 0; i < kNetIdentityIdBytes; i++)
		{
			if (hostId[i] != 0)
			{
				return true;
			}
		}

		return false;
	}

	// A host id as the list wants it, written by net_identity.h's own writer: 96 hex digits, small letters, in quotes
	void AddHostId(Writer &w, const uint8_t *hostId)
	{
		char hex[kNetIdentityIdHexBytes];
		NetIdentity_IdHex(hostId, hex);

		AddChar(w, '"');
		Add(w, hex);
		AddChar(w, '"');
	}

	// ---- Asking for online access (server #24). Added 2026-11-09 by Melanikus for the AvP port.

	// Bytes in base64, as the list wants a key: the standard letters (A to Z, a to z, 0 to 9, + and /), each three bytes written as four
	// letters of 6 bits each, highest bits first. Only for a whole number of threes, so there's never an = at the end, and one key can be
	// written only one way.
	void AddBase64(Writer &w, const uint8_t *bytes, size_t size)
	{
		static const char kLetters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

		for (size_t i = 0; i + 3 <= size; i += 3)
		{
			uint32_t const three = (static_cast<uint32_t>(bytes[i]) << 16) | (static_cast<uint32_t>(bytes[i + 1]) << 8) | bytes[i + 2];
			AddChar(w, kLetters[(three >> 18) & 63]);
			AddChar(w, kLetters[(three >> 12) & 63]);
			AddChar(w, kLetters[(three >> 6) & 63]);
			AddChar(w, kLetters[three & 63]);
		}
	}

	// A byte a name may have: printable ASCII, but never " or \, so that a name never needs escaping, in JSON or in the owner's window
	bool IsAccessNameLetter(char c)
	{
		unsigned char const byte = static_cast<unsigned char>(c);
		return byte >= 0x20 && byte <= 0x7E && c != '"' && c != '\\';
	}

	// The protocol's words in an answer, with what each means and the HTTP status it comes with
	struct AccessWord
	{
		const char *word;
		int status;
		RelayAccessAnswer answer;
	};

	const AccessWord kAccessWords[] = {
		{ "waiting", 200, kRelayAccessWaiting },
		{ "approved", 200, kRelayAccessApproved },
		{ "refused", 200, kRelayAccessRefused },
		{ "revoked", 200, kRelayAccessRevoked },
		{ "name", 400, kRelayAccessBadName },
		{ "key", 400, kRelayAccessBadKey },
		{ "shape", 400, kRelayAccessBadShape },
		{ "busy", 429, kRelayAccessBusy },
		{ "closed", 503, kRelayAccessClosed }
	};

	// The longest of those words: "approved"
	const size_t kAccessWordMostLetters = 8;
}

const char *RelayList_RefusalWords(RelayListRefusal why)
{
	switch (why)
	{
		case kRelayListAccepted: return "the answer was read";
		case kRelayListTooBig: return "the answer is bigger than any honest answer";
		case kRelayListHasAZero: return "the answer has a 0 byte in it";
		case kRelayListNotAnObject: return "the answer doesn't start with {";
		case kRelayListNotItsField: return "the answer's only field isn't the one the protocol says";
		case kRelayListNotAList: return "the answer's field isn't a list";
		case kRelayListTooMany: return "the answer has more entries than the protocol allows";
		case kRelayListNotAnEntry: return "something on the list isn't an entry";
		case kRelayListUnknownField: return "an entry has a field the protocol doesn't have";
		case kRelayListFieldTwice: return "an entry has a field twice";
		case kRelayListFieldMissing: return "an entry is missing a field";
		case kRelayListBadEscape: return "a text has an escape other than \\\" and \\\\";
		case kRelayListBadText: return "a name, level or mode isn't printable characters of its length";
		case kRelayListBadCode: return "a code isn't 8 code letters";
		case kRelayListBadRegion: return "a region isn't 1 to 16 small letters, digits and -";
		case kRelayListBadHost: return "a relay's host isn't a name, or is an address instead of one";
		case kRelayListBadSecret: return "an owner isn't 32 hex digits";
		case kRelayListBadNumber: return "a number isn't a whole number in its range";
		case kRelayListMorePlayersThanRoom: return "a game has more players than room for them";
		case kRelayListOtherVersion: return "a game is for another version of the game";
		case kRelayListTwice: return "the answer has the same game or region twice";
		case kRelayListEndsTooSoon: return "the answer stops before its end";
		case kRelayListNotWhatComesNext: return "the answer has something else where a , : } or ] has to be";
		case kRelayListMoreAfterTheEnd: return "the answer has more after its end";
		case kRelayListBadFingerprint: return "an answer's fingerprint isn't 32 hex digits";
		case kRelayListBadHostId: return "a game's host id isn't 96 hex digits";
		case kRelayListBadStatus: return "the answer has an HTTP status the protocol never gives an access request";
		case kRelayListBadWord: return "the answer's state or error isn't one of the protocol's words for its status";
		case kRelayListNotOurKey: return "the answer is about another key than this PC's";
	}

	return "the answer was refused for a reason with no words";
}

RelayListRefusal RelayList_ReadGames(const char *body, size_t bytes, int ourVersion, bool hostIds, RelayListGame *games, int *count)
{
	// No games until the whole list has been read: ReadList gives the count only when every game in it is good
	*count = 0;
	int n = 0;
	GamesInto into = { games, ourVersion, hostIds };
	RelayListRefusal const why = ReadAnswer(body, bytes, kRelayListMostGamesBytes, "games", ReadTheGames, &into, &n);

	if (why == kRelayListAccepted)
	{
		*count = n;
	}

	return why;
}

RelayListRefusal RelayList_ReadRelays(const char *body, size_t bytes, RelayListRelay *relays, int *count)
{
	*count = 0;
	int n = 0;
	RelayListRefusal const why = ReadAnswer(body, bytes, kRelayListMostRelaysBytes, "relays", ReadTheRelays, relays, &n);

	if (why == kRelayListAccepted)
	{
		*count = n;
	}

	return why;
}

RelayListRefusal RelayList_ReadOwner(const char *body, size_t bytes, uint8_t owner[kRelayListSecretBytes])
{
	int unused = 0;
	uint8_t read[kRelayListSecretBytes];
	RelayListRefusal const why = ReadAnswer(body, bytes, kRelayListMostOwnerBytes, "owner", ReadTheOwner, read, &unused);

	// The owner is only given back once the whole answer is known to be good
	if (why == kRelayListAccepted)
	{
		memcpy(owner, read, sizeof(read));
	}

	return why;
}

// Added 2026-11-09 by Melanikus for the AvP port (protocol version 2's draft): see relay_list_text.h
RelayListRefusal RelayList_ReadRelayKeys(const char *body, size_t bytes, const char *region, uint8_t p384[kNetCryptoP384PublicBytes],
	uint8_t kem[kNetPqKemPublicBytes])
{
	memset(p384, 0, kNetCryptoP384PublicBytes);
	memset(kem, 0, kNetPqKemPublicBytes);

	if (!region || !RelayList_IsRegion(region))
	{
		return kRelayListBadRegion;
	}

	if (bytes > kRelayKeysMostAnswerBytes)
	{
		return kRelayListTooBig;
	}

	if (!body)
	{
		return kRelayListEndsTooSoon;
	}

	if (bytes > 0 && memchr(body, 0, bytes) != NULL)
	{
		return kRelayListHasAZero;
	}

	// The whole answer is one entry with exactly these three fields, in any order, and nothing after it
	char answered[kRelayListRegionLetters + 1] = {};
	uint8_t readP384[kNetCryptoP384PublicBytes];
	uint8_t readKem[kNetPqKemPublicBytes];
	FieldRule const rules[] =
	{
		{ "region", kKindRegion, 0, 0, 0, answered, NULL },
		{ "p384", kKindHexBytes, 2 * kNetCryptoP384PublicBytes, 0, 0, NULL, NULL, readP384 },
		{ "mlkem", kKindHexBytes, 2 * kNetPqKemPublicBytes, 0, 0, NULL, NULL, readKem }
	};

	Reader r = { body, body + bytes };
	RelayListRefusal why = ReadEntry(r, rules, static_cast<int>(sizeof(rules) / sizeof(rules[0])));

	if (why == kRelayListAccepted && Next(r) != 0)
	{
		why = kRelayListMoreAfterTheEnd;
	}

	// Keys about another region than the one asked about are no use, and are never taken for this one's
	if (why == kRelayListAccepted && strcmp(answered, region) != 0)
	{
		why = kRelayListBadRegion;
	}

	if (why == kRelayListAccepted)
	{
		memcpy(p384, readP384, sizeof(readP384));
		memcpy(kem, readKem, sizeof(readKem));
	}

	return why;
}

bool RelayList_ListingBody(const RelayListListing &l, bool withHostId, const uint8_t *owner, char *body, size_t room)
{
	Writer w = { body, room, 0, room > 0 };

	if (room > 0)
	{
		body[0] = '\0';
	}

	if (!RelayList_IsRegion(l.region) || !IsCodeText(l.code) || l.passTime > kRelayListMostPassTime ||
		!IsListText(l.name, kRelayListTextLetters) || !IsListText(l.level, kRelayListLevelLetters) || !IsListText(l.mode, kRelayListTextLetters) ||
		l.gameStyle < 0 || l.gameStyle > 255 || l.levelNumber < 0 || l.levelNumber > kRelayListMostLevelNumber || l.coopLobby < 0 ||
		l.coopLobby > 255 || l.maxPlayers < 1 || l.maxPlayers > kRelayMostPlayers || l.players < 1 || l.players > l.maxPlayers ||
		l.version < 1 || l.version > kRelayListMostVersion || l.hostMs < 0 || l.hostMs > kRelayListMostHostMs || (withHostId && !IsAHostId(l.hostId)))
	{
		return false;
	}

	char code[kRelayCodeLetters + 1];
	memcpy(code, l.code, kRelayCodeLetters);
	code[kRelayCodeLetters] = '\0';

	// The PUT's owner first, then the fields in the protocol's order
	if (owner)
	{
		AddName(w, "owner");
		AddSecret(w, owner);
	}

	AddName(w, "region");
	AddText(w, l.region);
	AddName(w, "code");
	AddText(w, code);
	AddName(w, "pass");
	AddSecret(w, l.pass);
	AddName(w, "passTime");
	AddNumber(w, l.passTime);
	AddName(w, "name");
	AddText(w, l.name);
	AddName(w, "level");
	AddText(w, l.level);
	AddName(w, "mode");
	AddText(w, l.mode);
	AddName(w, "gameStyle");
	AddNumber(w, static_cast<unsigned long long>(l.gameStyle));
	AddName(w, "levelNumber");
	AddNumber(w, static_cast<unsigned long long>(l.levelNumber));
	AddName(w, "coopLobby");
	AddNumber(w, static_cast<unsigned long long>(l.coopLobby));
	AddName(w, "players");
	AddNumber(w, static_cast<unsigned long long>(l.players));
	AddName(w, "maxPlayers");
	AddNumber(w, static_cast<unsigned long long>(l.maxPlayers));
	AddName(w, "version");
	AddNumber(w, static_cast<unsigned long long>(l.version));

	// Only a v2 draft's listing carries the host's id (modified 2026-11-09 by Melanikus for the AvP port): version 1's has no such field.
	// It's "id", all 48 bytes, since 2026-11-10 (SERVER-BLOCKERS item 6).
	if (withHostId)
	{
		AddName(w, "id");
		AddHostId(w, l.hostId);
	}

	AddName(w, "hostMs");
	AddNumber(w, static_cast<unsigned long long>(l.hostMs));
	AddChar(w, '}');

	return Finish(w, kRelayListMostBodyBytes);
}

bool RelayList_EndingBody(const char *region, const char *code, const uint8_t owner[kRelayListSecretBytes], char *body, size_t room)
{
	Writer w = { body, room, 0, room > 0 };

	if (room > 0)
	{
		body[0] = '\0';
	}

	if (!RelayList_IsRegion(region) || !IsCodeText(code) || !owner)
	{
		return false;
	}

	char letters[kRelayCodeLetters + 1];
	memcpy(letters, code, kRelayCodeLetters);
	letters[kRelayCodeLetters] = '\0';

	AddName(w, "region");
	AddText(w, region);
	AddName(w, "code");
	AddText(w, letters);
	AddName(w, "owner");
	AddSecret(w, owner);
	AddChar(w, '}');

	return Finish(w, kRelayListMostBodyBytes);
}

void RelayList_FitText(const char *from, const char *fallback, char *to, size_t most)
{
	if (FitInto(from, to, most) == 0 && FitInto(fallback, to, most) == 0)
	{
		to[0] = '?';
		to[1] = '\0';
	}
}

const char *RelayList_ModeWords(int gameStyle, bool coopCampaign)
{
	// In the order of NETGAME_TYPE: Individual, CoopDeathmatch, LastManStanding, PredatorTag, Coop, AlienTag
	static const char *const kModes[] = { "Deathmatch", "Species deathmatch", "Last man standing", "Predator tag", "Co-op", "Alien tag" };

	if (gameStyle == 4 && coopCampaign)
	{
		return "Co-op campaign";
	}

	if (gameStyle < 0 || gameStyle >= static_cast<int>(sizeof(kModes) / sizeof(kModes[0])))
	{
		return "Game";
	}

	return kModes[gameStyle];
}

bool Relay_IsServerName(const char *name)
{
	if (!name)
	{
		return false;
	}

	size_t const length = strlen(name);

	if (length < 1 || length > 253)
	{
		return false;
	}

	size_t partLength = 0;

	for (size_t i = 0; i <= length; i++)
	{
		char const c = name[i];

		// The end of a part: at a dot, or at the end of the name
		if (c == '.' || c == '\0')
		{
			if (partLength < 1 || partLength > 63 || name[i - 1] == '-' || name[i - partLength] == '-')
			{
				return false;
			}

			partLength = 0;
			continue;
		}

		bool const allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';

		if (!allowed)
		{
			return false;
		}

		partLength++;
	}

	return true;
}

bool RelayList_IsRegion(const char *region)
{
	if (!region)
	{
		return false;
	}

	size_t n = 0;

	for (; region[n]; n++)
	{
		char const c = region[n];
		bool const allowed = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';

		if (!allowed || n >= kRelayListRegionLetters)
		{
			return false;
		}
	}

	return n >= 1;
}

RelayAddressRefusal RelayList_AddressRefusal(uint32_t address, bool developer)
{
	// Whether address is in the block that starts at first and has its leading `prefix` bits in common with it
	auto const in = [address](uint32_t first, int prefix)
	{
		uint32_t const mask = 0xFFFFFFFFu << (32 - prefix);
		return (address & mask) == (first & mask);
	};

	if (in(0x00000000u, 8))
	{
		return kRelayAddressThisNetwork;
	}

	if (address == 0xFFFFFFFFu)
	{
		return kRelayAddressBroadcast;
	}

	if (in(0xE0000000u, 4))
	{
		return kRelayAddressMulticast;
	}

	if (in(0xF0000000u, 4))
	{
		return kRelayAddressReserved;
	}

	if (!developer && in(0x7F000000u, 8))
	{
		return kRelayAddressLoopback;
	}

	if (!developer && (in(0x0A000000u, 8) || in(0xAC100000u, 12) || in(0xC0A80000u, 16) || in(0xA9FE0000u, 16) || in(0x64400000u, 10)))
	{
		return kRelayAddressPrivate;
	}

	return kRelayAddressUsable;
}

const char *RelayList_AddressWords(RelayAddressRefusal why)
{
	switch (why)
	{
		case kRelayAddressUsable: return "the relay's address can be used";
		case kRelayAddressThisNetwork: return "the relay's name looked up to an address that means this network, not a machine";
		case kRelayAddressBroadcast: return "the relay's name looked up to the address for everyone on this network";
		case kRelayAddressMulticast: return "the relay's name looked up to an address for many machines at once";
		case kRelayAddressReserved: return "the relay's name looked up to an address that's never used on the internet";
		case kRelayAddressPrivate: return "the relay's name looked up to an address inside a private network, which only -dev allows";
		case kRelayAddressLoopback: return "the relay's name looked up to this machine's own address, which only -dev allows";
	}

	return "the relay's address was refused for a reason with no words";
}

bool RelayList_IsRelayHost(const char *host)
{
	if (!host || strlen(host) > kRelayListHostLetters || !Relay_IsServerName(host))
	{
		return false;
	}

	// A name made of digits and dots alone is an address, such as 203.0.113.9, and the list never gives one
	for (size_t i = 0; host[i]; i++)
	{
		if (host[i] != '.' && (host[i] < '0' || host[i] > '9'))
		{
			return true;
		}
	}

	return false;
}

// Added 2026-11-09 by Melanikus for the AvP port: see relay_list_text.h, "Reaching a test server"
RelayIPv4Refusal RelayList_IPv4Refusal(const char *text, uint32_t *hostOrder)
{
	if (!text || !hostOrder || !text[0])
	{
		return kRelayIPv4NoText;
	}

	uint32_t address = 0;
	size_t at = 0;

	for (int part = 0; part < 4; part++)
	{
		// One to three digits, and a leading zero only for 0 itself. A fourth digit stops the reading, so nothing longer is ever added up.
		size_t digits = 0;
		uint32_t value = 0;

		while (text[at + digits] >= '0' && text[at + digits] <= '9' && digits < 4)
		{
			value = value * 10 + static_cast<uint32_t>(text[at + digits] - '0');
			digits++;
		}

		if (digits == 0)
		{
			return kRelayIPv4NotANumber;
		}

		if (digits > 3 || value > 255)
		{
			return kRelayIPv4TooBig;
		}

		if (digits > 1 && text[at] == '0')
		{
			return kRelayIPv4LeadingZero;
		}

		address = (address << 8) | value;
		at += digits;

		// A dot between the parts, and the end of the text after the last. Anything else there isn't part of an address.
		char const after = text[at];

		if (part < 3 && after == '\0')
		{
			return kRelayIPv4WrongShape;
		}

		if (part == 3 && after == '.')
		{
			return kRelayIPv4WrongShape;
		}

		if (part < 3 ? after != '.' : after != '\0')
		{
			return kRelayIPv4NotANumber;
		}

		at++;
	}

	*hostOrder = address;
	return kRelayIPv4Read;
}

bool RelayList_ReadIPv4(const char *text, uint32_t *hostOrder)
{
	return RelayList_IPv4Refusal(text, hostOrder) == kRelayIPv4Read;
}

// A pin's hex digit, small letter or capital (the pass reader's HexValue, above, takes small letters, as the list writes them)
static int PinDigitValue(char c)
{
	if (c >= '0' && c <= '9')
	{
		return c - '0';
	}

	if (c >= 'a' && c <= 'f')
	{
		return c - 'a' + 10;
	}

	if (c >= 'A' && c <= 'F')
	{
		return c - 'A' + 10;
	}

	return -1;
}

RelayPinRefusal RelayList_PinRefusal(const char *text, uint8_t pin[kRelayListPinBytes])
{
	if (!text || !pin || !text[0])
	{
		return kRelayPinNoText;
	}

	if (strlen(text) != 2 * static_cast<size_t>(kRelayListPinBytes))
	{
		return kRelayPinWrongLength;
	}

	uint8_t read[kRelayListPinBytes];

	for (int i = 0; i < kRelayListPinBytes; i++)
	{
		int const high = PinDigitValue(text[2 * i]);
		int const low = PinDigitValue(text[2 * i + 1]);

		if (high < 0 || low < 0)
		{
			return kRelayPinNotHex;
		}

		read[i] = static_cast<uint8_t>(high * 16 + low);
	}

	memcpy(pin, read, sizeof(read));
	return kRelayPinRead;
}

bool RelayList_ReadPin(const char *text, uint8_t pin[kRelayListPinBytes])
{
	return RelayList_PinRefusal(text, pin) == kRelayPinRead;
}

bool RelayList_PinMatches(const uint8_t digest[kRelayListPinBytes], const uint8_t pin[kRelayListPinBytes])
{
	if (!digest || !pin)
	{
		return false;
	}

	// Every byte is compared, whatever the first difference, so the time taken says nothing about where it was
	uint8_t difference = 0;

	for (int i = 0; i < kRelayListPinBytes; i++)
	{
		difference = static_cast<uint8_t>(difference | (digest[i] ^ pin[i]));
	}

	return difference == 0;
}

RelayShownNameRefusal RelayList_ShownNameRefusal(const char *name)
{
	if (!name || !name[0])
	{
		return kRelayShownNameNoText;
	}

	size_t const length = strlen(name);

	if (length > static_cast<size_t>(kRelayListShownNameLetters))
	{
		return kRelayShownNameTooLong;
	}

	for (size_t i = 0; i < length; i++)
	{
		char const c = name[i];
		bool const letterOrDigit = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');

		if (!letterOrDigit && c != ' ' && c != '\'' && c != '(' && c != ')' && c != '-' && c != '.' && c != ',')
		{
			return kRelayShownNameBadCharacter;
		}
	}

	return kRelayShownNameGood;
}

bool RelayList_IsShownName(const char *name)
{
	return RelayList_ShownNameRefusal(name) == kRelayShownNameGood;
}

// ---- Asking for online access (server #24). Added 2026-11-09 by Melanikus for the AvP port.

RelayAccessNameRefusal RelayAccess_NameRefusal(const char *name)
{
	if (!name || name[0] == '\0')
	{
		return kRelayAccessNameEmpty;
	}

	// Looks one byte past the most, so a name that's too long is known without reading the rest of it
	size_t const length = strnlen(name, kRelayAccessNameMostBytes + 1);

	if (length > kRelayAccessNameMostBytes)
	{
		return kRelayAccessNameTooLong;
	}

	for (size_t i = 0; i < length; i++)
	{
		if (!IsAccessNameLetter(name[i]))
		{
			return kRelayAccessNameBadLetter;
		}
	}

	if (name[0] == ' ' || name[length - 1] == ' ')
	{
		return kRelayAccessNameSpaceAtAnEnd;
	}

	return kRelayAccessNameGood;
}

void RelayAccess_NameFromProfile(const char *profile, size_t profileBytes, char *to)
{
	size_t n = 0;

	for (size_t i = 0; profile && i < profileBytes && profile[i] && n < kRelayAccessNameMostBytes; i++)
	{
		char const c = profile[i];

		// A letter the rule doesn't take is left out, and so is a space before the name has started
		if (IsAccessNameLetter(c) && !(c == ' ' && n == 0))
		{
			to[n++] = c;
		}
	}

	// Spaces at the end are taken off last, since cutting it to 15 can leave one there
	while (n > 0 && to[n - 1] == ' ')
	{
		n--;
	}

	to[n] = '\0';
}

void RelayAccess_Code(const uint8_t fingerprint[kNetIdentityFingerprintBytes], char out[kRelayAccessCodeTextBytes])
{
	static_assert(sizeof(kNetRelayCodeAlphabet) == 33, "32 code letters, so each is exactly 5 bits");
	static_assert(kRelayAccessCodeLetters * 5 <= kNetIdentityFingerprintBytes * 8, "the code's 80 bits are all in the fingerprint");

	size_t at = 0;

	for (int letter = 0; letter < kRelayAccessCodeLetters; letter++)
	{
		if (letter > 0 && letter % 4 == 0)
		{
			out[at++] = '-';
		}

		// The letter's 5 bits, one at a time: bit 0 is the highest bit of the fingerprint's first byte
		int value = 0;

		for (int b = 0; b < 5; b++)
		{
			int const bit = letter * 5 + b;
			value = value * 2 + ((fingerprint[bit / 8] >> (7 - bit % 8)) & 1);
		}

		out[at++] = kNetRelayCodeAlphabet[value];
	}

	out[at] = '\0';
}

bool RelayList_AccessBody(const char *name, const uint8_t statics[kNetIdentityStaticBytes], char *body, size_t room)
{
	Writer w = { body, room, 0, room > 0 };

	if (room > 0)
	{
		body[0] = '\0';
	}

	// The public halves start with the P-384 key, and a P-384 key starts with 04: all 0 is an identity never made
	if (RelayAccess_NameRefusal(name) != kRelayAccessNameGood || !statics || statics[0] != 0x04)
	{
		return false;
	}

	AddName(w, "v");
	AddNumber(w, 1);
	AddName(w, "name");
	AddText(w, name);
	AddName(w, "key");
	AddChar(w, '"');
	AddBase64(w, statics, kNetIdentityStaticBytes);
	AddChar(w, '"');
	AddChar(w, '}');

	return Finish(w, kRelayAccessMostBodyBytes);
}

const char *RelayAccess_AnswerWords(RelayAccessAnswer answer)
{
	switch (answer)
	{
		case kRelayAccessWaiting: return "the request is waiting for a person to look at it";
		case kRelayAccessApproved: return "the request was approved";
		case kRelayAccessRefused: return "the request was refused";
		case kRelayAccessRevoked: return "the approval was taken back";
		case kRelayAccessBadName: return "the list wouldn't take the name";
		case kRelayAccessBadKey: return "the list wouldn't take the key";
		case kRelayAccessBadShape: return "the list wouldn't take the request";
		case kRelayAccessBusy: return "the list is busy: try again in a minute";
		case kRelayAccessClosed: return "the list isn't taking requests just now";
	}

	return "the list's answer has no words";
}

RelayListRefusal RelayList_ReadAccess(int status, const char *body, size_t bytes, const uint8_t ours[kNetIdentityFingerprintBytes],
	RelayAccessAnswer *answer)
{
	if (status != 200 && status != 400 && status != 429 && status != 503)
	{
		return kRelayListBadStatus;
	}

	if (bytes > kRelayAccessMostAnswerBytes)
	{
		return kRelayListTooBig;
	}

	if (!body || !answer)
	{
		return kRelayListEndsTooSoon;
	}

	if (bytes > 0 && memchr(body, 0, bytes) != NULL)
	{
		return kRelayListHasAZero;
	}

	Reader r = { body, body + bytes };
	char const first = Next(r);

	if (first == 0)
	{
		return kRelayListEndsTooSoon;
	}

	if (first != '{')
	{
		return kRelayListNotAnObject;
	}

	// A 200 is about the key: its version, its fingerprint and its state. Every other status is an error, with its version and its word.
	long version = 0;
	char word[kAccessWordMostLetters + 1];
	uint8_t fingerprint[kNetIdentityFingerprintBytes];
	RelayListRefusal why = kRelayListAccepted;

	if (status == 200)
	{
		FieldRule const rules[] = {
			{ "v", kKindNumber, 0, 1, 1, NULL, &version, NULL },
			{ "fp", kKindFingerprint, 0, 0, 0, NULL, NULL, fingerprint },
			{ "state", kKindText, kAccessWordMostLetters, 0, 0, word, NULL, NULL }
		};
		why = ReadEntry(r, rules, 3);
	}
	else
	{
		FieldRule const rules[] = {
			{ "v", kKindNumber, 0, 1, 1, NULL, &version, NULL },
			{ "error", kKindText, kAccessWordMostLetters, 0, 0, word, NULL, NULL }
		};
		why = ReadEntry(r, rules, 2);
	}

	// A word too long for any of the protocol's is one it doesn't have
	if (why == kRelayListBadText)
	{
		return kRelayListBadWord;
	}

	if (why != kRelayListAccepted)
	{
		return why;
	}

	if (Next(r) != 0)
	{
		return kRelayListMoreAfterTheEnd;
	}

	// The word has to be one of the protocol's, and one that comes with this status: "busy" with a 200 is as wrong as an unknown word
	int found = -1;

	for (size_t i = 0; i < sizeof(kAccessWords) / sizeof(kAccessWords[0]); i++)
	{
		if (kAccessWords[i].status == status && strcmp(kAccessWords[i].word, word) == 0)
		{
			found = static_cast<int>(i);
		}
	}

	if (found < 0)
	{
		return kRelayListBadWord;
	}

	// The fingerprint the list worked out has to be this PC's own. That catches a list and a game that work it out differently. It's no
	// defence against someone in the middle, who would change this as well: the access code on the player's screen is.
	if (status == 200 && (!ours || memcmp(fingerprint, ours, sizeof(fingerprint)) != 0))
	{
		return kRelayListNotOurKey;
	}

	*answer = kAccessWords[found].answer;
	return kRelayListAccepted;
}
