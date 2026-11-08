// Is this build a beta? Written 2026-11-07 by Melanikus for the AvP port.
//
// A beta is a build testers get before a numbered release. Its version is the release it leads up to, then "-beta." and its number:
// how many betas of that release came before it. The first beta of 1.0.0 is 1.0.0-beta.0, the next 1.0.0-beta.1, and so on, as many
// in a day as are needed. tools/build-and-package.ps1 writes the version into the build, and tools/check-release.ps1 is what lets one
// through: it refuses any other way of writing it, and any number but the next. A numbered release (1.0.0) or a build the script didn't
// make has some other version, so it isn't a beta.
//
// Before 2026-11-08 a beta was named "betarelease" and its day, such as betarelease20261107, so there could only be one a day. That one
// beta is still on the releases page under that name, but nothing builds a version written that way now, and this doesn't count it.
//
// In a beta, the console's debug and cheat commands are there for testers to use, and using them doesn't count as cheating: saving,
// statistics, the films after a level and the next level's unlock all go on as normal (the user's ruling, 2026-11-07: "play on normally").
// See BuildIdentity_IsABeta in build_identity.cpp, and the three places that ask it: game_commands.cpp, gamevars.cpp and cheat_codes.cpp.
//
// This file touches nothing in the game, so tests/debug_commands_test.cpp can try any version text.
#ifndef BETA_BUILD_H
#define BETA_BUILD_H

#include <stddef.h>

// How long the number starting at version[at] is, if it's written the way a version's numbers are: 0 on its own, or digits that don't
// start with 0, at most `most` of them. Gives back 0 if there's no such number there. It reads one letter at a time and stops at the first
// that isn't a digit, so it never reads past the end of the text.
inline size_t BetaBuild_NumberHere(const char *version, size_t at, size_t most)
{
	size_t digits = 0;

	while (digits <= most && version[at + digits] >= '0' && version[at + digits] <= '9')
	{
		digits++;
	}

	// Too many digits, or a 0 in front of others: 01 would be a second way to write 1, and the same beta could then be counted twice
	if (digits > most || (digits > 1 && version[at] == '0'))
	{
		return 0;
	}

	return digits;
}

// True only for the exact form tools/check-release.ps1 lets through, such as 1.0.0-beta.0, and nothing else: three numbers with a dot
// between them (each 0, or up to 6 digits that don't start with 0), then "-beta.", then the beta's number (0, or up to 4 digits that
// don't start with 0), then the end of the text.
inline bool BetaBuild_VersionIsABeta(const char *version)
{
	static const char kBeta[] = "-beta.";

	if (version == 0)
	{
		return false;
	}

	size_t at = 0;

	// The release's three numbers, with a dot after the first two
	for (int part = 0; part < 3; part++)
	{
		size_t const digits = BetaBuild_NumberHere(version, at, 6);

		if (digits == 0)
		{
			return false;
		}

		at += digits;

		if (part < 2)
		{
			if (version[at] != '.')
			{
				return false;
			}

			at++;
		}
	}

	// "-beta.", one letter at a time. A text that ends early stops here too, at its end.
	for (size_t letter = 0; kBeta[letter] != 0; letter++, at++)
	{
		if (version[at] != kBeta[letter])
		{
			return false;
		}
	}

	// The beta's number, and then nothing more
	size_t const digits = BetaBuild_NumberHere(version, at, 4);

	if (digits == 0)
	{
		return false;
	}

	return version[at + digits] == 0;
}

#endif
