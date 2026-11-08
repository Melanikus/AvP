// A level's own log, as the game uses it. Written 2026-11-07 by Melanikus for the AvP port (card LEVELLOG-1).
//
// LoadRifFile in system.cpp starts gathering before it reads a level, and finishes once the level is read or has failed. Meanwhile the
// game's log hands every line it writes to LevelLog_NoteLine as well. level_log.h says when a level's log is written, where, and how
// the name and the file are checked.
#include "level_log.h"
#include "logString.h"
#include "utilities.h"
#include "md5.h"

#include <stdio.h>
#include <mutex>

// The commit this was built from. The build script writes this file, and it isn't in the repository (see d3d_render.cpp).
#if __has_include("build_commit.h")
#include "build_commit.h"
#else
#define AVP_BUILD_COMMIT "not known"
#endif

// The lines gathered so far. Other threads write to the game's log too (the music and the films have threads of their own), so the
// lines are only ever touched with this lock held. Both are made the first time they're asked for, because the game's log can be
// written before this file's own objects have been made, while the game is starting.
static LevelLogLines &Gathered()
{
	static LevelLogLines lines = { false, std::string(), 0, 0, 0 };
	return lines;
}

static std::mutex &GatheredLock()
{
	static std::mutex lock;
	return lock;
}

void LevelLog_StartGathering()
{
	std::lock_guard<std::mutex> hold(GatheredLock());
	Gathered().Start();
}

void LevelLog_NoteLine(const std::string &line, bool isError)
{
	std::lock_guard<std::mutex> hold(GatheredLock());
	Gathered().Note(line, isError);
}

// The level file's size and its MD5, so a map maker can tell which copy of the level this was about. MD5 is only a fingerprint here,
// not a security check. Read in pieces, so a big file isn't held in memory.
static std::string FactsAboutTheFile(const char *levelFile)
{
	if (!levelFile || !levelFile[0])
	{
		return "Its file: none, because its path couldn't be made\r\n";
	}

	HANDLE const file = ::CreateFileA(levelFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE)
	{
		return "Its file: it couldn't be opened\r\n";
	}

	struct MD5Context md5;
	MD5Init(&md5);

	unsigned long long size = 0;
	char piece[65536];
	DWORD read = 0;
	bool readAll = true;

	for (;;)
	{
		if (!::ReadFile(file, piece, sizeof(piece), &read, NULL))
		{
			readAll = false;
			break;
		}

		if (read == 0)
		{
			break;
		}

		MD5Update(&md5, (md5byte const *)piece, read);
		size += read;
	}

	::CloseHandle(file);

	unsigned char digest[16];
	MD5Final(digest, &md5);

	char line[200];

	if (readAll)
	{
		char hex[33];

		for (int i = 0; i < 16; i++)
		{
			_snprintf(hex + i * 2, 3, "%02x", digest[i]);
		}

		hex[32] = '\0';
		_snprintf(line, sizeof(line) - 1, "Its file: %llu bytes, MD5 %s\r\n", size, hex);
	}
	else
	{
		_snprintf(line, sizeof(line) - 1, "Its file: it couldn't all be read, after %llu bytes\r\n", size);
	}

	line[sizeof(line) - 1] = '\0';
	return line;
}

void LevelLog_Finish(const char *levelsFolder, const char *levelName, const char *levelFile, bool failed)
{
	// Taken out under the lock, so what's written below is what was gathered, and the log's own lines about it aren't gathered
	LevelLogLines finished;
	{
		std::lock_guard<std::mutex> hold(GatheredLock());
		Gathered().Stop();
		finished = Gathered();
		Gathered().text.clear();
	}

	// A level that loaded with nothing refused has nothing to say
	if (!failed && finished.errors == 0)
	{
		return;
	}

	std::string folder, file;
	bool const nameIsSafe = LevelLog_SplitName(levelName, &folder, &file);

	std::string text = kLevelLogFirstLine;
	text += "Level: " + (nameIsSafe ? folder + file : std::string("one whose name can't be used as a file name")) + "\r\n";
	text += FactsAboutTheFile(levelFile);
	text += std::string("The game: built from ") + AVP_BUILD_COMMIT + "\r\n";

	char counts[160];

	if (failed)
	{
		_snprintf(counts, sizeof(counts) - 1, "What happened: the level couldn't be loaded. The game said %d errors on the way.\r\n", finished.errors);
	}
	else
	{
		_snprintf(counts, sizeof(counts) - 1, "What happened: the level loaded, with %d things refused or left out.\r\n", finished.errors);
	}

	counts[sizeof(counts) - 1] = '\0';
	text += counts;
	text += "What the game said while loading it:\r\n";
	text += finished.text;

	if (finished.linesLeftOut > 0)
	{
		char more[80];
		_snprintf(more, sizeof(more) - 1, "... and %d more lines, left out to keep this file small\r\n", finished.linesLeftOut);
		more[sizeof(more) - 1] = '\0';
		text += more;
	}

	// Beside the level first, then the game's own folder
	std::string const beside = LevelLog_PathBesideTheLevel(levelsFolder, levelName);
	const char *problem = beside.empty() ? "the level's name can't be used as a file name" : LevelLog_WriteFile(beside, text);

	if (!problem)
	{
		LogString("level log: written beside the level, as " + beside);
		return;
	}

	std::string const fallback = LevelLog_PathInTheGamesFolder(GetSaveFolderPath(), levelName);
	const char *const fallbackProblem = LevelLog_WriteFile(fallback, text);

	if (!fallbackProblem)
	{
		LogString(std::string("level log: not beside the level (") + problem + "), so it was written as " + fallback);
	}
	else
	{
		LogString(std::string("level log: not written. Beside the level: ") + problem + ". In the game's folder: " + fallbackProblem);
	}
}
