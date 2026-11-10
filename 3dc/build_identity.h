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

// (Removed 2026-11-10 by Melanikus for the AvP port, #175 PR 3a: the commit as a number, which only the v2 draft's same-commit rule used.)

#endif
