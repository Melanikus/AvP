// Written 2026-11-04 by Melanikus for the AvP port.
//
// Finding the sound inside a WAV file, and reading and checking its header (added 2026-11-27, A WAV'S HEADER below). It sits in its own small
// file so that tests/wav_chunks_test.cpp can run the same code the game runs, without needing the rest of the sound system.

#ifndef _wav_chunks_h_
#define _wav_chunks_h_

#include "FileStream.h"

/*
	A WAV file is a row of "chunks". Each one starts with a four-letter name and its size, and the sound itself is in
	the chunk named 'data'. This moves along the row until it reaches that chunk and stops just after the name, so the
	next thing to read is the size of the sound.

	It gives back false if the file ends first. A damaged file may have no 'data' chunk at all, and without that check
	this loop would go round for ever. Every turn of the loop moves forward, because a chunk's size can't be negative,
	so it always reaches the end of the file.
*/
inline bool FindWavDataChunk(FileStream &stream)
{
	while (1)
	{
		uint32_t chunkName = stream.GetUint32BE();

		if (stream.RanOutOfData())
		{
			return false;
		}

		if (chunkName == 'data')
		{
			return true;
		}

		uint32_t chunkSize = stream.GetUint32LE();
		stream.Seek(chunkSize, FileStream::SeekCurrent);
	}
}

/*
	A WAV'S HEADER. (Added 2026-11-27 by Melanikus (Drake) for the AvP port; Tao 1 and 3.) A sound in a mod, or in a sound bank, is hostile input as a
	level is, and each loader used to take the header as it came:
	- the format tag wasn't looked at, so sound that isn't plain PCM was handed on as if it were;
	- channels and sample sizes other than the ones the game plays reached an assert (GetALFormat), and so did an 18-byte format with extra bytes in it;
	- the sound's size was checked against the file only after its memory was asked for, and a sound cut short reached another assert.
	So the header is read here, in one place for both loaders (openal_audio.cpp: LoadWavFile for a WAV file of its own, ExtractWavFile for one in a
	sound bank). Each thing it says is checked against what the game plays and against the bytes the file really has, before any memory is asked
	for. A WAV that fails is refused, with its reason in the log, and that sound alone stays silent.

	Every one of the game's own 1,656 WAVs passes. Each is plain PCM, of 1 or 2 channels and 8 or 16 bits, at 6,000 to 44,100 samples a second;
	its block size and bytes a second agree with those; and its sound is a whole number of blocks, inside its RIFF chunk, which is inside its file
	(each of the 1,520 packed in the fastfiles is exactly as long as its RIFF chunk says, and the 136 in the sound banks fit the bank).
*/

// The slowest and fastest a sound may play, in samples a second: the sound system's own limits (FREQUENCY_MINPLAT and FREQUENCY_MAXPLAT in
// psndplat.h; openal_audio.cpp checks that they stay the same)
uint32_t const kWavSlowestRate = 100;
uint32_t const kWavFastestRate = 100000;

// What a WAV's format chunk says: the 16 bytes every one has
struct WavFormat
{
	uint16_t formatTag;        // what kind of sound: 1 is plain PCM, the only kind the game plays
	uint16_t channels;
	uint32_t samplesPerSec;
	uint32_t avgBytesPerSec;   // the game works out how long a sound is from this
	uint16_t blockAlign;       // the bytes of one sample of all its channels
	uint16_t bitsPerSample;
};

// What WavHeader_Read found
struct WavHeader
{
	bool riffSizeRead;         // its RIFF chunk's size was read, so a sound bank can step over it to the next sound, at riffStart + riffSize
	int64_t riffStart;         // where the RIFF chunk's contents start in the file, just after its size
	uint32_t riffSize;         // the size the file gives them
	WavFormat format;
	int64_t dataStart;         // where the sound starts
	uint32_t dataBytes;        // and its size
};

// Why the game won't play sound of this format and size, or 0 when it will
inline char const *WavFormat_Refusal(WavFormat const &format, uint32_t dataBytes)
{
	if (format.formatTag != 1)
	{
		return "it isn't plain PCM sound (its format tag isn't 1)";
	}

	if (format.channels != 1 && format.channels != 2)
	{
		return "it has neither 1 channel nor 2";
	}

	if (format.bitsPerSample != 8 && format.bitsPerSample != 16)
	{
		return "its samples are neither 8 nor 16 bits";
	}

	if (format.samplesPerSec < kWavSlowestRate || format.samplesPerSec > kWavFastestRate)
	{
		return "its rate isn't from 100 to 100,000 samples a second";
	}

	// after those, a block is 1 to 4 bytes and a second at most 400,000, so neither sum below can run over
	if (format.blockAlign != format.channels * (format.bitsPerSample / 8))
	{
		return "its block size isn't its channels times its bytes a sample";
	}

	if (format.avgBytesPerSec != format.samplesPerSec * format.blockAlign)
	{
		return "its bytes a second aren't its rate times its block size";
	}

	if (dataBytes == 0)
	{
		return "it holds no sound";
	}

	if (dataBytes % format.blockAlign != 0)
	{
		return "its sound isn't a whole number of blocks";
	}

	return 0;
}

// Reads a WAV's header, from the "RIFF" at the stream's place to the start of its sound, and checks it. Gives back 0, with the stream at the sound,
// or why the WAV is refused. A sound of more than mostBytes, or more than the file has left, is refused before any memory is asked for it.
inline char const *WavHeader_Read(FileStream &stream, uint32_t mostBytes, WavHeader *header)
{
	*header = WavHeader();

	if (stream.GetUint32BE() != 'RIFF')
	{
		return "it doesn't start with RIFF";
	}

	header->riffSize = stream.GetUint32LE();

	if (stream.RanOutOfData())
	{
		return "it ends inside its first 8 bytes";
	}

	header->riffSizeRead = true;
	header->riffStart = stream.GetCurrentPos();

	// a sound bank steps over the chunk to find the next sound, so it has to be inside the file
	if (header->riffSize > stream.BytesLeft())
	{
		return "its RIFF chunk says it runs past the end of the file";
	}

	if (stream.GetUint32BE() != 'WAVE')
	{
		return "it isn't a WAVE file";
	}

	if (stream.GetUint32BE() != 'fmt ')
	{
		return "its first chunk isn't its format";
	}

	uint32_t const formatBytes = stream.GetUint32LE();

	header->format.formatTag = stream.GetUint16LE();
	header->format.channels = stream.GetUint16LE();
	header->format.samplesPerSec = stream.GetUint32LE();
	header->format.avgBytesPerSec = stream.GetUint32LE();
	header->format.blockAlign = stream.GetUint16LE();
	header->format.bitsPerSample = stream.GetUint16LE();

	// plain PCM's format is 16 bytes; 18 is allowed when the two more, the size of anything extra, say there's nothing
	if (formatBytes == 18)
	{
		if (stream.GetUint16LE() != 0)
		{
			return "its format has extra bytes, which plain PCM never has";
		}
	}
	else if (formatBytes != 16)
	{
		return "its format isn't the 16 bytes of plain PCM";
	}

	if (stream.RanOutOfData())
	{
		return "it ends inside its format";
	}

	if (!FindWavDataChunk(stream))
	{
		return "it has no sound (no data chunk)";
	}

	header->dataBytes = stream.GetUint32LE();

	if (stream.RanOutOfData())
	{
		return "it ends inside the size of its sound";
	}

	header->dataStart = stream.GetCurrentPos();

	if (header->dataBytes > mostBytes)
	{
		return "it holds more sound than the game takes from one WAV";
	}

	if (header->dataBytes > stream.BytesLeft())
	{
		return "it says it holds more sound than the file has";
	}

	if (header->dataStart + (int64_t)header->dataBytes > header->riffStart + (int64_t)header->riffSize)
	{
		return "its sound runs past the end of its RIFF chunk";
	}

	return WavFormat_Refusal(header->format, header->dataBytes);
}


#endif
