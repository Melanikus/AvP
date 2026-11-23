// RED STUB (Drake 2026-11-23, card 6): the names the plain file name test calls, taking every name as the game did before (BuildConfigPath
// put any name into its path), so that the test shows the old way failing. The next commit puts the real check in its place.
#ifndef PLAIN_FILE_NAME_H
#define PLAIN_FILE_NAME_H

#include <string.h>
#include <string>

static const size_t kPlainFileNameMost = 100;

inline bool PlainFileName_IsDeviceName(const char *)
{
	return false;
}

inline std::string PlainFileName_Why(const char *)
{
	return std::string();
}

inline std::string PlainFileName_Printable(const char *name)
{
	return name ? std::string(name) : std::string("(none)");
}

#endif
