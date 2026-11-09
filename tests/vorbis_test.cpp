// The music test. Written 2026-11-03 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// The game's Ogg Vorbis music comes from files, and a file can be damaged. VorbisPlayback reads a track with ov_read, from
// the libvorbis library, and that call answers with a number: bytes read, 0 at the end of the file, or a negative error.
// The player used to add every answer to its running total, errors included, and it would read a file that never gave
// any audio for ever. This test runs the real VorbisPlayer.cpp and hands it the answers a damaged file gives.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file together with the real VorbisPlayer.cpp, using the settings in tests/vorbis_test.args.
// libvorbis, OpenAL and pthreads aren't there: the headers the player needs from them are small stand-ins in
// tests/vorbis_test_stubs, and the functions are stand-ins below. So no music is played and no file is decoded: ov_read
// below simply gives the answers each test lists. The program gives back a non-zero result if any check failed.
//
// Modified 2026-11-08 by Melanikus for the AvP port:
//   a track ends at the end of its file instead of starting again, and finishes once all of it has been heard, so a level's music
//   goes on to its next track (#94, TRACKS-1); and asking for a track with none found reads nothing

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <initializer_list>
#include <string>
#include <vector>

#include "VorbisPlayer.h"

static int checksFailed = 0;

static void Check(bool passed, char const * what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);
	if (!passed)
	{
		checksFailed++;
	}
}

// What the stand-ins below count, so the tests can see what the player did.
static std::vector<long> answers;        // what ov_read gives, in order. The last one repeats.
static size_t nextAnswer = 0;
static int readCalls = 0;
static int seekCalls = 0;
static int writeCalls = 0;
static int errorsPrinted = 0;
static int threadLoops = 0;
static int loopsAllowed = 0;
static VorbisPlayback * playerInThread = 0;
static uint32_t buffersFree = 1;          // what the sound output says about its buffers: 3 of 3 free means all of it has been heard
static void * threadArgument = 0;          // the player a started music thread was given, so a test can run that thread itself

// ---- Stand-ins for libvorbis

ov_callbacks OV_CALLBACKS_DEFAULT;
static vorbis_info theInfo = { 2, 44100 };

// The real library takes charge of the file once it has opened it, and closes it in ov_clear. These two do the same.
static FILE * fileInLibrary = 0;

int ov_open_callbacks(void * datasource, OggVorbis_File *, char const *, long, ov_callbacks)
{
	fileInLibrary = (FILE *)datasource;
	return 0;
}

int ov_clear(OggVorbis_File *)
{
	if (fileInLibrary)
	{
		fclose(fileInLibrary);
		fileInLibrary = 0;
	}

	return 0;
}

vorbis_info * ov_info(OggVorbis_File *, int) { return &theInfo; }

int ov_raw_seek(OggVorbis_File *, long long)
{
	seekCalls++;
	return 0;
}

long ov_read(OggVorbis_File *, char *, int length, int, int, int, int *)
{
	readCalls++;

	// A player that never stops reading would hang the test, so this ends it with a failure instead.
	if (readCalls > 100000)
	{
		puts("  FAILED  the player kept on reading for ever");
		exit(1);
	}

	long const answer = answers[nextAnswer < answers.size() ? nextAnswer : answers.size() - 1];
	if (nextAnswer < answers.size())
	{
		nextAnswer++;
	}

	return answer > length ? length : answer;
}

// ---- Stand-ins for the sound output

AudioStream::AudioStream() {}
AudioStream::~AudioStream() {}
bool AudioStream::Init(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) { return true; }
int32_t AudioStream::Play() { return AUDIOSTREAM_OK; }
int32_t AudioStream::Stop() { return 0; }
int32_t AudioStream::SetVolume(uint32_t) { return 0; }
uint32_t AudioStream::GetVolume() { return 0; }

uint32_t AudioStream::WriteData(uint8_t *, uint32_t)
{
	writeCalls++;
	return 0;
}

// The update thread asks this once on each trip round its loop, so this is where a test stops the loop.
uint32_t AudioStream::GetNumFreeBuffers()
{
	if (++threadLoops >= loopsAllowed && playerInThread)
	{
		playerInThread->_isPlaying = false;
	}

	return buffersFree;
}

// ---- Stand-ins for the rest of the game

int musicVolume = 0;

// The tests run the update loop themselves. This keeps what the loop would have been given, so a test can run it on the player
// LoadVorbisTrack made.
int pthread_create(pthread_t *, void *, void * (*)(void *), void * argument)
{
	threadArgument = argument;
	return 0;
}
int pthread_join(pthread_t, void **) { return 0; }

void LogErrorString(std::string const &, int, char const *) {}
void LogErrorString(std::string const &) {}
void LogString(std::string const &) {}

void Con_PrintError(std::string const &)
{
	errorsPrinted++;
}

// ---- The tests

// The size of the player's buffer, in VorbisPlayer.cpp. A read asks for this many bytes.
static uint32_t const BufferBytes = 32768;

// Sets the answers ov_read will give, and clears the counts.
static void Answers(std::initializer_list<long> list)
{
	answers.assign(list);
	nextAnswer = 0;
	readCalls = seekCalls = writeCalls = errorsPrinted = 0;
	threadLoops = loopsAllowed = 0;
	playerInThread = 0;
	buffersFree = 1;
	threadArgument = 0;
}

// The player's update loop. VorbisPlayer.cpp doesn't declare it in its header, because only it starts the thread.
void * VorbisUpdateThread(void * argument);

// Runs the player's real update loop, on this thread, for this many trips round.
static void RunUpdateThread(VorbisPlayback & player, int trips)
{
	player._isPlaying = true;
	playerInThread = &player;
	threadLoops = 0;
	loopsAllowed = trips;

	VorbisUpdateThread(&player);

	playerInThread = 0;
}

static void TestOrdinaryFile()
{
	puts("1. an ordinary file");

	Answers({ 4096 });
	VorbisPlayback player;
	player._audioData = new uint8_t[BufferBytes];

	Check(player.GetVorbisData(BufferBytes) == BufferBytes, "the buffer is filled");
	Check(!player._readFailed, "no failure is remembered");
	Check(readCalls == 8, "it took 8 reads of 4096 bytes");
	Check(seekCalls == 0, "and it never went back to the start");
}

static void TestDecoderError()
{
	puts("2. a decoder error");

	{
		Answers({ OV_EBADLINK });
		VorbisPlayback player;
		player._audioData = new uint8_t[BufferBytes];

		Check(player.GetVorbisData(BufferBytes) == 0, "an error on the first read adds nothing to the total");
		Check(player._readFailed, "the failure is remembered");
		Check(readCalls == 1, "and reading stops at once");
	}

	{
		Answers({ 1000, OV_EBADLINK });
		VorbisPlayback player;
		player._audioData = new uint8_t[BufferBytes];

		Check(player.GetVorbisData(BufferBytes) == 1000, "after some data the total holds only the real bytes");
		Check(player._readFailed, "and the failure is remembered");
	}
}

static void TestUpdateThread()
{
	puts("3. the update thread after a failure");

	{
		Answers({ OV_EBADLINK });
		VorbisPlayback player;
		player._audioData = new uint8_t[BufferBytes];
		player._audioStream = new AudioStream;
		player.GetVorbisData(BufferBytes);

		RunUpdateThread(player, 5);

		Check(threadLoops == 5, "the loop carries on running, so what is already queued can play out");
		Check(readCalls == 1, "it never reads the file again");
		Check(writeCalls == 0, "and never writes to the sound output");
	}

	{
		Answers({ 40000, OV_EBADLINK });
		VorbisPlayback player;
		player._audioData = new uint8_t[BufferBytes];
		player._audioStream = new AudioStream;

		RunUpdateThread(player, 5);

		Check(writeCalls == 1, "a failure inside the loop: the good buffer was written and the bad one wasn't");
		Check(readCalls == 2, "and there were no more reads");
		Check(player._readFailed, "and the failure is remembered");
	}
}

static void TestGap()
{
	puts("4. a gap in the data");

	Answers({ 1000, OV_HOLE, 40000 });
	VorbisPlayback player;
	player._audioData = new uint8_t[BufferBytes];

	Check(player.GetVorbisData(BufferBytes) == BufferBytes, "reading carries on after the gap");
	Check(!player._readFailed, "a gap is not a failure");
	Check(readCalls == 3, "and the gap added nothing to the total");
}

static void TestNoAudio()
{
	puts("5. a file with no audio");

	{
		Answers({ 0 });
		VorbisPlayback player;
		player._audioData = new uint8_t[BufferBytes];

		Check(player.GetVorbisData(BufferBytes) == 0, "nothing is read");
		Check(player._readFailed, "the file counts as failed, so the player doesn't read it for ever");
		Check(readCalls == 1, "it reached the end once, with no audio before it");
		Check(seekCalls == 0, "and never went back to the start");
		Check(!player._reachedEnd, "an empty file isn't a track that has ended");
	}

	{
		Answers({ OV_HOLE, 0, OV_HOLE, 0 });
		VorbisPlayback player;
		player._audioData = new uint8_t[BufferBytes];

		Check(player.GetVorbisData(BufferBytes) == 0, "a file that is nothing but gaps gives nothing");
		Check(player._readFailed, "and also counts as failed");
		Check(seekCalls == 0, "and it never went back to the start");
	}
}

// #94: a track used to start again at the end of its file, so a level's music played its first track for ever
static void TestShortTrack()
{
	puts("6. tracks that really do reach the end");

	{
		Answers({ 10000, 0, 10000 });
		VorbisPlayback player;
		player._audioData = new uint8_t[BufferBytes];

		// ov_read here gives counts and writes nothing, so whatever was in the buffer before stays, unless the player clears it
		memset(player._audioData, 0xAB, BufferBytes);

		Check(player.GetVorbisData(BufferBytes) == 10000, "a track shorter than the buffer gives only its own audio");
		Check(player._reachedEnd, "and its end is remembered");
		Check(!player._readFailed, "that is not a failure");
		Check(seekCalls == 0, "it never goes back to the start, so the track isn't played again");
		Check(readCalls == 2, "and it stops reading at the end");

		bool restIsSilence = true;
		for (uint32_t i = 10000; i < BufferBytes; i++)
		{
			restIsSilence = restIsSilence && player._audioData[i] == 0;
		}
		Check(restIsSilence, "the rest of the buffer, after the end, is silence");
		Check(player._audioData[9999] == 0xAB, "and the track's own part is left as it came");
	}

	{
		Answers({ 40000, 0, 40000 });
		VorbisPlayback player;
		player._audioData = new uint8_t[BufferBytes];

		Check(player.GetVorbisData(BufferBytes) == BufferBytes, "a full buffer first");
		Check(!player._reachedEnd, "with the end not reached yet");
		Check(player.GetVorbisData(BufferBytes) == 0, "the end of the file right at the start of the next call gives nothing more");
		Check(player._reachedEnd && !player._readFailed, "and the track has ended, which is not a failure");
		Check(seekCalls == 0, "and it never went back to the start");
	}
}

// After the end, the music thread reads nothing more, lets what is queued play out, and says the track has finished only once all of
// it has been heard: when all 3 of the sound output's buffers have come back
static void TestTrackFinishes()
{
	puts("8. a track that has ended finishes");

	{
		Answers({ 40000, 40000, 10000, 0 });
		VorbisPlayback player;
		player._audioData = new uint8_t[BufferBytes];
		player._audioStream = new AudioStream;

		RunUpdateThread(player, 8);

		Check(writeCalls == 3, "two full buffers and the last part of the track go to the sound output");
		Check(readCalls == 4, "nothing is read after the end");
		Check(seekCalls == 0, "and it never went back to the start");
		Check(!player._finished, "with 1 buffer of 3 back, some of it hasn't been heard yet, so it hasn't finished");
		Check(threadLoops == 8, "and the thread waits");

		buffersFree = 3;
		RunUpdateThread(player, 8);

		Check(player._finished, "once all 3 buffers are back, all of it has been heard, and the track has finished");
		Check(threadLoops == 1, "and the thread stops at once");
		Check(writeCalls == 3, "with nothing more written");
	}

	{
		Answers({ 40000, OV_EBADLINK });
		VorbisPlayback player;
		player._audioData = new uint8_t[BufferBytes];
		player._audioStream = new AudioStream;
		buffersFree = 3;

		RunUpdateThread(player, 8);

		Check(player._readFailed && player._finished, "a track that can't be read any more finishes too, once what was sent has been heard");
		Check(readCalls == 2, "and it isn't read again");
	}
}

// What VorbisPlayer.cpp keeps of ogg_tracks.txt: one file name a line, the first one track 1
extern std::vector<std::string> TrackList;

static void TestTrackList()
{
	puts("9. asking for a track");

	Answers({ 40000 });
	TrackList.clear();

	LoadVorbisTrack(0);
	LoadVorbisTrack(1);
	Check(readCalls == 0 && errorsPrinted == 0, "with no tracks found, asking for track 0 or 1 opens nothing and reads nothing");
	Check(!IsVorbisPlaying(), "and nothing is playing");

	char const * const aTrack = "vorbis_test_track.tmp";
	FILE * madeHere = fopen(aTrack, "wb");
	Check(madeHere != 0, "the test can make its track file");
	if (madeHere)
	{
		fclose(madeHere);
	}

	TrackList.push_back(aTrack);

	LoadVorbisTrack(2);
	Check(readCalls == 0, "with one track, asking for track 2 opens nothing");

	LoadVorbisTrack(1);
	Check(readCalls == 1 && IsVorbisPlaying(), "track 1 is the first name in the list, and it plays");
	Check(threadArgument != 0, "its music thread was started");

	// The track ends, and all of it is heard. Its music thread is run here, on the player LoadVorbisTrack made, for at most 8 trips,
	// so a player that never finished would stop the test rather than hang it. The player is kept before Answers clears what was kept.
	VorbisPlayback * playing = static_cast<VorbisPlayback *>(threadArgument);
	Answers({ 0 });
	buffersFree = 3;
	if (playing)
	{
		playing->GetVorbisData(BufferBytes);
		RunUpdateThread(*playing, 8);
	}

	Check(playing != 0 && playing->_reachedEnd && playing->_finished, "at the end of its file the track finishes");
	Check(!IsVorbisPlaying(), "and then it isn't playing, so the game picks the level's next track (Music_Check, MusicPlayer.cpp)");

	Vorbis_CloseSystem();
	TrackList.clear();
	Check(remove(aTrack) == 0, "the track's file was closed, so the test can remove it");
}

// Open needs a file that really exists, though ov_open_callbacks above never reads it. So the test makes an empty one of its
// own, and removes it at the end.
static void TestOpen()
{
	puts("7. opening a track");

	char const * const aRealFile = "vorbis_test_file.tmp";
	FILE * madeHere = fopen(aRealFile, "wb");
	Check(madeHere != 0, "the test can make its file");
	if (madeHere)
	{
		fclose(madeHere);
	}

	{
		Answers({ OV_EBADLINK });
		VorbisPlayback * player = new VorbisPlayback;

		Check(!player->Open(aRealFile), "a file whose first read fails is refused");
		Check(errorsPrinted == 1, "with a message on the console");
		Check(writeCalls == 0, "nothing goes to the sound output");
		Check(!player->_isPlaying, "and it isn't playing");
		delete player;
	}

	{
		Answers({ 0 });
		VorbisPlayback * player = new VorbisPlayback;

		Check(!player->Open(aRealFile), "a file with no audio is refused");
		Check(writeCalls == 0, "and nothing goes to the sound output");
		delete player;
	}

	{
		Answers({ 40000 });
		VorbisPlayback * player = new VorbisPlayback;

		Check(player->Open(aRealFile), "a good file opens");
		Check(writeCalls == 1, "its first buffer goes to the sound output");
		Check(player->_isPlaying, "and it is playing");
		player->_isPlaying = false;
		delete player;
	}

	Check(remove(aRealFile) == 0, "the file was closed every time, so the test can remove it");
}

int main()
{
	TestOrdinaryFile();
	TestDecoderError();
	TestUpdateThread();
	TestGap();
	TestNoAudio();
	TestShortTrack();
	TestOpen();
	TestTrackFinishes();
	TestTrackList();

	if (checksFailed)
	{
		printf("The music test FAILED: %d check(s)\n", checksFailed);
		return 1;
	}

	puts("The music test passed");
	return 0;
}
