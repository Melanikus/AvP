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
 * Modified 2026-11-03 by Melanikus for the AvP port:
 *   BK-P1: reject frame sizes of 0 or above 7680 x 4800 in Open
 *   BK-P2: give the alpha plane the full frame size
 *   BK-P5: return at most the 3 planes a YUVbuffer holds
 *   BK-P6: reject more than 256 audio tracks
 *   BK-P7: cap the audio buffer size; fail Open when an allocation fails
 *   BK-P8: reject 0 frames and a frame index that does not increase
 *   BK-P9: reject a zero frame rate numerator or denominator
 *   BK-P11: skip a frame whose audio packet is larger than the frame
 *   BK-P15: free the bundle buffers in the destructor
 *   BK-P16: check handles and track indexes in the exports; stop at the last frame
 *   BK-P17: fail Open when an audio track can't be created
 *   BK-P20: keep C++ exceptions inside Bink_Open and Bink_GetNextFrame
 *   BK-P21: reject a header that claims more data than the file has
 */

#include "BinkDecoder.h"
#include "LogError.h"
#include "FFmpeg_includes.h"

std::vector<class BinkDecoder*> classInstances;

// the decoder a handle refers to, or 0 for an invalid or closed handle
static BinkDecoder *GetDecoder(const BinkHandle &handle)
{
	if (handle.instanceIndex < 0 || (size_t)handle.instanceIndex >= classInstances.size())
		return 0;

	return classInstances[handle.instanceIndex];
}

BinkHandle Bink_Open(const char* fileName)
{
	BinkHandle newHandle;
	newHandle.isValid = false;
	newHandle.instanceIndex = -1;

	BinkDecoder *newDecoder = 0;

	// no C++ exception may leave the DLL: a failed allocation fails the open, and the
	// decoder (never registered) is deleted together with whatever Open created
	try
	{
		newDecoder = new BinkDecoder();
		if (!newDecoder->Open(fileName))
		{
			delete newDecoder;
			return newHandle;
		}

		// add instance to global instance vector
		classInstances.push_back(newDecoder);
	}
	catch (...)
	{
		delete newDecoder;
		return newHandle;
	}

	// loaded ok, make handle valid
	newHandle.isValid = true;
#if 0
	// find a free slot if available
	for (int i = 0; i < classInstances.size(); i++)
	{
		if (!classInstances[i])
		{
			class
		}
	}
#endif

	// get a handle ID
	newHandle.instanceIndex = classInstances.size() - 1;
	
	return newHandle;
}

void Bink_Close(BinkHandle &handle)
{
	if (!GetDecoder(handle))
	{
		// invalid handle
		return;
	}

	// close bink decoder
	delete classInstances[handle.instanceIndex];
	classInstances[handle.instanceIndex] = 0;

	handle.instanceIndex = -1;
	handle.isValid = false;
}

uint32_t Bink_GetNumAudioTracks(BinkHandle &handle)
{
	BinkDecoder *decoder = GetDecoder(handle);
	if (!decoder)
		return 0;
	else
		return decoder->GetNumAudioTracks();
}

AudioInfo Bink_GetAudioTrackDetails(BinkHandle &handle, uint32_t trackIndex)
{
	BinkDecoder *decoder = GetDecoder(handle);
	if (!decoder || trackIndex >= decoder->GetNumAudioTracks())
	{
		AudioInfo none = { 0, 0, 0 };
		return none;
	}

	return decoder->GetAudioTrackDetails(trackIndex);
}

/* Get a frame's worth of audio data. 
 * 
 * 'data' needs to be a pointer to allocated memory that this function will fill.
 * You can find the size (in bytes) to make this buffer by calling Bink_GetAudioTrackDetails()
 * and checking the 'idealBufferSize' member in the returned AudioInfo struct
 */
uint32_t Bink_GetAudioData(BinkHandle &handle, uint32_t trackIndex, int16_t *data)
{
	BinkDecoder *decoder = GetDecoder(handle);
	if (!decoder || trackIndex >= decoder->GetNumAudioTracks())
		return 0;

	return decoder->GetAudioData(trackIndex, data);
}

uint32_t Bink_GetNumFrames(BinkHandle &handle)
{
	BinkDecoder *decoder = GetDecoder(handle);
	if (!decoder)
		return 0;

	return decoder->GetNumFrames();
}

void Bink_GetFrameSize(BinkHandle &handle, uint32_t &width, uint32_t &height)
{
	BinkDecoder *decoder = GetDecoder(handle);
	if (!decoder)
	{
		width  = 0;
		height = 0;
		return;
	}

	width  = decoder->frameWidth;
	height = decoder->frameHeight;
}

uint32_t Bink_GetCurrentFrameNum(BinkHandle &handle)
{
	BinkDecoder *decoder = GetDecoder(handle);
	if (!decoder)
		return 0;

	return decoder->GetCurrentFrameNum();
}

uint32_t Bink_GetNextFrame(BinkHandle &handle, YUVbuffer yuv)
{
	BinkDecoder *decoder = GetDecoder(handle);
	if (!decoder)
		return 0;

	uint32_t frameIndex = decoder->GetCurrentFrameNum();

	// no C++ exception may leave the DLL: a failed allocation loses this frame, and the
	// frame count still moves on so the caller's loop ends
	try
	{
		decoder->GetNextFrame(yuv);
	}
	catch (...)
	{
		decoder->DropFrame(yuv);
		if (decoder->GetCurrentFrameNum() == frameIndex)
			decoder->GotoFrame(frameIndex + 1);
	}

	return frameIndex;
}

float Bink_GetFrameRate(BinkHandle &handle)
{
	BinkDecoder *decoder = GetDecoder(handle);
	if (!decoder)
		return 0.0f;

	return decoder->GetFrameRate();
}

void Bink_GotoFrame(BinkHandle &handle, uint32_t frameNum)
{
	BinkDecoder *decoder = GetDecoder(handle);
	if (!decoder)
		return;

	decoder->GotoFrame(frameNum);
}

BinkDecoder::BinkDecoder()
{
	nFrames = 0;
	currentFrame = 0;

	// so the destructor's FreeBundles is safe when Open fails before InitBundles
	for (int i = 0; i < BINK_NB_SRC; i++)
		bundle[i].data = 0;
}

BinkDecoder::~BinkDecoder()
{
	for (uint32_t i = 0; i < planes.size(); i++)
	{
		delete[] planes[i].current;
		delete[] planes[i].last;
	}

	FreeBundles();

	for (uint32_t i = 0; i < audioTracks.size(); i++)
	{
		delete[] audioTracks[i]->buffer;
		delete[] audioTracks[i]->blockBuffer;

		if (kTransformTypeRDFT == audioTracks[i]->transformType)
			ff_rdft_end(&audioTracks[i]->trans.rdft);
		else if (kTransformTypeDCT == audioTracks[i]->transformType)
			ff_dct_end(&audioTracks[i]->trans.dct);

		delete audioTracks[i];
	}
}

uint32_t BinkDecoder::GetNumFrames()
{
	return nFrames;
}

uint32_t BinkDecoder::GetCurrentFrameNum()
{
	return currentFrame;
}

float BinkDecoder::GetFrameRate()
{
	return (float)fpsDividend / (float)fpsDivider;
}

void BinkDecoder::GotoFrame(uint32_t frameNum)
{
	// seek to the desired frame (just set currentFrame)
	currentFrame = frameNum;

	// what else? (memset some stuff?)
}

bool BinkDecoder::Open(const std::string &fileName)
{
	// open the file (read only)
	file.Open(fileName);
	if (!file.Is_Open())
	{
		BinkCommon::LogError("Can't open file " + fileName);
		return false;
	}

	// check the file signature
	signature = file.ReadUint32BE();
	if ((signature != kBIKfID) 
		&& (signature != kBIKgID)
		&& (signature != kBIKhID)
		&& (signature != kBIKiID))
	{
		BinkCommon::LogError("Unknown Bink signature");
		return false;
	}

	fileSize = file.ReadUint32LE() + 8;

	// the header must not claim more data than the file has (fileSize < 8 means the
	// 32-bit sum wrapped, a claim of 4 GB or more): every frame index entry below is
	// checked against fileSize, so it then lies inside the real file too
	if (fileSize < 8 || fileSize > file.GetFileSize())
	{
		BinkCommon::LogError("Invalid header, file size larger than the file");
		return false;
	}

	nFrames = file.ReadUint32LE();

	if (nFrames > 1000000)
	{
		BinkCommon::LogError("Invalid header, more than 1000000 frames");
		return false;
	}

	if (nFrames == 0)
	{
		BinkCommon::LogError("Invalid header, no frames");
		return false;
	}

	largestFrameSize = file.ReadUint32LE();
	if (largestFrameSize > fileSize)
	{
		BinkCommon::LogError("Largest frame size is greater than file size");
		return false;
	}

	// skip some unknown data
	file.Skip(4);

	frameWidth  = file.ReadUint32LE();
	frameHeight = file.ReadUint32LE();

	// reject sizes the plane and bundle buffers are not sized for (FFmpeg's limit is 7680 x 4800)
	if (!frameWidth || !frameHeight || frameWidth > 7680 || frameHeight > 4800)
	{
		BinkCommon::LogError("Invalid frame size");
		return false;
	}

	fpsDividend = file.ReadUint32LE();
	fpsDivider  = file.ReadUint32LE();

	if (fpsDividend == 0 || fpsDivider == 0)
	{
		BinkCommon::LogError("Invalid header, invalid frame rate");
		return false;
	}

	videoFlags  = file.ReadUint32LE();
	
	nAudioTracks = file.ReadUint32LE();

	// FFmpeg's limit (BINK_MAX_AUDIO_TRACKS)
	if (nAudioTracks > 256)
	{
		BinkCommon::LogError("Invalid header, more than 256 audio tracks");
		return false;
	}

	// audio is available
	if (nAudioTracks)
	{
		// skip some useless values (unknown and audio channels)
		file.Skip(4 * nAudioTracks);

		for (uint32_t i = 0; i < nAudioTracks; i++)
		{
			uint16_t sampleRate = file.ReadUint16LE();
			uint16_t flags      = file.ReadUint16LE();

			if (!CreateAudioTrack(sampleRate, flags))
			{
				BinkCommon::LogError("Can't create audio track");
				return false;
			}
		}

		// skip the audio track IDs
		file.Skip(4 * nAudioTracks);
	}

	// read the video frames
	frames.resize(nFrames);

	uint32_t pos, nextPos;

	nextPos = file.ReadUint32LE();
	
	for (uint32_t i = 0; i < nFrames; i++)
	{
		pos = nextPos;
		if (i == nFrames - 1)
		{
			nextPos = fileSize;
			frames[i].keyFrame = 0;
		}
		else
		{
			nextPos = file.ReadUint32LE();
			frames[i].keyFrame = pos & 1;
		}

		pos &= ~1;
		nextPos &= ~1;

		// each frame must end after it starts and inside the file
		if (nextPos <= pos || nextPos > fileSize)
		{
			BinkCommon::LogError("Invalid frame index table");
			return false;
		}

		frames[i].offset = pos;
		frames[i].size   = nextPos - pos;
	}

	// determine buffer sizes for audio tracks
	file.Seek(frames[0].offset);

	for (uint32_t trackIndex = 0; trackIndex < audioTracks.size(); trackIndex++)
	{
		// check for audio
		uint32_t audioPacketSize = file.ReadUint32LE();
			
		if (audioPacketSize >= 4)
		{
			// size in bytes of largest decoded audio
			uint32_t reportedSize = file.ReadUint32LE();

			// 4 MB is over 20 seconds of 48 kHz 16-bit stereo (the stock videos ask for 138,240 bytes)
			if (reportedSize > (1u << 22))
			{
				BinkCommon::LogError("Invalid audio buffer size");
				return false;
			}

			AudioTrack *track = audioTracks[trackIndex];

			// size in bytes
			track->buffer = new (std::nothrow) uint8_t[reportedSize];
			if (!track->buffer)
			{
				BinkCommon::LogError("Can't allocate audio buffer");
				return false;
			}
			track->bufferSize = reportedSize;

			// skip to next audio track (and -4 for reportedSize int we read) 
			file.Skip(audioPacketSize-4);
		}
		else
		{
			file.Skip(audioPacketSize);
		}
	}

	hasAlpha = videoFlags & kFlagAlpha;
	swapPlanes = signature >= kBIKhID;

	if (!InitBundles())
	{
		BinkCommon::LogError("Can't allocate bundle memory");
		return false;
	}
	InitTrees();

	uint32_t width  = frameWidth;
	uint32_t height = frameHeight;

	// init plane memory
	Plane newPlane;
	planes.push_back(newPlane);

	// luma plane
	planes.back().Init(width, height);

	// chroma planes
	width  /= 2;
	height /= 2;

	// 1
	planes.push_back(newPlane);
	planes.back().Init(width, height);

	// 2
	planes.push_back(newPlane);
	planes.back().Init(width, height);

	// alpha plane (full frame size: halving and doubling lost the last column and row of an odd size)
	if (hasAlpha)
	{
		planes.push_back(newPlane);
		planes.back().Init(frameWidth, frameHeight);
	}

	for (uint32_t i = 0; i < planes.size(); i++)
	{
		if (!planes[i].current || !planes[i].last)
		{
			BinkCommon::LogError("Can't allocate plane memory");
			return false;
		}
	}

	return true;
}

void BinkDecoder::GetNextFrame(YUVbuffer yuv)
{
	// past the last frame (GotoFrame takes any number): nothing to decode
	if (currentFrame >= nFrames)
		return;

	// seek to fame offset
	file.Seek(frames[currentFrame].offset);
	uint32_t frameSize = frames[currentFrame].size;
	bool frameValid = true;

	for (uint32_t trackIndex = 0; trackIndex < audioTracks.size(); trackIndex++)
	{
		// reset bytes read per frame (we might not get any audio for this frame)
		audioTracks[trackIndex]->bytesReadThisFrame = 0;

		if (!frameValid)
			continue;

		// check for audio
		uint32_t audioPacketSize = file.ReadUint32LE();

		// a packet bigger than the rest of the frame means a broken frame: decode nothing more of it
		if (frameSize < 4 || audioPacketSize > frameSize - 4)
		{
			BinkCommon::LogError("Audio packet is larger than the frame");
			frameValid = false;
			continue;
		}

		frameSize -= 4 + audioPacketSize;
			
		if (audioPacketSize >= 4)
		{
			uint32_t nSamples = file.ReadUint32LE();

			AudioPacket(trackIndex, audioPacketSize-4);
		}
		else
		{
			file.Skip(audioPacketSize);
		}
	}

	// get video packet
	if (frameValid)
		VideoPacket(frameSize);

	// set planes data (YUVbuffer has 3 entries: an alpha plane is decoded but not returned)
	for (uint32_t i = 0; i < planes.size() && i < 3; i++)
	{
		yuv[i].width  = planes[i].width;
		yuv[i].height = planes[i].height;
		yuv[i].pitch  = planes[i].pitch;
		yuv[i].data   = planes[i].last;
	}

	// frame done
	currentFrame++;
}

// after a C++ exception lost the frame being decoded: hand out the last complete
// picture (the zero-filled planes before the first one), so the caller never keeps
// unset pointers, and no audio for the lost frame
void BinkDecoder::DropFrame(YUVbuffer yuv)
{
	for (uint32_t i = 0; i < planes.size() && i < 3; i++)
	{
		yuv[i].width  = planes[i].width;
		yuv[i].height = planes[i].height;
		yuv[i].pitch  = planes[i].pitch;
		yuv[i].data   = planes[i].last;
	}

	for (uint32_t i = 0; i < audioTracks.size(); i++)
		audioTracks[i]->bytesReadThisFrame = 0;
}
