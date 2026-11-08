// Tests the checks on the new guns' sound file, in 3dc/avp/weapon_sound_bank.h, and that the game is wired to them.
// Written 2026-11-06 by Melanikus for the AvP port.
//
// Nothing here needs a sound card or the game's data. Each broken file below has to be refused for its own named
// reason, so a later check can't hide a missing earlier one. Whether the sounds play in the game needs a real run.
//
// It reads no file it's told about: the game's own real sound files aren't in the repository, so they were checked through the game's
// real loader instead (in the project's notes, outside the repository), and the test takes no path from its command line.

#include "avp/weapon_sound_bank.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "memory_wall.h"

static int checks = 0;
static int failures = 0;

static void Check(bool good, const char *what)
{
	checks++;
	failures += good ? 0 : 1;
	printf("  %s  %s\n", good ? "ok" : "FAILED", what);
}

typedef std::vector<uint8_t> Bytes;

static void Add32(Bytes &out, uint32_t value)
{
	for (int i = 0; i < 4; i++)
	{
		out.push_back((uint8_t)(value >> (8 * i)));
	}
}

static void Add16(Bytes &out, uint16_t value)
{
	out.push_back((uint8_t)value);
	out.push_back((uint8_t)(value >> 8));
}

static void AddLetters(Bytes &out, const char *letters)
{
	out.insert(out.end(), letters, letters + strlen(letters));
}

// What a small good .wav is made of, with each part open to being broken by a test.
struct WavParts
{
	const char *riff = "RIFF";
	const char *wave = "WAVE";
	const char *format = "fmt ";
	uint32_t formatSize = 16;
	uint16_t kind = 1;
	uint16_t channels = 1;
	uint32_t samplesPerSecond = 22050;
	uint32_t bytesPerSecond = 22050;
	uint16_t bits = 8;
	bool hasExtraBlock = false;
	uint32_t extraBlockSize = 4;
	const char *audio = "data";
	uint32_t audioSize = 6;
	bool sizeIsTrue = true;
	uint32_t riffSizeIfNotTrue = 0;
};

static Bytes MakeWav(const WavParts &parts)
{
	Bytes inside;
	AddLetters(inside, parts.wave);
	AddLetters(inside, parts.format);
	Add32(inside, parts.formatSize);
	Add16(inside, parts.kind);
	Add16(inside, parts.channels);
	Add32(inside, parts.samplesPerSecond);
	Add32(inside, parts.bytesPerSecond);
	Add16(inside, (uint16_t)(parts.channels * parts.bits / 8));
	Add16(inside, parts.bits);
	if (parts.formatSize == 18)
	{
		Add16(inside, 0);
	}
	if (parts.hasExtraBlock)
	{
		AddLetters(inside, "LIST");
		Add32(inside, parts.extraBlockSize);
		for (int i = 0; i < 4; i++)
		{
			inside.push_back(0);
		}
	}
	AddLetters(inside, parts.audio);
	Add32(inside, parts.audioSize);
	for (int i = 0; i < 6; i++)
	{
		inside.push_back((uint8_t)i);
	}

	Bytes wav;
	AddLetters(wav, parts.riff);
	Add32(wav, parts.sizeIsTrue ? (uint32_t)inside.size() : parts.riffSizeIfNotTrue);
	wav.insert(wav.end(), inside.begin(), inside.end());
	return wav;
}

static void AddEntry(Bytes &file, int number, int pitch, const char *name, const Bytes &wav)
{
	file.push_back((uint8_t)number);
	file.push_back((uint8_t)pitch);
	AddLetters(file, name);
	file.push_back(0);
	file.insert(file.end(), wav.begin(), wav.end());
}

static void AddEndMark(Bytes &file)
{
	file.push_back(0xff);
	file.push_back(0xff);
}

static Bytes OneEntryFile(const WavParts &parts)
{
	Bytes file;
	AddEntry(file, 33, 0, "AutoShotgun4.wav", MakeWav(parts));
	AddEndMark(file);
	return file;
}

// Modified 2026-11-08 by Melanikus for the AvP port: every file is read from memory that ends at a wall.
// The file's bytes are copied so that its last byte is the last one that can be read (memory_wall.h). If the reader ever looked even one
// byte past the end of a file, the test would stop there, every time. Read from the list the bytes were made in, a look past the end would
// land in whatever happened to be next to it, and a check could pass by luck. There's only one wall, so each call replaces what the last
// one put there: use what it gives back straight away.
static const uint8_t *AtTheWall(const Bytes &file)
{
	uint8_t *const at = (uint8_t *)MemoryEndingAtAWall(file.size());

	if (!file.empty())
	{
		memcpy(at, file.data(), file.size());
	}

	return at;
}

static SoundBankVerdict VerdictOf(const Bytes &file)
{
	int entries = 0;
	return SoundBank_CheckWholeFile(AtTheWall(file), file.size(), &entries);
}

static void TestGoodFiles()
{
	WavParts plain;
	Bytes file;
	AddEntry(file, 30, 0, "Revolver_Fire.wav", MakeWav(plain));
	AddEntry(file, 76, -4, "incinmid.wav", MakeWav(plain));
	AddEndMark(file);

	int entries = 0;
	Check(SoundBank_CheckWholeFile(AtTheWall(file), file.size(), &entries) == SOUND_BANK_REACHED_THE_END_MARK, "a good file of two sounds is accepted");
	Check(entries == 2, "a good file of two sounds counts two");

	SoundBankEntry first;
	Check(SoundBank_ReadEntry(AtTheWall(file), file.size(), 0, &first) == SOUND_BANK_ENTRY_IS_GOOD, "the first entry reads");
	Check(first.number == 30, "the first entry's number");
	Check(first.pitch == 0, "the first entry's pitch");
	Check(first.nameAt == 2, "the first entry's name starts after its two numbers");
	Check(first.nameLength == strlen("Revolver_Fire.wav"), "the first entry's name length");

	SoundBankEntry second;
	Check(SoundBank_ReadEntry(AtTheWall(file), file.size(), first.nextEntryAt, &second) == SOUND_BANK_ENTRY_IS_GOOD, "the second entry follows the first");
	Check(second.number == 76, "the second entry's number");
	Check(second.pitch == -4, "a pitch below zero reads as below zero");

	WavParts longer;
	longer.formatSize = 18;
	Check(VerdictOf(OneEntryFile(longer)) == SOUND_BANK_REACHED_THE_END_MARK, "a format block of 18 bytes is accepted");

	WavParts withExtra;
	withExtra.hasExtraBlock = true;
	Check(VerdictOf(OneEntryFile(withExtra)) == SOUND_BANK_REACHED_THE_END_MARK, "a block before the audio is stepped over");

	WavParts stereo;
	stereo.channels = 2;
	stereo.bits = 16;
	Check(VerdictOf(OneEntryFile(stereo)) == SOUND_BANK_REACHED_THE_END_MARK, "stereo at 16 bits is accepted");

	// 255 as a sound's number is fine as long as its pitch isn't also the end mark's.
	Bytes high;
	AddEntry(high, 255, 0, "high.wav", MakeWav(plain));
	AddEndMark(high);
	entries = 0;
	Check(SoundBank_CheckWholeFile(AtTheWall(high), high.size(), &entries) == SOUND_BANK_REACHED_THE_END_MARK, "a sound numbered 255 reads");
	Check(entries == 1, "number 255 alone isn't the end mark");

	// An empty file with just the end mark holds no sounds, and that's not an error.
	Bytes empty;
	AddEndMark(empty);
	entries = 99;
	Check(SoundBank_CheckWholeFile(AtTheWall(empty), empty.size(), &entries) == SOUND_BANK_REACHED_THE_END_MARK && entries == 0, "only an end mark is zero sounds");
}

static void TestBrokenFiles()
{
	WavParts good;

	// Stops before the end mark: nothing at all, one stray byte, and a good entry with no mark after it.
	Bytes nothing;
	Check(VerdictOf(nothing) == SOUND_BANK_FILE_STOPS_BEFORE_THE_END_MARK, "an empty file");
	Bytes oneByte(1, 0xff);
	Check(VerdictOf(oneByte) == SOUND_BANK_FILE_STOPS_BEFORE_THE_END_MARK, "half an end mark");
	Bytes noMark;
	AddEntry(noMark, 30, 0, "a.wav", MakeWav(good));
	Check(VerdictOf(noMark) == SOUND_BANK_FILE_STOPS_BEFORE_THE_END_MARK, "no end mark after a good entry");

	// A start past the end of the file, as a broken size elsewhere might ask for.
	SoundBankEntry entry;
	Bytes small(4, 0);
	Check(SoundBank_ReadEntry(AtTheWall(small), small.size(), 100, &entry) == SOUND_BANK_FILE_STOPS_BEFORE_THE_END_MARK, "a start beyond the file");

	// The name.
	Bytes noZero;
	noZero.push_back(1);
	noZero.push_back(0);
	AddLetters(noZero, "nameWithNoEnd");
	Check(VerdictOf(noZero) == SOUND_BANK_NAME_NEVER_ENDS, "a name that runs off the end of the file");

	Bytes longName;
	longName.push_back(1);
	longName.push_back(0);
	longName.insert(longName.end(), 500, 'a');
	longName.push_back(0);
	Check(VerdictOf(longName) == SOUND_BANK_NAME_NEVER_ENDS, "a name far longer than any real one");

	Bytes justLongEnough;
	std::string fits(kSoundBankLongestName, 'a');
	AddEntry(justLongEnough, 1, 0, fits.c_str(), MakeWav(good));
	AddEndMark(justLongEnough);
	Check(VerdictOf(justLongEnough) == SOUND_BANK_REACHED_THE_END_MARK, "a name exactly at the limit");

	Bytes tooLong;
	std::string overLimit(kSoundBankLongestName + 1, 'a');
	AddEntry(tooLong, 1, 0, overLimit.c_str(), MakeWav(good));
	AddEndMark(tooLong);
	Check(VerdictOf(tooLong) == SOUND_BANK_NAME_NEVER_ENDS, "a name one past the limit");

	Bytes noName;
	AddEntry(noName, 1, 0, "", MakeWav(good));
	AddEndMark(noName);
	Check(VerdictOf(noName) == SOUND_BANK_NAME_IS_EMPTY, "an empty name");

	Bytes oddLow;
	AddEntry(oddLow, 1, 0, "bad\x01name.wav", MakeWav(good));
	AddEndMark(oddLow);
	Check(VerdictOf(oddLow) == SOUND_BANK_NAME_HAS_ODD_LETTERS, "a control character in a name");

	Bytes oddHigh;
	AddEntry(oddHigh, 1, 0, "bad\xc3name.wav", MakeWav(good));
	AddEndMark(oddHigh);
	Check(VerdictOf(oddHigh) == SOUND_BANK_NAME_HAS_ODD_LETTERS, "a high character in a name");

	// RIFF.
	WavParts notRiff;
	notRiff.riff = "RIFX";
	Check(VerdictOf(OneEntryFile(notRiff)) == SOUND_BANK_NOT_A_RIFF, "the wrong letters instead of RIFF");

	Bytes cutShort;
	cutShort.push_back(1);
	cutShort.push_back(0);
	AddLetters(cutShort, "a.wav");
	cutShort.push_back(0);
	AddLetters(cutShort, "RIF");
	Check(VerdictOf(cutShort) == SOUND_BANK_NOT_A_RIFF, "the file stops inside RIFF");

	Bytes noRiffSize;
	noRiffSize.push_back(1);
	noRiffSize.push_back(0);
	AddLetters(noRiffSize, "a.wav");
	noRiffSize.push_back(0);
	AddLetters(noRiffSize, "RIFF");
	Check(VerdictOf(noRiffSize) == SOUND_BANK_NOT_A_RIFF, "the file stops straight after RIFF, before its size");

	WavParts tooBig;
	tooBig.sizeIsTrue = false;
	tooBig.riffSizeIfNotTrue = 100000;
	Check(VerdictOf(OneEntryFile(tooBig)) == SOUND_BANK_RIFF_RUNS_PAST_THE_FILE, "a RIFF size past the end of the file");

	WavParts wraps;
	wraps.sizeIsTrue = false;
	wraps.riffSizeIfNotTrue = 0xfffffff8u;
	Check(VerdictOf(OneEntryFile(wraps)) == SOUND_BANK_RIFF_RUNS_PAST_THE_FILE, "a RIFF size big enough to wrap round");

	// WAVE and the format block.
	WavParts notWave;
	notWave.wave = "AVI ";
	Check(VerdictOf(OneEntryFile(notWave)) == SOUND_BANK_NOT_A_WAVE, "the wrong letters instead of WAVE");

	WavParts tinyRiff;
	tinyRiff.sizeIsTrue = false;
	tinyRiff.riffSizeIfNotTrue = 2;
	Check(VerdictOf(OneEntryFile(tinyRiff)) == SOUND_BANK_NOT_A_WAVE, "a RIFF too small to hold WAVE");

	WavParts formatLate;
	formatLate.format = "LIST";
	Check(VerdictOf(OneEntryFile(formatLate)) == SOUND_BANK_FORMAT_BLOCK_NOT_FIRST, "the format block isn't first");

	WavParts formatSmall;
	formatSmall.formatSize = 14;
	Check(VerdictOf(OneEntryFile(formatSmall)) == SOUND_BANK_FORMAT_BLOCK_WRONG_SIZE, "a format block of 14 bytes");

	WavParts formatHuge;
	formatHuge.formatSize = 40;
	Check(VerdictOf(OneEntryFile(formatHuge)) == SOUND_BANK_FORMAT_BLOCK_WRONG_SIZE, "a format block of 40 bytes");

	WavParts cutFormat;
	cutFormat.sizeIsTrue = false;
	cutFormat.riffSizeIfNotTrue = 4 + 8 + 10;
	Check(VerdictOf(OneEntryFile(cutFormat)) == SOUND_BANK_FORMAT_BLOCK_WRONG_SIZE, "a RIFF that ends inside the format block");

	WavParts compressed;
	compressed.kind = 2;
	Check(VerdictOf(OneEntryFile(compressed)) == SOUND_BANK_NOT_PLAIN_AUDIO, "compressed audio");

	WavParts monoZero;
	monoZero.channels = 0;
	Check(VerdictOf(OneEntryFile(monoZero)) == SOUND_BANK_ODD_CHANNELS_OR_BITS, "no channels");

	WavParts threeChannels;
	threeChannels.channels = 3;
	Check(VerdictOf(OneEntryFile(threeChannels)) == SOUND_BANK_ODD_CHANNELS_OR_BITS, "three channels");

	WavParts oddBits;
	oddBits.bits = 12;
	Check(VerdictOf(OneEntryFile(oddBits)) == SOUND_BANK_ODD_CHANNELS_OR_BITS, "12 bits");

	WavParts noRate;
	noRate.samplesPerSecond = 0;
	Check(VerdictOf(OneEntryFile(noRate)) == SOUND_BANK_NO_SPEED_GIVEN, "no samples per second");

	WavParts noBytes;
	noBytes.bytesPerSecond = 0;
	Check(VerdictOf(OneEntryFile(noBytes)) == SOUND_BANK_NO_SPEED_GIVEN, "no bytes per second, which the game divides by");

	// The blocks after the format.
	WavParts noAudio;
	noAudio.sizeIsTrue = false;
	noAudio.riffSizeIfNotTrue = 4 + 8 + 16;
	Check(VerdictOf(OneEntryFile(noAudio)) == SOUND_BANK_NO_AUDIO_BLOCK, "a RIFF that ends straight after the format block");

	WavParts audioBig;
	audioBig.audioSize = 100000;
	Check(VerdictOf(OneEntryFile(audioBig)) == SOUND_BANK_BLOCK_RUNS_PAST_THE_RIFF, "audio bigger than the RIFF");

	WavParts audioWraps;
	audioWraps.audioSize = 0xfffffffcu;
	Check(VerdictOf(OneEntryFile(audioWraps)) == SOUND_BANK_BLOCK_RUNS_PAST_THE_RIFF, "an audio size big enough to wrap round");

	WavParts extraBig;
	extraBig.hasExtraBlock = true;
	extraBig.extraBlockSize = 100000;
	Check(VerdictOf(OneEntryFile(extraBig)) == SOUND_BANK_BLOCK_RUNS_PAST_THE_RIFF, "a block before the audio that runs past the RIFF");

	WavParts noDataBlock;
	noDataBlock.hasExtraBlock = true;
	noDataBlock.audio = "fact";
	noDataBlock.audioSize = 6;
	Check(VerdictOf(OneEntryFile(noDataBlock)) == SOUND_BANK_NO_AUDIO_BLOCK, "blocks to the end and none of them audio");

	// One bad entry after a good one still condemns the file.
	Bytes secondBad;
	AddEntry(secondBad, 30, 0, "good.wav", MakeWav(good));
	WavParts badOne;
	badOne.kind = 2;
	AddEntry(secondBad, 31, 0, "bad.wav", MakeWav(badOne));
	AddEndMark(secondBad);
	Check(VerdictOf(secondBad) == SOUND_BANK_NOT_PLAIN_AUDIO, "a bad entry after a good one");
}

// The first cut of every length of a good file must be refused, and must never read past what it's given.
static void TestEveryCut()
{
	WavParts good;
	Bytes file = OneEntryFile(good);
	bool allRefused = true;
	for (size_t size = 0; size + 2 < file.size(); size++)
	{
		Bytes cut(file.begin(), file.begin() + size);
		if (VerdictOf(cut) == SOUND_BANK_REACHED_THE_END_MARK)
		{
			allRefused = false;
		}
	}
	Check(allRefused, "every shortened copy of a good file is refused");
}

static std::string ReadText(const char *path)
{
	std::ifstream in(path, std::ios::binary);
	std::stringstream all;
	all << in.rdbuf();
	return all.str();
}

// /FC in this test's settings makes __FILE__ the whole path of this file, so the repository's top is found from it.
static std::string ReadSource(const char *name)
{
	const std::string path = __FILE__;
	const std::string::size_type tests = path.rfind("tests");
	if (tests == std::string::npos)
	{
		return "";
	}
	return ReadText((path.substr(0, tests) + name).c_str());
}

// The checks above mean nothing if the game doesn't use them, so read the game's own source.
static void TestWiring()
{
	const std::string project = ReadSource("3dc/avp/psndproj.cpp");
	Check(project.find("SoundBank_CheckWholeFile(") != std::string::npos, "the game checks the whole file");
	Check(project.find("LoadNewWeaponSounds();") != std::string::npos, "the game loads the new sounds");
	// Where the game reads its own sounds. Main changed that line to check what it read (39230231), so the check names the line as it is now.
	// It has to be found first: for a line that isn't there, find gives back a number bigger than any place, and this would fail for the wrong reason.
	const std::string::size_type gamesOwn = project.find("if (!ExtractWavFile(soundIndex, fStream))");
	Check(gamesOwn != std::string::npos && project.find("LoadNewWeaponSounds();") > gamesOwn, "the new sounds load after the game's own, so the game's own win");
	Check(project.find("if (GameSounds[wanted.number].loaded)") != std::string::npos, "a number already taken is left alone");
	Check(project.find("\"Revolver_Fire.wav\", SID_WEAPON_REVOLVER_FIRE") != std::string::npos, "the revolver's sound has its number");
	Check(project.find("\"headshot.wav\", SID_WEAPON_HEADSHOT") != std::string::npos, "the headshot sound has its number");
	Check(project.find("\"AutoShotgun4.wav\", SID_WEAPON_SHOTGUN_FIRE") != std::string::npos, "the shotgun's sound has its number");
	Check(project.find("\"SniperRifle_Fire.wav\", SID_WEAPON_SNIPER_FIRE") != std::string::npos, "the sniper's sound has its number");
	Check(project.find("\"shotgun2.wav\"") == std::string::npos, "the mod's pistol sound isn't taken: the player keeps the game's own");

	const std::string header = ReadSource("3dc/avp/psndproj.h");
	Check(header.find("SID_ENDOF_LOADSLOTS=796") != std::string::npos, "the new numbers are kept clear of the slots filled during a level");
	const size_t end = header.find("SID_ENDOF_LOADSLOTS=796");
	const size_t max = header.find("SID_MAXIMUM,");
	for (const char *name : { "SID_WEAPON_REVOLVER_FIRE", "SID_WEAPON_HEADSHOT", "SID_WEAPON_SHOTGUN_FIRE", "SID_WEAPON_SNIPER_FIRE" })
	{
		const size_t at = header.find(name);
		Check(at != std::string::npos && at > end && at < max, name);
	}
}

int main()
{
	TestGoodFiles();
	TestBrokenFiles();
	TestEveryCut();
	TestWiring();

	printf("%d checks, %d failed\n", checks, failures);
	return failures == 0 ? 0 : 1;
}
