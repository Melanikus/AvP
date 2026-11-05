// Written 2026-11-04 by Melanikus for the AvP port.
//
// Finding the sound inside a WAV file. It sits in its own small file so that tests/wav_chunks_test.cpp can run the
// same code the game runs, without needing the rest of the sound system.

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

#endif
