// Reads the published known answers in tests/crypto-kat, for tests/pq_mlkem_test and tests/pq_mldsa_test. Written 2026-11-09 by
// Melanikus for the AvP port.
//
// tests/crypto-kat/README.md says what the files are, and their shape. This reads exactly that shape and refuses anything else, saying
// what it found and where, so a damaged or changed file fails the test instead of quietly losing cases:
// - the whole file is one object with exactly five names: primitive, source and about (texts), count (a whole number) and cases (a list);
// - each case is an object whose values are all texts, each name there once;
// - a text is printable ASCII, and \" and \\ are its only escapes;
// - count is how many cases there are.
// Spaces, tabs and line ends between the parts are fine, as JSON has them. A checkout on Windows can end the lines with \r\n.
// KatBytes reads a text of hex, small letters only, two to a byte.

#ifndef CRYPTO_KAT_READER_H
#define CRYPTO_KAT_READER_H

#include <stddef.h>
#include <stdint.h>

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// One case: its names and texts, such as "id" -> "6", "ek" -> "a1b2...", "result" -> "valid"
typedef std::map<std::string, std::string> KatCase;

struct KatFile
{
	std::string primitive;
	std::string source;
	std::string about;
	std::vector<KatCase> cases;
};

// The largest file there is 3,554,624 bytes (ml-dsa-87/wycheproof-verify.json). Anything over this isn't one of ours.
constexpr size_t kKatLargestFile = 8 * 1024 * 1024;

// Reads one file, one character at a time from the front. Each Read... gives false and says why in why_ at the first thing that isn't
// the shape, and then nothing after it is read.
class KatParser
{
public:
	explicit KatParser(std::string const &text) : text_(text), at_(0) {}

	bool ReadFile(KatFile &file)
	{
		bool havePrimitive = false, haveSource = false, haveAbout = false, haveCount = false, haveCases = false;
		size_t count = 0;

		if (!Expect('{'))
		{
			return false;
		}

		for (;;)
		{
			std::string name;

			if (!ReadText(name) || !Expect(':'))
			{
				return false;
			}

			bool ok = false;

			if (name == "primitive" && !havePrimitive) { ok = ReadText(file.primitive); havePrimitive = true; }
			else if (name == "source" && !haveSource) { ok = ReadText(file.source); haveSource = true; }
			else if (name == "about" && !haveAbout) { ok = ReadText(file.about); haveAbout = true; }
			else if (name == "count" && !haveCount) { ok = ReadWholeNumber(count); haveCount = true; }
			else if (name == "cases" && !haveCases) { ok = ReadCases(file.cases); haveCases = true; }
			else { return Refuse("\"" + name + "\" isn't one of the five names a file has, or it's there twice"); }

			if (!ok)
			{
				return false;
			}

			SkipSpace();

			if (Next(','))
			{
				continue;
			}

			if (!Expect('}'))
			{
				return false;
			}

			break;
		}

		SkipSpace();

		if (at_ != text_.size())
		{
			return Refuse("there's more after the end of the file's one object");
		}

		if (!havePrimitive || !haveSource || !haveAbout || !haveCount || !haveCases)
		{
			return Refuse("one of primitive, source, about, count and cases is missing");
		}

		if (count != file.cases.size())
		{
			return Refuse("count says " + std::to_string(count) + " but there are " + std::to_string(file.cases.size()) + " cases");
		}

		return true;
	}

	std::string const &Why() const { return why_; }

private:
	std::string const &text_;
	size_t at_;
	std::string why_;

	bool Refuse(std::string const &what)
	{
		why_ = "at byte " + std::to_string(at_) + ": " + what;
		return false;
	}

	void SkipSpace()
	{
		while (at_ < text_.size() && (text_[at_] == ' ' || text_[at_] == '\t' || text_[at_] == '\n' || text_[at_] == '\r'))
		{
			at_++;
		}
	}

	// Takes the character c if it's next, after any space
	bool Next(char c)
	{
		SkipSpace();

		if (at_ < text_.size() && text_[at_] == c)
		{
			at_++;
			return true;
		}

		return false;
	}

	bool Expect(char c)
	{
		return Next(c) || Refuse(std::string("expected '") + c + "'");
	}

	bool ReadText(std::string &out)
	{
		out.clear();

		if (!Expect('"'))
		{
			return false;
		}

		while (at_ < text_.size())
		{
			unsigned char const c = static_cast<unsigned char>(text_[at_++]);

			if (c == '"')
			{
				return true;
			}

			if (c == '\\')
			{
				if (at_ < text_.size() && (text_[at_] == '"' || text_[at_] == '\\'))
				{
					out += text_[at_++];
					continue;
				}

				return Refuse("an escape other than \\\" or \\\\");
			}

			if (c < 0x20 || c > 0x7E)
			{
				return Refuse("a character that isn't printable ASCII, in a text");
			}

			out += static_cast<char>(c);
		}

		return Refuse("a text that doesn't end");
	}

	// Digits only, no sign, no leading zero, and small enough to count cases with
	bool ReadWholeNumber(size_t &out)
	{
		SkipSpace();
		size_t const start = at_;
		out = 0;

		while (at_ < text_.size() && text_[at_] >= '0' && text_[at_] <= '9')
		{
			if (at_ - start >= 9)
			{
				return Refuse("a number with more than 9 digits");
			}

			out = out * 10 + static_cast<size_t>(text_[at_] - '0');
			at_++;
		}

		if (at_ == start)
		{
			return Refuse("expected a whole number");
		}

		if (text_[start] == '0' && at_ - start > 1)
		{
			return Refuse("a number that starts with 0");
		}

		return true;
	}

	bool ReadCase(KatCase &one)
	{
		if (!Expect('{'))
		{
			return false;
		}

		if (Next('}'))
		{
			return true;
		}

		for (;;)
		{
			std::string name, value;

			if (!ReadText(name) || !Expect(':') || !ReadText(value))
			{
				return false;
			}

			if (!one.emplace(name, value).second)
			{
				return Refuse("\"" + name + "\" is there twice in one case");
			}

			if (Next(','))
			{
				continue;
			}

			return Expect('}');
		}
	}

	bool ReadCases(std::vector<KatCase> &cases)
	{
		if (!Expect('['))
		{
			return false;
		}

		if (Next(']'))
		{
			return true;
		}

		for (;;)
		{
			KatCase one;

			if (!ReadCase(one))
			{
				return false;
			}

			cases.push_back(one);

			if (Next(','))
			{
				continue;
			}

			return Expect(']');
		}
	}
};

// Reads tests/crypto-kat/<name>, from the repository whose top folder is top, into file. False, with why, if it isn't there, is too
// big, or isn't the shape.
inline bool KatRead(std::string const &top, std::string const &name, KatFile &file, std::string &why)
{
	std::ifstream in((top + "tests/crypto-kat/" + name).c_str(), std::ios::binary);

	if (!in)
	{
		why = "tests/crypto-kat/" + name + " isn't there";
		return false;
	}

	in.seekg(0, std::ios::end);
	std::streamoff const size = in.tellg();
	in.seekg(0, std::ios::beg);

	if (size < 0 || static_cast<unsigned long long>(size) > kKatLargestFile)
	{
		why = "tests/crypto-kat/" + name + " is bigger than any of ours, or its size can't be read";
		return false;
	}

	std::stringstream all;
	all << in.rdbuf();
	std::string const text = all.str();

	KatParser parser(text);

	if (!parser.ReadFile(file))
	{
		why = "tests/crypto-kat/" + name + ", " + parser.Why();
		return false;
	}

	return true;
}

// Hex in small letters, two to a byte, into bytes. False for an odd length or any other character.
inline bool KatBytes(std::string const &hex, std::vector<uint8_t> &bytes)
{
	bytes.clear();

	if (hex.size() % 2 != 0)
	{
		return false;
	}

	for (size_t i = 0; i < hex.size(); i += 2)
	{
		int value = 0;

		for (size_t j = i; j < i + 2; j++)
		{
			char const c = hex[j];
			int digit = 0;

			if (c >= '0' && c <= '9')
			{
				digit = c - '0';
			}
			else if (c >= 'a' && c <= 'f')
			{
				digit = c - 'a' + 10;
			}
			else
			{
				return false;
			}

			value = value * 16 + digit;
		}

		bytes.push_back(static_cast<uint8_t>(value));
	}

	return true;
}

// The repository's top folder, found from the whole path of the test's own file. Each test's settings have /FC, which makes the
// compiler give __FILE__ as the whole path, so the test passes __FILE__ here.
inline std::string KatRepositoryTop(char const *testFile)
{
	std::string const path = testFile;
	std::string::size_type const tests = path.rfind("tests");
	return (tests == std::string::npos) ? std::string() : path.substr(0, tests);
}

#endif
