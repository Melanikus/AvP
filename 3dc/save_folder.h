// Making the folder the game keeps its files in. Written 2026-11-05 by Melanikus for the AvP port.
//
// The game keeps its settings, its log and its saved games in a folder of its own: My Games/Aliens versus Predator, inside
// the player's Documents. This makes that folder and the two folders inside it, and says what went wrong if it can't.
//
// It's on its own so that a test can hand it a folder of the test's choosing, and see what happens when something goes wrong.
// GetSaveFolderPath in utilities.cpp is what the rest of the game calls.
#ifndef SAVE_FOLDER_H
#define SAVE_FOLDER_H

#include <windows.h>
#include <algorithm>
#include <string>
#include <string.h>

// The game's own folder, inside My Games. A second copy of the game on the same machine, for trying two players on one PC, keeps its
// files in a folder of its own, so the two don't share a log, settings or saved games (-dev -secondcopy, winmain.cpp). Both names are fixed
// here: no part of the command line, or of anything else from outside, ever becomes part of the path. (Added 2026-11-08.)
static const char *const kSaveFolderGameName = "Aliens versus Predator";
static const char *const kSaveFolderSecondCopyName = "Aliens versus Predator (second copy)";

static const char *const kSaveFolderNothingGiven = "there's no folder to make the save folder in";
static const char *const kSaveFolderCantMake = "a folder on the way to the save folder can't be made";
static const char *const kSaveFolderTooLong = "the save folder's path is too long";

// Makes one folder. A folder that's there already is fine, and so is any other answer from Windows except "the folder this
// goes in doesn't exist", because that's the only one that means there's no way on. So it says false only for that one, and
// where is told which folder it was, if it was given one.
inline bool SaveFolder_MakeOne(const std::string &path, std::string *where)
{
	if (::CreateDirectoryA(path.c_str(), NULL) != 0 || ::GetLastError() != ERROR_PATH_NOT_FOUND)
	{
		return true;
	}

	if (where)
	{
		*where = path;
	}

	return false;
}

// under is the folder to make it in, and folder is where the answer goes, which has room for size characters.
// It gives back 0 if it worked. Then folder holds the path, with / between its parts and one on the end, ready for a file's name.
// Otherwise it gives back what went wrong, and folder is empty, so nobody can mistake half a path for the whole of one. where can be left out. If it's given, it says which folder was the trouble.
// secondCopy makes the second copy's folder instead of the game's own (kSaveFolderSecondCopyName).
inline const char *SaveFolder_Make(const char *under, char *folder, size_t size, std::string *where, bool secondCopy = false)
{
	if (where)
	{
		where->clear();
	}

	if (!folder || size == 0)
	{
		return kSaveFolderNothingGiven;
	}

	folder[0] = 0;

	if (!under || !under[0])
	{
		return kSaveFolderNothingGiven;
	}

	std::string path = under;

	// the game's file names use forward slashes, and Windows takes either
	std::replace(path.begin(), path.end(), '\\', '/');

	path += "/My Games";

	if (!SaveFolder_MakeOne(path, where))
	{
		return kSaveFolderCantMake;
	}

	path += std::string("/") + (secondCopy ? kSaveFolderSecondCopyName : kSaveFolderGameName);

	if (!SaveFolder_MakeOne(path, where))
	{
		return kSaveFolderCantMake;
	}

	path += "/";

	if (!SaveFolder_MakeOne(path + "User_Profiles/", where) || !SaveFolder_MakeOne(path + "MPConfig/", where))
	{
		return kSaveFolderCantMake;
	}

	if (path.length() > size - 1)
	{
		if (where)
		{
			*where = path;
		}

		return kSaveFolderTooLong;
	}

	// The letters, then the 0 that ends them on a line of its own: the length was checked against the room just above
	memcpy(folder, path.c_str(), path.length());
	folder[path.length()] = 0;
	return 0;
}

// A FOLDER OF THE CALLER'S CHOOSING (SaveFolder_UseThisFolder, utilities.h). The map editor keeps the game's files (its log, its
// settings) in a folder of its own beside it, so it never writes into the player's My Games. The folder comes from outside the game, so
// it's checked here, before the game uses it. (Added 2026-11-14 by Melanikus for the AvP port, SAVE-FOLDER-1.)
//
// The longest name the game itself puts after the save folder's path, in letters, measured 2026-11-14 at afbf9545:
//   MPConfig/<a network setting's name, at most 21 letters: MP_Config_Name[22], menu_data.cpp>.skirmish_cfg     43
//   User_Profiles/<a player's name, at most 15: MAX_SIZE_OF_USERS_NAME, user_profile.h>_<a slot, 1 to 8>.sav    35
//   AliensVsPredator.cfg (ConfigFile.cpp) 20; mark-999999.png (mark_files.h) 15; avp_log.5.txt, the oldest kept log (log_files.h) 13
// A folder given has to leave room for it, so each of those paths fits in MAX_PATH. One name isn't the game's own: a level's log, when it
// can't be written beside the level, is named after the level (level_log.h), whose name can be 200 letters long. That one may not fit,
// and then it isn't written, which the game's own log says (level_log.cpp).
static const size_t kSaveFolderLongestName = 43;

static const char *const kSaveFolderGivenNothing = "no folder was given";
static const char *const kSaveFolderGivenNotWhole = "the folder isn't a whole path, starting with a drive letter, a colon and a slash";
static const char *const kSaveFolderGivenWholeDrive = "the folder is a whole drive";
static const char *const kSaveFolderGivenStream = "the folder's path has a colon after its drive letter, which names a hidden stream, not a folder";
static const char *const kSaveFolderGivenGoesUp = "the folder's path has a \"..\" part";
static const char *const kSaveFolderGivenPartEnding = "a part of the folder's path ends in a dot or a space";
static const char *const kSaveFolderGivenTwoSlashes = "the folder's path ends in more than one slash";
static const char *const kSaveFolderGivenTooLong = "the folder's path is too long to leave room for the game's file names";
static const char *const kSaveFolderGivenNotThere = "there's no folder there";
static const char *const kSaveFolderGivenAFile = "that's a file, not a folder";
static const char *const kSaveFolderGivenLink = "that's a link, or another reparse point, not a plain folder";

// Can the game keep its files in folder? It gives back 0 if it can, and then out (room for outSize letters) holds the folder, ending in
// exactly one '\', ready for a file's name. Otherwise it gives back why not, and out is empty. The folder must be:
// - a whole path from a drive letter, "C:\..." or "C:/...". Refused: a path from the current folder ("settings"), from the top of the
//   current drive ("\settings"), a drive's own current folder ("C:settings"), another computer's ("\\server\share") and Windows' own
//   device names ("\\.\", "\\?\").
// - not a whole drive ("C:\"), with no colon after the drive letter, no ".." part, and no part ending in a dot or a space;
// - ending in at most one slash, which is given back as '\';
// - short enough that the game's longest file name still fits after it (kSaveFolderLongestName), within MAX_PATH;
// - a plain folder that's there: not a file, and not a link or any other reparse point. That's asked of the folder's own name, never of
//   what a link points to (GetFileAttributesA doesn't follow the last part of a path), and without its last slash, which would make
//   Windows go through a link to the folder behind it.
inline const char *SaveFolder_WhyNotUsable(const char *folder, char *out, size_t outSize)
{
	if (out && outSize)
	{
		out[0] = 0;
	}

	if (!out || outSize == 0 || !folder || !folder[0])
	{
		return kSaveFolderGivenNothing;
	}

	bool const driveLetter = (folder[0] >= 'A' && folder[0] <= 'Z') || (folder[0] >= 'a' && folder[0] <= 'z');
	if (!driveLetter || folder[1] != ':' || (folder[2] != '\\' && folder[2] != '/'))
	{
		return kSaveFolderGivenNotWhole;
	}

	// Its length is found looking no further than MAX_PATH, so a path with no end is never read past that
	size_t const length = strnlen(folder, MAX_PATH);
	if (length >= MAX_PATH)
	{
		return kSaveFolderGivenTooLong;
	}
	std::string path(folder, length);

	if (path.find(':', 2) != std::string::npos)
	{
		return kSaveFolderGivenStream;
	}

	// One slash at its end is taken off here, and put back as '\' at the end. A second one is refused.
	if (path.back() == '\\' || path.back() == '/')
	{
		path.pop_back();
	}
	if (path.back() == '\\' || path.back() == '/')
	{
		return kSaveFolderGivenTwoSlashes;
	}
	if (path.length() <= 2)
	{
		return kSaveFolderGivenWholeDrive;
	}

	// Each part, between two slashes or at the end, from just after the drive's slash. A ".." part goes up a folder. A part that ends in a
	// dot or a space is refused too: Windows drops those from the end of a name, so ".. " could become "..", and no folder can be made
	// with such a name anyway (the game's rule for level file names refuses them as well: level_file_name.h).
	for (size_t start = 3; start <= path.length(); )
	{
		size_t end = path.find_first_of("\\/", start);
		if (end == std::string::npos)
		{
			end = path.length();
		}
		std::string const part = path.substr(start, end - start);
		if (part == "..")
		{
			return kSaveFolderGivenGoesUp;
		}
		if (!part.empty() && (part.back() == '.' || part.back() == ' '))
		{
			return kSaveFolderGivenPartEnding;
		}
		start = end + 1;
	}

	// The folder, its '\', the longest name and the 0 that ends them all have to fit in MAX_PATH, and the folder with its '\' in out
	if (path.length() + 1 + kSaveFolderLongestName + 1 > MAX_PATH || path.length() + 2 > outSize)
	{
		return kSaveFolderGivenTooLong;
	}

	DWORD const what = ::GetFileAttributesA(path.c_str());
	if (what == INVALID_FILE_ATTRIBUTES)
	{
		return kSaveFolderGivenNotThere;
	}
	if ((what & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
	{
		return kSaveFolderGivenLink;
	}
	if ((what & FILE_ATTRIBUTE_DIRECTORY) == 0)
	{
		return kSaveFolderGivenAFile;
	}

	memcpy(out, path.c_str(), path.length());
	out[path.length()] = '\\';
	out[path.length() + 1] = 0;
	return 0;
}

#endif
