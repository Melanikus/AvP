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
//   colours are looked up in a way that can't go past the end of the colour table
// Modified 2026-11-05 by Melanikus for the AvP port:
//   says the most planes a picture can have, the largest a picture can be, the most parts it can have, and why one was refused;
//   says the most bytes of pictures one level can open, and the least any picture costs of them

#pragma once

#include "FileStream.h"
#include <string>
#include <vector>
#include <assert.h>

struct BitMapHeader
{
	uint16_t width;
	uint16_t height;                 // raster width & height in pixels
	int16_t  x, y;                   // pixel position for this image
	uint8_t  nPlanes;                // # source bitplanes
	uint8_t  masking;
	uint8_t  compression;
	uint8_t  flags;
	uint16_t transparentColor;       // transparent "color number" (sort of)
	uint8_t  xAspect, yAspect;       // pixel aspect, a ratio width : height
	int16_t  pageWidth, pageHeight;  // source "page" size in pixels
};

// size of BitMapHeader struct
const int kBMHDSize = 22;

// compression types
const int kCompressionNone     = 0;
const int kCompressionByteRun1 = 1;

// mask types
const int kMaskNone = 0;
const int kMask = 1;
const int kMaskTransparentColour = 2;
const int kMaskLasso = 3;

// Each pixel is a number that fits in 16 bits, so a colour table can never use more colours than this. The biggest
// table in the game's own pictures has 1769.
const uint32_t kMaxColours = 65536;
const int kMostPlanes = 16;     // a colour number is held in 16 bits, one from each plane

// The size of a picture comes from its file, and memory and time are spent on the strength of it. The largest of the game's own are 640 x 480 by area and 7392 tall, in a font.
const uint32_t kLargestRimSide = 8192;
const uint64_t kMostRimPixels = 2048 * 2048;

// The most parts a picture can have, up to and including its data. The game's own all have 7. Each takes a read or two whatever it says, so a long file of nothing but parts would keep this
// busy for as long as the file lasted.
const int kMostRimParts = 16;

// The most bytes of pictures one level can open, at 4 bytes a pixel. A small file can still make a large picture: about 8 KB of packed data makes a 16 MB texture. Each is fine alone, but a level
// can name as many as it likes. Past this a picture counts as missing, and the log says why. Every picture the game has, all at once, comes to 72.9 MB, and its largest level names 38 of them,
// so this leaves plenty of room for a level with better pictures: 64 at 1024 x 1024.
const uint64_t kMostRimBytesPerLevel = 256 * 1024 * 1024;

// The least any picture costs of that, however small it is, and even if it turns out to be no use. Without this, a level
// could name a million tiny pictures, or one bad picture a million ways, and they'd cost next to nothing, while each one
// takes time to find and open, and each one kept makes the search for the next one longer. So a level gets 4,096 at most.
// The game has 1,028 in all, and only 32 of them are small enough to pay this, so they come to 74.0 MB instead of 72.9.
const uint64_t kLeastRimChargeBytes = 64 * 1024;

// Called as each level starts loading, and as the menus load, which gives each the whole allowance: what one level spent mustn't stop the menus having their pictures. A tool that reads many pictures calls it before each one, because its work isn't a level.
void RimLoader_NewLevel();
uint64_t RimLoader_BytesThisLevel();

// True the first time it's asked since the allowance began, and false after that, so that a level naming a million pictures
// past its limit makes one line in the log and not a million.
bool RimLoader_SayLevelIsFull();

// Why a picture wasn't used. Each has its own words for the log, so someone who made one can find out what's wrong with it.
enum RimProblem
{
	kRimFine,
	kRimNoFile,
	kRimTooLarge,
	kRimHasNoSize,
	kRimPlanesWrong,
	kRimPackingWrong,
	kRimColourTableTooBig,
	kRimColourTablePastEnd,
	kRimTooManyParts,
	kRimPartsWrong,
	kRimBodyPastEnd,
	kRimBodyTooShort,
	kRimRanOut,
	kRimBodyTooLong,
	kRimLevelHasTooMany,
	kRimProblemCount
};

const char *RimLoader_ProblemWords(RimProblem problem);

struct ColourRegister
{
	uint8_t red;
	uint8_t green;
	uint8_t blue;
};

// chunk IDs
const int LIST = 'LIST';
const int FORM = 'FORM';
const int CAT  = 'CAT ';

const int ILBM = 'ILBM';
const int BMHD = 'BMHD';
const int CMAP = 'CMAP';
const int BODY = 'BODY';
const int PROP = 'PROP';
const int TRAN = 'TRAN';

class RimLoader
{
	public:
		RimLoader::RimLoader();
		RimLoader::~RimLoader();
		bool Open(const std::string &fileName);
		bool Decode(uint8_t *dest, uint32_t destPitch);
		void GetDimensions(uint32_t &width, uint32_t &height);
		RimProblem Problem() const;     // why Open or Decode said no

	private:
		bool Refuse(RimProblem why);
		uint64_t MostBodyBytes() const;
		uint32_t GetUnpackedBytes();
		bool BodyCanHoldThePicture();
		bool LevelHasRoomForThePicture();
		uint8_t NextBodyByte();
		uint32_t GetBody();
		uint32_t GetScanLine();
		uint32_t GetUncompressedBytes();
		uint32_t GetRow();
		bool HandleBMHD();
		bool HandleCMAP();
		void OddChunkSizeCheck();
		ColourRegister GetColour(uint32_t index) const;

		FileStream file;

		uint32_t chunkSize;
		uint32_t chunkID;

		BitMapHeader header;
		ColourRegister *colourMap;
		uint32_t numColours;             // how many colours colourMap holds
		uint8_t  *src;
		uint8_t  *destRowStart;
		uint32_t rowBytes;

		std::vector<uint8_t> destRow;
		uint64_t bodyLeft;      // how much of the picture's data is still to be read
		bool ranOut;            // whether it ran out before the picture was finished
		RimProblem problem;
		uint32_t destRowIndex;

		// change this when code works
		uint8_t  *dest;
		uint32_t destPitch;
};
