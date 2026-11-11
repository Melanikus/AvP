// Tests for a run's film, which needs no game and no graphics card. Written 2026-11-05 by Melanikus for the AvP port.
//
// The first half tries the sums. The second half makes real films with Windows' own video writer, reads them back with Windows' own reader, and counts what's in them, including when the writer breaks, the file reaches its limit and the
// disk has no room. A machine with no Media Foundation, which some Windows Server machines are, can't do the second half, and says so plainly.
//
// With words, it reads a film from a real run:
//     hudson_video_test read <file.mp4> <pictures the result says were written>
// and fails if the film won't open, or holds a different number of pictures. That's what proves a real run's film is what its log says.
//
// The word "die" is for this test's own use: it starts a copy of itself that makes a film and stops dead part way, to prove that what was written so far can still be played.

#undef WINVER
#define WINVER 0x0A00
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00

#include "hudson_video.h"
#include "hudson_video_writer.h"

#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static int checksFailed = 0;

static void Check(bool good, const std::string &what)
{
	if (!good)
	{
		printf("FAILED  %s\n", what.c_str());
		checksFailed++;
	}
	else
	{
		printf("ok      %s\n", what.c_str());
	}
}

// On a machine whose job is to check changes, half a test not running is a blind spot, so it fails there. Anywhere else it says plainly what it couldn't try.
static bool MaySkipRealFilms(const char *ciWord, const char *actionsWord)
{
	return ciWord == 0 && actionsWord == 0;
}

static void TestTheSums()
{
	Check(MaySkipRealFilms(0, 0), "a machine that isn't checking a change may do without real films");
	Check(!MaySkipRealFilms("true", 0), "and one that says it's a CI machine may not");
	Check(!MaySkipRealFilms(0, "true"), "nor one that says it's GitHub's");

	Check(HudsonVideo_SlotAt(-1.0) == 0, "a moment before the start is the first picture");
	Check(HudsonVideo_SlotAt(0.0) == 0, "and so is the start");
	Check(HudsonVideo_SlotAt(0.04) == 1, "30 pictures a second means the second one starts at a thirtieth of a second");
	Check(HudsonVideo_SlotAt(1.0) == 30, "and a second in is the 31st");
	Check(HudsonVideo_SlotAt(1e12) == 2000000000, "a time too big for a number is held at the limit, not turned into nonsense");

	Check(HudsonVideo_TimeOfSlot(0) == 0, "the first picture starts at the start");
	Check(HudsonVideo_TimeOfSlot(30) == 10000000LL, "the 31st starts one second in, in units of 100 nanoseconds");
	Check(HudsonVideo_TimeOfSlot(2000000000) == 2000000000LL * 10000000LL / 30, "and the sum doesn't overflow at the limit");
	Check(HudsonVideo_SlotLength() == 333333, "a picture lasts a thirtieth of a second");

	Check(HudsonVideo_BitsPerSecond(1920, 1080) == 12441600, "the bit rate goes up with the size of the picture");
	Check(HudsonVideo_BitsPerSecond(kHudsonVideoMostWidth, kHudsonVideoMostHeight) > 0, "and the biggest fits in a number");

	Check(HudsonVideo_WhyNotThisSize(1920, 1080) == 0, "1920x1080 can be recorded");
	Check(HudsonVideo_WhyNotThisSize(kHudsonVideoMostWidth, kHudsonVideoMostHeight) == 0, "and so can the biggest");
	Check(HudsonVideo_WhyNotThisSize(0, 1080) != 0, "no width can't");
	Check(HudsonVideo_WhyNotThisSize(1920, 0) != 0, "nor no height");
	Check(HudsonVideo_WhyNotThisSize(-2, 1080) != 0, "nor a width below nothing");
	Check(HudsonVideo_WhyNotThisSize(1921, 1080) != 0, "nor an odd width");
	Check(HudsonVideo_WhyNotThisSize(1920, 1081) != 0, "nor an odd height");
	Check(HudsonVideo_WhyNotThisSize(kHudsonVideoMostWidth + 2, 1080) != 0, "nor one too wide");
	Check(HudsonVideo_WhyNotThisSize(1920, kHudsonVideoMostHeight + 2) != 0, "nor one too tall");

	long long const most = 1000;

	Check(HudsonVideo_ShouldStop(999, most, kHudsonVideoDiskToLeave) == kHudsonVideoGoOn, "a film under its limit goes on");
	Check(HudsonVideo_ShouldStop(1000, most, kHudsonVideoDiskToLeave) == kHudsonVideoFileIsFull, "and stops at it");
	Check(HudsonVideo_ShouldStop(0, most, kHudsonVideoDiskToLeave - 1) == kHudsonVideoDiskIsFull, "a disk with less than it has to keep stops it");
	Check(HudsonVideo_ShouldStop(0, most, -1) == kHudsonVideoDiskWontSay, "a disk that won't say how much room is left stops it too, with its own reason");
	Check(HudsonVideo_ShouldStop(0, most, 0) == kHudsonVideoDiskIsFull, "and a disk with nothing left is full, not one that won't say");

	Check(HudsonVideo_DiskHasRoom(kHudsonVideoDiskToLeave + most, most), "there's room to start if the whole film fits and leaves what it has to");
	Check(!HudsonVideo_DiskHasRoom(kHudsonVideoDiskToLeave + most - 1, most), "and not if it doesn't");
	Check(!HudsonVideo_DiskHasRoom(-1, most), "a disk that won't say how much room is left has no room for a film: not knowing isn't room");
}

static void TestTheQueue()
{
	HudsonVideoQueue q = { 0, 0 };

	Check(HudsonVideoQueue_PlaceToWrite(q) == -1, "an empty queue has nothing to write");
	Check(HudsonVideoQueue_PlaceToAdd(q) == 0, "and the first picture goes in the first place");

	for (int i = 0; i < kHudsonVideoRingFrames; i++)
	{
		Check(HudsonVideoQueue_PlaceToAdd(q) == i, "pictures go in one place after another");
		HudsonVideoQueue_Added(&q);
	}

	Check(HudsonVideoQueue_PlaceToAdd(q) == -1, "a full queue has no place for another");
	Check(HudsonVideoQueue_PlaceToWrite(q) == 0, "the oldest is written first");

	HudsonVideoQueue_Written(&q);

	Check(HudsonVideoQueue_PlaceToWrite(q) == 1, "and then the next");
	Check(HudsonVideoQueue_PlaceToAdd(q) == 0, "and writing one makes room, going round to the start");

	for (int i = 1; i < kHudsonVideoRingFrames; i++)
	{
		HudsonVideoQueue_Written(&q);
	}

	Check(q.first == 0 && q.waiting == 0, "writing them all empties it, back at the start of the ring");
}

static void TestCopying()
{
	// Two rows of two pixels, in rows 3 pixels long, each byte its own number
	uint8_t from[24];
	uint8_t to[16];

	for (int i = 0; i < 24; i++)
	{
		from[i] = (uint8_t)i;
	}

	memset(to, 99, sizeof(to));

	Check(HudsonVideo_CopyPicture(to, 2, 2, from, 12), "a picture is copied");
	Check(to[0] == 0 && to[7] == 7, "its first row is the source's first row");
	Check(to[8] == 12 && to[15] == 19, "and its second row starts where the source's second row does, with no gap");

	memset(to, 99, sizeof(to));

	Check(!HudsonVideo_CopyPicture(to, 2, 2, from, 7), "rows too short to hold the picture aren't copied");
	Check(to[0] == 99, "and nothing is touched");
	Check(!HudsonVideo_CopyPicture(to, 2, 2, from, -12), "nor rows going backwards");
	Check(HudsonVideo_CopyPicture(to, 2, 2, from, 8), "rows exactly long enough are");
}

// ---- real films

static HRESULT (WINAPI *TheStartup)(ULONG version, DWORD flags);
static HRESULT (WINAPI *TheShutdown)();
static HRESULT (WINAPI *MakeAttributes)(IMFAttributes **made, UINT32 howMany);
static HRESULT (WINAPI *MakeMediaType)(IMFMediaType **made);
static HRESULT (WINAPI *MakeReader)(LPCWSTR path, IMFAttributes *attributes, IMFSourceReader **made);

static bool LoadTheReader()
{
	HMODULE const platform = LoadLibraryExA("mfplat.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
	HMODULE const readWrite = LoadLibraryExA("mfreadwrite.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);

	if (!platform || !readWrite)
	{
		return false;
	}

	TheStartup = reinterpret_cast<decltype(TheStartup)>(GetProcAddress(platform, "MFStartup"));
	TheShutdown = reinterpret_cast<decltype(TheShutdown)>(GetProcAddress(platform, "MFShutdown"));
	MakeAttributes = reinterpret_cast<decltype(MakeAttributes)>(GetProcAddress(platform, "MFCreateAttributes"));
	MakeMediaType = reinterpret_cast<decltype(MakeMediaType)>(GetProcAddress(platform, "MFCreateMediaType"));
	MakeReader = reinterpret_cast<decltype(MakeReader)>(GetProcAddress(readWrite, "MFCreateSourceReaderFromURL"));

	return TheStartup && TheShutdown && MakeAttributes && MakeMediaType && MakeReader;
}

// The writer needs these three as well as what the reader needs (see LoadTheDlls in hudson_video_writer.cpp, which loads the same two
// files from the same place). A machine with only part of Media Foundation counts as having none, because the writer won't start a film there.
static bool TheWriterCanWorkHere()
{
	HMODULE const platform = LoadLibraryExA("mfplat.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
	HMODULE const readWrite = LoadLibraryExA("mfreadwrite.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);

	return platform && readWrite && GetProcAddress(platform, "MFCreateMemoryBuffer") && GetProcAddress(platform, "MFCreateSample") &&
		GetProcAddress(readWrite, "MFCreateSinkWriterFromURL");
}

struct WhatWasRead
{
	bool opened;
	int pictures;
	UINT32 width, height;
	bool topIsRed;
	double lastSeconds;
};

// Reads a whole film, one picture at a time, and asks what the top of the first picture looks like
static WhatWasRead ReadFilm(const char *path)
{
	WhatWasRead read = { false, 0, 0, 0, false, 0.0 };
	wchar_t wide[MAX_PATH] = { 0 };

	MultiByteToWideChar(CP_ACP, 0, path, -1, wide, MAX_PATH);

	if (FAILED(TheStartup(MF_VERSION, MFSTARTUP_LITE)))
	{
		return read;
	}

	IMFAttributes *attributes = 0;
	IMFSourceReader *reader = 0;
	IMFMediaType *want = 0;
	IMFMediaType *got = 0;

	MakeAttributes(&attributes, 1);
	attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);

	if (SUCCEEDED(MakeReader(wide, attributes, &reader)))
	{
		MakeMediaType(&want);
		want->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		want->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);

		if (SUCCEEDED(reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, want)) && SUCCEEDED(reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &got)))
		{
			INT32 stride = 0;

			read.opened = true;
			MFGetAttributeSize(got, MF_MT_FRAME_SIZE, &read.width, &read.height);
			got->GetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32 *)&stride);

			for (;;)
			{
				DWORD flags = 0;
				LONGLONG time = 0;
				IMFSample *sample = 0;

				if (FAILED(reader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, &flags, &time, &sample)) || (flags & MF_SOURCE_READERF_ENDOFSTREAM))
				{
					if (sample)
					{
						sample->Release();
					}

					break;
				}

				if (!sample)
				{
					continue;
				}

				if (read.pictures == 0)
				{
					IMFMediaBuffer *buffer = 0;
					BYTE *data = 0;

					sample->ConvertToContiguousBuffer(&buffer);
					buffer->Lock(&data, NULL, NULL);

					// Whether the first row in memory is the top of the picture depends on the sign of the stride
					DWORD const *const pixels = (DWORD const *)data;
					DWORD const top = stride < 0 ? pixels[(read.height - 1) * read.width + read.width / 2] : pixels[read.width / 2];

					read.topIsRed = ((top >> 16) & 0xFF) > (top & 0xFF);
					buffer->Unlock();
					buffer->Release();
				}

				read.lastSeconds = (double)time / 10000000.0;
				read.pictures++;
				sample->Release();
			}

			got->Release();
		}

		want->Release();
		reader->Release();
	}

	attributes->Release();
	TheShutdown();

	return read;
}

static std::string BesideThisProgram(const char *name)
{
	char path[MAX_PATH] = { 0 };

	GetModuleFileNameA(NULL, path, MAX_PATH);

	std::string folder(path);

	return folder.substr(0, folder.find_last_of('\\') + 1) + name;
}

// The top quarter red and the rest blue, so that "which way up" has an answer. The rows are one pixel longer than the picture, the way a graphics card's can be, and that pixel is the wrong colour.
static std::vector<uint8_t> MakePicture(int width, int height)
{
	std::vector<uint8_t> picture((size_t)(width + 1) * 4 * height);
	DWORD *const pixels = (DWORD *)&picture[0];

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x <= width; x++)
		{
			pixels[y * (width + 1) + x] = x == width ? 0xFF00FF00 : (y < height / 4 ? 0xFFFF0000 : 0xFF0000FF);
		}
	}

	return picture;
}

static HudsonVideoFacts Facts()
{
	HudsonVideoFacts facts;

	HudsonVideoWriter_GetFacts(&facts);

	return facts;
}

// Waits for the writer to have taken this many pictures, or to have stopped
static void WaitForWriter(long pictures)
{
	for (int i = 0; i < 5000 && Facts().written < pictures && Facts().goingOn; i++)
	{
		Sleep(1);
	}
}

static double MomentOf(int slot)
{
	return (slot + 0.5) / kHudsonVideoFramesPerSecond;
}

static int const kFilmWidth = 640, kFilmHeight = 360;

static void TestAFilm()
{
	std::string const path = BesideThisProgram("video-test-film.mp4");
	std::vector<uint8_t> const picture = MakePicture(kFilmWidth, kFilmHeight);
	int const pitch = (kFilmWidth + 1) * 4;
	int const pictures = 60;

	DeleteFileA(path.c_str());

	bool const started = HudsonVideoWriter_Start(path.c_str(), kFilmWidth, kFilmHeight, 0, kHudsonVideoMostBytes);

	// A machine can have Media Foundation's DLLs and no H.264 encoder, and this is where that shows
	Check(started, std::string("a film starts: ") + Facts().why);

	bool allWanted = true;

	for (int i = 0; i < pictures; i++)
	{
		allWanted = allWanted && HudsonVideoWriter_WantsPictureFor(MomentOf(i));
		HudsonVideoWriter_Add(&picture[0], pitch, MomentOf(i));
		WaitForWriter(i + 1);
	}

	Check(allWanted, "it wants a picture for each moment");
	Check(!HudsonVideoWriter_WantsPictureFor(MomentOf(pictures - 1)), "and not a second one for a moment it's had");

	HudsonVideoWriter_Add(&picture[0], pitch, MomentOf(pictures - 1));

	HudsonVideoWriter_Finish(30000);

	HudsonVideoFacts const facts = Facts();

	Check(facts.finishedInTime, "it finishes");
	Check(facts.stopped[0] == 0, std::string("nothing stopped it: ") + facts.why);
	Check(facts.written == pictures, "and it wrote every picture, and not twice for one moment");
	Check(facts.dropped == 0 && facts.noPicture == 0, "and says none were dropped or missed");
	// The size the film finished at, which is bigger than any it had while being written: the end of an mp4 is written last
	WIN32_FILE_ATTRIBUTE_DATA onTheDisk;
	bool const sized = GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &onTheDisk) != 0;
	long long const finishedAt = sized ? (((long long)onTheDisk.nFileSizeHigh << 32) | onTheDisk.nFileSizeLow) : -1;

	Check(facts.bytes > 0 && facts.bytes == finishedAt, "and how big the file came out, as it is on the disk: " + std::to_string(facts.bytes) + " of " + std::to_string(finishedAt));
	Check(!HudsonVideoWriter_WantsPictureFor(MomentOf(pictures + 1)), "a finished film wants no more pictures");

	HudsonVideoWriter_Finish(30000);
	Check(Facts().finishedInTime && Facts().stopped[0] == 0, "and finishing it twice does no harm");

	WhatWasRead const read = ReadFilm(path.c_str());

	Check(read.opened, "the film opens");
	Check(read.pictures == pictures, "and holds the pictures it says: " + std::to_string(read.pictures));
	Check(read.width == (UINT32)kFilmWidth && read.height >= (UINT32)kFilmHeight, "at the size it was given");
	Check(read.topIsRed, "the right way up, with the rows read correctly");
	Check(fabs(read.lastSeconds - (pictures - 1) / 30.0) < 0.1, "and it plays at real speed: its last picture is at " + std::to_string(read.lastSeconds));

	DeleteFileA(path.c_str());
}

static void TestMomentsWithNoPicture()
{
	std::string const path = BesideThisProgram("video-test-gap.mp4");
	std::vector<uint8_t> const picture = MakePicture(kFilmWidth, kFilmHeight);
	int const pitch = (kFilmWidth + 1) * 4;

	DeleteFileA(path.c_str());
	HudsonVideoWriter_Start(path.c_str(), kFilmWidth, kFilmHeight, 0, kHudsonVideoMostBytes);

	HudsonVideoWriter_Add(&picture[0], pitch, MomentOf(0));
	WaitForWriter(1);
	HudsonVideoWriter_Add(&picture[0], pitch, MomentOf(30));
	WaitForWriter(2);
	HudsonVideoWriter_Add(&picture[0], pitch, MomentOf(60));
	WaitForWriter(3);
	HudsonVideoWriter_Finish(30000);

	HudsonVideoFacts const facts = Facts();

	Check(facts.written == 3, "a game running slowly gives the film only the pictures it drew");
	Check(facts.noPicture == 58, "and the moments it drew nothing for are counted");
	Check(facts.dropped == 0, "and they aren't called dropped");

	WhatWasRead const read = ReadFilm(path.c_str());

	Check(fabs(read.lastSeconds - 2.0) < 0.1, "the film still plays at real speed: its last picture is at " + std::to_string(read.lastSeconds));

	DeleteFileA(path.c_str());
}

// Far more pictures than an encoder can take, as fast as they can be copied. The writer's thread does more for each one than this does, so the queue fills up.
static void TestThatAFullQueueDropsPictures(bool asksFirst)
{
	std::string const path = BesideThisProgram("video-test-flood.mp4");
	std::vector<uint8_t> const picture = MakePicture(1920, 1080);
	int const pitch = (1920 + 1) * 4;
	int const pictures = 120;

	DeleteFileA(path.c_str());
	HudsonVideoWriter_Start(path.c_str(), 1920, 1080, 0, kHudsonVideoMostBytes);

	bool everSaidNo = false;

	for (int i = 0; i < pictures; i++)
	{
		if (!asksFirst || HudsonVideoWriter_WantsPictureFor(MomentOf(i)))
		{
			HudsonVideoWriter_Add(&picture[0], pitch, MomentOf(i));
		}
		else
		{
			everSaidNo = true;
		}
	}

	// Then a slow frame, as the game has now and then: the encoder catches up, and the next picture goes in. The loop above is far faster
	// than the encoder, so its drops all come at its end; only a picture after them shows that each moment missed while the queue was full
	// is counted once, as dropped, and not a second time as a moment nobody had a picture for.
	int moments = pictures;

	if (asksFirst)
	{
		bool tookOneMore = false;

		for (int tries = 0; tries < 400 && !tookOneMore; tries++)
		{
			Sleep(50);

			if (HudsonVideoWriter_WantsPictureFor(MomentOf(moments)))
			{
				HudsonVideoWriter_Add(&picture[0], pitch, MomentOf(moments));
				tookOneMore = true;
			}

			moments++;
		}

		Check(tookOneMore, "once the queue has room again, the next picture goes in");
	}

	HudsonVideoWriter_Finish(60000);

	HudsonVideoFacts const facts = Facts();

	Check(everSaidNo == asksFirst && facts.dropped > 0, "a full queue drops pictures rather than making the game wait, and counts them: " + std::to_string(facts.dropped));
	Check(facts.written + facts.dropped + facts.noPicture == moments, "every moment is written, dropped or missed, each once, and nothing goes uncounted: "
		+ std::to_string(facts.written) + " + " + std::to_string(facts.dropped) + " + " + std::to_string(facts.noPicture) + " of " + std::to_string(moments));
	Check(facts.stopped[0] == 0, std::string("and it isn't an error that stops the film: ") + facts.why);

	WhatWasRead const read = ReadFilm(path.c_str());

	// The film holds a picture for every moment from its first to its last: a moment missed between them holds the picture before, as the
	// game's film does when a frame comes late. Without the picture after the drops, they all come at the end, and the film ends with the
	// last picture written.
	int const filmPictures = asksFirst ? moments : facts.written;

	Check(read.pictures == filmPictures, "the film holds a picture for each moment up to its last: " + std::to_string(read.pictures) + " of " + std::to_string(filmPictures));

	DeleteFileA(path.c_str());
}

static void TestBreakingOnPurpose()
{
	std::string const path = BesideThisProgram("video-test-broken.mp4");
	std::vector<uint8_t> const picture = MakePicture(kFilmWidth, kFilmHeight);
	int const pitch = (kFilmWidth + 1) * 4;

	DeleteFileA(path.c_str());
	Check(HudsonVideoWriter_Start(path.c_str(), kFilmWidth, kFilmHeight, 10, kHudsonVideoMostBytes), "a film that's going to break starts like any other");

	for (int i = 0; i < 30; i++)
	{
		HudsonVideoWriter_Add(&picture[0], pitch, MomentOf(i));
		WaitForWriter(i + 1);
	}

	HudsonVideoFacts const facts = Facts();

	Check(strcmp(facts.stopped, "encoder") == 0, std::string("the encoder failing stops the film and says so: ") + facts.stopped);
	Check(strstr(facts.why, "Windows said 0x") != 0, std::string("with Windows' own number: ") + facts.why);
	Check(facts.written == 9, "only the pictures before it were written: " + std::to_string(facts.written));
	Check(!facts.goingOn && !HudsonVideoWriter_WantsPictureFor(MomentOf(31)), "and after that it wants no more");
	Check(facts.dropped == 0, "and what it was given after that isn't called dropped: " + std::to_string(facts.dropped));

	DWORD const before = GetTickCount();

	HudsonVideoWriter_Finish(30000);

	Check(GetTickCount() - before < 30000, "finishing a broken film can't hold anything up");
	Check(Facts().finishedInTime, "and it does finish");
	Check(strcmp(Facts().stopped, "encoder") == 0, "and it still says why it stopped");

	DeleteFileA(path.c_str());
}

static void TestTheFileLimit()
{
	std::string const path = BesideThisProgram("video-test-limit.mp4");
	std::vector<uint8_t> const picture = MakePicture(kFilmWidth, kFilmHeight);
	int const pitch = (kFilmWidth + 1) * 4;

	DeleteFileA(path.c_str());
	HudsonVideoWriter_Start(path.c_str(), kFilmWidth, kFilmHeight, 0, 1);

	for (int i = 0; i < 300 && Facts().goingOn; i++)
	{
		HudsonVideoWriter_Add(&picture[0], pitch, MomentOf(i));
		WaitForWriter(i + 1);
	}

	HudsonVideoFacts const facts = Facts();

	Check(strcmp(facts.stopped, "file_full") == 0, std::string("a film at its limit stops: ") + facts.stopped);
	Check(strstr(facts.why, "the run goes on") != 0, std::string("and says the run goes on: ") + facts.why);
	HudsonVideoWriter_Finish(30000);
	Check(Facts().finishedInTime, "and it's finished properly");
	Check(ReadFilm(path.c_str()).opened, "so what was written can be played");

	DeleteFileA(path.c_str());
}

static void TestNotStarting()
{
	std::string const path = BesideThisProgram("video-test-nothing.mp4");

	Check(!HudsonVideoWriter_Start(path.c_str(), 641, 360, 0, kHudsonVideoMostBytes), "an odd size doesn't start");
	Check(strcmp(Facts().stopped, "size") == 0 && strstr(Facts().why, "even") != 0, std::string("and says that's why: ") + Facts().why);
	Check(!HudsonVideoWriter_WantsPictureFor(MomentOf(0)), "and it wants no pictures");
	HudsonVideoWriter_Finish(1000);

	Check(!HudsonVideoWriter_Start(BesideThisProgram("no-such-folder-here\\film.mp4").c_str(), kFilmWidth, kFilmHeight, 0, kHudsonVideoMostBytes), "a folder that isn't there doesn't start");
	Check(strcmp(Facts().stopped, "start") == 0 && strstr(Facts().why, "couldn't make the file") != 0, std::string("and says so: ") + Facts().why);
	HudsonVideoWriter_Finish(30000);

	Check(!HudsonVideoWriter_Start(path.c_str(), kFilmWidth, kFilmHeight, 0, 1LL << 62), "a film that can't fit on the disk doesn't start");
	Check(strcmp(Facts().stopped, "disk_full") == 0 && strstr(Facts().why, "no room on the disk") != 0, std::string("and says so: ") + Facts().why);
	HudsonVideoWriter_Finish(30000);
	Check(GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES, "and leaves no file behind");

	std::string longName(MAX_PATH + 10, 'a');

	Check(!HudsonVideoWriter_Start(longName.c_str(), kFilmWidth, kFilmHeight, 0, kHudsonVideoMostBytes), "a path too long for Windows doesn't start");
	Check(strcmp(Facts().stopped, "start") == 0 && strstr(Facts().why, "too long") != 0, std::string("and says so: ") + Facts().why);
	HudsonVideoWriter_Finish(1000);

	HudsonVideoWriter_StopBecause("first", "the first reason");
	HudsonVideoWriter_StopBecause("second", "the second reason");
	Check(strcmp(Facts().stopped, "start") == 0, "the first reason a film stopped is the one that stays");
}

static void TestFinishingBeforeStarting()
{
	HudsonVideoWriter_Finish(1000);
	Check(!Facts().started && Facts().stopped[0] == 0, "finishing a film nobody started does nothing, and isn't called a failure");
}

// Pictures still waiting when the end is asked for go in the film, however quickly it's asked. The queue has room for this many, so none can be dropped.
static void TestFinishingWritesWhatIsWaiting()
{
	std::string const path = BesideThisProgram("video-test-waiting.mp4");
	std::vector<uint8_t> const picture = MakePicture(kFilmWidth, kFilmHeight);
	int const pitch = (kFilmWidth + 1) * 4;

	DeleteFileA(path.c_str());
	HudsonVideoWriter_Start(path.c_str(), kFilmWidth, kFilmHeight, 0, kHudsonVideoMostBytes);

	for (int i = 0; i < kHudsonVideoRingFrames; i++)
	{
		HudsonVideoWriter_Add(&picture[0], pitch, MomentOf(i));
	}

	HudsonVideoWriter_Finish(30000);

	Check(Facts().written == kHudsonVideoRingFrames, "every picture waiting at the end is written: " + std::to_string(Facts().written));
	Check(ReadFilm(path.c_str()).pictures == kHudsonVideoRingFrames, "and is in the film");

	DeleteFileA(path.c_str());
}

static void TestNoSecondFilmAtOnce()
{
	std::string const path = BesideThisProgram("video-test-first.mp4");
	std::string const other = BesideThisProgram("video-test-second.mp4");
	std::vector<uint8_t> const picture = MakePicture(kFilmWidth, kFilmHeight);

	DeleteFileA(path.c_str());
	DeleteFileA(other.c_str());
	HudsonVideoWriter_Start(path.c_str(), kFilmWidth, kFilmHeight, 0, kHudsonVideoMostBytes);

	Check(!HudsonVideoWriter_Start(other.c_str(), kFilmWidth, kFilmHeight, 0, kHudsonVideoMostBytes), "a second film can't start while one is being written");
	Check(Facts().goingOn && Facts().stopped[0] == 0, "and the first goes on");
	Check(GetFileAttributesA(other.c_str()) == INVALID_FILE_ATTRIBUTES, "and there's no second file");

	HudsonVideoWriter_Add(&picture[0], (kFilmWidth + 1) * 4, MomentOf(0));
	HudsonVideoWriter_Finish(30000);
	DeleteFileA(path.c_str());
}

// Rows too short for the film's width: whoever's handing them over has got something wrong, and it mustn't read past the end of them
static void TestRowsTooShort()
{
	std::string const path = BesideThisProgram("video-test-short.mp4");
	std::vector<uint8_t> const picture = MakePicture(kFilmWidth, kFilmHeight);

	DeleteFileA(path.c_str());
	HudsonVideoWriter_Start(path.c_str(), kFilmWidth, kFilmHeight, 0, kHudsonVideoMostBytes);
	HudsonVideoWriter_Add(&picture[0], kFilmWidth, MomentOf(0));

	Check(strcmp(Facts().stopped, "grab") == 0, std::string("rows too short for the picture stop the film: ") + Facts().stopped);
	Check(!Facts().goingOn, "and it wants no more");

	HudsonVideoWriter_Finish(30000);
	Check(Facts().finishedInTime, "and it finishes");

	DeleteFileA(path.c_str());
}

// A film goes in fragments, so that if the game dies part way, what was written so far can still be played. A copy of this program makes one and stops dead, the way a crash would.
static void TestDyingPartWay()
{
	std::string const path = BesideThisProgram("video-test-die.mp4");
	char self[MAX_PATH] = { 0 };

	GetModuleFileNameA(NULL, self, MAX_PATH);

	std::string line = std::string("\"") + self + "\" die \"" + path + "\"";
	STARTUPINFOA startup;
	PROCESS_INFORMATION process;
	DWORD exitCode = 0;

	memset(&startup, 0, sizeof(startup));
	startup.cb = sizeof(startup);
	DeleteFileA(path.c_str());

	if (!CreateProcessA(self, &line[0], NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process))
	{
		Check(false, "a copy of this program should start, to die part way");
		return;
	}

	WaitForSingleObject(process.hProcess, 60000);
	GetExitCodeProcess(process.hProcess, &exitCode);
	CloseHandle(process.hProcess);
	CloseHandle(process.hThread);

	Check(exitCode == 7, "the copy stopped dead, with nothing finished: " + std::to_string(exitCode));

	WhatWasRead const read = ReadFilm(path.c_str());

	Check(read.opened, "what it wrote can still be opened");
	Check(read.pictures >= 60 && read.pictures <= 120, "and most of the 120 pictures it was given are in it: " + std::to_string(read.pictures));

	DeleteFileA(path.c_str());
}

static int MakeAFilmAndDie(const char *path)
{
	std::vector<uint8_t> const picture = MakePicture(kFilmWidth, kFilmHeight);

	CoInitializeEx(NULL, COINIT_MULTITHREADED);
	HudsonVideoWriter_Start(path, kFilmWidth, kFilmHeight, 0, kHudsonVideoMostBytes);

	for (int i = 0; i < 120; i++)
	{
		HudsonVideoWriter_Add(&picture[0], (kFilmWidth + 1) * 4, MomentOf(i));
		WaitForWriter(i + 1);
	}

	TerminateProcess(GetCurrentProcess(), 7);
	return 1;
}

static int ReadARealFilm(const char *path, const char *wordForPictures)
{
	if (!LoadTheReader())
	{
		printf("FAILED  there's no Media Foundation on this machine, so the film can't be read\n");
		return 1;
	}

	HRESULT const com = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	WhatWasRead const read = ReadFilm(path);
	int const said = atoi(wordForPictures);

	printf("opened=%s pictures=%d said=%d size=%ux%u last_picture_at=%.3f\n", read.opened ? "yes" : "no", read.pictures, said, read.width, read.height, read.lastSeconds);

	if (SUCCEEDED(com))
	{
		CoUninitialize();
	}

	if (!read.opened)
	{
		printf("FAILED  the film won't open\n");
		return 1;
	}

	if (read.pictures != said)
	{
		printf("FAILED  the film holds %d pictures, and the result says %d\n", read.pictures, said);
		return 1;
	}

	printf("ok      the film opens, and holds the pictures the result says\n");
	return 0;
}

// The test's own source and the writer's, read from where this file was compiled (/FC gives the whole path)
static std::string ReadSourceFile(char const *name)
{
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();
	return all.str();
}

// The writer takes Media Foundation only if every one of the seven functions it calls is there. A machine with Media Foundation has
// all seven, so leaving one out can't be seen by making a film: the writer's own text is read for it.
static void TestTheWriterNeedsAllItsFunctions()
{
	std::string const writer = ReadSourceFile("3dc/avp/hudson_video_writer.cpp");

	Check(writer.find("dllsAreLoaded = dll.Startup && dll.Shutdown && dll.CreateAttributes && dll.CreateMediaType && dll.CreateMemoryBuffer && dll.CreateSample && dll.CreateSinkWriter;")
		!= std::string::npos, "SOURCE: the writer uses Media Foundation only with all seven of the functions it calls");
}

// film=menus's clip names (hudson_video.h, HudsonVideo_ClipName). Added 2026-11-10 by Melanikus (Third) for the AvP port.
static void TestClipNames()
{
	char out[128];
	char tight[20];   // not "small": the Windows headers make that word mean char
	const char *const film = "hudson-video-2026-11-10-145152-clip01.mp4";

	Check(HudsonVideo_ClipName(film, "join-list.png", out, sizeof(out)) && strcmp(out, "hudson-video-2026-11-10-145152-clip01-join-list.mp4") == 0,
		"a clip that ends at a picture is named after it, without its .png");
	Check(HudsonVideo_ClipName(film, "before-level", out, sizeof(out)) && strcmp(out, "hudson-video-2026-11-10-145152-clip01-before-level.mp4") == 0,
		"and one that ends at a step that isn't a picture, after the step");
	Check(HudsonVideo_ClipName(film, "1234567890123456789012345678901234567890", out, sizeof(out)), "a step of 40 letters is fine");
	Check(HudsonVideo_ClipName(film, "1234567890123456789012345678901234567890.png", out, sizeof(out)), "and so is a picture's name of 40 before its .png");

	// What it refuses, each leaving out as it was
	struct { const char *film; const char *step; const char *why; } const refused[] = {
		{ film, "", "an empty step" },
		{ film, ".png", "a step that's only .png" },
		{ film, "12345678901234567890123456789012345678901", "a step of 41 letters" },
		{ film, "../evil", "a step that climbs out of the folder" },
		{ film, "a\\b", "a step with a backslash" },
		{ film, "a/b", "a step with a slash" },
		{ film, "c:x", "a step with a colon" },
		{ film, "Join-List", "a step with capitals" },
		{ film, "join list", "a step with a space" },
		{ film, "join.list", "a step with a dot that isn't .png's" },
		{ film, "join-list.PNG", "a .PNG in capitals, which isn't a picture's name here" },
		{ "hudson-video-2026-11-10-145152-clip01.avi", "join-list", "a film that isn't an mp4" },
		{ "other-video-2026-11-10-145152-clip01.mp4", "join-list", "a film that isn't Hudson's" },
		{ "hudson-video-.mp4", "join-list", "a film whose name is only the start and the end" },
		{ "hudson-video-..\\x.mp4", "join-list", "a film's name that climbs out" },
		{ "hudson-video-ABC.mp4", "join-list", "a film's name with capitals" },
		{ "C:\\runs\\hudson-video-1.mp4", "join-list", "a film given with its folder" },
		{ 0, "join-list", "no film's name" },
		{ film, 0, "no step" },
	};

	for (const auto &one : refused)
	{
		strcpy(out, "unchanged");
		Check(!HudsonVideo_ClipName(one.film, one.step, out, sizeof(out)) && strcmp(out, "unchanged") == 0, std::string("refused, writing nothing: ") + one.why);
	}

	strcpy(tight, "unchanged");
	Check(!HudsonVideo_ClipName("hudson-video-1.mp4", "join-list", tight, 18) && strcmp(tight, "unchanged") == 0, "a name too long for out is refused, writing nothing");
	Check(HudsonVideo_ClipName("hudson-video-1.mp4", "join-list", out, 29) && strcmp(out, "hudson-video-1-join-list.mp4") == 0, "and exactly enough room is enough");
	strcpy(out, "unchanged");
	Check(!HudsonVideo_ClipName("hudson-video-1.mp4", "join-list", out, 28) && strcmp(out, "unchanged") == 0, "one less isn't");
	Check(!HudsonVideo_ClipName("hudson-video-1.mp4", "join-list", out, 0), "and no room at all isn't");

	// A name it makes ends with its own 0, whatever out held before (Snyk Code, 2026-11-10: the 0 is written on a line of its own)
	memset(out, 'x', sizeof(out));
	Check(HudsonVideo_ClipName("hudson-video-1.mp4", "join-list", out, sizeof(out)) && out[28] == 0 && strcmp(out, "hudson-video-1-join-list.mp4") == 0,
		"a name made in a buffer full of other letters still ends right after its .mp4");
}

// film=menus's cut, as hudson.cpp makes it (CutTheFilm): a clip finished within the cut's wait, then the next started at once, perhaps at
// another size, each a whole film that plays. Added 2026-11-10 by Melanikus (Third) for the AvP port.
static void TestClipsBackToBack()
{
	int const sizes[2][2] = { { kFilmWidth, kFilmHeight }, { 320, 180 } };
	std::string paths[2] = { BesideThisProgram("video-test-clip01.mp4"), BesideThisProgram("video-test-clip02.mp4") };
	int const pictures = 30;

	for (int c = 0; c < 2; c++)
	{
		int const width = sizes[c][0], height = sizes[c][1];
		std::vector<uint8_t> const picture = MakePicture(width, height);
		int const pitch = (width + 1) * 4;

		DeleteFileA(paths[c].c_str());
		Check(HudsonVideoWriter_Start(paths[c].c_str(), width, height, 0, kHudsonVideoMostBytes), "clip " + std::to_string(c + 1) + " starts right after the one before: " + Facts().why);

		for (int i = 0; i < pictures; i++)
		{
			HudsonVideoWriter_Add(&picture[0], pitch, MomentOf(i));
			WaitForWriter(i + 1);
		}

		HudsonVideoWriter_Finish(kHudsonVideoCutWaitMilliseconds);
		Check(Facts().finishedInTime && Facts().written == pictures, "and is finished within a cut's wait, with every picture");
	}

	for (int c = 0; c < 2; c++)
	{
		WhatWasRead const read = ReadFilm(paths[c].c_str());

		Check(read.opened && read.pictures == pictures, "clip " + std::to_string(c + 1) + " opens and holds its pictures: " + std::to_string(read.pictures));
		Check(read.width == (UINT32)sizes[c][0] && read.height >= (UINT32)sizes[c][1], "at its own size");
		DeleteFileA(paths[c].c_str());
	}
}

int main(int count, char **words)
{
	if (count == 4 && strcmp(words[1], "read") == 0)
	{
		return ReadARealFilm(words[2], words[3]);
	}

	if (count == 3 && strcmp(words[1], "die") == 0)
	{
		return MakeAFilmAndDie(words[2]);
	}

	TestTheSums();
	TestTheQueue();
	TestCopying();
	TestTheWriterNeedsAllItsFunctions();
	TestClipNames();

	if (TheWriterCanWorkHere() && LoadTheReader())
	{
		HRESULT const com = CoInitializeEx(NULL, COINIT_MULTITHREADED);

		TestFinishingBeforeStarting();
		TestAFilm();
		TestMomentsWithNoPicture();
		TestClipsBackToBack();
		TestThatAFullQueueDropsPictures(true);
		TestThatAFullQueueDropsPictures(false);
		TestBreakingOnPurpose();
		TestTheFileLimit();
		TestFinishingWritesWhatIsWaiting();
		TestNoSecondFilmAtOnce();
		TestRowsTooShort();
		TestDyingPartWay();
		TestNotStarting();

		if (SUCCEEDED(com))
		{
			CoUninitialize();
		}
	}
	else if (MaySkipRealFilms(getenv("CI"), getenv("GITHUB_ACTIONS")))
	{
		printf("SKIPPED real films: there's no Media Foundation on this machine, so only the sums were tried\n");
	}
	else
	{
		Check(false, "there's no Media Foundation on this machine, and it checks changes, so half of this test can't run here. On Windows Server, it needs the Server-Media-Foundation feature");
	}

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");

	return checksFailed ? 1 : 0;
}
