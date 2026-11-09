// The words that go to and from the relay server's list. relay_list_text.h says what this is for, and what it refuses.
// Written 2026-11-08 by Melanikus for the AvP port.

#include "relay_list_text.h"

#include <stdio.h>
#include <string.h>

namespace
{
	// ---- Reading the list

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

	// A string, "like this". The server writes only printable ASCII, and the only escapes it ever writes for that are \" and \\, so any
	// other escape is refused, and so is any character outside printable ASCII. At most `most` characters, into out, which has room for
	// most + 1, the 0 at the end included.
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

	// A name, a level or a mode: 1 to 32 characters
	RelayListRefusal ReadText(Reader &r, char *out)
	{
		size_t length = 0;
		RelayListRefusal const why = ReadString(r, out, kRelayListTextLetters, &length);

		if (why == kRelayListAccepted && length == 0)
		{
			return kRelayListBadText;
		}

		return why;
	}

	// A whole number from least to most. Only digits: no sign, no point, no exponent, and no 0 in front of another digit, which is what
	// the server writes. Nine digits at most, which is past every limit here and can't overflow.
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

	// A game's eight fields, by the names the server gives them
	enum Field
	{
		kFieldCode,
		kFieldName,
		kFieldLevel,
		kFieldMode,
		kFieldPlayers,
		kFieldMaxPlayers,
		kFieldVersion,
		kFieldSeconds,
		kFields
	};

	const char *const kFieldNames[kFields] = { "code", "name", "level", "mode", "players", "maxPlayers", "version", "seconds" };

	// A field's name is at most this long: the longest of the eight, maxPlayers, is 10
	const size_t kMostFieldNameLetters = 16;

	// One game, { ... }, with exactly its eight fields, in any order
	RelayListRefusal ReadGame(Reader &r, int ourVersion, RelayListGame &game)
	{
		char const first = Next(r);

		if (first == 0)
		{
			return kRelayListEndsTooSoon;
		}

		if (first != '{')
		{
			return kRelayListNotAGame;
		}

		r.at++;

		if (Next(r) == '}')
		{
			return kRelayListFieldMissing;
		}

		bool seen[kFields] = {};

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

			for (int f = 0; f < kFields; f++)
			{
				if (strcmp(name, kFieldNames[f]) == 0)
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

			if (why != kRelayListAccepted)
			{
				return why;
			}

			long number = 0;
			size_t length = 0;

			switch (field)
			{
				case kFieldCode:
					why = ReadString(r, game.code, kRelayCodeLetters, &length);

					if (why == kRelayListBadText || (why == kRelayListAccepted && !Relay_IsCode(game.code, length)))
					{
						why = kRelayListBadCode;
					}
					break;

				case kFieldName:
					why = ReadText(r, game.name);
					break;

				case kFieldLevel:
					why = ReadText(r, game.level);
					break;

				case kFieldMode:
					why = ReadText(r, game.mode);
					break;

				case kFieldPlayers:
					why = ReadNumber(r, 1, kRelayMostPlayers, &number);
					game.players = static_cast<int>(number);
					break;

				case kFieldMaxPlayers:
					why = ReadNumber(r, 1, kRelayMostPlayers, &number);
					game.maxPlayers = static_cast<int>(number);
					break;

				case kFieldVersion:
					why = ReadNumber(r, 1, kRelayListMostVersion, &number);
					game.version = static_cast<int>(number);
					break;

				case kFieldSeconds:
					why = ReadNumber(r, 0, kRelayListMostSeconds, &number);
					game.seconds = number;
					break;
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

		for (int f = 0; f < kFields; f++)
		{
			if (!seen[f])
			{
				return kRelayListFieldMissing;
			}
		}

		if (game.players > game.maxPlayers)
		{
			return kRelayListMorePlayersThanRoom;
		}

		if (game.version != ourVersion)
		{
			return kRelayListOtherVersion;
		}

		return kRelayListAccepted;
	}

	// The whole answer, { "games": [ ... ] }, and nothing after it
	RelayListRefusal ReadAll(const char *body, size_t bytes, int ourVersion, RelayListGame *games, int *count)
	{
		if (bytes > kRelayListMostAnswerBytes)
		{
			return kRelayListTooBig;
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
			return kRelayListNotGames;
		}

		char name[kMostFieldNameLetters + 1];
		size_t nameLength = 0;
		RelayListRefusal why = ReadString(r, name, kMostFieldNameLetters, &nameLength);

		if (why == kRelayListBadText || (why == kRelayListAccepted && strcmp(name, "games") != 0))
		{
			return kRelayListNotGames;
		}

		if (why != kRelayListAccepted)
		{
			return why;
		}

		why = Expect(r, ':');

		if (why != kRelayListAccepted)
		{
			return why;
		}

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
		}
		else
		{
			for (;;)
			{
				if (n >= kRelayListMostGames)
				{
					return kRelayListTooManyGames;
				}

				why = ReadGame(r, ourVersion, games[n]);

				if (why != kRelayListAccepted)
				{
					return why;
				}

				for (int g = 0; g < n; g++)
				{
					if (strcmp(games[g].code, games[n].code) == 0)
					{
						return kRelayListSameGameTwice;
					}
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
		}

		// A second field after "games" means "games" wasn't the only one
		char const close = Next(r);

		if (close == ',')
		{
			return kRelayListNotGames;
		}

		why = Expect(r, '}');

		if (why != kRelayListAccepted)
		{
			return why;
		}

		if (Next(r) != 0)
		{
			return kRelayListMoreAfterTheEnd;
		}

		*count = n;
		return kRelayListAccepted;
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

	void AddNumber(Writer &w, long value)
	{
		char digits[24];
		snprintf(digits, sizeof(digits), "%ld", value);
		Add(w, digits);
	}

	// The token as the server wants it: 32 hex digits, small letters
	void AddToken(Writer &w, const uint8_t token[kRelayTokenBytes])
	{
		static const char kHex[] = "0123456789abcdef";

		AddChar(w, '"');

		for (int i = 0; i < kRelayTokenBytes; i++)
		{
			AddChar(w, kHex[token[i] >> 4]);
			AddChar(w, kHex[token[i] & 15]);
		}

		AddChar(w, '"');
	}

	// 1 to 32 printable ASCII characters, as a name, a level and a mode have to be
	bool IsListText(const char *text)
	{
		if (!text)
		{
			return false;
		}

		size_t n = 0;

		for (; text[n]; n++)
		{
			unsigned char const byte = static_cast<unsigned char>(text[n]);

			if (byte < 0x20 || byte > 0x7E || n >= kRelayListTextLetters)
			{
				return false;
			}
		}

		return n >= 1;
	}

	// The end of a body: false, with an empty body, if it didn't fit or is past what the server takes
	bool Finish(Writer &w)
	{
		if (!w.fits || w.used > kRelayListMostBodyBytes)
		{
			if (w.room > 0)
			{
				w.body[0] = '\0';
			}

			return false;
		}

		return true;
	}

	size_t FitInto(const char *from, char *to)
	{
		size_t n = 0;

		for (; from && from[n] && n < kRelayListTextLetters; n++)
		{
			unsigned char const byte = static_cast<unsigned char>(from[n]);
			to[n] = (byte < 0x20 || byte > 0x7E) ? '?' : from[n];
		}

		to[n] = '\0';
		return n;
	}
}

const char *RelayList_RefusalWords(RelayListRefusal why)
{
	switch (why)
	{
		case kRelayListAccepted: return "the list was read";
		case kRelayListTooBig: return "the list is bigger than any honest list";
		case kRelayListHasAZero: return "the list has a 0 byte in it";
		case kRelayListNotAnObject: return "the list doesn't start with {";
		case kRelayListNotGames: return "the list's only field isn't \"games\"";
		case kRelayListNotAList: return "\"games\" isn't a list";
		case kRelayListTooManyGames: return "the list has more than 100 games";
		case kRelayListNotAGame: return "something on the list isn't a game";
		case kRelayListUnknownField: return "a game has a field the protocol doesn't have";
		case kRelayListFieldTwice: return "a game has a field twice";
		case kRelayListFieldMissing: return "a game is missing a field";
		case kRelayListBadEscape: return "a text has an escape other than \\\" and \\\\";
		case kRelayListBadText: return "a name, level or mode isn't 1 to 32 printable characters";
		case kRelayListBadCode: return "a code isn't 8 code letters";
		case kRelayListBadNumber: return "a number isn't a whole number in its range";
		case kRelayListMorePlayersThanRoom: return "a game has more players than room for them";
		case kRelayListOtherVersion: return "a game is for another version of the game";
		case kRelayListSameGameTwice: return "the list has the same game twice";
		case kRelayListEndsTooSoon: return "the list stops before its end";
		case kRelayListNotWhatComesNext: return "the list has something else where a , : } or ] has to be";
		case kRelayListMoreAfterTheEnd: return "the list has more after its end";
	}

	return "the list was refused for a reason with no words";
}

RelayListRefusal RelayList_ReadGames(const char *body, size_t bytes, int ourVersion, RelayListGame *games, int *count)
{
	// No games until the whole list has been read: ReadAll gives the count only when it accepts the list
	*count = 0;

	if (!body && bytes > 0)
	{
		return kRelayListEndsTooSoon;
	}

	return ReadAll(body ? body : "", body ? bytes : 0, ourVersion, games, count);
}

bool RelayList_ListingBody(const uint8_t token[kRelayTokenBytes], const char *name, const char *level, const char *mode, int players,
	int maxPlayers, int version, char *body, size_t room)
{
	Writer w = { body, room, 0, room > 0 };

	if (room > 0)
	{
		body[0] = '\0';
	}

	if (!token || !IsListText(name) || !IsListText(level) || !IsListText(mode) || maxPlayers < 1 || maxPlayers > kRelayMostPlayers ||
		players < 1 || players > maxPlayers || version < 1 || version > kRelayListMostVersion)
	{
		return false;
	}

	Add(w, "{\"token\":");
	AddToken(w, token);
	Add(w, ",\"name\":");
	AddText(w, name);
	Add(w, ",\"level\":");
	AddText(w, level);
	Add(w, ",\"mode\":");
	AddText(w, mode);
	Add(w, ",\"players\":");
	AddNumber(w, players);
	Add(w, ",\"maxPlayers\":");
	AddNumber(w, maxPlayers);
	Add(w, ",\"version\":");
	AddNumber(w, version);
	Add(w, "}");

	return Finish(w);
}

bool RelayList_EndingBody(const uint8_t token[kRelayTokenBytes], char *body, size_t room)
{
	Writer w = { body, room, 0, room > 0 };

	if (room > 0)
	{
		body[0] = '\0';
	}

	if (!token)
	{
		return false;
	}

	Add(w, "{\"token\":");
	AddToken(w, token);
	Add(w, "}");

	return Finish(w);
}

void RelayList_FitText(const char *from, const char *fallback, char *to)
{
	if (FitInto(from, to) == 0 && FitInto(fallback, to) == 0)
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
