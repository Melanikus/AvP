// Written 2026-11-06 by Melanikus for the AvP port.
//
// The new guns' sounds come in a small file of their own, separate from the game's own sound file. This reads one
// entry of such a file and says whether it's safe to use. It doesn't touch the sound system, so a test can run it
// on any bytes at all, broken ones included.
//
// A sound file is a row of entries, and then two bytes of 255 that mark the end. Each entry is:
//   one byte, the sound's number
//   one byte, how far to shift its pitch
//   its name, with a zero after it
//   the sound itself, a whole .wav file: the letters RIFF, a size, the letters WAVE, then named blocks
//
// The game's reader for the .wav part (ExtractWavFile) trusts what it's told: a size that runs past the end of the
// file, or a claim of no bytes per second, would crash it. So everything it relies on is checked here first, in the
// same order it reads them. The file is treated as hostile, like everything else from outside the code.

#ifndef WEAPON_SOUND_BANK_H
#define WEAPON_SOUND_BANK_H

#include <stddef.h>
#include <stdint.h>

// What came of reading one entry. Every kind of refusal has its own name, so a test can say exactly which check
// stopped a broken file, and a later check can't quietly hide a missing earlier one.
enum SoundBankVerdict
{
	SOUND_BANK_ENTRY_IS_GOOD,
	SOUND_BANK_REACHED_THE_END_MARK,
	SOUND_BANK_FILE_STOPS_BEFORE_THE_END_MARK,
	SOUND_BANK_NAME_NEVER_ENDS,
	SOUND_BANK_NAME_IS_EMPTY,
	SOUND_BANK_NAME_HAS_ODD_LETTERS,
	SOUND_BANK_NOT_A_RIFF,
	SOUND_BANK_RIFF_RUNS_PAST_THE_FILE,
	SOUND_BANK_NOT_A_WAVE,
	SOUND_BANK_FORMAT_BLOCK_NOT_FIRST,
	SOUND_BANK_FORMAT_BLOCK_WRONG_SIZE,
	SOUND_BANK_NOT_PLAIN_AUDIO,
	SOUND_BANK_ODD_CHANNELS_OR_BITS,
	SOUND_BANK_NO_SPEED_GIVEN,
	SOUND_BANK_BLOCK_RUNS_PAST_THE_RIFF,
	SOUND_BANK_NO_AUDIO_BLOCK
};

// Where one good entry sits in the file.
struct SoundBankEntry
{
	int    number;         // the sound's number, as the file gives it
	int    pitch;          // the pitch shift, as the file gives it
	size_t nameAt;         // where its name starts: the game's reader starts here too
	size_t nameLength;
	size_t nextEntryAt;    // where the entry after this one starts
};

// The longest name we accept. The game's own file and the mod's stay well under it.
const size_t kSoundBankLongestName = 128;

inline uint32_t SoundBank_ReadLittle32(const uint8_t *bytes)
{
	return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

inline uint16_t SoundBank_ReadLittle16(const uint8_t *bytes)
{
	return (uint16_t)(bytes[0] | (bytes[1] << 8));
}

inline bool SoundBank_LettersAre(const uint8_t *bytes, const char *letters)
{
	return bytes[0] == (uint8_t)letters[0] && bytes[1] == (uint8_t)letters[1] && bytes[2] == (uint8_t)letters[2] && bytes[3] == (uint8_t)letters[3];
}

// Reads the entry that starts at "at". All sizes are compared by what's left, never by adding to a position, so a
// huge size can't wrap round to a small number and slip through.
inline SoundBankVerdict SoundBank_ReadEntry(const uint8_t *file, size_t fileSize, size_t at, SoundBankEntry *entry)
{
	if (at > fileSize || fileSize - at < 2)
	{
		return SOUND_BANK_FILE_STOPS_BEFORE_THE_END_MARK;
	}

	const int number = file[at];
	const int pitch  = (signed char)file[at + 1];

	if (file[at] == 0xff && file[at + 1] == 0xff)
	{
		return SOUND_BANK_REACHED_THE_END_MARK;
	}

	// The name, up to its zero. It goes into text the game prints, so only plain letters are allowed.
	const size_t nameAt = at + 2;
	size_t nameLength = 0;
	while (true)
	{
		if (nameAt + nameLength >= fileSize || nameLength > kSoundBankLongestName)
		{
			return SOUND_BANK_NAME_NEVER_ENDS;
		}
		const uint8_t letter = file[nameAt + nameLength];
		if (letter == 0)
		{
			break;
		}
		if (letter < 0x20 || letter > 0x7e)
		{
			return SOUND_BANK_NAME_HAS_ODD_LETTERS;
		}
		nameLength++;
	}
	if (nameLength == 0)
	{
		return SOUND_BANK_NAME_IS_EMPTY;
	}

	// The .wav: RIFF, then its size, which counts everything after the size itself.
	const size_t riffAt = nameAt + nameLength + 1;
	if (fileSize - riffAt < 8 || !SoundBank_LettersAre(file + riffAt, "RIFF"))
	{
		return SOUND_BANK_NOT_A_RIFF;
	}
	const uint32_t riffSize = SoundBank_ReadLittle32(file + riffAt + 4);
	const size_t insideAt = riffAt + 8;
	if (riffSize > fileSize - insideAt)
	{
		return SOUND_BANK_RIFF_RUNS_PAST_THE_FILE;
	}
	const size_t insideEnd = insideAt + riffSize;

	// WAVE, and straight after it the block that describes the sound: the game's reader expects that order.
	if (insideEnd - insideAt < 4 || !SoundBank_LettersAre(file + insideAt, "WAVE"))
	{
		return SOUND_BANK_NOT_A_WAVE;
	}
	size_t place = insideAt + 4;
	if (insideEnd - place < 8 || !SoundBank_LettersAre(file + place, "fmt "))
	{
		return SOUND_BANK_FORMAT_BLOCK_NOT_FIRST;
	}
	const uint32_t formatSize = SoundBank_ReadLittle32(file + place + 4);
	place += 8;
	if ((formatSize != 16 && formatSize != 18) || insideEnd - place < formatSize)
	{
		return SOUND_BANK_FORMAT_BLOCK_WRONG_SIZE;
	}
	const uint16_t kind       = SoundBank_ReadLittle16(file + place);
	const uint16_t channels   = SoundBank_ReadLittle16(file + place + 2);
	const uint32_t samples    = SoundBank_ReadLittle32(file + place + 4);
	const uint32_t bytesPerSec = SoundBank_ReadLittle32(file + place + 8);
	const uint16_t bits       = SoundBank_ReadLittle16(file + place + 14);
	if (kind != 1 || (formatSize == 18 && SoundBank_ReadLittle16(file + place + 16) != 0))
	{
		return SOUND_BANK_NOT_PLAIN_AUDIO;
	}
	if ((channels != 1 && channels != 2) || (bits != 8 && bits != 16))
	{
		return SOUND_BANK_ODD_CHANNELS_OR_BITS;
	}
	// The game divides by the bytes per second to work out how long the sound lasts.
	if (samples == 0 || bytesPerSec == 0)
	{
		return SOUND_BANK_NO_SPEED_GIVEN;
	}
	place += formatSize;

	// Walks the other blocks until the audio itself, stepping by exactly the sizes the game's reader steps by.
	while (true)
	{
		if (insideEnd - place < 8)
		{
			return SOUND_BANK_NO_AUDIO_BLOCK;
		}
		const uint32_t blockSize = SoundBank_ReadLittle32(file + place + 4);
		const bool isAudio = SoundBank_LettersAre(file + place, "data");
		place += 8;
		if (blockSize > insideEnd - place)
		{
			return SOUND_BANK_BLOCK_RUNS_PAST_THE_RIFF;
		}
		if (isAudio)
		{
			break;
		}
		place += blockSize;
	}

	entry->number = number;
	entry->pitch = pitch;
	entry->nameAt = nameAt;
	entry->nameLength = nameLength;
	entry->nextEntryAt = insideEnd;
	return SOUND_BANK_ENTRY_IS_GOOD;
}

// Reads the whole file and says whether every entry is good and the end mark is there. A file with one bad entry
// isn't used at all: a broken file can't be trusted about the parts that happen to look right.
inline SoundBankVerdict SoundBank_CheckWholeFile(const uint8_t *file, size_t fileSize, int *entries)
{
	size_t at = 0;
	int count = 0;
	while (true)
	{
		SoundBankEntry entry;
		const SoundBankVerdict verdict = SoundBank_ReadEntry(file, fileSize, at, &entry);
		if (verdict == SOUND_BANK_REACHED_THE_END_MARK)
		{
			if (entries)
			{
				*entries = count;
			}
			return verdict;
		}
		if (verdict != SOUND_BANK_ENTRY_IS_GOOD)
		{
			return verdict;
		}
		at = entry.nextEntryAt;
		count++;
	}
}

#endif
