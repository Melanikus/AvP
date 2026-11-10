// Which build of the game is running, for the top of the log. Written 2026-11-07 by Melanikus for the AvP port.
//
// A tester's log is only useful if it says which program wrote it. So the log starts with the version and the commit it was built from,
// when the build script said them, and always with the program file's own size and fingerprint, which tells two builds apart even when
// nobody wrote their version down.
#ifndef BUILD_IDENTITY_H
#define BUILD_IDENTITY_H

#include <stddef.h>
#include <stdint.h>
#include <string>

// Three lines, with no line end after the last: "build: version ..., commit ...", "build: this program is ... bytes, MD5 ..." and
// "build: running on Windows 10.0, build 26200".
std::string BuildIdentity_Lines();

// The last of those lines alone
std::string BuildIdentity_Windows();

// Whether this build is a beta: see beta_build.h. In a beta the console's debug and cheat commands are there, and using them doesn't
// count as cheating.
bool BuildIdentity_IsABeta();

// Added 2026-11-09 by Melanikus for the AvP port: the commit this build was made from, as a number, for the -dev v2 draft's rule that two
// copies play only if they were made from the same commit (networking.cpp, Net_V2DraftOn). It's the commit's first 8 hex digits, which
// tell two drafts apart; it proves nothing about who made them, and isn't meant to. False, with 0, when the build doesn't know its commit:
// any build not made by tools/build-and-package.ps1, such as one made in Visual Studio on someone's own machine.
bool BuildIdentity_CommitWord(uint32_t *out);

// The same from a commit's text: exactly 40 hex digits in small letters, as git writes one, and nothing after them. False, with 0, for
// anything else, "not known" included. Game-free, so a test can try it.
inline bool BuildIdentity_WordFromCommit(const char *commit, uint32_t *out)
{
	*out = 0;

	if (!commit)
	{
		return false;
	}

	uint32_t word = 0;

	// The loop stops at the first letter that isn't a hex digit, so it never reads past a shorter text's end
	for (size_t i = 0; i < 40; i++)
	{
		char const c = commit[i];
		uint32_t digit = 0;

		if (c >= '0' && c <= '9')
		{
			digit = static_cast<uint32_t>(c - '0');
		}
		else if (c >= 'a' && c <= 'f')
		{
			digit = static_cast<uint32_t>(c - 'a' + 10);
		}
		else
		{
			return false;
		}

		if (i < 8)
		{
			word = (word << 4) | digit;
		}
	}

	if (commit[40] != 0)
	{
		return false;
	}

	*out = word;
	return true;
}

#endif
