// Joins the two films of a pair run into one wide film, the host on the left and the joiner on the right, each half 1920x1080, lined up to the same moment. And takes a picture from any moment of a film.
// Written 2026-11-07 by Melanikus for the AvP port. A test tool, like the rest of Hudson: it never goes in the game.
// Modified 2026-11-09 by Melanikus for the AvP port:
//   its checks are in join_films_reads.cpp, moved out of the game's hudson_plan.cpp. Built from both, with 3dc/avp as a folder to search
// Modified 2026-11-07 by Melanikus for the AvP port:
//   it reads a picture whatever kind of buffer it comes in, checks its rows fit in that buffer first, and says which step failed and why
//   --skip-left lines up two films that have no clock, and the last line says how many pictures of each were used
//   seconds are read strictly, and nothing is written over a file that's already there
//   it waits for the encoder instead of handing it pictures faster than it can take them, and says how much memory that costs
//   the encoder is asked to hold back as few pictures as it can, and every line is printed as it's written
//
// HOW THE TWO ARE LINED UP: a siege writes the machine's clock, in milliseconds, into the top left corner of each film's pictures. Both games of a pair run are on one machine, so the same number is the same moment in both.
// The clock is read from each film's first picture, and from then on each picture's own time in its film says how far on it is. Films with no clock in their corner are taken to start at the same moment, unless
// --skip-left says how many seconds the host's film started before the joiner's. Then that much of the host's film is left out.
//
// HOW IT READS AND WRITES: Windows' own Media Foundation, as the game's film writer does. A Source Reader turns each film back into pictures, and a Sink Writer makes the new film. Nothing is downloaded.
// A film is from outside this program, so what it says about its own pictures is checked before anything is read: their size, the part that's shown, and that every row is inside the buffer it came in.
//
//   join-films [--skip-left <seconds>] <host film> <joiner film> <new film>    writes the wide film, 30 pictures a second, for as long as both films have pictures
//   join-films --picture <film> <seconds> <picture.png>                        writes the picture that film shows that many seconds in
//
// Seconds are written with a point, such as 7.4 or 85.967, from 0 to 3600. The new film or picture must not be there already: nothing is written over.
// Exit code: 0 done, 1 wrong words, 2 a film couldn't be read (or stopped being readable partway, after which the new film ends there), 3 the new film or picture couldn't be written.

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wincodec.h>
#include <psapi.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "join_films_reads.h"

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "psapi.lib")

static const int kHalfWidth = 1920, kHalfHeight = 1080;
static const int kFramesPerSecond = 30;
static const UINT32 kBitsPerSecond = 24000000;
static const LONGLONG kTicksPerSecond = 10000000; // Media Foundation counts time in tenths of a microsecond

// The writer can take pictures faster than the encoder turns them into film, and it holds the ones waiting. Each wide picture is
// 16.6 MB. Joins used more than a gigabyte, and one ran out of memory, which is what a growing queue of them would do. So no more than
// this many wait at a time. The encoder is asked to hold back as few as it can (MF_LOW_LATENCY), but it may still keep some until it has the
// next ones, so if it makes no progress for this long, it's given another rather than waited on. A first try waited 2 seconds for that, with
// room for 8, and nearly every picture waited the whole 2 seconds.
static const ULONGLONG kMostPicturesWaiting = 16;
static const DWORD kWaitForTheEncoderAtMost = 100;   // milliseconds
static const int kLargestPicture = 8192;          // across or down: bigger than any film a run makes

// Lets go of a Media Foundation object when it goes out of scope
template <typename T> struct Held
{
	T *it = nullptr;
	~Held() { if (it) it->Release(); }
};

static std::wstring Wide(const char *text)
{
	int const length = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0);
	std::wstring out(length > 0 ? length : 1, L'\0');

	MultiByteToWideChar(CP_ACP, 0, text, -1, &out[0], length);
	return out;
}

// Whether a file or folder is there already
static bool AlreadyThere(const char *path)
{
	return GetFileAttributesW(Wide(path).c_str()) != INVALID_FILE_ATTRIBUTES;
}

// One film being read, one picture at a time, as top-down rows of four bytes to the pixel
struct Film
{
	Held<IMFSourceReader> reader;
	int width = 0, height = 0;           // the part of each picture that's shown, which is what's kept
	int frameWidth = 0, frameHeight = 0; // the whole picture as it comes out, which can be a little taller: H.264 works in blocks of 16 rows
	int shownLeft = 0, shownTop = 0;
	LONG stride = 0;                     // in a plain buffer, the step from one row to the next, negative when the rows are stored bottom up
	std::vector<unsigned char> picture;
	LONGLONG time = 0;                   // the picture's time in its film
	long number = 0;                     // which picture of the film this is, counting from 1
	bool ended = false;
	const char *failedAt = nullptr;      // the step that went wrong, if one did
	HRESULT failure = S_OK;

	bool Fail(const char *step, HRESULT hr)
	{
		failedAt = step;
		failure = hr;
		ended = true;
		return false;
	}

	bool Open(const char *path)
	{
		Held<IMFAttributes> settings;
		Held<IMFMediaType> wanted;
		HRESULT hr;

		// Turning the film's pictures into plain colours is done by Media Foundation's own video processing
		if (FAILED(hr = MFCreateAttributes(&settings.it, 1)) || FAILED(hr = settings.it->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE)))
		{
			return Fail("making the reader's settings", hr);
		}

		if (FAILED(hr = MFCreateSourceReaderFromURL(Wide(path).c_str(), settings.it, &reader.it)))
		{
			return Fail("opening the film", hr);
		}

		if (FAILED(hr = MFCreateMediaType(&wanted.it)))
		{
			return Fail("making the kind of picture wanted", hr);
		}

		wanted.it->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		wanted.it->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);

		if (FAILED(hr = reader.it->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE)) || FAILED(hr = reader.it->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE)))
		{
			return Fail("choosing the film's pictures", hr);
		}

		if (FAILED(hr = reader.it->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, wanted.it)))
		{
			return Fail("asking for the pictures in plain colours", hr);
		}

		return ReadTheFormat();
	}

	// What the pictures will be: their size, the part that's shown, and how their rows are laid out. Read when the film opens, and again if the film says it has changed, as a film's decoder
	// can once it has read the first picture and knows the real size.
	bool ReadTheFormat()
	{
		Held<IMFMediaType> given;
		HRESULT hr;

		if (FAILED(hr = reader.it->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &given.it)))
		{
			return Fail("reading what the pictures will be", hr);
		}

		UINT32 w = 0, h = 0;

		if (FAILED(hr = MFGetAttributeSize(given.it, MF_MT_FRAME_SIZE, &w, &h)))
		{
			return Fail("reading the pictures' size", hr);
		}

		if (w == 0 || h == 0 || w > kLargestPicture || h > kLargestPicture)
		{
			return Fail("the film says its pictures are a size no film of a run is", MF_E_INVALIDMEDIATYPE);
		}

		frameWidth = width = (int)w;
		frameHeight = height = (int)h;
		shownLeft = shownTop = 0;

		// Which way up a plain buffer's rows are, and how far apart. Without a word from the film, it's what Windows says a picture of this kind is.
		UINT32 givenStride = 0;

		if (SUCCEEDED(given.it->GetUINT32(MF_MT_DEFAULT_STRIDE, &givenStride)))
		{
			stride = (LONG)givenStride;
		}
		else if (FAILED(MFGetStrideForBitmapInfoHeader(MFVideoFormat_RGB32.Data1, w, &stride)))
		{
			stride = (LONG)w * 4;
		}

		// The part of each picture that's shown, if the film says. It must be inside the picture, or all of the picture is kept.
		MFVideoArea area;
		UINT32 areaSize = 0;

		memset(&area, 0, sizeof(area));

		if ((SUCCEEDED(given.it->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8 *)&area, sizeof(area), &areaSize)) || SUCCEEDED(given.it->GetBlob(MF_MT_GEOMETRIC_APERTURE, (UINT8 *)&area, sizeof(area), &areaSize))))
		{
			if (areaSize == sizeof(area) && JoinFilms_ApertureFits(area.OffsetX.value, area.OffsetY.value, (int)area.Area.cx, (int)area.Area.cy, frameWidth, frameHeight))
			{
				shownLeft = area.OffsetX.value;
				shownTop = area.OffsetY.value;
				width = (int)area.Area.cx;
				height = (int)area.Area.cy;
			}
			else
			{
				printf("join-films: the film says the part of its pictures that's shown isn't inside them, so all of each picture is kept\n");
			}
		}

		picture.assign((size_t)width * height * 4, 0);
		return true;
	}

	// The next picture, into picture. False once the film has no more, or can't be read any more, which failedAt says.
	bool Next()
	{
		while (!ended)
		{
			DWORD flags = 0;
			LONGLONG at = 0;
			Held<IMFSample> sample;
			HRESULT hr;

			if (FAILED(hr = reader.it->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, &flags, &at, &sample.it)))
			{
				return Fail("reading the next picture", hr);
			}

			if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
			{
				ended = true;
				return false;
			}

			if ((flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) && !ReadTheFormat())
			{
				return false;
			}

			if (!sample.it)
			{
				continue;
			}

			Held<IMFMediaBuffer> buffer;

			if (FAILED(hr = sample.it->ConvertToContiguousBuffer(&buffer.it)))
			{
				return Fail("putting the picture's parts together", hr);
			}

			// Where the rows are, and the buffer they must be inside, as numbers, so nothing points anywhere until they're known to fit
			Held<IMF2DBuffer2> rows;
			BYTE *firstRowPointer = nullptr;
			BYTE *start = nullptr;
			LONG pitch = 0;
			DWORD length = 0;
			long long firstRow = 0;

			if (SUCCEEDED(buffer.it->QueryInterface(IID_PPV_ARGS(&rows.it))))
			{
				// A buffer that knows its own rows says where the top one is, the step to the next, and where the buffer starts and how long it is
				if (FAILED(hr = rows.it->Lock2DSize(MF2DBuffer_LockFlags_Read, &firstRowPointer, &pitch, &start, &length)))
				{
					return Fail("opening the picture's rows", hr);
				}

				firstRow = (long long)(uintptr_t)firstRowPointer;
			}
			else
			{
				// A plain buffer: the rows are one stride apart, and stored bottom up the top row is the last in the buffer
				if (FAILED(hr = buffer.it->Lock(&start, NULL, &length)))
				{
					return Fail("opening the picture", hr);
				}

				pitch = stride;
				firstRow = (long long)(uintptr_t)start + (stride < 0 ? (long long)(frameHeight - 1) * -(long long)stride : 0);
			}

			bool const fits = JoinFilms_RowsFit(firstRow, pitch, frameHeight, (long long)frameWidth * 4, (long long)(uintptr_t)start, (long long)length);

			if (fits)
			{
				for (int y = 0; y < height; y++)
				{
					BYTE const *const row = (BYTE const *)(uintptr_t)(firstRow + (long long)(shownTop + y) * pitch);

					memcpy(&picture[(size_t)y * width * 4], row + (size_t)shownLeft * 4, (size_t)width * 4);
				}
			}

			if (rows.it)
			{
				rows.it->Unlock2D();
			}
			else
			{
				buffer.it->Unlock();
			}

			if (!fits)
			{
				return Fail("checking the picture's rows are inside its buffer, which they aren't, so it isn't read", MF_E_BUFFERTOOSMALL);
			}

			time = at;
			number++;
			return true;
		}

		return false;
	}
};

// Opens a film and reads its first picture, or says what went wrong
static bool OpenAndRead(Film &film, const char *path, const char *name)
{
	if (film.Open(path) && film.Next())
	{
		printf("join-films: the %s film's pictures are %dx%d, of %dx%d as they come out, rows a step of %ld apart in a plain buffer\n", name, film.width, film.height, film.frameWidth, film.frameHeight, (long)film.stride);
		return true;
	}

	if (film.failedAt)
	{
		printf("join-films: the %s film couldn't be read: %s, which said 0x%08lX\n", name, film.failedAt, (unsigned long)film.failure);
	}
	else
	{
		printf("join-films: the %s film has no pictures\n", name);
	}

	return false;
}

// Puts a picture into one part of a bigger one, made bigger or smaller to fill it, each new pixel blended from the four nearest of the old
static void FitInto(const Film &film, std::vector<unsigned char> &into, int intoWidth, int left, int width, int height)
{
	for (int y = 0; y < height; y++)
	{
		float const sy = ((float)y + 0.5f) * film.height / height - 0.5f;
		int const y0 = sy < 0 ? 0 : (int)sy;
		int const y1 = y0 + 1 < film.height ? y0 + 1 : y0;
		float const fy = sy < 0 ? 0 : sy - y0;
		unsigned char *const out = &into[((size_t)y * intoWidth + left) * 4];

		for (int x = 0; x < width; x++)
		{
			float const sx = ((float)x + 0.5f) * film.width / width - 0.5f;
			int const x0 = sx < 0 ? 0 : (int)sx;
			int const x1 = x0 + 1 < film.width ? x0 + 1 : x0;
			float const fx = sx < 0 ? 0 : sx - x0;
			unsigned char const *const a = &film.picture[((size_t)y0 * film.width + x0) * 4];
			unsigned char const *const b = &film.picture[((size_t)y0 * film.width + x1) * 4];
			unsigned char const *const c = &film.picture[((size_t)y1 * film.width + x0) * 4];
			unsigned char const *const d = &film.picture[((size_t)y1 * film.width + x1) * 4];

			for (int k = 0; k < 3; k++)
			{
				float const top = a[k] + (b[k] - a[k]) * fx;
				float const bottom = c[k] + (d[k] - c[k]) * fx;

				out[x * 4 + k] = (unsigned char)(top + (bottom - top) * fy + 0.5f);
			}

			out[x * 4 + 3] = 255;
		}
	}
}

// The machine's clock, in milliseconds, at this film's current picture, from the clock read at its first picture
struct Clock
{
	bool known = false;
	unsigned long first = 0;
	LONGLONG firstTime = 0;

	long long Now(const Film &film) const
	{
		return (long long)first + (film.time - firstTime) / 10000;
	}
};

static Clock ReadTheClock(const Film &film, const char *name)
{
	Clock clock;

	clock.firstTime = film.time;
	clock.known = JoinFilms_ReadStampedNumber(film.picture.data(), film.width * 4, film.width, film.height, &clock.first);

	if (clock.known)
	{
		printf("join-films: the %s film's clock starts at %lu\n", name, clock.first);
	}
	else
	{
		printf("join-films: the %s film has no clock in its corner\n", name);
	}

	return clock;
}

// Says why a film stopped being readable partway, if it did
static bool StoppedPartway(const Film &film, const char *name)
{
	if (!film.failedAt)
	{
		return false;
	}

	printf("join-films: the %s film stopped being readable after its picture %ld: %s, which said 0x%08lX. The new film ends there\n", name, film.number, film.failedAt, (unsigned long)film.failure);
	return true;
}

// How much memory this program has, in megabytes, for the log
static double MegabytesInUse()
{
	PROCESS_MEMORY_COUNTERS_EX counters;

	memset(&counters, 0, sizeof(counters));
	counters.cb = sizeof(counters);

	if (!GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&counters, sizeof(counters)))
	{
		return 0.0;
	}

	return (double)counters.PrivateUsage / (1024.0 * 1024.0);
}

// How many pictures the writer has been given and the encoder hasn't finished. If the writer won't say, none are counted.
static ULONGLONG PicturesWaiting(IMFSinkWriter *writer, DWORD stream, ULONGLONG *encoded)
{
	MF_SINK_WRITER_STATISTICS stats;

	memset(&stats, 0, sizeof(stats));
	stats.cb = sizeof(stats);

	if (FAILED(writer->GetStatistics(stream, &stats)))
	{
		*encoded = 0;
		return 0;
	}

	*encoded = stats.qwNumSamplesEncoded;
	return stats.qwNumSamplesReceived > stats.qwNumSamplesEncoded ? stats.qwNumSamplesReceived - stats.qwNumSamplesEncoded : 0;
}

// Waits while kMostPicturesWaiting or more are waiting for the encoder, for as long as it's making progress
static void WaitForTheEncoder(IMFSinkWriter *writer, DWORD stream, ULONGLONG *mostWaiting)
{
	ULONGLONG encoded = 0;
	ULONGLONG waiting = PicturesWaiting(writer, stream, &encoded);
	ULONGLONG lastEncoded = encoded;
	DWORD lastProgressAt = GetTickCount();

	*mostWaiting = waiting > *mostWaiting ? waiting : *mostWaiting;

	while (waiting >= kMostPicturesWaiting && GetTickCount() - lastProgressAt < kWaitForTheEncoderAtMost)
	{
		Sleep(5);
		waiting = PicturesWaiting(writer, stream, &encoded);

		if (encoded != lastEncoded)
		{
			lastEncoded = encoded;
			lastProgressAt = GetTickCount();
		}
	}
}

static int JoinTwo(const char *leftPath, const char *rightPath, const char *outPath, bool skipAsked, long skipLeft)
{
	if (AlreadyThere(outPath))
	{
		printf("join-films: %s is there already, and nothing is written over\n", outPath);
		return 1;
	}

	Film left, right;

	if (!OpenAndRead(left, leftPath, "host's") || !OpenAndRead(right, rightPath, "joiner's"))
	{
		return 2;
	}

	Clock leftClock = ReadTheClock(left, "host's");
	Clock rightClock = ReadTheClock(right, "joiner's");
	bool const bothHaveClocks = leftClock.known && rightClock.known;

	if (bothHaveClocks && skipAsked)
	{
		printf("join-films: both films have a clock, which lines them up to the millisecond, so --skip-left would only put them out of step. Nothing was written\n");
		return 1;
	}

	if (!bothHaveClocks)
	{
		// The host's film is taken to start skipLeft milliseconds before the joiner's, so the new film starts that far into it
		leftClock.first = 0;
		rightClock.first = (unsigned long)skipLeft;

		if (skipAsked)
		{
			printf("join-films: the joiner's film is taken to start %ld.%03ld seconds after the host's, as --skip-left says, so that much of the host's is left out\n", skipLeft / 1000, skipLeft % 1000);
		}
		else
		{
			printf("join-films: without a clock in both, the two films are taken to start at the same moment\n");
		}
	}

	// The new film starts when both have started
	long long const start = leftClock.Now(left) > rightClock.Now(right) ? leftClock.Now(left) : rightClock.Now(right);

	Held<IMFSinkWriter> writer;
	Held<IMFMediaType> film, pictures;
	Held<IMFAttributes> writerSettings;
	DWORD stream = 0;
	int const width = kHalfWidth * 2, height = kHalfHeight;

	// The encoder holds back as few pictures as it can: nothing here needs the few percent it saves by looking ahead
	if (FAILED(MFCreateAttributes(&writerSettings.it, 1)) || FAILED(writerSettings.it->SetUINT32(MF_LOW_LATENCY, TRUE)))
	{
		printf("join-films: the new film's settings couldn't be made\n");
		return 3;
	}

	if (FAILED(MFCreateSinkWriterFromURL(Wide(outPath).c_str(), NULL, writerSettings.it, &writer.it)) || FAILED(MFCreateMediaType(&film.it)) || FAILED(MFCreateMediaType(&pictures.it)))
	{
		printf("join-films: the new film couldn't be made\n");
		return 3;
	}

	film.it->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	film.it->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
	film.it->SetUINT32(MF_MT_AVG_BITRATE, kBitsPerSecond);
	film.it->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	MFSetAttributeSize(film.it, MF_MT_FRAME_SIZE, width, height);
	MFSetAttributeRatio(film.it, MF_MT_FRAME_RATE, kFramesPerSecond, 1);
	MFSetAttributeRatio(film.it, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	pictures.it->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	pictures.it->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
	pictures.it->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	// A positive number here says the first row in memory is the top of the picture
	pictures.it->SetUINT32(MF_MT_DEFAULT_STRIDE, width * 4);
	MFSetAttributeSize(pictures.it, MF_MT_FRAME_SIZE, width, height);
	MFSetAttributeRatio(pictures.it, MF_MT_FRAME_RATE, kFramesPerSecond, 1);
	MFSetAttributeRatio(pictures.it, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

	if (FAILED(writer.it->AddStream(film.it, &stream)) || FAILED(writer.it->SetInputMediaType(stream, pictures.it, NULL)) || FAILED(writer.it->BeginWriting()))
	{
		printf("join-films: the new film couldn't be started\n");
		return 3;
	}

	std::vector<unsigned char> wide((size_t)width * height * 4, 0);
	long long written = 0;
	ULONGLONG mostWaiting = 0;
	double mostMegabytes = 0.0;
	long leftUsed = 0, rightUsed = 0, leftLastUsed = 0, rightLastUsed = 0;

	for (;;)
	{
		long long const now = start + written * 1000 / kFramesPerSecond;

		// Each film moves on to the last picture it had by now, and the joining stops when either runs out
		while (!left.ended && leftClock.Now(left) + 1000 / kFramesPerSecond <= now)
		{
			left.Next();
		}

		while (!right.ended && rightClock.Now(right) + 1000 / kFramesPerSecond <= now)
		{
			right.Next();
		}

		if (left.ended || right.ended)
		{
			break;
		}

		// A picture shown twice, because the other film has more of them, is only counted once
		if (left.number != leftLastUsed)
		{
			leftUsed++;
			leftLastUsed = left.number;
		}

		if (right.number != rightLastUsed)
		{
			rightUsed++;
			rightLastUsed = right.number;
		}

		FitInto(left, wide, width, 0, kHalfWidth, kHalfHeight);
		FitInto(right, wide, width, kHalfWidth, kHalfWidth, kHalfHeight);

		WaitForTheEncoder(writer.it, stream, &mostWaiting);

		Held<IMFMediaBuffer> buffer;
		Held<IMFSample> sample;
		BYTE *into = nullptr;
		DWORD const bytes = (DWORD)wide.size();

		if (FAILED(MFCreateMemoryBuffer(bytes, &buffer.it)) || FAILED(buffer.it->Lock(&into, NULL, NULL)))
		{
			printf("join-films: ran out of memory for a picture, after %lld, with %.0f MB in use\n", written, MegabytesInUse());
			return 3;
		}

		memcpy(into, wide.data(), bytes);
		buffer.it->Unlock();
		buffer.it->SetCurrentLength(bytes);

		if (FAILED(MFCreateSample(&sample.it)) || FAILED(sample.it->AddBuffer(buffer.it)) || FAILED(sample.it->SetSampleTime(written * kTicksPerSecond / kFramesPerSecond))
			|| FAILED(sample.it->SetSampleDuration(kTicksPerSecond / kFramesPerSecond)) || FAILED(writer.it->WriteSample(stream, sample.it)))
		{
			printf("join-films: a picture couldn't be written, after %lld\n", written);
			return 3;
		}

		written++;

		double const megabytes = MegabytesInUse();

		mostMegabytes = megabytes > mostMegabytes ? megabytes : mostMegabytes;

		if (written % (kFramesPerSecond * 10) == 0)
		{
			ULONGLONG encoded = 0;

			printf("join-films: %lld seconds written, %llu pictures waiting for the encoder, %.0f MB in use\n", written / kFramesPerSecond, PicturesWaiting(writer.it, stream, &encoded), megabytes);
			fflush(stdout);
		}
	}

	if (FAILED(writer.it->Finalize()))
	{
		printf("join-films: the new film couldn't be finished\n");
		return 3;
	}

	printf("join-films: wrote %lld pictures, %.2f seconds, %dx%d, to %s, from %ld of the host's pictures and %ld of the joiner's\n", written, (double)written / kFramesPerSecond, width, height, outPath, leftUsed, rightUsed);
	printf("join-films: at most %llu pictures waited for the encoder at once, and at most %.0f MB was in use\n", mostWaiting, mostMegabytes);

	bool const leftStopped = StoppedPartway(left, "host's");
	bool const rightStopped = StoppedPartway(right, "joiner's");

	return leftStopped || rightStopped ? 2 : 0;
}

// Writes the picture a film shows that many milliseconds in as a PNG, with Windows' own picture coder
static int TakeAPicture(const char *filmPath, long milliseconds, const char *outPath)
{
	if (AlreadyThere(outPath))
	{
		printf("join-films: %s is there already, and nothing is written over\n", outPath);
		return 1;
	}

	Film film;

	if (!OpenAndRead(film, filmPath, "asked-for"))
	{
		return 2;
	}

	LONGLONG const wanted = film.time + (LONGLONG)milliseconds * 10000;

	while (film.time < wanted && film.Next())
	{
	}

	if (StoppedPartway(film, "asked-for"))
	{
		return 2;
	}

	Held<IWICImagingFactory> factory;
	Held<IWICStream> file;
	Held<IWICBitmapEncoder> encoder;
	Held<IWICBitmapFrameEncode> frame;
	WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;

	if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory.it))) || FAILED(factory.it->CreateStream(&file.it))
		|| FAILED(file.it->InitializeFromFilename(Wide(outPath).c_str(), GENERIC_WRITE)) || FAILED(factory.it->CreateEncoder(GUID_ContainerFormatPng, NULL, &encoder.it))
		|| FAILED(encoder.it->Initialize(file.it, WICBitmapEncoderNoCache)) || FAILED(encoder.it->CreateNewFrame(&frame.it, NULL)) || FAILED(frame.it->Initialize(NULL))
		|| FAILED(frame.it->SetSize((UINT)film.width, (UINT)film.height)) || FAILED(frame.it->SetPixelFormat(&format))
		|| FAILED(frame.it->WritePixels((UINT)film.height, (UINT)film.width * 4, (UINT)film.picture.size(), film.picture.data())) || FAILED(frame.it->Commit()) || FAILED(encoder.it->Commit()))
	{
		printf("join-films: the picture couldn't be written\n");
		return 3;
	}

	unsigned long clock = 0;
	bool const clockKnown = JoinFilms_ReadStampedNumber(film.picture.data(), film.width * 4, film.width, film.height, &clock);

	printf("join-films: wrote the film's picture %ld, at %.4f seconds, %dx%d, to %s%s", film.number, (double)film.time / kTicksPerSecond, film.width, film.height, outPath, clockKnown ? "" : "\n");

	if (clockKnown)
	{
		printf(". Its clock says %lu\n", clock);
	}

	return 0;
}

int main(int argc, char **argv)
{
	// Each line goes out as it's written, so a join that's stopped partway still says how far it got
	setvbuf(stdout, NULL, _IONBF, 0);

	bool const picture = argc == 5 && strcmp(argv[1], "--picture") == 0;
	bool const skip = argc == 6 && strcmp(argv[1], "--skip-left") == 0;
	long milliseconds = 0;

	if (!picture && !skip && argc != 4)
	{
		printf("join-films [--skip-left <seconds>] <host film> <joiner film> <new film>\njoin-films --picture <film> <seconds> <picture.png>\n");
		return 1;
	}

	if (picture || skip)
	{
		const char *const why = JoinFilms_ReadSeconds(picture ? argv[3] : argv[2], &milliseconds);

		if (why)
		{
			printf("join-films: %s\n", why);
			return 1;
		}
	}

	if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED)) || FAILED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET)))
	{
		printf("join-films: Media Foundation couldn't start\n");
		return 2;
	}

	int const code = picture ? TakeAPicture(argv[2], milliseconds, argv[4]) : skip ? JoinTwo(argv[3], argv[4], argv[5], true, milliseconds) : JoinTwo(argv[1], argv[2], argv[3], false, 0);

	MFShutdown();
	CoUninitialize();
	return code;
}
