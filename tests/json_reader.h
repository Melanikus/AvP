// A small, strict JSON reader for the tests' files of known answers (tests/vectors/). Written 2026-11-09 by Melanikus for the AvP port,
// from net_relay_test, where it was first written (2026-11-08), so net_relay_test and net_crypto_test read their files the same way.
//
// The file is JSON. Only as much of JSON as the file uses is read here: objects, lists, texts (with \" and \\ the only escapes), whole
// numbers from 0 up, true, false and null. Anything else in it fails the reading, and so the test, as a damaged file should.

#ifndef JSON_READER_H
#define JSON_READER_H

#include <stdint.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

int const kJsonNothing = 0;     // null
int const kJsonTrueFalse = 1;
int const kJsonNumber = 2;
int const kJsonText = 3;
int const kJsonList = 4;
int const kJsonObject = 5;

struct JsonValue
{
	int kind = kJsonNothing;
	bool truth = false;
	uint64_t number = 0;
	std::string text;
	std::vector<JsonValue> items;     // a list's values, in order
	std::vector<std::string> names;   // an object's names, in the file's order...
	std::vector<JsonValue> values;    // ...and the value each one has

	// An object's value with this name, or NULL if it has none
	const JsonValue *Member(const char *name) const
	{
		for (size_t i = 0; i < names.size(); i++)
		{
			if (names[i] == name)
			{
				return &values[i];
			}
		}

		return NULL;
	}
};

class JsonReader
{
public:
	explicit JsonReader(const std::string &all) : text(all), at(0)
	{
	}

	// Reads the whole text as one value, with nothing after it but spaces and line ends. False if it isn't that.
	bool ReadAll(JsonValue *out)
	{
		if (!Value(out, 0))
		{
			return false;
		}

		Spaces();
		return at == text.size();
	}

private:
	// How deep lists and objects can be inside each other. The file goes 4 deep; this stops a damaged one from going on and on.
	static size_t const kMostDepth = 16;

	const std::string &text;
	size_t at;   // how far it has read

	void Spaces()
	{
		while (at < text.size() && (text[at] == ' ' || text[at] == '\n' || text[at] == '\r' || text[at] == '\t'))
		{
			at++;
		}
	}

	// Takes this one letter, after any spaces. False, taking nothing, if the next letter is another.
	bool Take(char c)
	{
		Spaces();

		if (at < text.size() && text[at] == c)
		{
			at++;
			return true;
		}

		return false;
	}

	// Takes this word if it's next
	bool Word(const char *word)
	{
		size_t const n = strlen(word);

		if (text.compare(at, n, word) == 0)
		{
			at += n;
			return true;
		}

		return false;
	}

	bool Text(std::string *out)
	{
		if (!Take('"'))
		{
			return false;
		}

		out->clear();

		while (at < text.size())
		{
			char const c = text[at++];

			if (c == '"')
			{
				return true;
			}

			// A line end or other control letter can't be inside a JSON text
			if (static_cast<unsigned char>(c) < 0x20)
			{
				return false;
			}

			if (c == '\\')
			{
				if (at >= text.size() || (text[at] != '"' && text[at] != '\\'))
				{
					return false;
				}

				*out += text[at++];
			}
			else
			{
				*out += c;
			}
		}

		return false;   // the text never ended
	}

	bool Number(uint64_t *out)
	{
		size_t const first = at;
		uint64_t n = 0;

		while (at < text.size() && text[at] >= '0' && text[at] <= '9')
		{
			uint64_t const digit = static_cast<uint64_t>(text[at] - '0');

			// One more digit would need more than 64 bits
			if (n > (UINT64_MAX - digit) / 10)
			{
				return false;
			}

			n = n * 10 + digit;
			at++;
		}

		// No digits at all, or a 0 in front of others, which JSON doesn't allow. A "-", "." or "e" after the digits isn't taken, and then
		// whatever reads next fails on it.
		if (at == first || (text[first] == '0' && at - first > 1))
		{
			return false;
		}

		*out = n;
		return true;
	}

	bool Value(JsonValue *out, size_t depth)
	{
		Spaces();

		if (at >= text.size() || depth > kMostDepth)
		{
			return false;
		}

		char const c = text[at];

		if (c == '{')
		{
			at++;
			out->kind = kJsonObject;

			if (Take('}'))
			{
				return true;
			}

			do
			{
				std::string name;
				JsonValue value;

				// A name given twice would leave which value counts to whoever reads it, so that's refused too
				if (!Text(&name) || out->Member(name.c_str()) || !Take(':') || !Value(&value, depth + 1))
				{
					return false;
				}

				out->names.push_back(name);
				out->values.push_back(value);
			} while (Take(','));

			return Take('}');
		}

		if (c == '[')
		{
			at++;
			out->kind = kJsonList;

			if (Take(']'))
			{
				return true;
			}

			do
			{
				JsonValue item;

				if (!Value(&item, depth + 1))
				{
					return false;
				}

				out->items.push_back(item);
			} while (Take(','));

			return Take(']');
		}

		if (c == '"')
		{
			out->kind = kJsonText;
			return Text(&out->text);
		}

		if (c >= '0' && c <= '9')
		{
			out->kind = kJsonNumber;
			return Number(&out->number);
		}

		if (Word("true"))
		{
			out->kind = kJsonTrueFalse;
			out->truth = true;
			return true;
		}

		if (Word("false"))
		{
			out->kind = kJsonTrueFalse;
			return true;
		}

		if (Word("null"))
		{
			out->kind = kJsonNothing;
			return true;
		}

		return false;
	}
};

// A whole file of this repository, byte for byte, found from this test's own place: /FC in its settings makes __FILE__ the whole path of
// this file. Empty if it can't be read.
static std::string ReadRepositoryFile(const char *name)
{
	std::string const here = __FILE__;
	size_t const tests = here.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((here.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();
	return all.str();
}

static int HexDigit(char c)
{
	if (c >= '0' && c <= '9')
	{
		return c - '0';
	}

	if (c >= 'a' && c <= 'f')
	{
		return c - 'a' + 10;
	}

	return -1;   // not a hex digit, or a capital one, which the file never writes
}

// Bytes from hex digits, two to a byte. False for an odd count, or anything that isn't a hex digit.
static bool FromHex(const std::string &hex, std::vector<uint8_t> *out)
{
	out->clear();

	if (hex.size() % 2 != 0)
	{
		return false;
	}

	for (size_t i = 0; i < hex.size(); i += 2)
	{
		int const high = HexDigit(hex[i]);
		int const low = HexDigit(hex[i + 1]);

		if (high < 0 || low < 0)
		{
			return false;
		}

		out->push_back(static_cast<uint8_t>(high * 16 + low));
	}

	return true;
}

// A vector's text field, or NULL if it hasn't one
static const JsonValue *TextMember(const JsonValue &vector, const char *name)
{
	const JsonValue *const field = vector.Member(name);
	return field && field->kind == kJsonText ? field : NULL;
}

#endif
