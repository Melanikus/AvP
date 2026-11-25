// The test for A 3D SOUND WITH NOBODY TO HEAR IT. Written 2026-11-25 by Melanikus (Vasquez) for the AvP port (the card of 2026-11-24 17:05 in
// AvP-docs/director/cards-after-alpha5.md, Tao 1).
//
// WHAT IT TESTS
//
// A 3D sound is one that comes from a place in the world: Sound_Play with 'd' (a position) or 'n' (a position and its ranges) in its format
// (3dc/avp/psnd.cpp). How loud it is depends on how far it is from the one who hears it, the listener, and the game's listener is its view:
// Global_VDB_Ptr. 3dc/avp/win95/openal_audio.cpp's PlatDo3dSound works out that distance, when the sound starts (PlatPlaySound), on every
// frame (SoundSys_Management) and when its volume changes (Sound_ChangeVolume). Global_VDB_Ptr starts at 0 and is set when a level makes
// its view, so before that there is no listener. PlatDo3dSound read it with no check: a 3D sound played before a level (on a start screen,
// in a test, from a new tool) read memory at address 0 and stopped the program. Every real path today sets the view first, but that rested
// on the order of five files, not on a check where it is read. Now PlatDo3dSound refuses a 3D sound while there is no listener, with
// SOUND_PLATFORMERROR, and PlatPlaySound stops it and gives its source back, as it does for any sound that can't play.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this with the game's own OpenAL sound code (openal_audio.cpp) and its file reading (FileStream.cpp), using
// tests/sound_no_listener_test.args. OpenAL isn't there: tests/sound_no_listener_test_stubs has small stand-ins for its headers, and its
// functions are stand-ins below, a pretend sound card that hands out sources and buffers and remembers which source is playing. The sound
// is a WAV of the test's own (a quarter of a second of a tone), written to the machine's temporary folder and read by the game's own loader
// (LoadWavFile). No sound is played. The program gives back a non-zero result if any check failed.
// psnd.cpp, the part above it, isn't built here: it reads the menus' headers, and they the renderer's. So the test fills a sound's slot
// the way Sound_Play does (psnd.cpp, "fill out the active sound") and calls openal_audio.cpp as Sound_Play, SoundSys_Management and
// Sound_ChangeVolume do.
//
// RED before the fix: the first 3D sound with no listener stopped the program in PlatDo3dSound (an access violation reading address 0's
// view), which the test catches and reports as a failed check.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "psnd.h"
#include "psndplat.h"
#include "FastFile.h"

#include <AL/al.h>
#include <AL/alc.h>

// openal_audio.cpp's, the parts this test calls
extern int LoadWavFile(int soundNum, const std::string &fileName);
extern std::vector<sndSource> sourceList;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok    " : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- The listener: the game's view (view.cpp has it; 0 until a level makes its view)
VIEWDESCRIPTORBLOCK *Global_VDB_Ptr = nullptr;
static VIEWDESCRIPTORBLOCK testView = {};

// ---- Stand-ins for OpenAL: a pretend sound card. Each source remembers whether it is playing, paused or stopped.
static int const kStandInSources = 32;
static ALint sourceState[kStandInSources + 1];   // by source name, 1 to kStandInSources; 0 is no source
static ALuint sourcesMade = 0, buffersMade = 0;
static ALenum pendingError = AL_NO_ERROR;
static char standInDevice = 0, standInContext = 0;

static bool IsStandInSource(ALuint source)
{
	return source >= 1 && source <= sourcesMade;
}

ALenum alGetError(void)
{
	ALenum const error = pendingError;
	pendingError = AL_NO_ERROR;
	return error;
}

void *alGetProcAddress(const ALchar *)
{
	return nullptr;
}

ALenum alGetEnumValue(const ALchar *)
{
	return 0;
}

void alDistanceModel(ALenum)
{
}

void alListenerf(ALenum, ALfloat)
{
}

void alListenerfv(ALenum, const ALfloat *)
{
}

void alGenSources(ALsizei n, ALuint *sources)
{
	for (ALsizei i = 0; i < n; i++)
	{
		if (sourcesMade >= (ALuint)kStandInSources)
		{
			pendingError = AL_OUT_OF_MEMORY;   // as a sound card with no more sources says
			return;
		}

		sourcesMade++;
		sourceState[sourcesMade] = AL_STOPPED;
		sources[i] = sourcesMade;
	}
}

void alDeleteSources(ALsizei, const ALuint *)
{
}

void alSourcef(ALuint, ALenum, ALfloat)
{
}

void alSource3f(ALuint, ALenum, ALfloat, ALfloat, ALfloat)
{
}

void alSourcefv(ALuint, ALenum, const ALfloat *)
{
}

void alSourcei(ALuint, ALenum, ALint)
{
}

void alSource3i(ALuint, ALenum, ALint, ALint, ALint)
{
}

void alGetSourcei(ALuint source, ALenum param, ALint *value)
{
	if (param == AL_SOURCE_STATE && IsStandInSource(source))
	{
		*value = sourceState[source];
		return;
	}

	*value = 0;
}

void alGetSourceiv(ALuint source, ALenum param, ALint *values)
{
	alGetSourcei(source, param, values);
}

void alSourcePlay(ALuint source)
{
	if (IsStandInSource(source))
	{
		sourceState[source] = AL_PLAYING;
	}
}

void alSourceStop(ALuint source)
{
	if (IsStandInSource(source))
	{
		sourceState[source] = AL_STOPPED;
	}
}

void alSourcePause(ALuint source)
{
	if (IsStandInSource(source))
	{
		sourceState[source] = AL_PAUSED;
	}
}

void alGenBuffers(ALsizei n, ALuint *buffers)
{
	for (ALsizei i = 0; i < n; i++)
	{
		buffers[i] = ++buffersMade;
	}
}

void alDeleteBuffers(ALsizei, const ALuint *)
{
}

void alBufferData(ALuint, ALenum, const ALvoid *, ALsizei, ALsizei)
{
}

const ALCchar *alcGetString(ALCdevice *, ALCenum param)
{
	// the device list is names one after another, each ended by a 0, and the list by another 0
	return param == ALC_DEVICE_SPECIFIER ? "stand-in sound card\0" : "stand-in sound card";
}

ALCdevice *alcOpenDevice(const ALCchar *)
{
	return reinterpret_cast<ALCdevice *>(&standInDevice);
}

ALCboolean alcCloseDevice(ALCdevice *)
{
	return 1;
}

ALCcontext *alcCreateContext(ALCdevice *, const ALCint *)
{
	return reinterpret_cast<ALCcontext *>(&standInContext);
}

ALCboolean alcMakeContextCurrent(ALCcontext *)
{
	return 1;
}

void alcDestroyContext(ALCcontext *)
{
}

ALCboolean alcIsExtensionPresent(ALCdevice *, const ALCchar *)
{
	return 0;   // no room echo (ALC_EXT_EFX): openal_audio.cpp then asks for none of its functions
}

void alcGetIntegerv(ALCdevice *, ALCenum, ALCsizei size, ALCint *values)
{
	if (size > 0 && values)
	{
		values[0] = kStandInSources;
	}
}

// ---- Stand-ins for the rest of the game the sound code names but this test never reaches (psnd.cpp's switch, the player, the game's
// settings, the console, the log, the settings file, the fast files). The sound code itself (openal_audio.cpp), its file reading
// (FileStream.cpp) and its fixed-point maths (mathline.cpp) are the game's own.
DISPLAYBLOCK *Player = nullptr;
AVP_GAME_DESC AvP = {};
int SoundSwitchedOn = 1;   // psnd.cpp's: the sound system is on

int Magnitude(VECTORCH *v)   // plspecfn.cpp's, the same sum: how far a point is from 0
{
	double const x = v->vx, y = v->vy, z = v->vz;
	return (int)(sqrt(x * x + y * y + z * z) + 0.5);
}

void Con_PrintError(const std::string &text)
{
	printf("  (game error) %s\n", text.c_str());
}

void Con_PrintMessage(const std::string &)
{
}

void Con_PrintDebugMessage(const std::string &)
{
}

void LogString(const std::string &)
{
}

bool Config_Save()
{
	return true;
}

int Config_GetInt(const std::string &, const std::string &, int defaultValue)
{
	return defaultValue;
}

bool Config_GetBool(const std::string &, const std::string &, bool defaultValue)
{
	return defaultValue;
}

void Config_SetBool(const std::string &, const std::string &, bool)
{
}

// FastFile.cpp's: the game's archives of files. None is open here, so no file is in one, and FileStream reads the WAV from the disk.
FastFileHandle *FF_Find(const std::string &)
{
	return nullptr;
}

const std::string &FF_GetFastFileName(int32_t)
{
	static std::string const none;
	return none;
}

// What a sound that can't play becomes, from its source's point of view: how many of the pretend card's sources are playing now
static int SourcesPlaying()
{
	int playing = 0;

	for (ALuint source = 1; source <= sourcesMade; source++)
	{
		if (sourceState[source] == AL_PLAYING)
		{
			playing++;
		}
	}

	return playing;
}

// How many of openal_audio.cpp's sources are handed out (not free)
static int SourcesHandedOut()
{
	int handedOut = 0;

	for (sndSource const &source : sourceList)
	{
		if (!source._isFree)
		{
			handedOut++;
		}
	}

	return handedOut;
}

// A sound in slot 0, as Sound_Play fills it (psnd.cpp, "fill out the active sound"): the game sound, its volume and pitch, and for a 3D
// one ('d') its place in the world, with the ranges Sound_Play gives it
static int const kSlot = 0;

static void FillSlot(SOUNDINDEX sound, VECTORCH const *where)
{
	ActiveSounds[kSlot] = BlankActiveSound;
	ActiveSounds[kSlot].soundIndex = sound;
	ActiveSounds[kSlot].priority = ASP_Minimum;
	ActiveSounds[kSlot].volume = GameSounds[sound].volume;
	ActiveSounds[kSlot].pitch = GameSounds[sound].pitch;

	if (where)
	{
		VECTORCH const zeroPosn = {0, 0, 0};
		ActiveSounds[kSlot].threedeedata.position = *where;
		ActiveSounds[kSlot].threedeedata.velocity = zeroPosn;
		ActiveSounds[kSlot].threedeedata.inner_range = 0;
		ActiveSounds[kSlot].threedeedata.outer_range = 32000;
		ActiveSounds[kSlot].threedee = 1;
	}
	else
	{
		ActiveSounds[kSlot].threedee = 0;
	}
}

// The game's code, run so that a read of address 0 is reported rather than stopping the program. Windows' own structured exceptions
// (__try / __except) catch the access violation; a function that uses them can hold nothing with a destructor, so each is one plain call.
static bool PlayCaught(int *result)
{
	__try
	{
		*result = PlatPlaySound(kSlot);
	}
	__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		return false;
	}

	return true;
}

static bool Do3dCaught(int *result)
{
	__try
	{
		*result = PlatDo3dSound(kSlot);
	}
	__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		return false;
	}

	return true;
}

// The sound in the slot stopped and the slot emptied, as Sound_Stop does
static void EmptySlot()
{
	PlatStopSound(kSlot);
	ActiveSounds[kSlot] = BlankActiveSound;
}

// psnd.cpp's, which PlatEndSoundSys calls first: every sound stopped and let go
void SoundSys_RemoveAll(void)
{
	for (int i = 0; i < SOUND_MAXACTIVE; i++)
	{
		if (ActiveSounds[i].soundIndex != SID_NOSOUND)
		{
			PlatStopSound(i);
			ActiveSounds[i] = BlankActiveSound;
		}
	}
}

// A WAV of our own: 22050 Hz, 16-bit, mono, a quarter of a second of a 440 Hz tone
static bool WriteTone(std::string const &path)
{
	int const rate = 22050, samples = rate / 4;
	std::vector<short> pcm((size_t)samples);

	for (int i = 0; i < samples; i++)
	{
		pcm[(size_t)i] = (short)(8000.0 * sin(6.283185307 * 440.0 * i / rate));
	}

	FILE *f = nullptr;

	if (fopen_s(&f, path.c_str(), "wb") != 0 || !f)
	{
		return false;
	}

	unsigned const dataBytes = (unsigned)(samples * 2), riffBytes = 36 + dataBytes, fmtBytes = 16, byteRate = (unsigned)rate * 2;
	unsigned short const format = 1, channels = 1, align = 2, bits = 16;
	bool written = true;

	written = written && fwrite("RIFF", 1, 4, f) == 4;
	written = written && fwrite(&riffBytes, 4, 1, f) == 1;
	written = written && fwrite("WAVEfmt ", 1, 8, f) == 8;
	written = written && fwrite(&fmtBytes, 4, 1, f) == 1;
	written = written && fwrite(&format, 2, 1, f) == 1;
	written = written && fwrite(&channels, 2, 1, f) == 1;
	written = written && fwrite(&rate, 4, 1, f) == 1;
	written = written && fwrite(&byteRate, 4, 1, f) == 1;
	written = written && fwrite(&align, 2, 1, f) == 1;
	written = written && fwrite(&bits, 2, 1, f) == 1;
	written = written && fwrite("data", 1, 4, f) == 4;
	written = written && fwrite(&dataBytes, 4, 1, f) == 1;
	written = written && fwrite(pcm.data(), 2, (size_t)samples, f) == (size_t)samples;
	return (fclose(f) == 0) && written;
}

int main()
{
	setvbuf(stdout, nullptr, _IONBF, 0);   // every line out at once: if the game's code stops the program, the checks before it still show
	printf("A 3D sound with nobody to hear it (Global_VDB_Ptr 0)\n");

	// the WAV goes in the machine's temporary folder, named for this program and its process, so two runs side by side never share it
	char temporary[MAX_PATH + 1] = {};
	DWORD const length = GetTempPathA(MAX_PATH, temporary);

	if (length == 0 || length > MAX_PATH)
	{
		printf("SOME CHECKS FAILED: the temporary folder couldn't be found\n");
		return 1;
	}

	std::string const wav = std::string(temporary) + "sound_no_listener_test_" + std::to_string(GetCurrentProcessId()) + ".wav";
	Check(WriteTone(wav), "the test's own WAV is written (" + wav + ")");

	// the sound system, started on the pretend card, and the WAV loaded by the game's own loader into one of the game's sounds, marked
	// loaded with its volume and pitch as the game's sound list does (psndproj.cpp)
	for (int i = 0; i < SID_MAXIMUM; i++)
	{
		GameSounds[i] = BlankGameSound;
	}

	for (int i = 0; i < SOUND_MAXACTIVE; i++)
	{
		ActiveSounds[i] = BlankActiveSound;
	}

	Check(PlatStartSoundSys() == 1, "the game's sound code starts on the stand-in sound card");
	Check(sourceList.size() == (size_t)kStandInSources, "and it made the card's " + std::to_string(kStandInSources) + " sources (made " + std::to_string(sourceList.size()) + ")");

	const SOUNDINDEX sound = (SOUNDINDEX)1;
	bool const loaded = LoadWavFile(sound, wav) != 0;
	Check(loaded, "the WAV is read by the game's own loader into game sound 1");
	DeleteFileA(wav.c_str());

	if (!loaded)
	{
		printf("SOME CHECKS FAILED: %d (nothing to play)\n", checksFailed);
		return 1;
	}

	GameSounds[sound].loaded = 1;
	GameSounds[sound].volume = VOLUME_DEFAULT;
	GameSounds[sound].pitch = PITCH_DEFAULTPLAT;

	VECTORCH const where = {1000, 0, 2000};
	int result = 0;

	// 1. NO LISTENER: a 3D sound is refused, and its source is given back
	printf("No listener yet (before a level makes its view):\n");
	Global_VDB_Ptr = nullptr;
	FillSlot(sound, &where);
	bool const ran = PlayCaught(&result);
	Check(ran, "a 3D sound with no listener doesn't stop the program (before the fix, PlatDo3dSound read the view at address 0)");

	if (ran)
	{
		Check(result == SOUND_PLATFORMERROR, "it is refused: PlatPlaySound gives back SOUND_PLATFORMERROR (" + std::to_string(result) + "), so Sound_Play empties its slot");
		Check(ActiveSounds[kSlot].source == nullptr && SourcesHandedOut() == 0, "its source is given back (" + std::to_string(SourcesHandedOut()) + " handed out)");
		Check(SourcesPlaying() == 0, "and no source plays it (" + std::to_string(SourcesPlaying()) + " playing)");
	}

	ActiveSounds[kSlot] = BlankActiveSound;   // as Sound_Play does with a refused sound

	// 2. a 2D sound needs no listener: it still plays with none
	FillSlot(sound, nullptr);
	bool const ran2D = PlayCaught(&result);
	Check(ran2D && result != SOUND_PLATFORMERROR && SourcesPlaying() == 1, "a 2D sound (no place in the world) still plays with no listener");
	EmptySlot();
	Check(SourcesPlaying() == 0 && SourcesHandedOut() == 0, "and stops again, its source given back");

	// 3. WITH A LISTENER: the same 3D sound plays (so the refusal above is the listener's, nothing else)
	printf("With a listener (a level's view):\n");
	Global_VDB_Ptr = &testView;
	FillSlot(sound, &where);
	bool const ranHeard = PlayCaught(&result);
	Check(ranHeard && result != SOUND_PLATFORMERROR, "the same 3D sound plays (" + std::to_string(result) + ")");
	Check(SourcesPlaying() == 1 && SourcesHandedOut() == 1, "one source plays it (" + std::to_string(SourcesPlaying()) + " playing, " + std::to_string(SourcesHandedOut()) + " handed out)");

	// 4. THE LISTENER GOES while it plays: a frame's update (SoundSys_Management and Sound_ChangeVolume both call PlatDo3dSound, and
	// pay no heed to what it gives back) leaves it alone
	printf("The listener goes while it plays:\n");
	Global_VDB_Ptr = nullptr;
	bool const ranFrame = Do3dCaught(&result);
	Check(ranFrame, "a frame's update of the 3D sound (PlatDo3dSound) doesn't stop the program");

	if (ranFrame)
	{
		Check(result == SOUND_PLATFORMERROR, "it does nothing, and says so: SOUND_PLATFORMERROR (" + std::to_string(result) + ")");
		Check(ActiveSounds[kSlot].source != nullptr && SourcesPlaying() == 1, "and the sound is still playing, on its source");
	}

	// the listener back: the next update works out its distance again
	Global_VDB_Ptr = &testView;
	bool const ranBack = Do3dCaught(&result);   // called first: the words below are made before Check is called, so they show this call's answer
	Check(ranBack && result != SOUND_PLATFORMERROR, "with the listener back, the next update works on it again (" + std::to_string(result) + ")");

	EmptySlot();
	PlatEndSoundSys();
	Check(SourcesPlaying() == 0, "the sound code ends with nothing playing");

	if (checksFailed > 0)
	{
		printf("SOME CHECKS FAILED: %d\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
