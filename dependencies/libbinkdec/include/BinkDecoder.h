/*
 * libbinkdec - Bink video decoder
 * Copyright (C) 2011 Barry Duncan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* This code is based on the Bink decoder from the FFmpeg project which can be obtained from http://www.ffmpeg.org/
 * below is the license from FFmpeg
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Bink video decoder
 * Copyright (c) 2009 Konstantin Shishkov
 * Copyright (C) 2011 Peter Ross <pross@xvid.org>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * Modified 2026-11-03 for the AvP port:
 *   BK-P2: pad the plane buffers for 16x16 blocks and zero-fill them
 */

#ifndef _BinkDecoder_h_
#define _BinkDecoder_h_

#include <stdint.h>
#include <string>
#include "FileStream.h"
#include "BinkAudio.h"
#include "BinkVideo.h"
#include "BitReader.h"

const int kBIKbID = 'BIKb'; // not supported
const int kBIKfID = 'BIKf';
const int kBIKgID = 'BIKg';
const int kBIKhID = 'BIKh';
const int kBIKiID = 'BIKi';

const int kFlagAlpha = 0x00100000;
const int kFlagGray  = 0x00020000;

const int kNumTrees = 16;

// exportable interface
struct BinkHandle
{
	bool isValid;
	int instanceIndex;
};

struct AudioInfo
{
	uint32_t sampleRate;
	uint32_t nChannels;

	uint32_t idealBufferSize;
};

struct ImagePlane
{
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint8_t  *data;
};

typedef ImagePlane YUVbuffer[3];

BinkHandle Bink_Open                 (const char* fileName);
void       Bink_Close                (BinkHandle &handle);
uint32_t   Bink_GetNumAudioTracks    (BinkHandle &handle);
void       Bink_GetFrameSize         (BinkHandle &handle, uint32_t &width, uint32_t &height);
AudioInfo  Bink_GetAudioTrackDetails (BinkHandle &handle, uint32_t trackIndex);
uint32_t   Bink_GetAudioData         (BinkHandle &handle, uint32_t trackIndex, int16_t *data);
float      Bink_GetFrameRate         (BinkHandle &handle);
uint32_t   Bink_GetNumFrames         (BinkHandle &handle);
uint32_t   Bink_GetCurrentFrameNum   (BinkHandle &handle);
uint32_t   Bink_GetNextFrame         (BinkHandle &handle, YUVbuffer yuv);
void       Bink_GotoFrame            (BinkHandle &handle, uint32_t frameNum);

// for internal use only
struct Plane
{
	uint8_t *current;
	uint8_t *last;

	uint32_t width;
	uint32_t height;
	uint32_t pitch;

	bool Init(uint32_t width, uint32_t height)
	{
		// pad to a multiple of 16, plus 16 columns and rows: the 8x8 block grid
		// reaches up to 8 past a chroma plane's size, and a 16x16 (scaled) block
		// at the last block column or row writes 8 more
		uint32_t paddedWidth  = ((width  + 15) & ~15u) + 16;
		uint32_t paddedHeight = ((height + 15) & ~15u) + 16;

		// zero-filled, so a frame never shows uninitialised memory
		current = new uint8_t[paddedWidth * paddedHeight]();
		last    = new uint8_t[paddedWidth * paddedHeight]();
		this->width  = width;
		this->height = height;
		this->pitch  = paddedWidth;

		return true;
	}

	void Swap()
	{
		std::swap(current, last);
	}
};

class BinkDecoder
{
	public:

		uint32_t frameWidth;
		uint32_t frameHeight;

		BinkDecoder();
		~BinkDecoder();

		bool Open(const std::string &fileName);
		uint32_t GetNumFrames();
		uint32_t GetCurrentFrameNum();
		float GetFrameRate();
		void GetNextFrame(YUVbuffer yuv);
		void GotoFrame(uint32_t frameNum);

		AudioInfo GetAudioTrackDetails(uint32_t trackIndex);
		uint32_t GetNumAudioTracks();
		uint32_t GetAudioData(uint32_t trackIndex, int16_t *audioBuffer);

	private:

		BinkCommon::FileStream file;

		uint32_t signature;
		uint32_t fileSize;

		uint32_t nFrames;
		uint32_t largestFrameSize;

		uint32_t fpsDividend;
		uint32_t fpsDivider;
		uint32_t videoFlags;

		bool hasAlpha;
		bool swapPlanes;

		uint32_t currentFrame;

		std::vector<Plane> planes;

		Bundle bundle[BINK_NB_SRC];
		Tree col_high[kNumTrees];   // trees for decoding high nibble in "colours" data type
		int  col_lastval;           // value of last decoded high nibble in "colours" data type

		std::vector<VideoFrame>  frames;
		std::vector<AudioTrack*> audioTracks;
		uint32_t nAudioTracks;

		// huffman tree functions
		void InitTrees();
		void Merge(BinkCommon::BitReader &bits, uint8_t *dst, uint8_t *src, int size);
		void ReadTree(BinkCommon::BitReader &bits, Tree *tree);
		uint8_t GetHuff(BinkCommon::BitReader &bits);
		uint8_t GetHuffSymbol(BinkCommon::BitReader &bits, Tree &tree);

		// video related functions
		void InitBundles();
		void FreeBundles();
		void InitLengths(int width, int bw);
		void ReadBundle(BinkCommon::BitReader &bits, int bundle_num);
		int ReadRuns(BinkCommon::BitReader &bits, Bundle *b);
		int ReadMotionValues(BinkCommon::BitReader &bits, Bundle *b);
		int ReadBlockTypes(BinkCommon::BitReader &bits, Bundle *b);
		int ReadPatterns(BinkCommon::BitReader &bits, Bundle *b);
		int ReadColors(BinkCommon::BitReader &bits, Bundle *b);
		int ReadDCs(BinkCommon::BitReader &bits, Bundle *b, int start_bits, int has_sign);
		int GetValue(int bundle);
		int ReadDCTcoeffs(BinkCommon::BitReader &bits, int32_t block[64], const uint8_t *scan, const int32_t quant_matrices[16][64], int q);
		int ReadResidue(BinkCommon::BitReader &bits, DCTELEM block[64], int masks_count);
		void PutPixels8x8Overlapped(uint8_t *dst, uint8_t *src, int stride);
		int DecodePlane(BinkCommon::BitReader &bits, int plane_idx, int is_chroma);
		int DecodeFrame(BinkCommon::BitReader &bits);
		void PutPixelsTab(uint8_t *dest, uint8_t *prev, uint32_t pitch, uint32_t size);
		void FillBlockTab(uint8_t *dest, uint32_t value, uint32_t pitch, uint32_t size);
		void AddPixels8(uint8_t *dest, DCTELEM *block, uint32_t stride);
		void ClearBlock(DCTELEM *block);
		void VideoPacket(uint32_t packetSize);

		// audio functions
		float GetAudioFloat(BinkCommon::BitReader &bits);
		bool CreateAudioTrack(uint32_t sampleRate, uint16_t flags);
		void AudioPacket(uint32_t trackIndex, uint32_t packetSize);
		void DecodeAudioBlock(uint32_t trackIndex, BinkCommon::BitReader &bits);
};

#endif
