// Copyright (C) 2011 Barry Duncan. All Rights Reserved.
// The original author of this code can be contacted at: bduncan22@hotmail.com

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// DEVELOPERS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Modified 2026-11-03 by Melanikus for the AvP port:
//   colours are looked up in a way that can't go past the end of the colour table, a colour table too big
//   to use is refused, and a file with no header reads as an empty picture
// Modified 2026-11-05 by Melanikus for the AvP port:
//   a picture with more planes than a colour number has bits is refused, so is one packed in a way this can't read,
//   and a no-op in a packed row no longer counts as the bytes of the run before it;
//   a picture too large is refused, so is one that can't hold what its size says, and reading stops where its data does;
//   so is one with no planes, too many parts or a colour table past its end, packed data stops at the most a picture needs, and each refusal has its reason;
//   the pictures one level opens have a limit in bytes
//   every picture costs at least 64 KB of that limit, even one that's no use; one with no size is refused; and once the
//   limit is reached, no more pictures are even opened
// Modified 2026-11-08 by Melanikus for the AvP port:
//   the colour table is a list that frees itself and knows its own size, so a copy of a loader can't free it twice

#include "RimLoader.h"
#include <algorithm>

// How many bytes of pictures have been opened since the level started. See kMostRimBytesPerLevel.
static uint64_t bytesThisLevel = 0;
static bool saidLevelIsFull = false;

void RimLoader_NewLevel()
{
	bytesThisLevel = 0;
	saidLevelIsFull = false;
}

bool RimLoader_SayLevelIsFull()
{
	bool const say = !saidLevelIsFull;
	saidLevelIsFull = true;
	return say;
}

uint64_t RimLoader_BytesThisLevel()
{
	return bytesThisLevel;
}

// Constructor
RimLoader::RimLoader()
{
	chunkSize = 0;
	chunkID   = 0;

	// a file can leave out the part that sets these, so they need a safe value: no size at all, which Open refuses
	memset(&header, 0, sizeof(header));

	destRowStart = 0;
	rowBytes = 0;
	destRowIndex = 0;
	bodyLeft = 0;
	ranOut = false;
	problem = kRimFine;

	// change this when code works
	dest = 0;
	destPitch = 0;
}

const char *RimLoader_ProblemWords(RimProblem problem)
{
	switch (problem)
	{
		case kRimFine:               return "there's nothing wrong with it";
		case kRimNoFile:             return "the file isn't there";
		case kRimTooLarge:           return "it's larger than a picture can be, which is 8192 on a side and 2048 x 2048 in all";
		case kRimHasNoSize:         return "it has no size";
		case kRimPlanesWrong:        return "it has a number of planes that can't be read, which is 1 to 16";
		case kRimPackingWrong:       return "it's packed in a way that can't be read";
		case kRimColourTableTooBig:  return "its colour table has more colours than can be used";
		case kRimColourTablePastEnd: return "its colour table runs past the end of the file";
		case kRimTooManyParts:       return "it has too many parts before its picture";
		case kRimPartsWrong:         return "its parts aren't what a .rim's should be";
		case kRimBodyPastEnd:        return "its picture runs past the end of the file";
		case kRimBodyTooShort:       return "its picture is too short for its size";
		case kRimRanOut:             return "the file ends before the picture does";
		case kRimBodyTooLong:        return "its packed picture takes more than a picture of its size can need";
		case kRimLevelHasTooMany:    return "this level's pictures have reached their limit of 256 MB";
		default:                     return "of a problem with no name";
	}
}

RimProblem RimLoader::Problem() const
{
	return problem;
}

// Notes why, and gives back the false that Open's steps give when they say no
bool RimLoader::Refuse(RimProblem why)
{
	problem = why;

	// A picture that's no use still took the time to open and read, so it pays for that. A file that isn't there took next
	// to none, and a stock level may name one. A picture refused because the level is full has nothing left to pay with.
	if (why != kRimNoFile && why != kRimLevelHasTooMany)
	{
		bytesThisLevel += kLeastRimChargeBytes;   // Open made sure there's room for this before it started
	}

	return false;
}

void RimLoader::GetDimensions(uint32_t &width, uint32_t &height)
{
	width  = header.width;
	height = header.height;
}

bool RimLoader::Decode(uint8_t *dest, uint32_t destPitch)
{
	if (!dest) {
		return false;
	}

	this->dest = dest;
	this->destPitch = destPitch;

	GetBody();

	// a packed picture can say it's larger than what it holds, and that only shows when it's unpacked
	return !ranOut;
}

void RimLoader::OddChunkSizeCheck()
{
	if (chunkSize % 2 != 0)
	{
		// skip a byte
		file.GetByte();
	}
}

bool RimLoader::HandleBMHD()
{
	// read the rest of the header
	chunkSize = file.GetUint32BE();

	header.width  = file.GetUint16BE();
	header.height = file.GetUint16BE();

	header.x = file.GetUint16BE();
	header.y = file.GetUint16BE();
	header.nPlanes = file.GetByte();
	header.masking = file.GetByte();
	header.compression = file.GetByte();
	header.flags       = file.GetByte();
	header.transparentColor = file.GetUint16BE();
	header.xAspect    = file.GetByte();
	header.yAspect    = file.GetByte();
	header.pageWidth  = file.GetUint16BE();
	header.pageHeight = file.GetUint16BE();

	rowBytes = (header.width+7) / 8;

	if ((uint32_t)header.width > kLargestRimSide || (uint32_t)header.height > kLargestRimSide || (uint64_t)header.width * header.height > kMostRimPixels)
	{
		return Refuse(kRimTooLarge);
	}

	// A picture with no planes has no data at all, so a few bytes could still ask for the largest picture there is. None of the game's own has none.
	if (header.nPlanes == 0)
	{
		return Refuse(kRimPlanesWrong);
	}

	// Both of these come from the file. A colour number is made of one bit from each plane, and it only has room for 16. The game's own pictures have 11 at most.
	if (header.nPlanes > kMostPlanes)
	{
		return Refuse(kRimPlanesWrong);
	}

	// There are only two ways of packing a row that this can read. All of the game's own pictures use one of them.
	if (header.compression > kCompressionByteRun1)
	{
		return Refuse(kRimPackingWrong);
	}

	return true;
}

bool RimLoader::HandleCMAP()
{
	// read the colour map size
	chunkSize = file.GetUint32BE();

	uint32_t nEntries = chunkSize / 3;

	// the size comes from the file, so it could be far more memory than the game could ever get
	if (nEntries > kMaxColours)
	{
		return Refuse(kRimColourTableTooBig);
	}

	// Each colour is three reads, so one that isn't there is no reason to read them from nothing
	if ((uint64_t)nEntries * 3 > file.BytesLeft())
	{
		return Refuse(kRimColourTablePastEnd);
	}

	// a file can hold more than one table: the last one counts, and takes the place of the one before it
	ColourRegister const black = { 0, 0, 0 };
	colourMap.assign(nEntries, black);

	// read the values in
	for (uint32_t i = 0; i < nEntries; i++)
	{
		colourMap[i].red   = file.GetByte();
		colourMap[i].green = file.GetByte();
		colourMap[i].blue  = file.GetByte();
	}

	return true;
}

bool RimLoader::Open(const std::string &fileName)
{
	// With no room for even the smallest picture, nothing can fit, so there's no reason to spend any time on this one.
	if (kMostRimBytesPerLevel - bytesThisLevel < kLeastRimChargeBytes)
	{
		return Refuse(kRimLevelHasTooMany);
	}

	file.Open(fileName, FileStream::FileRead);
	if (!file.IsGood())
	{
		// unable to open file
		return Refuse(kRimNoFile);
	}

	// we want to search to the BODY tag
	bool gotBody = false;
	int parts = 0;

	while (!gotBody)
	{
		if (++parts > kMostRimParts)
		{
			return Refuse(kRimTooManyParts);
		}

		chunkID = file.GetUint32BE();

		switch (chunkID)
		{
			case LIST:
				chunkSize = file.GetUint32BE();
				// this should be first in the file! CHECKME
				break;
			case ILBM:
			{
				chunkID = file.GetUint32BE();
				switch (chunkID)
				{
					case PROP:
						chunkSize = file.GetUint32BE();
						break;
					case TRAN:
						// skip the tran data
						file.Seek(4+8, FileStream::SeekCurrent);
						break;
					case BMHD:
						if (!HandleBMHD())
						{
							return false;
						}
						OddChunkSizeCheck();
						break;
					default:
						// ERROR!
						return Refuse(kRimPartsWrong);
				}
				break;
			}
			case FORM:
				chunkSize = file.GetUint32BE();
				break;
			case CMAP:
				if (!HandleCMAP())
				{
					return false;
				}
				OddChunkSizeCheck();
				break;
			case BODY:
				chunkSize = file.GetUint32BE();

				// A picture with no size has nothing in it to show. The game has none, and a texture can't be made for one. This is checked
				// here, where every picture has to come through, and not where the size is read, because a file can leave that part out.
				if (header.width == 0 || header.height == 0)
				{
					return Refuse(kRimHasNoSize);
				}

				if (!BodyCanHoldThePicture() || !LevelHasRoomForThePicture())
				{
					return false;
				}

				gotBody = true;
				break;
			default:
				// ERROR!
				return Refuse(kRimPartsWrong);
		}
	}

	return true;
}

// Whether the picture's data can be as long as its size says, before anything is allocated for it. A few bytes could otherwise ask for a very large picture, and every row of it would then
// be read from nothing, one byte at a time. A plain picture needs all of it. A packed one can make 128 bytes from 2 at best. The data also has to be in this file: read past the end of a
// file inside a fastfile, and it would be the next file.
bool RimLoader::BodyCanHoldThePicture()
{
	uint64_t const needed = (uint64_t)rowBytes * header.nPlanes * header.height;

	if (chunkSize > file.BytesLeft())
	{
		return Refuse(kRimBodyPastEnd);
	}

	if (header.compression == kCompressionNone ? chunkSize < needed : (uint64_t)chunkSize * 64 < needed)
	{
		return Refuse(kRimBodyTooShort);
	}

	bodyLeft = (std::min)((uint64_t)chunkSize, MostBodyBytes());
	ranOut = false;
	return true;
}

// A picture that has passed every other check is charged its real size here, before the card is asked for anything. One refused before this has paid the smallest charge, in Refuse. The sum can't
// go below zero, because what's been charged is never more than the limit: this looks before it charges, Open looks before Refuse can charge, the first refusal ends Open, and nothing can refuse
// a picture after this.
bool RimLoader::LevelHasRoomForThePicture()
{
	uint64_t const bytes = (std::max)((uint64_t)header.width * header.height * 4, kLeastRimChargeBytes);

	if (bytes > kMostRimBytesPerLevel - bytesThisLevel)
	{
		return Refuse(kRimLevelHasTooMany);
	}

	bytesThisLevel += bytes;
	return true;
}

// The most of its data a picture of this size can use. A packed one could take twice its plain size, with every byte in a run of its own, which takes 2. Nothing honest takes more: the game's
// own use 0.9 of their plain size at most. Only no-ops, which add nothing to the picture, could go on past that, and each is a read, so a file of them that zips down to nothing could otherwise
// keep this busy for as long as it liked. A plain picture never gets this far: it reads its plain size and stops, and BodyCanHoldThePicture has already seen that it has that much.
uint64_t RimLoader::MostBodyBytes() const
{
	return (uint64_t)rowBytes * header.nPlanes * header.height * 2;
}

// The next byte of the picture's data, or 0 and a note that there's none left. Nothing reads past this.
uint8_t RimLoader::NextBodyByte()
{
	if (bodyLeft == 0)
	{
		problem = (chunkSize > MostBodyBytes()) ? kRimBodyTooLong : kRimRanOut;
		ranOut = true;
		return 0;
	}

	bodyLeft--;
	return file.GetByte();
}

// do the byterun1 decompression here
uint32_t RimLoader::GetUnpackedBytes()
{
	uint32_t bytesDone = 0;
	uint32_t length    = 0;

	while (bytesDone < rowBytes)
	{
		// get the value we'll work with
		int8_t val = NextBodyByte();

		if (val == -128)
		{
			// a no-op adds nothing to the row, so it counts as nothing
			length = 0;
		}
		else if (val >= 0)
		{
			length = val+1;

			length = (std::min)(length, rowBytes-bytesDone);

			// get next n bytes literally
			for (uint32_t i = 0; i < length; i++)
			{
				destRow[destRowIndex++] = NextBodyByte();
			}
		}
		else if (val > -128)
		{
			// replicate the next byte -val+1 times
			length = (-val)+1;

			length = (std::min)(length, rowBytes-bytesDone);

			// get next byte and memset it to dest times length
			memset(&destRow[destRowIndex], NextBodyByte(), length);
			destRowIndex += length;
		}

		bytesDone += length;
	}

	return bytesDone;
}

uint32_t RimLoader::GetBody()
{
	uint32_t bytesDone = 0;

	// destination row buffer
	destRow.resize(rowBytes * header.nPlanes);

	for (uint16_t y = 0; y < header.height && !ranOut; y++)
	{
		// reset
		destRowIndex = 0;

		bytesDone += GetScanLine();
	}

	return bytesDone;
}

// Looks up a colour in the picture's colour table. The file sets both the size of the table and the numbers looked up
// in it, so a number could be past the end of the table, or there could be no table at all. Anything not in the table
// comes out black. All 1028 of the game's own pictures stay inside their tables, so this never changes how they look.
ColourRegister RimLoader::GetColour(uint32_t index) const
{
	ColourRegister colour = { 0, 0, 0 };

	if (index < colourMap.size())
	{
		colour = colourMap[index];
	}

	return colour;
}

// call once per scanline
uint32_t RimLoader::GetScanLine()
{
	uint32_t bytesDone = 0;

	for (uint8_t p = 0; p < header.nPlanes; p++)
	{
		bytesDone += GetRow();
	}

	// a row that ran out isn't written: what came before it is all the picture has
	if (ranOut)
	{
		return bytesDone;
	}

/*
	after parsing and uncompacting if necessary, you will have N planes of pixel data.  Color register used for
	each pixel is specified by looking at each pixel thru the planes.  I.e., if you have 5 planes, and the bit for
	a particular pixel is set in planes
	0 and 3:

		   PLANE     4 3 2 1 0
		   PIXEL     0 1 0 0 1

	then that pixel uses color register binary 01001 = 9
*/
	uint8_t *thisDest = dest;

	for (uint16_t x = 0; x < header.width; x++)
	{
		uint16_t pixel = 0;

		// 128, then 64, 32, 16 etc (and wrap back to 128)
		uint8_t bitMask = 128 >> (x % 8);

		// look through each plane
		for (uint8_t p = 0; p < header.nPlanes; p++)
		{
			uint8_t src = destRow[(x / 8) + (p * rowBytes)];

			if (src & bitMask)
			{
				// set the bit at position p to 'on'
				pixel |= 1 << p;
			}
		}

		ColourRegister temp = GetColour(pixel);

		// default alpha to 255
		uint8_t alpha = 255;

		if (header.masking == kMaskTransparentColour)
		{
			ColourRegister transColour = GetColour(header.transparentColor);
			if ((transColour.red   == temp.red)   &&
				(transColour.green == temp.green) &&
				(transColour.blue  == temp.blue))
			{
				alpha = 0;
			}
		}

		// write colour values to output buffer (in BGR order)
		(*thisDest++) = temp.blue;
		(*thisDest++) = temp.green;
		(*thisDest++) = temp.red;
		(*thisDest++) = alpha;
	}

	dest += destPitch;
	destRowIndex = 0;

	return bytesDone;
}

// this only exists so I can modify dest buffer without touching the original pointer (due to the copy)
// remove this and handle this properly
uint32_t RimLoader::GetUncompressedBytes()
{
	for (uint32_t i = 0; i < rowBytes; i++)
	{
		destRow[destRowIndex++] = NextBodyByte();
	}

	return rowBytes;
}

// called for each bit plane
uint32_t RimLoader::GetRow()
{
	if (!header.compression)
	{
		return GetUncompressedBytes();
	}
	else
	{
		return GetUnpackedBytes();
	}
}
