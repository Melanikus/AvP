// Is this build a beta? Written 2026-11-07 by Melanikus for the AvP port.
//
// A beta is the build testers get before a numbered release. tools/build-and-package.ps1 writes its version as "betarelease" and the
// day as eight digits, such as betarelease20261107, and that's the only beta name tools/check-release.ps1 lets through. A numbered release
// (three numbers with dots) or any build the script didn't make has some other version, so it isn't a beta.
//
// In a beta, the console's debug and cheat commands are there for testers to use, and using them doesn't count as cheating: saving,
// statistics, the films after a level and the next level's unlock all go on as normal (the user's ruling, 2026-11-07: "play on normally").
// See BuildIdentity_IsABeta in build_identity.cpp, and the three places that ask it: game_commands.cpp, gamevars.cpp and cheat_codes.cpp.
//
// This file touches nothing in the game, so tests/debug_commands_test.cpp can try any version text.
#ifndef BETA_BUILD_H
#define BETA_BUILD_H

#include <stddef.h>

// True only for "betarelease" followed by exactly eight digits and nothing else. It reads one letter at a time and stops at the first
// that doesn't fit, so it never reads past the end of a shorter text.
inline bool BetaBuild_VersionIsABeta(const char *version)
{
	static const char kWord[] = "betarelease";

	if (version == 0)
	{
		return false;
	}

	size_t at = 0;

	for (; kWord[at] != 0; at++)
	{
		if (version[at] != kWord[at])
		{
			return false;
		}
	}

	for (int digit = 0; digit < 8; digit++, at++)
	{
		if (version[at] < '0' || version[at] > '9')
		{
			return false;
		}
	}

	return version[at] == 0;
}

#endif
