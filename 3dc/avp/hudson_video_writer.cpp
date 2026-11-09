// See hudson_video_writer.h. Written 2026-11-05 by Melanikus for the AvP port. This file knows nothing about the game or the graphics card, so a test can make real films with it.

// The video writer's headers only show what's needed here to a program that says it's for a recent Windows. Nothing here links to anything newer: all of it is found by name when a film starts.
#undef WINVER
#define WINVER 0x0A00
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00

#include "hudson_video_writer.h"
#include "hudson_video.h"

#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// This one line makes the headers below define the video writer's long ID numbers here, rather than only say they exist somewhere else, so nothing extra has to be linked for them.
#include <initguid.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#pragma comment(lib, "ole32.lib")

namespace
{
	// Hands something back to Windows when it's no longer needed, however the function ends
	template <typename T> class Held
	{
	public:
		Held() : it(0) {}
		~Held() { if (it) { it->Release(); } }
		T *it;

	private:
		Held(const Held &);
		Held &operator=(const Held &);
	};

	// The few things needed from the video writer's DLLs
	struct VideoDlls
	{
		HRESULT (WINAPI *Startup)(ULONG version, DWORD flags);
		HRESULT (WINAPI *Shutdown)();
		HRESULT (WINAPI *CreateAttributes)(IMFAttributes **made, UINT32 howMany);
		HRESULT (WINAPI *CreateMediaType)(IMFMediaType **made);
		HRESULT (WINAPI *CreateMemoryBuffer)(DWORD bytes, IMFMediaBuffer **made);
		HRESULT (WINAPI *CreateSample)(IMFSample **made);
		HRESULT (WINAPI *CreateSinkWriter)(LPCWSTR path, IMFByteStream *stream, IMFAttributes *attributes, IMFSinkWriter **made);
	};

	// There's only ever one film, so there's only one of these. Its memory is never given back while the program runs, because the end of a run can come from another thread while a picture is being copied, and this way there's nothing it can pull out from under that.
	struct Recording
	{
		CRITICAL_SECTION lock;         // looks after queue, bytes, stopped and why
		HudsonVideoQueue queue;
		uint8_t *pictures;             // kHudsonVideoRingFrames of them, one after another
		int slots[kHudsonVideoRingFrames];
		int width, height, pictureBytes;
		int breakAtPicture;
		long long mostBytes;
		wchar_t path[MAX_PATH];
		wchar_t folder[MAX_PATH];
		HANDLE wake;                   // tells the writer's thread there's something to do
		HANDLE ready;                  // it's ready, or it has given up
		HANDLE thread;
		int lastSlot;                  // only the thread that adds pictures uses this
		long long bytes;
		char stopped[16];
		char why[256];
		bool started;
		bool finishedInTime;
		volatile LONG goingOn;
		volatile LONG finishAsked;
		volatile LONG written;
		volatile LONG dropped;
		volatile LONG noPicture;
	};
}

static VideoDlls dll;
static bool dllsAreLoaded = false;
static Recording video;
static bool lockIsMade = false;

// LOAD_LIBRARY_SEARCH_SYSTEM32 means Windows' own folder and nowhere else, so that a file planted beside the game can't be picked up instead.
static bool LoadTheDlls()
{
	if (dllsAreLoaded)
	{
		return true;
	}

	HMODULE const platform = LoadLibraryExA("mfplat.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
	HMODULE const readWrite = LoadLibraryExA("mfreadwrite.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);

	if (!platform || !readWrite)
	{
		return false;
	}

	dll.Startup = reinterpret_cast<decltype(dll.Startup)>(GetProcAddress(platform, "MFStartup"));
	dll.Shutdown = reinterpret_cast<decltype(dll.Shutdown)>(GetProcAddress(platform, "MFShutdown"));
	dll.CreateAttributes = reinterpret_cast<decltype(dll.CreateAttributes)>(GetProcAddress(platform, "MFCreateAttributes"));
	dll.CreateMediaType = reinterpret_cast<decltype(dll.CreateMediaType)>(GetProcAddress(platform, "MFCreateMediaType"));
	dll.CreateMemoryBuffer = reinterpret_cast<decltype(dll.CreateMemoryBuffer)>(GetProcAddress(platform, "MFCreateMemoryBuffer"));
	dll.CreateSample = reinterpret_cast<decltype(dll.CreateSample)>(GetProcAddress(platform, "MFCreateSample"));
	dll.CreateSinkWriter = reinterpret_cast<decltype(dll.CreateSinkWriter)>(GetProcAddress(readWrite, "MFCreateSinkWriterFromURL"));

	dllsAreLoaded = dll.Startup && dll.Shutdown && dll.CreateAttributes && dll.CreateMediaType && dll.CreateMemoryBuffer && dll.CreateSample && dll.CreateSinkWriter;

	return dllsAreLoaded;
}

bool HudsonVideoWriter_CanRecordHere()
{
	return LoadTheDlls();
}

void HudsonVideoWriter_StopBecause(const char *word, const char *why)
{
	if (lockIsMade)
	{
		EnterCriticalSection(&video.lock);
	}

	if (video.stopped[0] == 0)
	{
		_snprintf(video.stopped, sizeof(video.stopped) - 1, "%s", word);
		_snprintf(video.why, sizeof(video.why) - 1, "%s", why);
	}

	if (lockIsMade)
	{
		LeaveCriticalSection(&video.lock);
	}

	InterlockedExchange(&video.goingOn, 0);
}

static void StopBecauseOf(const char *word, const char *what, HRESULT hr)
{
	char why[256] = { 0 };

	_snprintf(why, sizeof(why) - 1, "%s: Windows said 0x%08lX", what, (unsigned long)hr);
	HudsonVideoWriter_StopBecause(word, why);
}

static long long BytesInTheFile()
{
	WIN32_FILE_ATTRIBUTE_DATA facts;

	if (!GetFileAttributesExW(video.path, GetFileExInfoStandard, &facts))
	{
		return 0;
	}

	return ((long long)facts.nFileSizeHigh << 32) | facts.nFileSizeLow;
}

// -1 if the disk won't say
static long long BytesFreeOnTheDisk()
{
	ULARGE_INTEGER available;

	if (!GetDiskFreeSpaceExW(video.folder[0] ? video.folder : NULL, &available, NULL, NULL))
	{
		return -1;
	}

	return (long long)available.QuadPart;
}

// The writer, the film it's to make, and the pictures it'll be given. Anything that fails says which step it was.
static bool OpenTheWriter(IMFSinkWriter **sink, DWORD *stream)
{
	Held<IMFAttributes> attributes;
	Held<IMFMediaType> film;
	Held<IMFMediaType> pictures;
	UINT32 const width = (UINT32)video.width, height = (UINT32)video.height;

	HRESULT hr = dll.CreateAttributes(&attributes.it, 2);

	if (FAILED(hr))
	{
		StopBecauseOf("start", "couldn't make the writer's settings", hr);
		return false;
	}

	// Fragments, so that a film can be played even if the game dies part way. And the graphics card's own encoder if it has one, which costs the game less than doing it on the processor.
	attributes.it->SetGUID(MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_FMPEG4);
	attributes.it->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

	hr = dll.CreateSinkWriter(video.path, NULL, attributes.it, sink);

	if (FAILED(hr))
	{
		StopBecauseOf("start", "couldn't make the file", hr);
		return false;
	}

	hr = dll.CreateMediaType(&film.it);

	if (SUCCEEDED(hr))
	{
		film.it->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		film.it->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
		film.it->SetUINT32(MF_MT_AVG_BITRATE, (UINT32)HudsonVideo_BitsPerSecond(video.width, video.height));
		film.it->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
		MFSetAttributeSize(film.it, MF_MT_FRAME_SIZE, width, height);
		MFSetAttributeRatio(film.it, MF_MT_FRAME_RATE, kHudsonVideoFramesPerSecond, 1);
		MFSetAttributeRatio(film.it, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
		hr = (*sink)->AddStream(film.it, stream);
	}

	if (FAILED(hr))
	{
		StopBecauseOf("start", "couldn't say what film to make", hr);
		return false;
	}

	hr = dll.CreateMediaType(&pictures.it);

	if (SUCCEEDED(hr))
	{
		pictures.it->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		pictures.it->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
		pictures.it->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
		// A positive number here says the first row in memory is the top of the picture. Leave it out and the film comes out upside down.
		pictures.it->SetUINT32(MF_MT_DEFAULT_STRIDE, width * 4);
		MFSetAttributeSize(pictures.it, MF_MT_FRAME_SIZE, width, height);
		MFSetAttributeRatio(pictures.it, MF_MT_FRAME_RATE, kHudsonVideoFramesPerSecond, 1);
		MFSetAttributeRatio(pictures.it, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
		hr = (*sink)->SetInputMediaType(*stream, pictures.it, NULL);
	}

	if (FAILED(hr))
	{
		StopBecauseOf("start", "couldn't say what pictures it'll be given", hr);
		return false;
	}

	hr = (*sink)->BeginWriting();

	if (FAILED(hr))
	{
		StopBecauseOf("start", "couldn't start writing", hr);
		return false;
	}

	return true;
}

// Hands one picture to the encoder. This is where the thread waits, if the encoder is slow.
static bool WriteOnePicture(IMFSinkWriter *sink, DWORD stream, int place)
{
	Held<IMFMediaBuffer> buffer;
	Held<IMFSample> sample;
	BYTE *into = 0;

	HRESULT hr = dll.CreateMemoryBuffer((DWORD)video.pictureBytes, &buffer.it);

	if (SUCCEEDED(hr))
	{
		hr = buffer.it->Lock(&into, NULL, NULL);
	}

	if (SUCCEEDED(hr))
	{
		memcpy(into, video.pictures + (size_t)place * video.pictureBytes, video.pictureBytes);
		hr = buffer.it->Unlock();
	}

	if (SUCCEEDED(hr))
	{
		hr = buffer.it->SetCurrentLength((DWORD)video.pictureBytes);
	}

	if (SUCCEEDED(hr))
	{
		hr = dll.CreateSample(&sample.it);
	}

	if (SUCCEEDED(hr))
	{
		hr = sample.it->AddBuffer(buffer.it);
	}

	if (SUCCEEDED(hr))
	{
		hr = sample.it->SetSampleTime(HudsonVideo_TimeOfSlot(video.slots[place]));
	}

	if (SUCCEEDED(hr))
	{
		hr = sample.it->SetSampleDuration(HudsonVideo_SlotLength());
	}

	if (SUCCEEDED(hr))
	{
		// To break it on purpose it asks for a stream that isn't there, so that the failure is a real one from the real writer, and goes the way a real one would.
		bool const breakIt = video.breakAtPicture > 0 && video.written + 1 == video.breakAtPicture;

		hr = sink->WriteSample(breakIt ? stream + 1 : stream, sample.it);
	}

	if (FAILED(hr))
	{
		StopBecauseOf("encoder", "the encoder wouldn't take a picture, so the film stops here and the run goes on", hr);
		return false;
	}

	InterlockedIncrement(&video.written);
	return true;
}

static long long NoteTheSizeOfTheFile()
{
	long long const bytes = BytesInTheFile();

	EnterCriticalSection(&video.lock);
	video.bytes = bytes;
	LeaveCriticalSection(&video.lock);

	return bytes;
}

// Stops the film if it's as big as it may be, or the disk is getting full
static void StopIfThereIsNoRoom()
{
	long long const bytes = NoteTheSizeOfTheFile();
	char why[256] = { 0 };

	switch (HudsonVideo_ShouldStop(bytes, video.mostBytes, BytesFreeOnTheDisk()))
	{
		case kHudsonVideoFileIsFull:
			_snprintf(why, sizeof(why) - 1, "the film has reached its limit of %lld bytes, so it stops here and the run goes on", video.mostBytes);
			HudsonVideoWriter_StopBecause("file_full", why);
			break;

		case kHudsonVideoDiskIsFull:
			_snprintf(why, sizeof(why) - 1, "the disk has less than %lld bytes left, so the film stops here and the run goes on", kHudsonVideoDiskToLeave);
			HudsonVideoWriter_StopBecause("disk_full", why);
			break;

		case kHudsonVideoDiskWontSay:
			HudsonVideoWriter_StopBecause("disk_unknown", "the disk won't say how much room is left, so the film stops here, before it could fill it, and the run goes on");
			break;

		default:
			break;
	}
}

static unsigned __stdcall WriterThread(void *)
{
	HRESULT const com = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	HRESULT const startup = !LoadTheDlls() ? E_FAIL : FAILED(com) ? com : dll.Startup(MF_VERSION, MFSTARTUP_LITE);
	IMFSinkWriter *sink = 0;
	DWORD stream = 0;

	if (FAILED(startup))
	{
		StopBecauseOf("start", "couldn't start Windows' video writer", startup);
	}
	else if (BytesFreeOnTheDisk() < 0)
	{
		HudsonVideoWriter_StopBecause("disk_unknown", "the disk won't say how much room is left, so there's no film, because it could fill the disk, and the run goes on");
	}
	else if (!HudsonVideo_DiskHasRoom(BytesFreeOnTheDisk(), video.mostBytes))
	{
		char why[256] = { 0 };

		_snprintf(why, sizeof(why) - 1, "there's no room on the disk for a film of up to %lld bytes and %lld left over, so there's no film, and the run goes on", video.mostBytes, kHudsonVideoDiskToLeave);
		HudsonVideoWriter_StopBecause("disk_full", why);
	}
	else if (!OpenTheWriter(&sink, &stream) && sink)
	{
		sink->Release();
		sink = 0;
	}

	SetEvent(video.ready);

	while (sink)
	{
		// It looks now and then even if it isn't woken, so that it can't miss being told something
		WaitForSingleObject(video.wake, 100);

		// This is read before writing what's waiting, so that a picture added just before the end was asked for can't be left behind
		bool const thatIsTheEnd = video.finishAsked != 0;

		for (;;)
		{
			EnterCriticalSection(&video.lock);
			int const place = HudsonVideoQueue_PlaceToWrite(video.queue);
			LeaveCriticalSection(&video.lock);

			if (place < 0 || !video.goingOn || !WriteOnePicture(sink, stream, place))
			{
				break;
			}

			EnterCriticalSection(&video.lock);
			HudsonVideoQueue_Written(&video.queue);
			LeaveCriticalSection(&video.lock);

			StopIfThereIsNoRoom();
		}

		if (!video.goingOn || thatIsTheEnd)
		{
			break;
		}
	}

	if (sink)
	{
		HRESULT const finished = sink->Finalize();

		if (FAILED(finished) && video.stopped[0] == 0)
		{
			StopBecauseOf("finish", "couldn't finish the film, though what was written so far can still be played", finished);
		}

		sink->Release();

		// Only to say how big it came out. It's finished, so it can't be said to have stopped early.
		NoteTheSizeOfTheFile();
	}

	InterlockedExchange(&video.goingOn, 0);

	if (SUCCEEDED(startup))
	{
		dll.Shutdown();
	}

	if (SUCCEEDED(com))
	{
		CoUninitialize();
	}

	return 0;
}

bool HudsonVideoWriter_Start(const char *path, int width, int height, int breakAtPicture, long long mostBytes)
{
	// A film whose thread hasn't finished could still be using all of this, so another can't start
	if (video.started && !video.finishedInTime)
	{
		return false;
	}

	if (!lockIsMade)
	{
		InitializeCriticalSection(&video.lock);
		lockIsMade = true;
	}

	free(video.pictures);
	video.pictures = 0;
	video.queue.first = video.queue.waiting = 0;
	video.lastSlot = -1;
	video.bytes = 0;
	video.stopped[0] = video.why[0] = 0;
	video.started = video.finishedInTime = false;
	video.goingOn = video.finishAsked = video.written = video.dropped = video.noPicture = 0;
	video.width = width;
	video.height = height;
	video.breakAtPicture = breakAtPicture;
	video.mostBytes = mostBytes;

	const char *const wrongSize = HudsonVideo_WhyNotThisSize(width, height);

	if (wrongSize)
	{
		HudsonVideoWriter_StopBecause("size", wrongSize);
		return false;
	}

	if (!MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, path, -1, video.path, MAX_PATH))
	{
		HudsonVideoWriter_StopBecause("start", "the film's path is too long, or can't be written");
		return false;
	}

	memcpy(video.folder, video.path, sizeof(video.folder));

	wchar_t *const lastSlash = wcsrchr(video.folder, L'\\');

	if (lastSlash)
	{
		lastSlash[1] = 0;
	}
	else
	{
		video.folder[0] = 0;
	}

	video.pictureBytes = width * height * 4;
	video.pictures = (uint8_t *)malloc((size_t)video.pictureBytes * kHudsonVideoRingFrames);

	if (!video.pictures)
	{
		HudsonVideoWriter_StopBecause("start", "there isn't the memory for the queue of pictures");
		return false;
	}

	if (!video.wake)
	{
		video.wake = CreateEventA(NULL, FALSE, FALSE, NULL);
		video.ready = CreateEventA(NULL, FALSE, FALSE, NULL);
	}

	if (!video.wake || !video.ready)
	{
		HudsonVideoWriter_StopBecause("start", "couldn't make what the thread waits on");
		return false;
	}

	ResetEvent(video.ready);
	InterlockedExchange(&video.goingOn, 1);

	uintptr_t const thread = _beginthreadex(NULL, 0, WriterThread, NULL, 0, NULL);

	if (thread == 0)
	{
		HudsonVideoWriter_StopBecause("start", "couldn't start the writer's thread");
		return false;
	}

	video.thread = (HANDLE)thread;
	video.started = true;

	// This is the one place the game waits for it, once, before the first picture. If it's still not ready after this, pictures are dropped and counted until it is.
	WaitForSingleObject(video.ready, 15000);

	return video.goingOn != 0;
}

// Moves on to this moment, counting any moments in between that nobody had a picture for
static void ReachedSlot(int slot)
{
	int const missed = slot - video.lastSlot - 1;

	if (missed > 0)
	{
		InterlockedExchangeAdd(&video.noPicture, missed);
	}

	video.lastSlot = slot;
}

bool HudsonVideoWriter_WantsPictureFor(double seconds)
{
	if (!video.goingOn || video.finishAsked)
	{
		return false;
	}

	int const slot = HudsonVideo_SlotAt(seconds);

	if (slot <= video.lastSlot)
	{
		return false;
	}

	EnterCriticalSection(&video.lock);
	int const place = HudsonVideoQueue_PlaceToAdd(video.queue);
	LeaveCriticalSection(&video.lock);

	if (place < 0)
	{
		InterlockedIncrement(&video.dropped);
		ReachedSlot(slot);
		return false;
	}

	return true;
}

void HudsonVideoWriter_Add(const uint8_t *picture, int pitch, double seconds)
{
	if (!video.goingOn || video.finishAsked)
	{
		return;
	}

	int const slot = HudsonVideo_SlotAt(seconds);

	if (slot <= video.lastSlot)
	{
		return;
	}

	EnterCriticalSection(&video.lock);
	int const place = HudsonVideoQueue_PlaceToAdd(video.queue);
	LeaveCriticalSection(&video.lock);

	if (place < 0)
	{
		InterlockedIncrement(&video.dropped);
		ReachedSlot(slot);
		return;
	}

	// Only this thread adds, so this place stays free, and the writer's thread won't look at it until it's counted below
	if (!HudsonVideo_CopyPicture(video.pictures + (size_t)place * video.pictureBytes, video.width, video.height, picture, pitch))
	{
		HudsonVideoWriter_StopBecause("grab", "the frame's rows are shorter than the film's, so the film stops here and the run goes on");
		return;
	}

	video.slots[place] = slot;

	EnterCriticalSection(&video.lock);
	HudsonVideoQueue_Added(&video.queue);
	LeaveCriticalSection(&video.lock);

	ReachedSlot(slot);
	SetEvent(video.wake);
}

void HudsonVideoWriter_Finish(unsigned long mostMilliseconds)
{
	if (!video.started || InterlockedExchange(&video.finishAsked, 1) != 0)
	{
		return;
	}

	SetEvent(video.wake);

	if (WaitForSingleObject(video.thread, mostMilliseconds) == WAIT_OBJECT_0)
	{
		video.finishedInTime = true;
		CloseHandle(video.thread);
		video.thread = 0;
	}
	else
	{
		HudsonVideoWriter_StopBecause("finish", "the writer didn't finish in time, so the run goes on without it. What was written so far can still be played");
	}
}

void HudsonVideoWriter_GetFacts(HudsonVideoFacts *facts)
{
	memset(facts, 0, sizeof(*facts));

	if (lockIsMade)
	{
		EnterCriticalSection(&video.lock);
	}

	facts->bytes = video.bytes;
	memcpy(facts->stopped, video.stopped, sizeof(facts->stopped));
	memcpy(facts->why, video.why, sizeof(facts->why));

	if (lockIsMade)
	{
		LeaveCriticalSection(&video.lock);
	}

	facts->started = video.started;
	facts->goingOn = video.goingOn != 0;
	facts->finishedInTime = video.finishedInTime;
	facts->written = video.written;
	facts->width = video.width;
	facts->height = video.height;
	facts->dropped = video.dropped;
	facts->noPicture = video.noPicture;
}
