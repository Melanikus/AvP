// The test that docs/protocol/ holds the online server's design exactly as copied: every file docs/protocol/COPIED.md lists is there,
// of that size and that sha256, and every file there is listed. Written 2026-11-09 by Melanikus for the AvP port, at the owner's
// ruling (through PO) that the game repository alone must be enough to understand and rebuild online play.
//
// WHY. The game's online play is built from the server repository's CRYPTO.md and PROTOCOL.md. Copies of them live here, so a stranger
// who finds only this repository still has the design. A copy that drifts, by an edit here or by a copy cut short, would quietly stop
// matching the server's, so this fails instead. Taking a newer copy is a deliberate commit: docs/protocol/COPIED.md says how.
//
// HOW. COPIED.md's lines name each file with its sha256 and size (its last section says the exact form). The sha256 is Windows' own
// (bcrypt). The checks are pure functions, run twice: on the real files, and on made-up cases where each must say no, so taking one
// of them out is noticed (tests/takeouts/protocol_copy_test.json).
//
// tests/build.ps1 compiles this, and the program gives back a non-zero result if any check failed. It reads only docs/protocol/.

#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>
#include <string.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// One file COPIED.md lists: its name in docs/protocol/, its sha256 as 64 small hex digits, and its size in bytes
struct CopiedFile
{
	std::string name;
	std::string sha256;
	unsigned long long bytes;
};

// The SHA-256 of some bytes, as 64 small hex digits, by Windows' own cryptography. Empty if Windows couldn't do it.
static std::string Sha256Hex(const std::string &data)
{
	BCRYPT_ALG_HANDLE algorithm = NULL;
	BCRYPT_HASH_HANDLE hash = NULL;
	unsigned char digest[32] = {};
	bool done = BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0))
		&& BCRYPT_SUCCESS(BCryptCreateHash(algorithm, &hash, NULL, 0, NULL, 0, 0));

	// BCryptHashData takes at most 4 GB at a time; these files are far smaller, and anything bigger is refused rather than cut short
	if (done && data.size() > 0xFFFFFFFFu)
	{
		done = false;
	}

	if (done)
	{
		done = BCRYPT_SUCCESS(BCryptHashData(hash, (PUCHAR)data.data(), (ULONG)data.size(), 0))
			&& BCRYPT_SUCCESS(BCryptFinishHash(hash, digest, sizeof(digest), 0));
	}

	if (hash)
	{
		BCryptDestroyHash(hash);
	}

	if (algorithm)
	{
		BCryptCloseAlgorithmProvider(algorithm, 0);
	}

	if (!done)
	{
		return "";
	}

	static const char hexDigits[] = "0123456789abcdef";
	std::string text;

	for (unsigned char b : digest)
	{
		text += hexDigits[b >> 4];
		text += hexDigits[b & 15];
	}

	return text;
}

static bool IsSmallHex(const std::string &text, size_t length)
{
	if (text.size() != length)
	{
		return false;
	}

	for (char c : text)
	{
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
		{
			return false;
		}
	}

	return true;
}

// A file's name, as COPIED.md may give it: letters, digits, '.', '_' and '-' only, so never a folder, and never "." or ".."
static bool IsPlainName(const std::string &name)
{
	if (name.empty() || name == "." || name == "..")
	{
		return false;
	}

	for (char c : name)
	{
		if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
		{
			return false;
		}
	}

	return true;
}

// The text between the first two backticks from 'at' in 'line', and where the second one is; false if there aren't two
static bool Quoted(const std::string &line, size_t at, std::string *inside, size_t *end)
{
	size_t const open = line.find('`', at);
	size_t const close = open == std::string::npos ? std::string::npos : line.find('`', open + 1);

	if (close == std::string::npos)
	{
		return false;
	}

	*inside = line.substr(open + 1, close - open - 1);
	*end = close;
	return true;
}

// Reads COPIED.md's lines: the "From:" line's commit, and one line a file. A line that starts as one of them but isn't exactly in
// the form refuses the whole file, with why. Every other line is the page's own words, and is left alone.
static bool ReadCopied(const std::string &text, std::string *commit, std::vector<CopiedFile> *files, std::string *why)
{
	std::istringstream lines(text);
	std::string line;
	std::set<std::string> seen;
	commit->clear();
	files->clear();

	while (std::getline(lines, line))
	{
		if (!line.empty() && line.back() == '\r')
		{
			line.pop_back();
		}

		static const char from[] = "From: AVPx-Dedicated-Server, commit `";

		if (line.compare(0, strlen(from), from) == 0)
		{
			std::string found;
			size_t end = 0;

			if (!commit->empty() || !Quoted(line, strlen(from) - 1, &found, &end) || !IsSmallHex(found, 40))
			{
				*why = "the From: line isn't once, with a 40-digit commit: " + line;
				return false;
			}

			*commit = found;
			continue;
		}

		if (line.compare(0, 3, "- `") != 0)
		{
			continue;
		}

		CopiedFile file;
		std::string bytes;
		size_t end = 0;
		size_t hashEnd = 0;

		if (!Quoted(line, 2, &file.name, &end) || line.compare(end + 1, 10, ": sha256 `") != 0
			|| !Quoted(line, end + 1, &file.sha256, &hashEnd))
		{
			*why = "a file's line isn't in the form: " + line;
			return false;
		}

		if (!IsPlainName(file.name))
		{
			*why = "a file's name isn't a plain name: " + line;
			return false;
		}

		if (!IsSmallHex(file.sha256, 64))
		{
			*why = "a file's sha256 isn't 64 small hex digits: " + line;
			return false;
		}

		static const char tail[] = " bytes";
		std::string const rest = line.substr(hashEnd + 1);

		if (rest.size() <= 2 + strlen(tail) || rest.compare(0, 2, ", ") != 0
			|| rest.compare(rest.size() - strlen(tail), strlen(tail), tail) != 0)
		{
			*why = "a file's line doesn't end with its size in bytes: " + line;
			return false;
		}

		bytes = rest.substr(2, rest.size() - 2 - strlen(tail));

		if (bytes.empty() || bytes.size() > 12 || bytes.find_first_not_of("0123456789") != std::string::npos || (bytes.size() > 1 && bytes[0] == '0'))
		{
			*why = "a file's size isn't a whole number: " + line;
			return false;
		}

		file.bytes = std::stoull(bytes);

		if (!seen.insert(file.name).second)
		{
			*why = "a file is listed twice: " + file.name;
			return false;
		}

		files->push_back(file);
	}

	if (commit->empty())
	{
		*why = "there's no From: line with the server's commit";
		return false;
	}

	if (files->empty())
	{
		*why = "no file is listed";
		return false;
	}

	return true;
}

// Whether these bytes are the file COPIED.md lists: its size, and its sha256
static bool CopyMatches(const std::string &bytes, const CopiedFile &file)
{
	return bytes.size() == file.bytes && Sha256Hex(bytes) == file.sha256;
}

// The names in the folder that COPIED.md doesn't list. COPIED.md itself and .gitattributes are the folder's own, not copies.
static std::vector<std::string> Unlisted(const std::vector<std::string> &inFolder, const std::vector<CopiedFile> &files)
{
	std::set<std::string> listed;
	std::vector<std::string> extra;

	for (const CopiedFile &file : files)
	{
		listed.insert(file.name);
	}

	for (const std::string &name : inFolder)
	{
		if (name != "COPIED.md" && name != ".gitattributes" && listed.count(name) == 0)
		{
			extra.push_back(name);
		}
	}

	return extra;
}

// The repository's top folder, found from where this file is: everything before "tests". When the compiler was given this file's
// path from the top folder, that's empty, and the test reads from the folder it runs in, as tests.yml runs every test from the top.
static bool RepositoryTop(std::string *top)
{
	std::string const here = __FILE__;
	size_t const tests = here.rfind("tests");

	if (tests == std::string::npos)
	{
		return false;
	}

	*top = here.substr(0, tests);
	return true;
}

static bool ReadWhole(const std::string &path, std::string *bytes)
{
	std::ifstream file(path.c_str(), std::ios::binary);

	if (!file)
	{
		return false;
	}

	std::stringstream all;
	all << file.rdbuf();
	*bytes = all.str();
	return true;
}

// The made-up cases: each of the checks above must say no where it should, or taking it out would go unnoticed
static void MadeUpCases()
{
	printf("\nThe checks themselves, on made-up cases\n");

	// FIPS 180-2's own example: the SHA-256 of "abc"
	Check(Sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "Windows' SHA-256 gives the standard's own answer for \"abc\"");

	CopiedFile const abc = { "x.md", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", 3 };
	Check(CopyMatches("abc", abc), "the right bytes match");
	Check(!CopyMatches("abd", abc), "a copy with one byte changed doesn't match");
	CopiedFile const wrongSize = { "x.md", abc.sha256, 4 };
	Check(!CopyMatches("abc", wrongSize), "the right sha256 with another size doesn't match");

	std::string const head = "From: AVPx-Dedicated-Server, commit `8b90047af4636478ce39dad91dcbdd26bf01d0ee` (branch v2)\n";
	std::string const good = "- `x.md`: sha256 `" + abc.sha256 + "`, 3 bytes\n";
	std::string commit;
	std::vector<CopiedFile> files;
	std::string why;
	Check(ReadCopied("Words.\n" + head + "\n" + good, &commit, &files, &why) && files.size() == 1 && files[0].name == "x.md"
		&& files[0].bytes == 3 && commit == "8b90047af4636478ce39dad91dcbdd26bf01d0ee", "a page in the form reads to its commit and its one file");

	struct Bad
	{
		std::string text;
		const char *what;
	};

	std::string const upper = abc.sha256.substr(0, 63) + "D";
	Bad const bads[] = {
		{ good, "no From: line is refused" },
		{ head + head + good, "two From: lines are refused" },
		{ "From: AVPx-Dedicated-Server, commit `8b90047`\n" + good, "a short commit is refused" },
		{ head, "no file at all is refused" },
		{ head + good + good, "a file listed twice is refused" },
		{ head + "- `x.md`: sha256 `" + abc.sha256.substr(1) + "`, 3 bytes\n", "a sha256 of 63 digits is refused" },
		{ head + "- `x.md`: sha256 `" + upper + "`, 3 bytes\n", "a sha256 with a capital is refused" },
		{ head + "- `../x.md`: sha256 `" + abc.sha256 + "`, 3 bytes\n", "a name with a folder in it is refused" },
		{ head + "- `x.md`: sha256 `" + abc.sha256 + "`, 03 bytes\n", "a size with a 0 in front is refused" },
		{ head + "- `x.md`: sha256 `" + abc.sha256 + "`, 3\n", "a size with no \"bytes\" is refused" },
		{ head + "- `x.md`: sha256 `" + abc.sha256 + "`\n", "a line with no size is refused" },
		{ head + "- `x.md`: sha1 `" + abc.sha256 + "`, 3 bytes\n", "another hash's name is refused" },
	};

	for (const Bad &bad : bads)
	{
		Check(!ReadCopied(bad.text, &commit, &files, &why), bad.what);
	}

	std::vector<CopiedFile> const listed = { { "CRYPTO.md", abc.sha256, 3 }, { "PROTOCOL.md", abc.sha256, 3 } };
	std::vector<std::string> const extra = Unlisted({ "COPIED.md", ".gitattributes", "CRYPTO.md", "PROTOCOL.md", "EXTRA.md" }, listed);
	Check(extra.size() == 1 && extra[0] == "EXTRA.md", "a file in the folder that isn't listed is found");
	Check(Unlisted({ "COPIED.md", ".gitattributes", "CRYPTO.md", "PROTOCOL.md" }, listed).empty(), "the folder's own two files aren't copies");
}

// The real folder
static void TheCopies()
{
	printf("\ndocs/protocol/, against COPIED.md\n");

	std::string top;
	bool const found = RepositoryTop(&top);
	std::string const folder = top + "docs/protocol/";
	std::string text;
	Check(found && ReadWhole(folder + "COPIED.md", &text), "docs/protocol/COPIED.md is there");

	std::string commit;
	std::vector<CopiedFile> files;
	std::string why;
	bool const read = ReadCopied(text, &commit, &files, &why);
	Check(read, read ? "COPIED.md reads: the server's commit " + commit + ", and " + std::to_string(files.size()) + " files" : "COPIED.md reads: " + why);

	std::set<std::string> names;

	for (const CopiedFile &file : files)
	{
		names.insert(file.name);
		std::string bytes;
		bool const there = ReadWhole(folder + file.name, &bytes);
		Check(there && CopyMatches(bytes, file), file.name + " is the server's file: " + std::to_string(file.bytes) + " bytes, sha256 " + file.sha256);
	}

	Check(names.count("CRYPTO.md") == 1 && names.count("PROTOCOL.md") == 1, "both of the design's files are listed: CRYPTO.md and PROTOCOL.md");

	std::vector<std::string> inFolder;
	std::error_code error;

	for (const auto &entry : std::filesystem::directory_iterator(folder, error))
	{
		inFolder.push_back(entry.path().filename().string());
	}

	Check(!error && !inFolder.empty(), "the folder can be listed");
	std::vector<std::string> const extra = Unlisted(inFolder, files);
	std::string said;

	for (const std::string &name : extra)
	{
		said += " " + name;
	}

	Check(extra.empty(), extra.empty() ? "every file in the folder is listed" : "files in the folder that COPIED.md doesn't list:" + said);
}

int main()
{
	printf("protocol_copy_test: the server's design, copied here byte for byte\n");
	MadeUpCases();
	TheCopies();
	printf("\n%s: %d check%s failed\n", checksFailed == 0 ? "PASSED" : "FAILED", checksFailed, checksFailed == 1 ? "" : "s");
	return checksFailed == 0 ? 0 : 1;
}
