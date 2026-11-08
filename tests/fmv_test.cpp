// The test of the video screens. Written 2026-11-04 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// Some of the game's levels have video screens: a picture on a wall that plays a small film. The picture's name and size come from the
// level, and the film's size comes from the film's own file, so nothing about them can be trusted. The picture's memory is one fixed
// size, 128 x 128, and the list of video screens has room for 10. A file that says otherwise used to make the game write past the end of
// them. These checks give the real code those files, and the size the stock game uses too, because stock films have to keep playing just as
// they do. They also check a film with no sound, which has no sound stream, and that a film lets go of everything when it's finished.
//
// HOW IT RUNS
//
// tests/build.ps1 compiles this file with the game's real FmvCutscenes.cpp, SmackerPlayback.cpp and BinkPlayback.cpp, using the settings in
// tests/fmv_test.args. Everything else is a stand-in below: there's no graphics card, no film file and no sound. The picture's memory has a
// long stretch filled with a known pattern after its end, so anything written past the end changes the pattern. The program gives back a
// non-zero result if anything failed. Some failures on broken code are a crash instead, which also gives a non-zero result.
//
// WHAT IT CAN'T TEST
//
// Of BinkPlayback.cpp, only opening a film and closing it run here. Its threads and its frame copy don't, because they need the real decoder and a sound card. A run of the real
// decoders under the address sanitizer covers those: see tests/takeouts/README.md.
//
// The real decoder, the real sound stream (openal_stream.cpp) and a level's room echo number (openal_audio.cpp) are in code that
// brings most of the game with it, or a sound card. This test reads the ones that matter as text, at the end, and checks what they say. It can't
// run them. The stand-in decoder below reports what the real one does: one sound track, or none for a film without sound.
// (Modified 2026-11-08 by Melanikus for the AvP port: the sound stream's code moved to openal_stream.cpp, so it's read from there.
// tests/audio_stream_test.cpp runs it.)

#pragma comment(lib, "winmm.lib")

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FmvPlayback.h"
#include "FmvCutscenes.h"
#include "TextureManager.h"
#include "SmackerPlayback.h"
#include <new>
#include "BinkPlayback.h"
#include "console.h"
#include "io.h"
#include "user_profile.h"

// These are in FmvCutscenes.cpp, with no header that says so
void ScanImagesForFMVs();
void InitFmvCutscenes();
int32_t OpenFMV(const char *filenamePtr);
extern int MoviesAreActive;
extern SmackerPlayback *fmvList[];
extern void StartTriggerPlotFMV(int number);

static int checksFailed = 0;

static void Check(bool passed, std::string const &what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what.c_str());

	if (!passed)
	{
		checksFailed++;
	}
}

// ---- What the test says the level and the film are like ------------------------------------------------------------------------

struct TestTexture
{
	std::string name;
	uint32_t width;
	uint32_t height;
};

static std::vector<TestTexture> levelTextures;
static std::vector<std::string> filmsThatExist;
static uint32_t filmWidth = 128;
static uint32_t filmHeight = 96;
static uint32_t filmSoundTracks = 1;
static std::vector<std::string> printedErrors;

static int setVolumeCalls = 0;
static int setPanCalls = 0;
static int stopCalls = 0;
static int decodersOpen = 0;
bool pthreadCreateFails = false;
int pthreadCreatesLeft = -1;
int pthreadJoins = 0;
static uint32_t filmSoundBytes = 4096;
static int binkDecodersOpen = 0;
static bool binkOpens = false;
static bool textureCanBeMade = true;

// ---- Stand-ins for the parts of the game the video code talks to --------------------------------------------------------------

void Con_PrintError(const std::string &errorString)
{
	printedErrors.push_back(errorString);
}

void Tex_GetNamesVector(std::vector<std::string> &namesArray)
{
	namesArray.clear();

	for (size_t i = 0; i < levelTextures.size(); i++)
	{
		namesArray.push_back(levelTextures[i].name);
	}
}

void Tex_GetDimensions(texID_t textureID, uint32_t &width, uint32_t &height)
{
	if (textureID < levelTextures.size())
	{
		width = levelTextures[textureID].width;
		height = levelTextures[textureID].height;
	}
	else
	{
		width = 0;
		height = 0;
	}
}

FILE *avp_fopen(const char *fileName, const char *mode)
{
	for (size_t i = 0; i < filmsThatExist.size(); i++)
	{
		if (filmsThatExist[i] == fileName)
		{
			// any file that can be opened does for "this film exists"
			return tmpfile();
		}
	}

	(void)mode;
	return 0;
}

// FmvCutscenes.cpp has more in it than the video screens, such as the full-screen films, so these have to exist for it to link. The checks
// below don't reach them, or only with no objects in the level, which is all they need to be right for.
void DrawFmvFrame(uint32_t, uint32_t, const std::vector<texID_t> &) {}
void Tex_Release(texID_t) {}
void CheckForWindowsMessages(void) {}
void FlipBuffers(void) {}
void DirectReadKeyboard(void) {}
int _DotProduct(VECTORCH *, VECTORCH *) { return 0; }
void Normalise(VECTORCH *) {}
int Approximate3dMagnitude(VECTORCH *) { return 0; }
void UpdateFMVTexture(FMVTEXTURE *) {}
void ThisFramesRenderingHasBegun(void) {}
void ThisFramesRenderingHasFinished(void) {}
bool GotAnyKey = false;
bool DebouncedGotAnyKey = false;
int CheatMode_Active = CHEATMODE_NONACTIVE;   // the game plays no films with a cheat on
VIEWDESCRIPTORBLOCK *Global_VDB_Ptr = 0;
char LevelName[] = "test";
int NumActiveBlocks = 0;
DISPLAYBLOCK *ActiveBlockList[1] = { 0 };

// The game's numbers with 16 places after the point, multiplied as it does
int MUL_FIXED(int a, int b)
{
	return (int)(((__int64)a * b) >> 16);
}

// A fixed run of numbers, so that a failure can be repeated
int FastRandom(void)
{
	static unsigned int state = 12345;
	state = state * 1103515245u + 12345u;
	return (int)(state >> 8);
}

// ---- The other film decoder, and the textures its player makes

BinkHandle Bink_Open(const char *)
{
	BinkHandle handle;
	memset(&handle, 0, sizeof(handle));
	handle.isValid = binkOpens;
	handle.instanceIndex = binkOpens ? 0 : -1;
	binkDecodersOpen += binkOpens ? 1 : 0;
	return handle;
}

// the same test the real one makes: it goes by the number, so a handle nobody set up would look like a decoder to close
void Bink_Close(BinkHandle &handle)
{
	if (handle.instanceIndex >= 0)
	{
		binkDecodersOpen--;
		handle.isValid = false;
		handle.instanceIndex = -1;
	}
}

float Bink_GetFrameRate(BinkHandle &) { return 15.0f; }
uint32_t Bink_GetNumAudioTracks(BinkHandle &) { return filmSoundTracks; }

AudioInfo Bink_GetAudioTrackDetails(BinkHandle &, uint32_t)
{
	AudioInfo info;
	memset(&info, 0, sizeof(info));
	info.sampleRate = 22050;
	info.nChannels = 1;
	info.idealBufferSize = filmSoundBytes;
	return info;
}

uint32_t Bink_GetAudioData(BinkHandle &, uint32_t, int16_t *) { return 0; }
uint32_t Bink_GetNumFrames(BinkHandle &) { return 1; }
uint32_t Bink_GetCurrentFrameNum(BinkHandle &) { return 0; }
void Bink_GetFrameSize(BinkHandle &, uint32_t &width, uint32_t &height) { width = filmWidth; height = filmHeight; }
uint32_t Bink_GetNextFrame(BinkHandle &, YUVbuffer) { return 0; }
void Bink_GotoFrame(BinkHandle &, uint32_t) {}
bool Bink_IsEndOfFile(BinkHandle &) { return false; }

texID_t MISSING_TEXTURE = 0xFFFF;
namespace Util { std::string IntToString(const int value) { return std::to_string(value); } }
texID_t Tex_Create(const std::string &, uint32_t, uint32_t, uint32_t, enum TextureUsage) { return textureCanBeMade ? 1 : MISSING_TEXTURE; }
bool Tex_Lock(texID_t, uint8_t **data, uint32_t *pitch, enum TextureLock) { *data = 0; *pitch = 0; return false; }
bool Tex_Unlock(texID_t) { return true; }

// ---- The film decoder

SmackerHandle Smacker_Open(const char *)
{
	SmackerHandle handle;
	memset(&handle, 0, sizeof(handle));
	handle.isValid = true;
	decodersOpen++;
	return handle;
}

void Smacker_Close(SmackerHandle &handle)
{
	if (handle.isValid)
	{
		decodersOpen--;
		handle.isValid = false;
	}
}
uint32_t Smacker_GetNumAudioTracks(SmackerHandle &) { return filmSoundTracks; }

SmackerAudioInfo Smacker_GetAudioTrackDetails(SmackerHandle &, uint32_t)
{
	SmackerAudioInfo info;
	memset(&info, 0, sizeof(info));
	info.sampleRate = 22050;
	info.nChannels = 1;
	info.bitsPerSample = 16;
	info.idealBufferSize = filmSoundBytes;
	return info;
}

uint32_t Smacker_GetAudioData(SmackerHandle &, uint32_t, int16_t *) { return 0; }
uint32_t Smacker_GetNumFrames(SmackerHandle &) { return 1; }

void Smacker_GetFrameSize(SmackerHandle &, uint32_t &width, uint32_t &height)
{
	width = filmWidth;
	height = filmHeight;
}

uint32_t Smacker_GetCurrentFrameNum(SmackerHandle &) { return 0; }
uint32_t Smacker_GetNextFrame(SmackerHandle &) { return 0; }
float Smacker_GetFrameRate(SmackerHandle &) { return 15.0f; }

// A palette where each colour says what number it is
void Smacker_GetPalette(SmackerHandle &, uint8_t *palette)
{
	for (int i = 0; i < 256; i++)
	{
		palette[i * 3 + 0] = (uint8_t)i;
		palette[i * 3 + 1] = (uint8_t)(i ^ 0x55);
		palette[i * 3 + 2] = (uint8_t)(i ^ 0xAA);
	}
}

// A film whose every pixel says where it is, so a check can tell which part of the film ended up where
static uint8_t FilmPixel(uint32_t x, uint32_t y)
{
	return (uint8_t)((x * 3 + y * 7) & 255);
}

void Smacker_GetFrame(SmackerHandle &, uint8_t *frame)
{
	for (uint32_t y = 0; y < filmHeight; y++)
	{
		for (uint32_t x = 0; x < filmWidth; x++)
		{
			frame[y * filmWidth + x] = FilmPixel(x, y);
		}
	}
}

// ---- The sound stream

AudioStream::AudioStream() {}
AudioStream::~AudioStream() {}
bool AudioStream::Init(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) { return true; }
uint32_t AudioStream::WriteData(uint8_t *, uint32_t) { return 0; }
void AudioStream::WaitForFreeBuffer() {}
uint32_t AudioStream::GetBufferSize() { return 4096; }
uint32_t AudioStream::GetNumFreeBuffers() { return 0; }
int32_t AudioStream::SetVolume(uint32_t) { setVolumeCalls++; return 0; }
int32_t AudioStream::SetPan(uint32_t) { setPanCalls++; return 0; }
int32_t AudioStream::Play() { return 0; }
int32_t AudioStream::Stop() { stopCalls++; return 0; }

// ---- The picture's memory, with the known pattern after its end ---------------------------------------------------------------

static size_t const kPictureBytes = kFMVTextureSize * kFMVTextureSize * sizeof(uint32_t);
static size_t const kPatternBytes = 64 * 1024 * 1024;   // a 4,096 x 4,096 film at four bytes a pixel is this long
static unsigned char const kPattern = 0xAB;
static unsigned char const kUntouched = 0xCD;

static std::vector<unsigned char> pictureMemory;

static uint8_t *FreshPicture()
{
	pictureMemory.assign(kPictureBytes + kPatternBytes, kPattern);
	memset(pictureMemory.data(), kUntouched, kPictureBytes);
	return pictureMemory.data();
}

static bool NothingPastTheEnd()
{
	for (size_t i = kPictureBytes; i < pictureMemory.size(); i++)
	{
		if (pictureMemory[i] != kPattern)
		{
			return false;
		}
	}

	return true;
}

// What the game's SetupFMVTexture does, with the test's own memory
void SetupFMVTexture(FMVTEXTURE *ftPtr)
{
	ftPtr->RGBBuffer = new uint8_t[kPictureBytes];
	ftPtr->SoundVolume = 0;
}

// ---- The checks -----------------------------------------------------------------------------------------------------------------

// A pixel the film should have put in the picture, in the order the card wants
static bool PixelIsFromTheFilm(uint32_t pictureX, uint32_t pictureY)
{
	uint8_t const *const pixel = pictureMemory.data() + (size_t)pictureY * kFMVTextureSize * 4 + (size_t)pictureX * 4;
	uint8_t const number = FilmPixel(pictureX, pictureY);

	return pixel[0] == (uint8_t)(number ^ 0xAA) && pixel[1] == (uint8_t)(number ^ 0x55) && pixel[2] == number && pixel[3] == 255;
}

static bool PixelIsUntouched(uint32_t pictureX, uint32_t pictureY)
{
	uint8_t const *const pixel = pictureMemory.data() + (size_t)pictureY * kFMVTextureSize * 4 + (size_t)pictureX * 4;

	return pixel[0] == kUntouched && pixel[1] == kUntouched && pixel[2] == kUntouched && pixel[3] == kUntouched;
}

// Plays one frame of a film of a given size on a 128 x 128 picture, and checks which part of the picture it filled
static void CheckOneFilmSize(uint32_t width, uint32_t height, char const *what)
{
	filmWidth = width;
	filmHeight = height;
	filmSoundTracks = 1;

	SmackerPlayback film;
	int const opened = film.Open("FMVs/test.smk");
	std::string const name = what;

	Check(opened == FMV_OK, name + ": it opens");

	film._frameReady = true;
	uint8_t *const buffer = FreshPicture();
	bool const converted = film.ConvertFrame(kFMVTextureSize, kFMVTextureSize, buffer, kFMVTextureSize * sizeof(uint32_t));

	Check(converted, name + ": the frame converts");
	Check(NothingPastTheEnd(), name + ": nothing is written past the end of the picture");

	uint32_t const expectWidth = (width < kFMVTextureSize) ? width : kFMVTextureSize;
	uint32_t const expectHeight = (height < kFMVTextureSize) ? height : kFMVTextureSize;
	bool filled = true, restAlone = true;

	for (uint32_t y = 0; y < kFMVTextureSize; y++)
	{
		for (uint32_t x = 0; x < kFMVTextureSize; x++)
		{
			if (x < expectWidth && y < expectHeight)
			{
				filled = filled && PixelIsFromTheFilm(x, y);
			}
			else
			{
				restAlone = restAlone && PixelIsUntouched(x, y);
			}
		}
	}

	Check(filled, name + ": the top left of the film is in the top left of the picture, pixel for pixel");
	Check(restAlone, name + ": the rest of the picture is left alone");
	Check(!film._frameReady, name + ": the frame counts as used");

	film.Close();
}

static void CheckFilmSizes()
{
	printf("A film on a 128 x 128 picture:\n");

	CheckOneFilmSize(128, 96, "128 x 96, the size of all 53 stock films with a picture");
	CheckOneFilmSize(128, 128, "128 x 128, a perfect fit");
	CheckOneFilmSize(64, 32, "64 x 32, smaller");
	CheckOneFilmSize(4, 4, "4 x 4, the size of the stock menu music's film");
	CheckOneFilmSize(129, 96, "129 x 96, one pixel too wide");
	CheckOneFilmSize(128, 129, "128 x 129, one row too tall");
	CheckOneFilmSize(4096, 8, "4,096 x 8, far too wide");
	CheckOneFilmSize(8, 4096, "8 x 4,096, far too tall");
	CheckOneFilmSize(4096, 4096, "4,096 x 4,096, the largest the decoder allows");
}

static void CheckFilmStates()
{
	printf("A film in other states:\n");

	filmWidth = 128;
	filmHeight = 96;
	filmSoundTracks = 1;

	{
		SmackerPlayback film;
		film.Open("FMVs/test.smk");
		film._frameReady = true;
		bool const converted = film.ConvertFrame(kFMVTextureSize, kFMVTextureSize, 0, kFMVTextureSize * sizeof(uint32_t));
		Check(converted && !film._frameReady, "with no picture to write to, it uses up the frame and writes nothing");
		film.Close();
	}

	{
		SmackerPlayback film;
		film.Open("FMVs/test.smk");
		film.Close();
		uint8_t *const buffer = FreshPicture();
		Check(!film.ConvertFrame(kFMVTextureSize, kFMVTextureSize, buffer, kFMVTextureSize * sizeof(uint32_t)), "once closed, it says so");
		Check(PixelIsUntouched(0, 0), "and writes nothing");
	}
}

static void AddPicture(char const *name, uint32_t width, uint32_t height)
{
	TestTexture const texture = { name, width, height };
	levelTextures.push_back(texture);
}

static void StartLevel()
{
	levelTextures.clear();
	filmsThatExist.clear();
	printedErrors.clear();
	NumberOfFMVTextures = 0;
}

static void EndLevel()
{
	for (uint32_t i = 0; i < NumberOfFMVTextures && i < MAX_NO_FMVTEXTURES; i++)
	{
		delete[] FMVTexture[i].RGBBuffer;
		FMVTexture[i].RGBBuffer = 0;
	}

	NumberOfFMVTextures = 0;
}

static int ErrorsContaining(char const *words)
{
	int count = 0;

	for (size_t i = 0; i < printedErrors.size(); i++)
	{
		count += (printedErrors[i].find(words) != std::string::npos) ? 1 : 0;
	}

	return count;
}

static void CheckTheLevelsList()
{
	printf("The level's list of video screens:\n");

	{
		StartLevel();
		AddPicture("graphics\\walls\\brick.RIM", 256, 256);
		AddPicture("graphics\\FMVs\\triggered_plot.RIM", 128, 128);
		ScanImagesForFMVs();
		Check(NumberOfFMVTextures == 1, "a stock level's one video picture makes one screen");
		Check(FMVTexture[0].textureID == 1, "and it's that picture");
		Check(FMVTexture[0].IsTriggeredPlotFMV == 1, "and it plays what the level triggers");
		Check(FMVTexture[0].fmvHandle == -1 && FMVTexture[0].RGBBuffer != 0, "and it's ready, with no film yet");
		Check(printedErrors.empty(), "and nothing complains");
		EndLevel();
	}

	{
		StartLevel();
		AddPicture("graphics\\FMVs\\alien.RIM", 128, 128);
		filmsThatExist.push_back("FMVs\\alien.smk");
		ScanImagesForFMVs();
		Check(NumberOfFMVTextures == 0, "a picture with a film of its own name stays as it always has: no screen");
		EndLevel();
	}

	{
		StartLevel();

		for (int i = 0; i < 40; i++)
		{
			AddPicture("graphics\\FMVs\\many.RIM", 128, 128);
		}

		ScanImagesForFMVs();
		Check(NumberOfFMVTextures == MAX_NO_FMVTEXTURES, "a level with 40 video pictures gets the 10 screens there's room for");
		Check(ErrorsContaining("more video screens") == 1, "and it says so once");
		EndLevel();
	}

	{
		StartLevel();

		for (int i = 0; i < MAX_NO_FMVTEXTURES; i++)
		{
			AddPicture("graphics\\FMVs\\many.RIM", 128, 128);
		}

		ScanImagesForFMVs();
		Check(NumberOfFMVTextures == MAX_NO_FMVTEXTURES, "exactly 10 all fit");
		Check(ErrorsContaining("more video screens") == 0, "and nothing complains");
		EndLevel();
	}

	struct WrongSize { uint32_t width, height; };
	WrongSize const wrongSizes[] = { { 256, 256 }, { 129, 128 }, { 128, 129 }, { 127, 128 }, { 128, 127 }, { 0, 0 }, { 128, 0 }, { 0, 128 }, { 65536, 65536 } };

	for (size_t i = 0; i < sizeof(wrongSizes) / sizeof(wrongSizes[0]); i++)
	{
		StartLevel();
		AddPicture("graphics\\FMVs\\wrong.RIM", wrongSizes[i].width, wrongSizes[i].height);
		ScanImagesForFMVs();
		std::string const size = std::to_string(wrongSizes[i].width) + " x " + std::to_string(wrongSizes[i].height);
		Check(NumberOfFMVTextures == 0, "a video picture " + size + " makes no screen");
		Check(ErrorsContaining("wrong size") == 1, "and it says so");
		EndLevel();
	}

	{
		StartLevel();
		AddPicture("graphics\\FMVs\\wrong.RIM", 300, 300);
		AddPicture("graphics\\FMVs\\right.RIM", 128, 128);
		ScanImagesForFMVs();
		Check(NumberOfFMVTextures == 1 && FMVTexture[0].textureID == 1, "the right size after a wrong one still makes a screen");
		EndLevel();
	}
}

// One screen, ready to draw on, with the test's picture memory for its buffer
static void OneScreen(FMVTEXTURE &screen, texID_t texture, int handle)
{
	memset(&screen, 0, sizeof(screen));
	screen.textureID = texture;
	screen.fmvHandle = handle;
	screen.RGBBuffer = FreshPicture();
}

static void CheckDrawingAFrame()
{
	printf("Drawing a frame on a screen:\n");

	MoviesAreActive = 1;

	{
		// With no film playing, the screen shows static.
		StartLevel();
		AddPicture("graphics\\FMVs\\screen.RIM", 128, 128);
		FMVTEXTURE screen;
		OneScreen(screen, 0, -1);
		NextFMVTextureFrame(&screen);
		bool anyLeft = false;

		for (uint32_t y = 0; y < kFMVTextureSize; y++)
		{
			for (uint32_t x = 0; x < kFMVTextureSize; x++)
			{
				anyLeft = anyLeft || PixelIsUntouched(x, y);
			}
		}

		Check(screen.StaticImageDrawn, "with no film, it draws static");
		Check(!anyLeft, "over the whole picture");
		Check(NothingPastTheEnd(), "and no further");
	}

	struct WrongSize { uint32_t width, height; char const *name; };
	WrongSize const wrongSizes[] = { { 4096, 4096, "4,096 x 4,096" }, { 129, 128, "129 x 128" }, { 128, 129, "128 x 129" }, { 0, 0, "0 x 0" }, { 0, 128, "0 x 128" } };

	for (size_t i = 0; i < sizeof(wrongSizes) / sizeof(wrongSizes[0]); i++)
	{
		// A picture that's the wrong size by now. The static would write that many pixels, and with no pixels at all, it would write four billion.
		StartLevel();
		AddPicture("graphics\\FMVs\\screen.RIM", wrongSizes[i].width, wrongSizes[i].height);
		FMVTEXTURE screen;
		OneScreen(screen, 0, -1);
		int const answer = NextFMVTextureFrame(&screen);
		std::string const name = wrongSizes[i].name;
		Check(answer == 0, "a picture " + name + " draws nothing");
		Check(NothingPastTheEnd(), "and nothing goes past the end");
		Check(PixelIsUntouched(0, 0), "and the picture is left alone");
	}

	{
		StartLevel();
		AddPicture("graphics\\FMVs\\screen.RIM", 128, 128);
		FMVTEXTURE screen;
		OneScreen(screen, 0, -1);
		screen.RGBBuffer = 0;
		Check(NextFMVTextureFrame(&screen) == 0, "a screen with no memory draws nothing");
	}

	for (int sound = 0; sound < 2; sound++)
	{
		// A film playing on a screen, with a sound track and without one
		StartLevel();
		AddPicture("graphics\\FMVs\\screen.RIM", 128, 128);
		filmWidth = 128;
		filmHeight = 96;
		filmSoundTracks = (uint32_t)sound;
		setVolumeCalls = 0;
		setPanCalls = 0;

		int const handle = OpenFMV("FMVs/test.smk");
		std::string const name = sound ? "a film with sound" : "a film with no sound";
		Check(handle != -1, name + " opens");

		fmvList[handle]->_frameReady = true;

		FMVTEXTURE screen;
		OneScreen(screen, 0, handle);
		NextFMVTextureFrame(&screen);

		Check(PixelIsFromTheFilm(127, 95), name + " draws its frame");
		Check(PixelIsUntouched(0, 96), name + " leaves the rows it hasn't got");
		Check(NothingPastTheEnd(), name + " stays inside the picture");
		Check(setVolumeCalls == sound && setPanCalls == sound, name + (sound ? " has its volume and pan set" : " has no stream to set them on"));

		delete fmvList[handle];
		fmvList[handle] = 0;
	}
}

// A film has to let go of everything when it's done with, whichever way it got there
static void CheckLettingGo()
{
	printf("Letting go of a film:\n");

	filmWidth = 128;
	filmHeight = 96;
	filmSoundTracks = 1;

	{
		int const before = decodersOpen;
		{
			SmackerPlayback film;
			Check(film.Open("FMVs/test.smk") == FMV_OK && decodersOpen == before + 1, "opening a film opens its decoder");
			stopCalls = 0;
		}
		Check(decodersOpen == before, "and finishing with it closes the decoder");
		Check(stopCalls >= 1, "and stops its sound first, because that's what a decoding thread could be waiting on");
	}

	{
		int const before = decodersOpen;
		{
			SmackerPlayback neverOpened;
		}
		Check(decodersOpen == before, "a film that was never opened has nothing to close, and that's safe");
	}

	{
		filmSoundTracks = 0;
		int const before = decodersOpen;
		{
			SmackerPlayback film;
			Check(film.Open("FMVs/test.smk") == FMV_OK && film._audioStream == 0, "a film with no sound opens, with no stream");
		}
		Check(decodersOpen == before, "and closes the same way");
		filmSoundTracks = 1;
	}

	{
		pthreadCreateFails = true;
		SmackerPlayback film;
		Check(film.Open("FMVs/test.smk") == FMV_ERROR, "if a thread can't be started, the film says it couldn't open");
		Check(!film.IsPlaying(), "and it doesn't say it's playing");
		pthreadCreateFails = false;
	}

	{
		// Started the decoding thread, couldn't start the sound one. Whatever is letting go of the film has to wait for the one that did start.
		{
			pthreadCreatesLeft = 1;
			pthreadJoins = 0;
			SmackerPlayback film;
			Check(film.Open("FMVs/test.smk") == FMV_ERROR, "if the second thread can't be started, the film says it couldn't open");
			pthreadCreatesLeft = -1;
		}
		Check(pthreadJoins == 1, "and letting go of it waits for the first thread, which would otherwise go on using what was let go of");
	}

	{
		// A film that has finished on a screen used to keep its place on the list of films for good, and the list has room for few
		StartLevel();
		AddPicture("graphics\\FMVs\\triggered_plot.RIM", 128, 128);
		filmsThatExist.push_back("FMVs/message5.smk");
		ScanImagesForFMVs();
		int const before = decodersOpen;
		bool allOpened = true;

		for (int i = 0; i < 50; i++)
		{
			StartTriggerPlotFMV(5);
			allOpened = allOpened && FMVTexture[0].fmvHandle != -1;

			if (FMVTexture[0].fmvHandle != -1)
			{
				fmvList[FMVTexture[0].fmvHandle]->Close();
			}
		}

		Check(allOpened, "a screen can start a film 50 times over, each after the last has finished");
		Check(decodersOpen == before + 1, "and only the last one is still open");

		if (FMVTexture[0].fmvHandle != -1)
		{
			delete fmvList[FMVTexture[0].fmvHandle];
			fmvList[FMVTexture[0].fmvHandle] = 0;
			FMVTexture[0].fmvHandle = -1;
		}

		EndLevel();
	}
}

static std::string ReadSourceFile(char const *name)
{
	// /FC in this test's settings makes __FILE__ the whole path of this file, so the repository's top is found from it
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

// The text of a function, found by what it starts with, as the last place that says so
static std::string FunctionStartingWith(std::string const &source, char const *start)
{
	std::string::size_type const at = source.rfind(start);

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
			return source.substr(at, i - at);
		}
	}

	return "";
}

// Puts a frame's sound in and takes stream buffers out, the way the two threads do, in the real ring. Being stuck means neither could go on, which on a real film is a freeze.
static bool RingKeepsMoving(uint32_t ringSize, uint32_t frameSound, uint32_t streamBuffer)
{
	RingBuffer ring;

	if (!ring.Init(ringSize))
	{
		return false;
	}

	std::vector<uint8_t> frame(frameSound + 1);
	std::vector<uint8_t> buffer(streamBuffer + 1);

	for (int step = 0; step < 2000; step++)
	{
		bool moved = false;

		if (ring.GetReadableSize() >= streamBuffer)
		{
			ring.ReadData(&buffer[0], streamBuffer);
			moved = true;
		}

		if (ring.GetWritableSize() >= frameSound)
		{
			ring.WriteData(&frame[0], frameSound);
			moved = true;
		}

		if (!moved)
		{
			return false;
		}
	}

	return true;
}

static void CheckBink()
{
	printf("The other film player:\n");

	filmWidth = 128;
	filmHeight = 96;
	filmSoundTracks = 1;
	filmSoundBytes = 4096;
	binkOpens = true;

	{
		int const before = binkDecodersOpen;
		{
			BinkPlayback film;
			Check(film.Open("FMVs/test.bik") == FMV_OK && binkDecodersOpen == before + 1, "opening a film opens its decoder");
			stopCalls = 0;
			pthreadJoins = 0;
		}
		Check(binkDecodersOpen == before, "and finishing with it closes the decoder, which was never closed before");
		Check(stopCalls >= 1, "and stops its sound first");
		Check(pthreadJoins == 2, "and waits for both of its threads");
	}

	{
		int const before = binkDecodersOpen;
		{
			// Made in memory that's already full of a small number, so a handle nobody set up would look like an open one. All ones wouldn't do: that reads as -1, which is what no decoder looks like
			alignas(BinkPlayback) static unsigned char memory[sizeof(BinkPlayback)];
			memset(memory, 0x01, sizeof(memory));
			BinkPlayback *neverOpened = new (memory) BinkPlayback();
			neverOpened->~BinkPlayback();
		}
		Check(binkDecodersOpen == before, "a film that was never opened has nothing to close, and that's safe");
	}

	{
		binkOpens = false;
		int const before = binkDecodersOpen;
		{
			BinkPlayback film;
			Check(film.Open("FMVs/test.bik") == FMV_INVALID_FILE, "a file that can't be opened says so");
		}
		Check(binkDecodersOpen == before, "and there's nothing to close");
		binkOpens = true;
	}

	{
		filmSoundTracks = 0;
		int const before = binkDecodersOpen;
		{
			BinkPlayback film;
			Check(film.Open("FMVs/test.bik") == FMV_OK && film._audioStream == 0, "a film with no sound opens, with no stream");
			pthreadJoins = 0;
		}
		Check(binkDecodersOpen == before && pthreadJoins == 1, "and closes the same way, waiting for its one thread");
		filmSoundTracks = 1;
	}

	{
		int const before = binkDecodersOpen;
		{
			pthreadCreateFails = true;
			BinkPlayback film;
			Check(film.Open("FMVs/test.bik") == FMV_ERROR, "if a thread can't be started, the film says it couldn't open, where it used to say it had");
			Check(!film.IsPlaying(), "and it doesn't say it's playing");
			pthreadCreateFails = false;
			pthreadJoins = 0;
		}
		Check(pthreadJoins == 0, "and letting go of it doesn't wait for a thread that never started");
		Check(binkDecodersOpen == before, "and its decoder is still closed");
	}

	{
		{
			pthreadCreatesLeft = 1;
			pthreadJoins = 0;
			BinkPlayback film;
			Check(film.Open("FMVs/test.bik") == FMV_ERROR, "if the second thread can't be started, the film says it couldn't open");
			pthreadCreatesLeft = -1;
		}
		Check(pthreadJoins == 1, "and letting go of it waits for the first thread, which used to go on using memory that had been let go of");
	}

	{
		int const before = binkDecodersOpen;
		{
			textureCanBeMade = false;
			pthreadJoins = 0;
			BinkPlayback film;
			Check(film.Open("FMVs/test.bik") == FMV_ERROR, "a film whose textures can't be made says it couldn't open");
			textureCanBeMade = true;
		}
		Check(pthreadJoins == 0 && binkDecodersOpen == before, "with no thread started, and its decoder closed");
	}

	printf("The ring of sound, in both players:\n");

	Check(FmvPlayback_RingSize(23520, 4096, 3) == 70560, "the sound of the game's own Smacker films: three frames' worth, as always");
	Check(FmvPlayback_RingSize(194040, 4096, 3) == 582120, "the largest of them, the same");
	Check(FmvPlayback_RingSize(138240, 4096, 3) == 414720, "the game's own Bink films, the same");
	Check(FmvPlayback_RingSize(1365, 4096, 3) == 5461, "1365 bytes a frame: enough for one frame and one stream buffer, where three frames' worth is 4095");
	Check(FmvPlayback_RingSize(2047, 4096, 3) == 6143, "one under where three frames' worth is enough");
	Check(FmvPlayback_RingSize(2048, 4096, 3) == 6144, "and where it is");
	Check(FmvPlayback_RingSize(0, 4096, 3) == 4096, "no sound at all still leaves a usable ring");

	Check(!RingKeepsMoving(1365 * 3, 1365, 4096), "the old size really does get stuck: with 1365 bytes a frame, it can never hold the 4096 the other thread is waiting for");
	Check(RingKeepsMoving(FmvPlayback_RingSize(1365, 4096, 3), 1365, 4096), "and the new one keeps moving");

	bool keepsMoving = true;
	char const *const whichFilm[2] = { "Smacker", "Bink" };

	for (uint32_t bytes = 1; bytes < 9000; bytes++)
	{
		keepsMoving = keepsMoving && RingKeepsMoving(FmvPlayback_RingSize(bytes, 4096, 3), bytes, 4096);
	}

	Check(keepsMoving, "and at every size from 1 to 9000 bytes a frame");

	for (int player = 0; player < 2; player++)
	{
		uint32_t const sizes[3] = { 100, 1365, 30000 };

		for (int s = 0; s < 3; s++)
		{
			filmSoundBytes = sizes[s];
			uint32_t room = 0;

			if (player == 0)
			{
				SmackerPlayback film;
				room = (film.Open("FMVs/test.smk") == FMV_OK) ? film._ringBuffer->GetWritableSize() : 0;
			}
			else
			{
				BinkPlayback film;
				room = (film.Open("FMVs/test.bik") == FMV_OK) ? film._ringBuffer->GetWritableSize() : 0;
			}

			Check(room == FmvPlayback_RingSize(sizes[s], 4096, 3), std::string(whichFilm[player]) + " makes the ring that size, at " + std::to_string(sizes[s]) + " bytes a frame");
		}
	}

	filmSoundBytes = 4096;
	binkOpens = false;
}

static void CheckTheRealCode()
{
	printf("Reading the code this test can't run:\n");

	std::string const decoder = ReadSourceFile("dependencies/libsmackerdec/src/SmackerDecoder.cpp");
	std::string const tracks = FunctionStartingWith(decoder, "uint32_t Smacker_GetNumAudioTracks(");
	Check(tracks.find("GetNumAudioTracks()") != std::string::npos && tracks.find("return 1") == std::string::npos, "the decoder's number of sound tracks comes from the film, and isn't always 1");

	std::string const made = FunctionStartingWith(decoder, "SmackerDecoder::SmackerDecoder()");
	Check(made.find("audioTracks[i].sampleRate = 0") != std::string::npos && made.find("audioTracks[i].bufferSize = 0") != std::string::npos, "a track a film doesn't have holds zeros, not whatever was in that memory");
	Check(decoder.find("std::vector<class SmackerDecoder*> classInstances") == std::string::npos, "the list of decoders can't move under another film's thread");

	std::string const sound = ReadSourceFile("3dc/avp/win95/openal_audio.cpp");
	std::string const streams = ReadSourceFile("3dc/avp/win95/openal_stream.cpp");
	std::string const stop = FunctionStartingWith(streams, "int32_t AudioStream::Stop()");
	Check(stop.find("if (_source)") != std::string::npos, "stopping a stream that couldn't start reads nothing");

	std::string const letGo = FunctionStartingWith(streams, "AudioStream::~AudioStream()");
	Check(letGo.find("if (_threadStarted)") != std::string::npos && letGo.find("if (_mutexMade)") != std::string::npos && letGo.find("if (_source)") != std::string::npos, "and letting go of it only lets go of what it got as far as making");

	std::string const wait = FunctionStartingWith(streams, "void AudioStream::WaitForFreeBuffer()");
	Check(wait.find("&& _isPlaying") != std::string::npos, "and waiting on a stream ends when it's stopped");

	std::string const environment = FunctionStartingWith(sound, "void PlatSetEnviroment(");
	Check(environment.find("env_index >= (unsigned int)kMaxReverbPresets") != std::string::npos, "and a room echo number is checked against the list");

	std::string const bink = FunctionStartingWith(ReadSourceFile("3dc/video/BinkPlayback.cpp"), "BinkPlayback::~BinkPlayback()");
	Check(bink.find("_audioStream->Stop()") != std::string::npos && bink.find("_audioStream->Stop()") < bink.find("pthread_join"), "and the other film player stops its sound before waiting for its threads too");

	std::string const menus = ReadSourceFile("3dc/avp/win95/frontend/menus.cpp");

	std::string const binkDecoder = ReadSourceFile("dependencies/libbinkdec/src/BinkDecoder.cpp");
	Check(binkDecoder.find("static class BinkDecoder *classInstances[kMostOpenDecoders];") != std::string::npos && binkDecoder.find("classInstances.push_back") == std::string::npos, "and the Bink decoders are on a list that can't move under a playing film");
	std::string const binkLookup = FunctionStartingWith(binkDecoder, "static BinkDecoder *GetDecoder(");
	Check(binkLookup.find("handle.instanceIndex >= kMostOpenDecoders") != std::string::npos, "and a handle is checked against the end of that list");
	std::string const binkOpen = FunctionStartingWith(binkDecoder, "BinkHandle Bink_Open(");
	Check(binkOpen.find("Open(fileName)") < binkOpen.find("classInstances[place] = newDecoder") && binkOpen.find("classInstances[place] = newDecoder") != std::string::npos, "and a decoder only goes on the list once it's ready");

	std::string const binkPlayer = ReadSourceFile("3dc/video/BinkPlayback.cpp");
	std::string const smackerPlayer = ReadSourceFile("3dc/video/SmackerPlayback.cpp");
	std::string const music = ReadSourceFile("3dc/VorbisPlayer.cpp");

	// The threads themselves don't run here, because they need the real decoders and a sound card, so these only show the lines are there.
	Check(binkPlayer.find("::Sleep(timetoSleep);") != std::string::npos, "Bink's sound thread sleeps between its turns, as Smacker's does");
	Check(binkPlayer.find("new(std::nothrow) AudioStream()") != std::string::npos && binkPlayer.find("!_audioStream || !_audioStream->Init(") != std::string::npos, "and running out of memory for its sound stream is noticed");
	Check(smackerPlayer.find("new(std::nothrow) AudioStream()") != std::string::npos && smackerPlayer.find("!_audioStream || !_audioStream->Init(") != std::string::npos, "and for Smacker's");
	Check(music.find("if (pthread_create(&_playbackThread, NULL, VorbisUpdateThread, static_cast<void*>(this)) == 0) {\r\n\t\t\t_decodeThreadInited = true;") != std::string::npos, "and a music thread is only waited for if it really started");
	Check(ReadSourceFile("dependencies/libbinkdec/src/LogError.cpp").find("LastError") == std::string::npos, "and Bink's errors aren't kept in a string every thread writes to");
	Check(ReadSourceFile("dependencies/libsmackerdec/src/LogError.cpp").find("LastError") == std::string::npos, "and nor are Smacker's");

	Check(binkPlayer.find("if (!_ringBuffer || !_ringBuffer->Init(") != std::string::npos, "and running out of memory for the Bink ring is noticed, because it can't be made to happen here");
	Check(binkPlayer.find("if (_audioDataBuffer == NULL)") != std::string::npos, "and so is running out for its sound buffer");
}

int main()
{
	InitFmvCutscenes();

	CheckFilmSizes();
	CheckFilmStates();
	CheckTheLevelsList();
	CheckDrawingAFrame();
	CheckLettingGo();
	CheckBink();
	CheckTheRealCode();

	if (checksFailed)
	{
		printf("%d checks FAILED\n", checksFailed);
		return 1;
	}

	printf("All checks passed\n");
	return 0;
}
