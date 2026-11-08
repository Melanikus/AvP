// The test of a tester's mark: its pictures' names and numbers, keeping the newest 20, writing only new files into a plain folder, one
// mark a second, the mark's line, and where the game uses them. Written 2026-11-07 by Melanikus for the AvP port.
//
// WHAT IT'S FOR: mark_files.h says what a mark is and why its folder is treated as input. The picture itself needs a graphics card, so
// that part is read, not run.
//
// WHAT THIS TESTS: on real files in a folder of its own, made inside the folder it runs in and taken away afterwards:
// - the names a mark's picture can have, and names that only look like one;
// - the next number, after whatever is in the folder, and the most there can be;
// - keeping the newest 20, and leaving alone anything that isn't a plain file;
// - writing a picture only as a new file, into a plain folder;
// - one mark a second;
// - the mark's line, in a small space too, and that it comes out of the log's leaving out with no user name or address in it;
// - by reading, that the game uses all of it where it should.
// Links are only made on GitHub's machines, where GITHUB_ACTIONS is set, because making them is what antivirus programs watch for.
// Run it from the top of the repository, because that's where the source it reads is found from.
#include "mark_files.h"

#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <string>

#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif

static int checksFailed = 0;

static void Check(bool passed, const char *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

// A source file, with its line ends written \n whatever the checkout made them
static std::string ReadSource(const char *name)
{
	std::ifstream file(name, std::ios::binary);
	std::stringstream text;
	text << file.rdbuf();

	std::string out;

	for (char c : text.str())
	{
		if (c != '\r')
		{
			out += c;
		}
	}

	return out;
}

static bool Has(const std::string &text, const char *what)
{
	return text.find(what) != std::string::npos;
}

static bool ComesBefore(const std::string &text, const char *first, const char *second)
{
	size_t const a = text.find(first);
	return a != std::string::npos && text.find(second, a) != std::string::npos;
}

// This run's own folder, inside the folder the test runs in, named by nothing but this program's number, with a slash on the end as the
// game's save folder has
static std::string TestFolder()
{
	return "mark_files_test_" + std::to_string(::GetCurrentProcessId()) + "/";
}

static void MakeFile(const std::string &name, const char *text)
{
	HANDLE const file = ::CreateFileA(name.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file != INVALID_HANDLE_VALUE)
	{
		DWORD written = 0;
		::WriteFile(file, text, (DWORD)strlen(text), &written, NULL);
		::CloseHandle(file);
	}
}

static std::string ReadFile(const std::string &name)
{
	HANDLE const file = ::CreateFileA(name.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	std::string text;

	if (file != INVALID_HANDLE_VALUE)
	{
		char buffer[256];
		DWORD read = 0;

		while (::ReadFile(file, buffer, sizeof(buffer), &read, NULL) && read > 0)
		{
			text.append(buffer, read);
		}

		::CloseHandle(file);
	}

	return text;
}

static bool IsThere(const std::string &name)
{
	return ::GetFileAttributesA(name.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// Takes away everything this test can have made in its folder, and the folder
static void TakeAway(const std::string &folder)
{
	WIN32_FIND_DATAA found;
	HANDLE const search = ::FindFirstFileA((folder + "*").c_str(), &found);

	if (search != INVALID_HANDLE_VALUE)
	{
		do
		{
			std::string const name = folder + found.cFileName;

			if (strcmp(found.cFileName, ".") == 0 || strcmp(found.cFileName, "..") == 0)
			{
				continue;
			}

			if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				::RemoveDirectoryA(name.c_str());
			}
			else
			{
				::DeleteFileA(name.c_str());
			}
		}
		while (::FindNextFileA(search, &found));

		::FindClose(search);
	}

	::RemoveDirectoryA(folder.substr(0, folder.size() - 1).c_str());
}

// ---- Names and numbers ------------------------------------------------------------------------------------------------------------

static void TestNames()
{
	printf("A mark's picture's name:\n");

	Check(Mark_PictureName("C:/x/", 12) == "C:/x/mark-12.png", "mark 12's picture is mark-12.png, beside the log");
	Check(Mark_NumberInName("mark-1.png") == 1 && Mark_NumberInName("mark-999999.png") == 999999 && Mark_NumberInName("MARK-7.PNG") == 7, "a mark's own names give back their numbers, in any capitals");

	static const char *const notOnes[] = { "mark-0.png", "mark-01.png", "mark-1234567.png", "mark-.png", "mark-x.png", "mark-5.png.txt", "mark-5.pngx", "xmark-5.png", "mark-5", "mark--5.png", "mark-5 .png" };
	bool noneCount = true;

	for (size_t i = 0; i < sizeof(notOnes) / sizeof(notOnes[0]); i++)
	{
		noneCount = noneCount && Mark_NumberInName(notOnes[i]) == 0;
	}

	Check(noneCount, "names that only look like a mark's count for nothing: a 0, a leading 0, 7 digits, letters, anything after .png");
	Check(kMarkPicturesKept == 20 && kMarkHighestNumber == 999999, "the newest 20 are kept, and the highest number is 999999");
}

static void TestNumbersAndKeeping()
{
	printf("Numbers and keeping the newest:\n");

	std::string const folder = TestFolder();
	TakeAway(folder);
	::CreateDirectoryA(folder.substr(0, folder.size() - 1).c_str(), NULL);

	Check(Mark_NextNumber(folder) == 1, "an empty folder's first mark is 1");

	// 25 marks, as the game makes them
	bool countedUp = true;

	for (int i = 0; i < 25; i++)
	{
		int const n = Mark_NextNumber(folder);
		countedUp = countedUp && n == i + 1;
		Mark_WritePicture(Mark_PictureName(folder, n), "png", 3);
		Mark_KeepTheNewest(folder, n);
	}

	Check(countedUp, "the marks count up from 1, even while the oldest are being thrown away");

	bool newestKept = true;
	bool oldestGone = true;

	for (int n = 1; n <= 25; n++)
	{
		if (n <= 5)
		{
			oldestGone = oldestGone && !IsThere(Mark_PictureName(folder, n));
		}
		else
		{
			newestKept = newestKept && IsThere(Mark_PictureName(folder, n));
		}
	}

	Check(Mark_NextNumber(folder) == 26, "each mark takes the next number after the highest in the folder");
	Check(newestKept && oldestGone, "after 25 marks, the newest 20 are there and the oldest 5 are gone, so they can't grow without end");

	// Names that only look like a mark's, and a folder named like one: none of them count, and none is thrown away
	MakeFile(folder + "mark-900000.txt", "not a picture");
	MakeFile(folder + "mark-0001.png", "a leading 0");
	::CreateDirectoryA((folder + "mark-500000.png").c_str(), NULL);
	MakeFile(folder + "notes.txt", "a tester's own");

	Check(Mark_NextNumber(folder) == 26, "a folder named like a mark's picture doesn't count, nor do names that only look like one");
	::CreateDirectoryA((folder + "mark-2.png").c_str(), NULL);
	Mark_KeepTheNewest(folder, 26);
	Check((::GetFileAttributesA((folder + "mark-2.png").c_str()) & FILE_ATTRIBUTE_DIRECTORY) != 0, "an old number that's a folder is left alone");
	Check(IsThere(folder + "mark-0001.png") && IsThere(folder + "mark-900000.txt") && IsThere(folder + "notes.txt"), "and so is everything else that isn't a mark's own picture");
	::RemoveDirectoryA((folder + "mark-2.png").c_str());
	::RemoveDirectoryA((folder + "mark-500000.png").c_str());

	// The highest number there can be
	MakeFile(folder + "mark-999999.png", "the last");
	Check(Mark_NextNumber(folder) == 0, "a folder holding the highest number gets no more marks, rather than going round to 1");
	::DeleteFileA((folder + "mark-999999.png").c_str());

	// Links: only on GitHub's machines
	if (getenv("GITHUB_ACTIONS"))
	{
		std::string const other = folder + "someone-elses.txt";
		std::string const link = folder + "mark-3.png";
		MakeFile(other, "someone else's");

		if (::CreateHardLinkA(link.c_str(), other.c_str(), NULL))
		{
			Mark_KeepTheNewest(folder, 30);
			Check(IsThere(link) && ReadFile(other) == "someone else's", "an old number that's a second name for another file isn't thrown away");
			::DeleteFileA(link.c_str());
		}
		else
		{
			Check(false, "a second name for a file can be made on this disk, so that it can be tried");
		}

		if (::CreateSymbolicLinkA(link.c_str(), other.c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE))
		{
			Check(!Mark_WritePicture(link, "png", 3) && ReadFile(other) == "someone else's", "a picture is never written through a link");
			::DeleteFileA(link.c_str());
		}
		else
		{
			printf("  (not tried: this machine doesn't let a test make a link)\n");
		}

		::DeleteFileA(other.c_str());
	}
	else
	{
		printf("  (links not tried here: they only run on GitHub's machines, where GITHUB_ACTIONS is set)\n");
	}

	TakeAway(folder);
	Check(!IsThere(folder.substr(0, folder.size() - 1)), "this test's files are gone again");
}

// ---- Writing a picture ----------------------------------------------------------------------------------------------------------

static void TestWriting()
{
	printf("Writing a picture:\n");

	std::string const folder = TestFolder();
	TakeAway(folder);
	::CreateDirectoryA(folder.substr(0, folder.size() - 1).c_str(), NULL);

	std::string const name = Mark_PictureName(folder, 1);
	Check(Mark_WritePicture(name, "first", 5) && ReadFile(name) == "first", "a new picture is written");
	Check(!Mark_WritePicture(name, "second", 6) && ReadFile(name) == "first", "nothing is ever written over a picture that's there");

	::CreateDirectoryA((folder + "mark-2.png").c_str(), NULL);
	Check(!Mark_WritePicture(folder + "mark-2.png", "x", 1), "nor over a folder with a picture's name");
	::RemoveDirectoryA((folder + "mark-2.png").c_str());

	Check(Mark_FolderIsPlain(folder) && Mark_FolderIsPlain(""), "the test's folder, and the current one, are plain folders");
	Check(!Mark_FolderIsPlain(folder + "not-there/"), "a folder that isn't there isn't one");
	MakeFile(folder + "a-file", "x");
	Check(!Mark_FolderIsPlain(folder + "a-file"), "and nor is a file");

	TakeAway(folder);
}

// ---- One a second ---------------------------------------------------------------------------------------------------------------

static void TestOneASecond()
{
	printf("One mark a second:\n");

	Check(!Mark_TooSoon(5000, 0), "the first mark is never too soon");
	Check(Mark_TooSoon(5999, 5000) && Mark_TooSoon(5000, 5000), "another within a second is");
	Check(!Mark_TooSoon(6000, 5000), "a second later isn't");
	Check(!Mark_TooSoon(500, 0xFFFFFC00) && Mark_TooSoon(500, 0xFFFFFF00), "and the time between them is right when Windows' clock goes round, every 49 days");
}

// ---- The mark's line --------------------------------------------------------------------------------------------------------------

static void TestTheLine()
{
	printf("The mark's line:\n");

	Mark_Moment m;
	memset(&m, 0, sizeof(m));
	m.number = 12;
	m.hours = 19;
	m.minutes = 5;
	m.seconds = 34;
	m.level = "derelict";
	m.species = "marine";
	m.x = -1200;
	m.y = 340;
	m.z = 98765;
	m.facing = 2048;
	m.health = 87;
	m.weapon = 3;
	m.network = NULL;

	char words[512];
	Mark_DescribeMoment(m, words, sizeof(words));
	Check(strcmp(words, "MARK 12 at 19:05:34: derelict, a marine at (-1200, 340, 98765) facing 2048, health 87, weapon 3, picture mark-12.png") == 0, "it says the mark's number, the time, the level, where and which way, health, weapon and its picture");

	m.network = "connection 1 at 45 ms, connection 2 at 120 ms";
	Mark_DescribeMoment(m, words, sizeof(words));
	Check(Has(words, "picture mark-12.png; connection 1 at 45 ms, connection 2 at 120 ms"), "in a network game, each connection by number with its round trip");

	// Through the log's leaving out, with a made-up player whose names are in the level's and the connections' words
	LogFiles_Private p;
	LogFiles_PrivateClear(p);
	LogFiles_PrivateAddWord(p, "TesterName", "<user>");
	LogFiles_PrivateAddWord(p, "TesterPC", "<computer>");
	m.level = "C:\\Users\\TesterName\\Levels\\mine";
	m.network = "connection 1 at 45 ms from 192.168.0.20 on TesterPC";
	Mark_DescribeMoment(m, words, sizeof(words));
	std::string const written = LogFiles_LeaveOutThePlayer(std::string(words), p);
	Check(!Has(written, "TesterName") && !Has(written, "TesterPC") && !Has(written, "192.168") && Has(written, "MARK 12 at 19:05:34") && Has(written, "health 87"), "through the log, no user name, computer name or address is left, and the rest is");

	char cramped[24 + 8];
	memset(cramped, 'g', sizeof(cramped));
	Mark_DescribeMoment(m, cramped, 24);
	bool guardsKept = true;

	for (size_t i = 24; i < sizeof(cramped); i++)
	{
		guardsKept = guardsKept && cramped[i] == 'g';
	}

	Check(guardsKept && strlen(cramped) == 23 && strncmp(cramped, "MARK 12 at 19:05:34", 19) == 0, "a small space is never written past, and the line still ends");

	m.level = NULL;
	m.species = NULL;
	Mark_DescribeMoment(m, words, sizeof(words));
	Check(Has(words, ": no level, a player at"), "no level and no species still make a line");

	unsigned char controls[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	Check(Mark_KeyIsIn(controls, sizeof(controls), 5) && !Mark_KeyIsIn(controls, sizeof(controls), 9), "a key given to a control is found among the controls");
}

// ---- By reading -----------------------------------------------------------------------------------------------------------------------

static void TestTheGameUsesIt()
{
	printf("Where the game uses it, by reading:\n");

	std::string const key = ReadSource("3dc/avp/mark_key.cpp");
	Check(Has(key, "static const unsigned char kMarkKey = KEY_F2;"), "the key is F2");
	Check(Has(key, "if (!DebouncedKeyboardInput[kMarkKey] || momentWaiting || !Player || !Player->ObStrategyBlock || KeyIsTaken())"), "it marks once a press, and not when the player has given F2 to one of their controls");
	Check(Has(key, "if (Mark_TooSoon(now, lastMarkAt))"), "nor within a second of the last");
	Check(Has(key, "int const number = (fromFolder != 0 && fromFolder <= lastNumber) ? lastNumber + 1 : fromFolder;"), "two marks never share a number, even when a picture couldn't be saved");
	Check(ComesBefore(key, "static ID3DXBuffer *PictureOfThisFrame(", "device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)") && Has(key, "D3DXSaveSurfaceToFileInMemory(&picture, D3DXIFF_PNG, copy, NULL, NULL)"), "the picture is the game's own frame, made in memory");
	Check(!Has(key, "D3DXSaveSurfaceToFile(") && !Has(key, "D3DXSaveSurfaceToFileA(") && !Has(key, "GetDC(") && !Has(key, "BitBlt("), "never written straight to a path, and never the desktop");
	Check(ComesBefore(key, "if (device && Mark_FolderIsPlain(folder))", "saved = Mark_WritePicture(Mark_PictureName(folder, moment.number), picture->GetBufferPointer(), picture->GetBufferSize());"), "it's only written into a plain folder, as a new file");
	Check(ComesBefore(key, "\tif (saved)\n\t{\n\t\tMark_KeepTheNewest(folder, moment.number);", "LogString(std::string(words)"), "then the oldest go, and the line goes through the log, which leaves out the player");

	std::string const loop = ReadSource("3dc/avp/win95/winmain.cpp");
	Check(ComesBefore(loop, "\t\t\t\t\t\t\tMaintainHUD();\n", "\t\t\t\t\t\t\tMark_ReadKey();\n"), "the key is read while a level is played, once its HUD is drawn");

	std::string const render = ReadSource("3dc/d3d_render.cpp");
	Check(ComesBefore(render, "\t\t\tR_EndScene();\n", "\t\tMark_EndFrame(d3d.lpD3DDevice);\n"), "the picture is taken once the frame is drawn");

	std::string const net = ReadSource("3dc/networking.cpp");
	std::string const connections = net.substr(net.find("void Net_DescribeConnections(char *words, size_t size)") == std::string::npos ? net.size() : net.find("void Net_DescribeConnections(char *words, size_t size)"));
	Check(Has(connections, "\"%sconnection %u at %u ms\"") && Has(connections, "(unsigned)peer.roundTripTime"), "each connection is named by number and round trip");
	Check(!connections.empty() && !Has(connections, "address") && !Has(connections, "enet_address"), "and never by its address");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestNames();
	TestNumbersAndKeeping();
	TestWriting();
	TestOneASecond();
	TestTheLine();
	TestTheGameUsesIt();

	if (checksFailed)
	{
		printf("SOME CHECKS FAILED: %d\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
