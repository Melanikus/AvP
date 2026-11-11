// The sound stream test. Written 2026-11-08 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// A sound stream (3dc/avp/win95/openal_stream.cpp) is sound that comes in pieces while it plays: a level's music, or a film's sound. Two
// threads use one at once. Its user writes each piece into a free buffer and hands it to OpenAL to play; the stream's own thread takes
// back each buffer OpenAL has played, so it's free again. Both change the list of free buffers. Before, its length was read in five
// places without the lock that guards it, while the other thread changed it, and C++ makes no promise about what that does.
//
// This builds the real stream code, runs its two threads for real, and checks that no buffer is ever lost, doubled or made up. It also
// reads the code, to check that nothing looks at the list without the lock, because a thread that reads at the wrong moment can be
// lucky for a long time.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with the real openal_stream.cpp, using tests/audio_stream_test.args. OpenAL and pthreads aren't there:
// tests/audio_stream_test_stubs has small stand-ins for their headers, and the functions are stand-ins below. The threads and locks
// are real ones, from C++'s own std::thread and std::mutex. No sound is played. The program gives back a non-zero result if any check
// failed.

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <chrono>
#include <deque>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "AudioStreaming.h"
#include "openal_shared.h"

static int checksFailed = 0;

static void Check(bool passed, std::string const &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());
	if (!passed)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for pthreads, made of real threads and locks

int pthread_create(pthread_t *thread, void *, void * (*start)(void *), void *argument)
{
	*thread = new std::thread([start, argument]() { start(argument); });
	return 0;
}

int pthread_join(pthread_t thread, void **)
{
	std::thread *running = static_cast<std::thread *>(thread);
	running->join();
	delete running;
	return 0;
}

int pthread_mutex_init(pthread_mutex_t *mutex, void *)
{
	*mutex = new std::mutex;
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	static_cast<std::mutex *>(*mutex)->lock();
	return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	static_cast<std::mutex *>(*mutex)->unlock();
	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	delete static_cast<std::mutex *>(*mutex);
	*mutex = 0;
	return 0;
}

// ---- A pretend OpenAL: one source, which plays every buffer the moment it's queued. It guards its own calls, as OpenAL does.

static std::mutex fakeLock;
static std::deque<ALuint> queued;     // on the source, oldest first. All of them count as played.
static ALuint nextBufferName = 1;
static long queueCalls = 0;
static long unqueueCalls = 0;
static long doubleQueued = 0;          // a buffer queued while the source already had it
static long unknownBuffer = 0;         // a buffer that isn't one OpenAL made
static bool unqueueFails = false;      // so a test can have OpenAL refuse to hand a buffer back
static float lastGain = -1.0f;
static thread_local ALenum lastError = AL_NO_ERROR;
static const ALuint kSource = 7;

ALenum alGetError(void)
{
	ALenum const error = lastError;
	lastError = AL_NO_ERROR;
	return error;
}

void alGenBuffers(ALsizei n, ALuint *buffers)
{
	std::lock_guard<std::mutex> hold(fakeLock);

	for (ALsizei i = 0; i < n; i++)
	{
		buffers[i] = nextBufferName++;
	}
}

void alDeleteBuffers(ALsizei, const ALuint *) {}
void alBufferData(ALuint, ALenum, const ALvoid *, ALsizei, ALsizei) {}
void alSourcePlay(ALuint) {}
void alSourceStop(ALuint) {}
void alSourcei(ALuint, ALenum, ALint) {}

void alSourcef(ALuint, ALenum parameter, ALfloat value)
{
	if (parameter == AL_GAIN)
	{
		lastGain = value;
	}
}

void alSourceQueueBuffers(ALuint, ALsizei n, const ALuint *buffers)
{
	std::lock_guard<std::mutex> hold(fakeLock);

	for (ALsizei i = 0; i < n; i++)
	{
		if (std::find(queued.begin(), queued.end(), buffers[i]) != queued.end())
		{
			doubleQueued++;
		}

		if (buffers[i] == 0 || buffers[i] >= nextBufferName)
		{
			unknownBuffer++;
		}

		queued.push_back(buffers[i]);
		queueCalls++;
	}
}

void alSourceUnqueueBuffers(ALuint, ALsizei n, ALuint *buffers)
{
	std::lock_guard<std::mutex> hold(fakeLock);

	if (unqueueFails || (ALsizei)queued.size() < n)
	{
		lastError = AL_INVALID_VALUE;
		return;
	}

	for (ALsizei i = 0; i < n; i++)
	{
		buffers[i] = queued.front();
		queued.pop_front();
		unqueueCalls++;
	}
}

void alGetSourcei(ALuint, ALenum parameter, ALint *value)
{
	std::lock_guard<std::mutex> hold(fakeLock);

	if (parameter == AL_BUFFERS_PROCESSED)
	{
		*value = (ALint)queued.size();
	}
}

// ---- Stand-ins for the rest of the game

static bool soundOn = true;
static sndSource theSource;
static int errorsPrinted = 0;

void Con_PrintError(const std::string &)
{
	errorsPrinted++;
}

bool OpenAL_SoundIsOn()
{
	return soundOn;
}

sndSource *OpenAL_FindFreeSource()
{
	theSource._alSource = kSource;
	theSource._isFree = false;
	return &theSource;
}

float OpenAL_StreamGain(uint32_t volume)
{
	return (volume > 127 ? 127 : volume) / 127.0f;
}

ALenum AlCheckError()
{
	return alGetError();
}

// ---- The tests

static uint32_t const kBytes = 4096;   // one piece, and one buffer: 1,024 samples of 16-bit stereo
static uint32_t const kBuffers = 3;    // as the music uses

static void Reset()
{
	std::lock_guard<std::mutex> hold(fakeLock);

	queued.clear();
	nextBufferName = 1;
	queueCalls = unqueueCalls = doubleQueued = unknownBuffer = 0;
	unqueueFails = false;
	lastGain = -1.0f;
	soundOn = true;
	errorsPrinted = 0;
}

// Waits up to the time given for a stream to say it has this many free buffers
static bool WaitForFree(AudioStream &stream, uint32_t count, int milliseconds)
{
	auto const giveUp = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);

	while (stream.GetNumFreeBuffers() != count)
	{
		if (std::chrono::steady_clock::now() > giveUp)
		{
			return false;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	return true;
}

static void TestTwoThreads()
{
	puts("1. the stream's two threads at once");

	Reset();
	AudioStream *stream = new AudioStream;
	Check(stream->Init(2, 44100, 16, kBytes, kBuffers), "a stream starts, with its own thread");

	// The stream's user: a thread of its own, as the music's is, writing a piece whenever a buffer is free, as fast as it can. The
	// stream's thread takes buffers back just as fast, because the pretend source plays each one the moment it has it.
	int const kPieces = 1500;
	std::vector<uint8_t> piece(kBytes, 0x40);
	int written = 0;
	bool tookTooLong = false;
	uint32_t mostFree = 0;
	auto const giveUp = std::chrono::steady_clock::now() + std::chrono::seconds(60);

	std::thread user([&]() {
		while (written < kPieces)
		{
			if (std::chrono::steady_clock::now() > giveUp)
			{
				tookTooLong = true;
				return;
			}

			uint32_t const free = stream->GetNumFreeBuffers();
			mostFree = (std::max)(mostFree, free);

			if (free == 0)
			{
				std::this_thread::yield();
				continue;
			}

			stream->WriteData(piece.data(), kBytes);
			written++;
		}
	});

	user.join();

	Check(!tookTooLong && written == kPieces, "all 1,500 pieces were written, in under a minute: the user was never left waiting for a buffer that never came back");
	Check(WaitForFree(*stream, kBuffers, 5000), "once all of them were played, all 3 buffers were free again: none was lost");
	Check(mostFree <= kBuffers, "the stream never said more buffers were free than it has");

	{
		std::lock_guard<std::mutex> hold(fakeLock);

		Check(queueCalls == kPieces && unqueueCalls == kPieces, "OpenAL was given every piece once, and gave every buffer back once");
		Check(doubleQueued == 0, "no buffer was given to OpenAL while it already had it");
		Check(unknownBuffer == 0, "and none that isn't one of the stream's");
		Check(queued.empty(), "and nothing is left on the source");
	}

	uint64_t const samples = (uint64_t)kPieces * kBytes / 2 / 2;
	Check(stream->GetNumSamplesPlayed() == samples, "every piece is counted as played");

	delete stream;
	Check(theSource._isFree, "letting go of the stream stops its thread and frees its source");
}

static void TestNotStarted()
{
	puts("2. a stream that didn't start");

	Reset();
	soundOn = false;
	AudioStream *stream = new AudioStream;

	Check(!stream->Init(2, 44100, 16, kBytes, kBuffers), "with no sound, a stream doesn't start");
	Check(stream->GetNumFreeBuffers() == 0, "it has no free buffers, and no lock is touched, because none was made");
	Check(stream->GetNumSamplesPlayed() == 0, "it has played nothing, which is 0, not a division by 0");

	ALuint buffer = 99;
	Check(!stream->TakeFreeBuffer(&buffer) && buffer == 99, "and there's no buffer to take");

	stream->WaitForFreeBuffer();
	Check(true, "waiting for a free buffer on it ends at once");

	delete stream;
	Check(true, "and it can be let go of");
}

static void TestVolume()
{
	puts("3. the volume");

	Reset();
	AudioStream *stream = new AudioStream;
	stream->Init(2, 44100, 16, kBytes, kBuffers);

	stream->SetVolume(64);
	Check(stream->GetVolume() == 64, "the volume set is the volume the stream says it has");
	Check(lastGain > 0.50f && lastGain < 0.51f, "and OpenAL is given it as a gain");

	delete stream;
}

static void TestBufferNotHandedBack()
{
	puts("4. a buffer OpenAL couldn't hand back");

	Reset();
	AudioStream *stream = new AudioStream;
	stream->Init(2, 44100, 16, kBytes, kBuffers);

	{
		std::lock_guard<std::mutex> hold(fakeLock);
		unqueueFails = true;
	}

	std::vector<uint8_t> piece(kBytes, 0x40);
	stream->WriteData(piece.data(), kBytes);

	// The stream's thread sees a played buffer, asks for it, and is refused, over and over, for a fifth of a second
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	Check(stream->GetNumFreeBuffers() == kBuffers - 1, "while OpenAL won't hand it back, it isn't counted as free, and no made-up buffer takes its place");

	{
		std::lock_guard<std::mutex> hold(fakeLock);
		unqueueFails = false;
	}

	Check(WaitForFree(*stream, kBuffers, 5000), "once OpenAL hands it back, it's free again");

	// Every free buffer is one of the stream's own, and each is there once
	std::vector<ALuint> free;
	ALuint buffer = 0;
	while (stream->TakeFreeBuffer(&buffer))
	{
		free.push_back(buffer);
	}

	std::sort(free.begin(), free.end());
	Check(free.size() == kBuffers && free[0] >= 1 && free.back() < nextBufferName && std::adjacent_find(free.begin(), free.end()) == free.end(), "and the free buffers are the stream's own 3, each once");

	delete stream;
}

// ---- Reading the code

static std::string ReadSourceFile(char const *name)
{
	std::ifstream file(name, std::ios::binary);
	std::stringstream text;
	text << file.rdbuf();

	std::string result = text.str();
	std::string::size_type at = 0;

	while ((at = result.find("\r\n", at)) != std::string::npos)
	{
		result.erase(at, 1);
	}

	return result;
}

// The function whose first line starts with this, up to its closing brace
static std::string FunctionStartingWith(std::string const &source, char const *start)
{
	std::string::size_type const at = source.find(start);

	if (at == std::string::npos)
	{
		return "";
	}

	std::string::size_type const open = source.find('{', at);
	int depth = 0;

	for (std::string::size_type i = open; i != std::string::npos && i < source.size(); i++)
	{
		depth += (source[i] == '{') ? 1 : (source[i] == '}') ? -1 : 0;

		if (depth == 0)
		{
			return source.substr(at, i + 1 - at);
		}
	}

	return "";
}

static int CountOf(std::string const &text, char const *what)
{
	int count = 0;

	for (std::string::size_type at = text.find(what); at != std::string::npos; at = text.find(what, at + 1))
	{
		count++;
	}

	return count;
}

static void TestTheCode()
{
	puts("5. the code, read");

	std::string const code = ReadSourceFile("3dc/avp/win95/openal_stream.cpp");
	Check(!code.empty(), "openal_stream.cpp can be read");

	int inTheThree = 0;

	for (char const *start : { "bool AudioStream::TakeFreeBuffer(", "void AudioStream::GiveBackBuffer(", "uint32_t AudioStream::CountFreeBuffers(" })
	{
		std::string const body = FunctionStartingWith(code, start);
		std::string::size_type const lock = body.find("pthread_mutex_lock(&_bufferMutex);");
		std::string::size_type const unlock = body.rfind("pthread_mutex_unlock(&_bufferMutex);");
		std::string::size_type const first = body.find("_freeBufferQueue");
		std::string::size_type const last = body.rfind("_freeBufferQueue");

		Check(lock != std::string::npos && unlock != std::string::npos && first != std::string::npos && lock < first && last < unlock,
			std::string(start) + "...) looks at the list of free buffers only with the lock held");

		inTheThree += CountOf(body, "_freeBufferQueue");
	}

	Check(inTheThree > 0 && CountOf(code, "_freeBufferQueue") == inTheThree, "and nothing else in the stream's code looks at the list at all");

	std::string const header = ReadSourceFile("3dc/AudioStreaming.h");
	Check(header.find("std::atomic<bool> _isPlaying;") != std::string::npos && header.find("std::atomic<bool> _isPaused;") != std::string::npos,
		"whether a stream is playing, and whether it's paused, can be read by both threads at once");
	Check(header.find("std::atomic<uint64_t> _totalBytesPlayed;") != std::string::npos, "and so can how much it has played");

	std::string::size_type const list = header.find("std::deque<ALuint> _freeBufferQueue;");
	std::string::size_type const opensUp = header.find("public:", header.find("class AudioStream"));
	Check(list != std::string::npos && opensUp != std::string::npos && list < opensUp, "the list itself is private, so nothing outside the stream can reach it");
}

int main()
{
	TestTwoThreads();
	TestNotStarted();
	TestVolume();
	TestBufferNotHandedBack();
	TestTheCode();

	if (checksFailed)
	{
		printf("The sound stream test FAILED: %d check(s)\n", checksFailed);
		return 1;
	}

	puts("The sound stream test passed");
	return 0;
}
