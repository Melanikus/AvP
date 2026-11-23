// The plain write test. Written 2026-11-23 by Melanikus (Drake) for the AvP port (card 6, Tao 1 and 3).
//
// WHAT IT TESTS
//
// 3dc/plain_write.h opens a file to write it without ever writing through a link: a symbolic link, a junction for its folder, or a
// second name (a hard link) of another file. Before it, the game wrote its settings, profiles, saves and logs with a plain CreateFile or
// fopen, which follows whatever is at the name, so a link put there by anyone made the game empty and write some other file.
//
// It tries PlainWrite_Open in each of its ways (made, emptied, kept, only new, only there), PlainWrite_FOpen in fopen's writing modes,
// and FileStream's write, which the multiplayer settings are saved with. Each on a plain file, on a folder where the file should be, in
// a folder that isn't there, and on a device's name (NUL), which a plain CreateFile opens as the device: writes vanish there, and the
// game thinks it saved. Then, by reading the source, that every way the game writes a file goes through it.
//
// The links themselves (a hard link, a symbolic link to a file and to nothing, a junction for the folder) are only made on GitHub's
// machines, where GITHUB_ACTIONS is set, because making links is what antivirus programs watch for. Anywhere else the test says they
// were skipped.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles it with tests/plain_write_test.args, together with the game's real FileStream.cpp. Run it from the top of
// the repository: it makes its files in a folder of its own there, and reads the source from there. Non-zero if any check failed.
#include "plain_write.h"
#include "FileStream.h"
#include "FastFile.h"

#include <windows.h>
#include <winioctl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// FileStream can also read from the game's archives. This test uses plain files, so these two are stand-ins that are never asked for anything.
FastFileHandle* FF_Find(const std::string &)
{
	return 0;
}

const std::string & FF_GetFastFileName(int32_t)
{
	static const std::string noName;
	return noName;
}

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

static std::string ReadWholeFile(const std::string &name)
{
	std::ifstream file(name.c_str(), std::ios::binary);
	std::stringstream text;
	text << file.rdbuf();
	return text.str();
}

static void WriteWholeFile(const std::string &name, const std::string &bytes)
{
	std::ofstream file(name.c_str(), std::ios::binary | std::ios::trunc);
	file << bytes;
}

// The whole path of a name in the folder the test runs in, as a link's target must be given (see TestLinks)
static std::string WholePath(const std::string &name)
{
	char whole[MAX_PATH];
	DWORD const length = ::GetFullPathNameA(name.c_str(), MAX_PATH, whole, NULL);
	return length > 0 && length < MAX_PATH ? std::string(whole) : name;
}

static bool IsThere(const std::string &name)
{
	return ::GetFileAttributesA(name.c_str()) != INVALID_FILE_ATTRIBUTES;
}

static bool IsAFolder(const std::string &name)
{
	DWORD const what = ::GetFileAttributesA(name.c_str());
	return what != INVALID_FILE_ATTRIBUTES && (what & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// Writes bytes through an open handle and closes it; true if all of them went
static bool WriteAndClose(HANDLE file, const std::string &bytes)
{
	DWORD written = 0;
	bool const ok = ::WriteFile(file, bytes.data(), (DWORD)bytes.size(), &written, NULL) && written == bytes.size();
	::CloseHandle(file);
	return ok;
}

// This run's own folder, inside the folder the test runs in, named by nothing but this program's number
static std::string TestFolder()
{
	return "plain_write_test_" + std::to_string(::GetCurrentProcessId());
}

// A source file, with its line ends written \n whatever the checkout made them
static std::string ReadSource(const std::string &name)
{
	std::string out;

	for (char c : ReadWholeFile(name))
	{
		if (c != '\r')
		{
			out += c;
		}
	}

	return out;
}

// The text of a function, from the line that starts it to the first line that is only "}"
static std::string FunctionText(const std::string &source, const std::string &start)
{
	size_t const from = source.find(start);

	if (from == std::string::npos)
	{
		return std::string();
	}

	size_t const to = source.find("\n}\n", from);
	return source.substr(from, to == std::string::npos ? std::string::npos : to + 3 - from);
}

// ---- PlainWrite_Open on plain files --------------------------------------------------------------------------------------------------

static void TestOpenWays(const std::string &base)
{
	puts("PlainWrite_Open, each way, on plain files");

	std::string const name = base + "\\file.txt";
	std::string why;

	HANDLE file = PlainWrite_Open(name, GENERIC_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, &why);
	Check(file != INVALID_HANDLE_VALUE && WriteAndClose(file, "new") && ReadWholeFile(name) == "new", "CREATE_ALWAYS makes a file that isn't there");

	WriteWholeFile(name, "old old old");
	file = PlainWrite_Open(name, GENERIC_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, &why);
	Check(file != INVALID_HANDLE_VALUE && WriteAndClose(file, "x") && ReadWholeFile(name) == "x", "CREATE_ALWAYS empties a plain file that is there");

	WriteWholeFile(name, "kept");
	file = PlainWrite_Open(name, GENERIC_WRITE, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, &why);
	Check(file != INVALID_HANDLE_VALUE && WriteAndClose(file, "") && ReadWholeFile(name) == "kept", "OPEN_ALWAYS keeps a plain file as it is");

	file = PlainWrite_Open(name, GENERIC_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, &why);
	Check(file == INVALID_HANDLE_VALUE && ReadWholeFile(name) == "kept" && !why.empty(), "CREATE_NEW refuses a name that's taken, and says why");

	file = PlainWrite_Open(name, GENERIC_WRITE, 0, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, &why);
	Check(file != INVALID_HANDLE_VALUE && WriteAndClose(file, "") && ReadWholeFile(name).empty(), "TRUNCATE_EXISTING empties a plain file that is there");

	std::string const missing = base + "\\missing.txt";
	file = PlainWrite_Open(missing, GENERIC_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, &why);
	Check(file == INVALID_HANDLE_VALUE && !IsThere(missing), "OPEN_EXISTING refuses a name with nothing there, and makes nothing");

	file = PlainWrite_Open(missing, GENERIC_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, &why);
	Check(file != INVALID_HANDLE_VALUE && WriteAndClose(file, "made") && ReadWholeFile(missing) == "made", "CREATE_NEW makes a file that isn't there");

	::DeleteFileA(name.c_str());
	::DeleteFileA(missing.c_str());
}

static void TestOpenRefusals(const std::string &base)
{
	puts("PlainWrite_Open refuses what isn't a plain file");

	std::string why;
	std::string const folder = base + "\\a folder";
	::CreateDirectoryA(folder.c_str(), NULL);

	HANDLE file = PlainWrite_Open(folder, GENERIC_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, &why);
	Check(file == INVALID_HANDLE_VALUE && IsAFolder(folder) && !why.empty(), "a folder where the file should be is refused, and left a folder");

	std::string const inNothing = base + "\\no such folder\\file.txt";
	file = PlainWrite_Open(inNothing, GENERIC_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, &why);
	Check(file == INVALID_HANDLE_VALUE && !IsThere(base + "\\no such folder"), "a file in a folder that isn't there is refused, and no folder is made");

	// A plain CreateFile opens a path ending in \NUL as the device: what's written vanishes, and the writer thinks it saved
	file = PlainWrite_Open(base + "\\NUL", GENERIC_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, &why);
	Check(file == INVALID_HANDLE_VALUE, "a device's name (NUL) is refused: it isn't a file on a disk");

	if (file != INVALID_HANDLE_VALUE)
	{
		::CloseHandle(file);
	}

	::RemoveDirectoryA(folder.c_str());
}

// ---- PlainWrite_FOpen --------------------------------------------------------------------------------------------------------------

static void TestFOpen(const std::string &base)
{
	puts("PlainWrite_FOpen, fopen's writing modes");

	std::string const name = base + "\\stream.txt";
	std::string why;

	FILE *file = PlainWrite_FOpen(name, "w", &why);
	Check(file && fputs("line\n", file) >= 0 && fclose(file) == 0 && ReadWholeFile(name) == "line\r\n", "\"w\" makes a text file, as fopen does (\\n written as \\r\\n)");

	file = PlainWrite_FOpen(name, "wb", &why);
	Check(file && fwrite("ab\n", 1, 3, file) == 3 && fclose(file) == 0 && ReadWholeFile(name) == "ab\n", "\"wb\" empties it and writes the bytes as they are");

	file = PlainWrite_FOpen(name, "ab", &why);
	Check(file && fwrite("cd", 1, 2, file) == 2 && fclose(file) == 0 && ReadWholeFile(name) == "ab\ncd", "\"ab\" adds to its end");

	file = PlainWrite_FOpen(name, "r+b", &why);
	Check(file && fwrite("X", 1, 1, file) == 1 && fclose(file) == 0 && ReadWholeFile(name) == "Xb\ncd", "\"r+b\" opens it as it is, to write over its start");

	file = PlainWrite_FOpen(base + "\\missing.txt", "r+", &why);
	Check(!file && !IsThere(base + "\\missing.txt"), "\"r+\" refuses a file that isn't there");

	file = PlainWrite_FOpen(name, "rb", &why);
	Check(!file && !why.empty(), "a mode that only reads is refused: that's fopen's job");

	if (file)
	{
		fclose(file);
	}

	file = PlainWrite_FOpen(base + "\\NUL", "w", &why);
	Check(!file, "\"w\" on a device's name (NUL) is refused");

	if (file)
	{
		fclose(file);
	}

	::DeleteFileA(name.c_str());
}

// ---- The file opened is the one in the folder held (Frost's review of #294, MUST 1) ----------------------------------------------------

// A folder opened as PlainWrite_Open holds it, and a file opened only to be looked at
static HANDLE OpenFolder(const std::string &folder)
{
	return ::CreateFileA(folder.c_str(), FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
}

static HANDLE OpenToLook(const std::string &file)
{
	return ::CreateFileA(file.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
}

static void TestSameFolder(const std::string &base)
{
	puts("PlainWrite_SameFolder: the file opened lies in the folder held");

	// A folder above the one held could be swapped for a junction between the folder's open and the file's (FILE_FLAG_OPEN_REPARSE_POINT
	// stops only a path's last part being followed): then the file opened lies somewhere else. That swap can't be timed in a test, so the
	// check itself is tried on two real folders: the file in A, A held, is in it; the file in B, A held, isn't (a "bc" beside "b" too).
	std::string const a = base + "\\b", b = base + "\\bc";
	::CreateDirectoryA(a.c_str(), NULL);
	::CreateDirectoryA(b.c_str(), NULL);
	WriteWholeFile(a + "\\x.cfg", "in b");
	WriteWholeFile(b + "\\x.cfg", "in bc");

	HANDLE const heldA = OpenFolder(a);
	HANDLE const inA = OpenToLook(a + "\\x.cfg");
	HANDLE const inB = OpenToLook(b + "\\x.cfg");
	bool const opened = heldA != INVALID_HANDLE_VALUE && inA != INVALID_HANDLE_VALUE && inB != INVALID_HANDLE_VALUE;
	Check(opened, "(the two folders and their files are made and opened)");
	Check(opened && PlainWrite_SameFolder(heldA, inA), "a file in the folder held is in it");
	Check(opened && !PlainWrite_SameFolder(heldA, inB), "a file in another folder isn't, though its name is the same and its folder's name starts the same (b, bc)");

	// any capitals: the folder's name written in other capitals is the same folder (the final paths are compared as NTFS does, CompareStringOrdinal)
	HANDLE const heldUpper = OpenFolder(base + "\\B");
	Check(heldUpper != INVALID_HANDLE_VALUE && PlainWrite_SameFolder(heldUpper, inA), "the same folder named in other capitals is the same folder");

	for (HANDLE h : { heldA, inA, inB, heldUpper })
	{
		if (h != INVALID_HANDLE_VALUE)
		{
			::CloseHandle(h);
		}
	}

	::DeleteFileA((a + "\\x.cfg").c_str());
	::DeleteFileA((b + "\\x.cfg").c_str());
	::RemoveDirectoryA(a.c_str());
	::RemoveDirectoryA(b.c_str());
}

// ---- A whole file at once, and a copy (Frost's review of #294: the D3DX pictures, the level backup) ----------------------------------------

static void TestWholeFileAndCopy(const std::string &base)
{
	puts("PlainWrite_WriteWholeFile and PlainWrite_CopyFile");

	std::string why;
	std::string const name = base + "\\picture.png";

	Check(PlainWrite_WriteWholeFile(name, "picture", 7, &why) && ReadWholeFile(name) == "picture", "a whole file is written");
	Check(PlainWrite_WriteWholeFile(name, "pic", 3, &why) && ReadWholeFile(name) == "pic", "and written again, emptied first");
	Check(!PlainWrite_WriteWholeFile(base + "\\NUL", "lost", 4, &why), "a device's name (NUL) is refused, never \"saved\" into nothing");

	std::string const source = base + "\\level.rif";
	std::string const copy = base + "\\level B9.rif";
	WriteWholeFile(source, "the level's bytes");
	Check(PlainWrite_CopyFile(source, copy, false, &why) && ReadWholeFile(copy) == "the level's bytes", "a copy is made");
	WriteWholeFile(source, "changed");
	Check(PlainWrite_CopyFile(source, copy, false, &why) && ReadWholeFile(copy) == "changed", "and made again over the old copy");
	Check(!PlainWrite_CopyFile(source, copy, true, &why) && ReadWholeFile(copy) == "changed", "a copy asked to be new is refused over one that is there");
	Check(!PlainWrite_CopyFile(source, base + "\\NUL", false, &why), "a copy onto a device's name (NUL) is refused");
	Check(!PlainWrite_CopyFile(base + "\\no such level.rif", base + "\\nothing.rif", false, &why) && !IsThere(base + "\\nothing.rif"),
		"a copy of a file that isn't there makes nothing");

	::DeleteFileA(name.c_str());
	::DeleteFileA(source.c_str());
	::DeleteFileA(copy.c_str());
}

// C11's "x" (only a new file): "wx" must never empty a file that is there (Frost's nit on #294)
static void TestExclusiveMode(const std::string &base)
{
	puts("PlainWrite_FOpen's \"x\"");

	std::string why;
	std::string const name = base + "\\only new.txt";
	WriteWholeFile(name, "there before");

	FILE *file = PlainWrite_FOpen(name, "wx", &why);
	Check(!file && ReadWholeFile(name) == "there before", "\"wx\" refuses a file that is there, and leaves it as it was");

	if (file)
	{
		fclose(file);
	}

	::DeleteFileA(name.c_str());
	file = PlainWrite_FOpen(name, "wbx", &why);
	Check(file && fwrite("new", 1, 3, file) == 3 && fclose(file) == 0 && ReadWholeFile(name) == "new", "\"wbx\" makes a file that isn't there");
	::DeleteFileA(name.c_str());
}

// ---- FileStream's write ------------------------------------------------------------------------------------------------------------

static bool StreamWrite(const std::string &name, const std::string &bytes)
{
	FileStream stream;

	if (!stream.Open(name, FileStream::FileWrite, FileStream::SkipFastFileCheck) || !stream.IsGood())
	{
		return false;
	}

	bool const ok = stream.WriteBytes((const uint8_t *)bytes.data(), bytes.size());
	stream.Close();
	return ok;
}

static void TestFileStream(const std::string &base)
{
	puts("FileStream's write (the multiplayer settings are saved with it)");

	std::string const name = base + "\\settings.cfg";

	Check(StreamWrite(name, "first") && ReadWholeFile(name) == "first", "it makes a file that isn't there");
	Check(StreamWrite(name, "2nd") && ReadWholeFile(name) == "2nd", "and empties a plain one that is there before it writes");

	std::string const folder = base + "\\settings folder";
	::CreateDirectoryA(folder.c_str(), NULL);
	Check(!StreamWrite(folder, "x") && IsAFolder(folder), "a folder where the file should be is refused");
	::RemoveDirectoryA(folder.c_str());

	Check(!StreamWrite(base + "\\NUL", "lost"), "a device's name (NUL) is refused, never \"saved\" into nothing");

	::DeleteFileA(name.c_str());
}

// ---- Links: only on GitHub's machines -----------------------------------------------------------------------------------------------

// The layout Windows reads a junction from (REPARSE_DATA_BUFFER's mount point part, which only the driver kit's headers have)
struct JunctionData
{
	DWORD ReparseTag;
	WORD ReparseDataLength;
	WORD Reserved;
	WORD SubstituteNameOffset;
	WORD SubstituteNameLength;
	WORD PrintNameOffset;
	WORD PrintNameLength;
	WCHAR PathBuffer[1024];
};

// Makes folder a junction to target (a folder that is there). True if it worked.
static bool MakeJunction(const std::string &folder, const std::string &target)
{
	wchar_t full[MAX_PATH];
	std::wstring const wideTarget(target.begin(), target.end());

	if (!::GetFullPathNameW(wideTarget.c_str(), MAX_PATH, full, NULL) || !::CreateDirectoryA(folder.c_str(), NULL))
	{
		return false;
	}

	std::wstring const print(full);
	std::wstring const substitute = L"\\??\\" + print;
	JunctionData data = {};

	if ((substitute.size() + 1 + print.size() + 1) > sizeof(data.PathBuffer) / sizeof(WCHAR))
	{
		return false;
	}

	data.ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
	data.SubstituteNameOffset = 0;
	data.SubstituteNameLength = (WORD)(substitute.size() * sizeof(WCHAR));
	data.PrintNameOffset = (WORD)((substitute.size() + 1) * sizeof(WCHAR));
	data.PrintNameLength = (WORD)(print.size() * sizeof(WCHAR));
	memcpy(data.PathBuffer, substitute.c_str(), (substitute.size() + 1) * sizeof(WCHAR));
	memcpy(data.PathBuffer + substitute.size() + 1, print.c_str(), (print.size() + 1) * sizeof(WCHAR));
	data.ReparseDataLength = (WORD)(8 + (substitute.size() + 1 + print.size() + 1) * sizeof(WCHAR));

	HANDLE const handle = ::CreateFileA(folder.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);

	if (handle == INVALID_HANDLE_VALUE)
	{
		::RemoveDirectoryA(folder.c_str());
		return false;
	}

	DWORD returned = 0;
	bool const made = ::DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, &data, data.ReparseDataLength + 8, NULL, 0, &returned, NULL) != 0;
	::CloseHandle(handle);

	if (!made)
	{
		::RemoveDirectoryA(folder.c_str());
	}

	return made;
}

// Each way the game writes, tried on one name: true if none of them opened it
static bool EveryWayRefuses(const std::string &name)
{
	std::string why;
	HANDLE const file = PlainWrite_Open(name, GENERIC_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, &why);
	bool refused = file == INVALID_HANDLE_VALUE;

	if (file != INVALID_HANDLE_VALUE)
	{
		WriteAndClose(file, "written through it");
	}

	FILE *const stream = PlainWrite_FOpen(name, "wb", &why);
	refused = refused && !stream;

	if (stream)
	{
		fputs("written through it", stream);
		fclose(stream);
	}

	// a whole file written at once (the D3DX pictures, made in memory first), and a copy made onto the name (the level backup)
	bool const wholeRefused = !PlainWrite_WriteWholeFile(name, "written through it", 18, &why);
	std::string const source = name + ".source";
	WriteWholeFile(source, "copied through it");
	bool const copyRefused = !PlainWrite_CopyFile(source, name, false, &why);
	::DeleteFileA(source.c_str());

	return !StreamWrite(name, "written through it") && refused && wholeRefused && copyRefused;
}

static void TestLinks(const std::string &base)
{
	puts("links (made only on GitHub's machines)");

	if (!getenv("GITHUB_ACTIONS"))
	{
		puts("  (links not tried here: they only run on GitHub's machines, where GITHUB_ACTIONS is set)");
		return;
	}

	std::string const other = base + "\\someone elses.txt";
	std::string const second = base + "\\second name.cfg";
	WriteWholeFile(other, "someone else's");

	if (::CreateHardLinkA(second.c_str(), other.c_str(), NULL))
	{
		Check(EveryWayRefuses(second), "a second name (a hard link) of another file is refused, every way");
		Check(ReadWholeFile(other) == "someone else's", "and the other file is just as it was");
		::DeleteFileA(second.c_str());
	}
	else
	{
		Check(false, "a second name for a file can be made on this disk, so that it can be tried");
	}

	// A symbolic link's target is given as a WHOLE path: a relative one is read from the link's own folder, so "<test folder>\target.txt" would point at
	// <test folder>\<test folder>\target.txt, a file that isn't there, and a plain CreateFile would fail on it too, so the check would prove nothing (the
	// first red run on GitHub's machine, 2026-11-23, passed both symbolic link checks on the old code that way). Each link is first shown to lead where meant.
	std::string const link = base + "\\link.cfg";
	std::string const target = base + "\\target.txt";
	WriteWholeFile(target, "what a link points to");

	if (::CreateSymbolicLinkA(link.c_str(), WholePath(target).c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE))
	{
		Check(ReadWholeFile(link) == "what a link points to", "(the symbolic link leads to the file)");
		Check(EveryWayRefuses(link), "a symbolic link to a file is refused, every way");
		Check(ReadWholeFile(target) == "what a link points to", "and what it points to is just as it was");
		::DeleteFileA(link.c_str());
	}
	else
	{
		Check(false, "a symbolic link can be made on GitHub's machine, so that it can be tried");
	}

	std::string const dangling = base + "\\dangling.cfg";
	std::string const nothing = base + "\\nothing yet.txt";

	if (::CreateSymbolicLinkA(dangling.c_str(), WholePath(nothing).c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE))
	{
		// shown to lead there: a file put there for a moment is read through it, then taken away, so the link points at nothing
		WriteWholeFile(nothing, "for a moment");
		Check(ReadWholeFile(dangling) == "for a moment", "(the symbolic link leads to the name it was made for)");
		::DeleteFileA(nothing.c_str());
		Check(!IsThere(nothing), "(and nothing is there now)");
		Check(EveryWayRefuses(dangling), "a symbolic link to nothing is refused, every way");
		Check(!IsThere(nothing), "and nothing is made where it points (a plain CreateFile made it)");
		::DeleteFileA(dangling.c_str());
	}
	else
	{
		Check(false, "a symbolic link to nothing can be made on GitHub's machine");
	}

	// The folder itself a junction to another folder: MPConfig, say, pointed somewhere else
	std::string const real = base + "\\real folder";
	std::string const junction = base + "\\junction folder";
	::CreateDirectoryA(real.c_str(), NULL);

	if (MakeJunction(junction, real))
	{
		WriteWholeFile(real + "\\marker.txt", "in the real folder");
		Check(ReadWholeFile(junction + "\\marker.txt") == "in the real folder", "(the junction leads into the real folder)");
		::DeleteFileA((real + "\\marker.txt").c_str());
		Check(EveryWayRefuses(junction + "\\settings.cfg"), "a file in a folder that is a junction is refused, every way");
		Check(!IsThere(real + "\\settings.cfg"), "and nothing is made in the folder it leads to");
		::RemoveDirectoryA(junction.c_str());   // takes the junction itself away, never what it leads to
	}
	else
	{
		Check(false, "a junction can be made on GitHub's machine, so that it can be tried");
	}

	::RemoveDirectoryA(real.c_str());
	::DeleteFileA(other.c_str());
	::DeleteFileA(target.c_str());
	::DeleteFileA(nothing.c_str());
}

// ---- By reading: every way the game writes goes through it ------------------------------------------------------------------------

// Every source file (.cpp .c .h .hpp) under folder, by walking it. A folder that is a link is never walked into.
static void FindSources(const std::string &folder, std::vector<std::string> *found)
{
	WIN32_FIND_DATAA entry;
	HANDLE const search = ::FindFirstFileA((folder + "\\*").c_str(), &entry);

	if (search == INVALID_HANDLE_VALUE)
	{
		return;
	}

	do
	{
		std::string const name = entry.cFileName;

		if (name == "." || name == "..")
		{
			continue;
		}

		std::string const path = folder + "/" + name;

		if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			if ((entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
			{
				FindSources(path, found);
			}

			continue;
		}

		size_t const dot = name.find_last_of('.');
		std::string const ending = dot == std::string::npos ? std::string() : name.substr(dot);

		if (ending == ".cpp" || ending == ".c" || ending == ".h" || ending == ".hpp")
		{
			found->push_back(path);
		}
	}
	while (::FindNextFileA(search, &entry));

	::FindClose(search);
}

// The files that judge what they open themselves, so a CreateFile asking to write is right there
static bool IsJudgedItself(const std::string &name)
{
	static const char *const judged[] =
	{
		"3dc/plain_write.h",            // this: the folder held, the name opened as itself and judged, the same file written
		"3dc/log_files.h",              // the game's log: FILE_FLAG_OPEN_REPARSE_POINT, then LogFiles_HandleIsPlainFile, before anything is written
		"3dc/level_log.h",              // the level log: the same, on its own handle
		"3dc/mark_files.h",             // the editor's marks: CREATE_NEW with FILE_FLAG_OPEN_REPARSE_POINT, so only a new file, made as itself
		"3dc/net_identity_store.cpp",   // the identity: CREATE_NEW in %LOCALAPPDATA%\AVPx, and its reads refuse links (net_identity_store.h)
	};

	for (const char *one : judged)
	{
		if (name == one)
		{
			return true;
		}
	}

	return false;
}

// The code alone, as the compiler for Windows reads it: every // and /* */ comment blanked (so a comment that names fopen isn't taken for a
// call), every #include line, and every line of an "#ifdef _XBOX" part (the Xbox build is never made). Its line breaks are kept, so a line number
// found in it is the file's own.
static std::string CodeOnly(const std::string &text)
{
	std::string code = text;
	size_t i = 0;

	while (i < code.size())
	{
		if (code.compare(i, 2, "//") == 0)
		{
			while (i < code.size() && code[i] != '\n')
			{
				code[i++] = ' ';
			}
		}
		else if (code.compare(i, 2, "/*") == 0)
		{
			while (i < code.size() && code.compare(i, 2, "*/") != 0)
			{
				code[i] = code[i] == '\n' ? '\n' : ' ';
				i++;
			}

			for (int k = 0; k < 2 && i < code.size(); k++)
			{
				code[i++] = ' ';
			}
		}
		else if (code[i] == '"')
		{
			// a string's words stay (a mode is in one), but a // or /* inside one is never taken for a comment
			i++;

			while (i < code.size() && code[i] != '"' && code[i] != '\n')
			{
				i += code[i] == '\\' ? 2 : 1;
			}

			i++;
		}
		else
		{
			i++;
		}
	}

	// "#ifdef _XBOX" (or "#if defined(_XBOX)") parts: blanked to their #else or #endif; the nesting of other #if's inside them is followed
	std::vector<int> stack;   // 1: inside an _XBOX part, 0: any other #if
	size_t lineStart = 0;

	while (lineStart < code.size())
	{
		size_t lineEnd = code.find('\n', lineStart);
		lineEnd = lineEnd == std::string::npos ? code.size() : lineEnd;
		size_t first = code.find_first_not_of(" \t", lineStart);
		std::string const line = first != std::string::npos && first < lineEnd ? code.substr(first, lineEnd - first) : std::string();
		bool const xboxIf = line.compare(0, 12, "#ifdef _XBOX") == 0 || line.compare(0, 18, "#if defined(_XBOX)") == 0;

		if (line.compare(0, 3, "#if") == 0)
		{
			stack.push_back(xboxIf ? 1 : 0);
		}
		else if (line.compare(0, 5, "#else") == 0 && !stack.empty() && stack.back() == 1)
		{
			stack.back() = 0;
		}
		else if (line.compare(0, 6, "#endif") == 0 && !stack.empty())
		{
			stack.pop_back();
		}
		else if (line.compare(0, 8, "#include") == 0 || std::find(stack.begin(), stack.end(), 1) != stack.end())   // an #include names a header, never a write
		{
			for (size_t k = lineStart; k < lineEnd; k++)
			{
				code[k] = ' ';
			}
		}

		lineStart = lineEnd + 1;
	}

	return code;
}

// The lines in text that write a file without plain_write.h: a plain fopen in a writing mode, a plain CreateFile asking to write, any
// std::ofstream or std::fstream, a D3DX picture saved straight to a file (D3DXSave...ToFile, never ...ToFileInMemory), a CopyFile, and a
// freopen (but of the console, CONOUT$ or CONIN$, which is a device, not a file)
static std::vector<size_t> RawWrites(const std::string &text)
{
	std::string const code = CodeOnly(text);
	std::vector<size_t> lines;
	const char *const kinds[] = { "fopen(", "CreateFile(", "CreateFile (", "CreateFileA(", "CreateFileA (", "CreateFileW(", "CreateFileW (", "CreateFile2(",
		"ofstream", "fstream", "D3DXSave", "CopyFile", "freopen(" };

	for (const char *kind : kinds)
	{
		std::string const k = kind;

		for (size_t at = code.find(k); at != std::string::npos; at = code.find(k, at + 1))
		{
			// a name of its own: not avp_fopen, _wfopen, avp_CreateFile, ifstream and the like (a letter or _ before it makes it another name)
			if (at > 0 && (code[at - 1] == '_' || isalnum((unsigned char)code[at - 1])))
			{
				continue;
			}

			size_t const statementEnd = code.find(';', at);
			std::string const call = code.substr(at, statementEnd == std::string::npos ? std::string::npos : statementEnd - at);
			bool writes = false;

			if (k == "fopen(")
			{
				writes = call.find("\"w") != std::string::npos || call.find("\"a") != std::string::npos || call.find("+\"") != std::string::npos;
			}
			else if (k.compare(0, 10, "CreateFile") == 0)
			{
				writes = k == "CreateFile2(" || call.find("GENERIC_WRITE") != std::string::npos || call.find("FILE_WRITE_DATA") != std::string::npos
					|| call.find("FILE_APPEND_DATA") != std::string::npos || call.find("GENERIC_ALL") != std::string::npos;
			}
			else if (k == "D3DXSave")
			{
				size_t const open = code.find('(', at);
				writes = open != std::string::npos && code.substr(at, open - at).find("InMemory") == std::string::npos;
			}
			else if (k == "freopen(")
			{
				writes = call.find("CONOUT$") == std::string::npos && call.find("CONIN$") == std::string::npos;
			}
			else
			{
				writes = true;   // ofstream, fstream, CopyFile: a write whatever its words
			}

			if (writes)
			{
				lines.push_back((size_t)std::count(code.begin(), code.begin() + at, '\n') + 1);
			}
		}
	}

	return lines;
}

static void TestTheGameUsesIt()
{
	puts("where the game writes, by reading");

	std::string const utilities = ReadSource("3dc/utilities.cpp");
	Check(!utilities.empty(), "3dc/utilities.cpp is read (run from the top of the repository)");

	std::string const userfile = FunctionText(utilities, "FILE *avp_open_userfile(");
	std::string const plainfile = FunctionText(utilities, "FILE *avp_fopen(");
	std::string const createfile = FunctionText(utilities, "HANDLE avp_CreateFile(");
	Check(userfile.find("PlainWrite_ModeWrites(mode)") != std::string::npos && userfile.find("PlainWrite_FOpen(") != std::string::npos,
		"avp_open_userfile writes only through PlainWrite_FOpen");
	Check(plainfile.find("PlainWrite_ModeWrites(mode)") != std::string::npos && plainfile.find("PlainWrite_FOpen(") != std::string::npos,
		"avp_fopen writes only through PlainWrite_FOpen");
	Check(createfile.find("PlainWrite_Open(") != std::string::npos, "avp_CreateFile opens a file to write only through PlainWrite_Open");
	Check(createfile.find("dwCreationDisposition != OPEN_EXISTING") != std::string::npos && createfile.find("(dwDesiredAccess & ~reads) != 0") != std::string::npos,
		"and anything but a plain read of a file that is there (OPEN_ALWAYS makes one; DELETE, WRITE_DAC and the like change one) goes through it too");

	std::string const stream = FunctionText(ReadSource("3dc/FileStream.cpp"), "bool FileStream::OpenActual(");
	Check(stream.find("PlainWrite_Open(") != std::string::npos, "FileStream opens a file to write only through PlainWrite_Open");

	std::string const save = ReadSource("3dc/save_file_write.h");
	Check(save.find("PlainWrite_Open(") != std::string::npos && save.find("::CreateFileA(") == std::string::npos, "a saved game's new file is made through PlainWrite_Open");

	std::string const rif = ReadSource("3dc/win95/mishchnk.cpp");
	Check(rif.find("CreateFileA (temp_name, GENERIC_WRITE") == std::string::npos, "a level's new file (mishchnk.cpp) is never made with a plain CreateFileA");

	// No other raw way to write, anywhere in the game's own code (every file under 3dc): no plain fopen in a writing mode, and no plain
	// CreateFile asking to write. The only ones left are in the files that judge what they open themselves.
	std::vector<std::string> sources;
	FindSources("3dc", &sources);
	FindSources("win32", &sources);   // the Direct3D code there (d3_func.cpp) writes the screenshot
	Check(sources.size() > 500, "the game's source files under 3dc and win32 are found to be read");

	std::vector<std::string> raw;

	for (const std::string &name : sources)
	{
		if (IsJudgedItself(name))
		{
			continue;
		}

		std::string const text = ReadSource(name);

		for (size_t line : RawWrites(text))
		{
			raw.push_back(name + ":" + std::to_string((unsigned long long)line));
		}
	}

	for (const std::string &where : raw)
	{
		printf("    a raw write at %s\n", where.c_str());
	}

	Check(raw.empty(), "no file under 3dc or win32 writes without plain_write.h: no plain fopen, CreateFile, ofstream, fstream, D3DXSave...ToFile, CopyFile or freopen (each raw write is named above)");
}

int main()
{
	std::string const base = TestFolder();
	::CreateDirectoryA(base.c_str(), NULL);

	TestOpenWays(base);
	TestOpenRefusals(base);
	TestFOpen(base);
	TestSameFolder(base);
	TestWholeFileAndCopy(base);
	TestExclusiveMode(base);
	TestFileStream(base);
	TestLinks(base);
	TestTheGameUsesIt();

	::RemoveDirectoryA(base.c_str());

	if (checksFailed)
	{
		printf("The plain write test FAILED: %d check(s)\n", checksFailed);
		return 1;
	}

	puts("The plain write test passed");
	return 0;
}
