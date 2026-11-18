/*
 * libsmackerenc - Smacker video encoder
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

/* This code is derived from libsmackerdec (dependencies/libsmackerdec, Copyright (C) 2011 Barry Duncan), which is based on smacker.c
 * from the FFmpeg project (http://www.ffmpeg.org/, Copyright (c) 2006 Konstantin Shishkov, GNU Lesser General Public License 2.1 or later).
 *
 * Modified 2026-11-18 by Melanikus for the AvP port: an ENCODER, the other way round from Barry's decoder. It writes what his
 * SmackerDecoder reads (the same header, the same header trees in his DecodeTree / DecodeBigTree layouts, the same frame chunks), so
 * the game's own player plays it. The first version writes: Smacker 2 ("SMK2"), one palette per frame (left out when it is the
 * frame before's), every 4 x 4 block FULL, and 8-bit mono sound compressed as his DecodeAudio reads it (his decoder plays only
 * compressed sound: raw sound is skipped, SM-P14).
 */

/* THE CALL. Everything the caller gives is checked before anything is made; nothing is written past what it gives.
 *
 *   SmackerEncodeInput in = {};
 *   in.width = 128; in.height = 96; in.frameCount = n; in.fps = 15;
 *   in.rgb = frames; in.rgbBytes = n * 128 * 96 * 3;        // frame after frame, rows top to bottom, R G B, one byte each
 *   in.sampleRate = 22050; in.pcm = samples; in.pcmBytes = m; // 8-bit unsigned mono (128 is silence); pcmBytes 0: no sound
 *   uint8_t *smk = 0; size_t smkBytes = 0;
 *   int r = Smacker_Encode(&in, &smk, &smkBytes);            // SMACKER_ENCODE_DONE, or a reason (Smacker_EncodeWords)
 *   ... write smkBytes bytes of smk to the .smk file ...
 *   Smacker_FreeEncoded(smk);                                 // the bytes are this library's: only it frees them
 *
 * AvP's message videos (FMVs\messageNNN.smk) are 128 x 96 at 15 frames a second with 22050 Hz 8-bit mono sound.
 * THE PICTURE: each frame gets its own palette of at most 255 colours (Smacker's palette holds 64 levels a colour, smk_pal), chosen by
 * median cut when the frame has more. A frame of 255 colours or fewer, each on those levels, comes back exactly.
 * THE SOUND: the decoder's player sizes its sound buffer from the first frame, so the first frame carries one second more than the
 * others (as AvP's own films do: message100.smk's first frame holds 16 frames' worth at 15 a second). Sound past the end of that
 * (the frames' time plus that second) is not kept.
 */

#ifndef _SmackerEncoder_h_
#define _SmackerEncoder_h_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the limits every call is checked against */
#define SMACKER_ENCODE_MOST_SIDE    4096u          /* width and height: multiples of 4, 4 to this */
#define SMACKER_ENCODE_MOST_PIXELS  (1u << 24)     /* width x height, as the decoder takes */
#define SMACKER_ENCODE_MOST_FRAMES  100000u        /* over 1 hour 50 minutes at 15 a second */
#define SMACKER_ENCODE_MOST_FPS     100u
#define SMACKER_ENCODE_MOST_RATE    96000u         /* samples a second */
#define SMACKER_ENCODE_LEAST_RATE   1000u

typedef struct SmackerEncodeInput
{
	uint32_t width;          /* pixels, a multiple of 4 */
	uint32_t height;         /* pixels, a multiple of 4 */
	uint32_t frameCount;     /* 1 or more */
	uint32_t fps;            /* frames a second, 1 to SMACKER_ENCODE_MOST_FPS */
	const uint8_t *rgb;      /* frameCount frames of width x height pixels, R G B */
	size_t rgbBytes;         /* frameCount x width x height x 3, exactly */
	uint32_t sampleRate;     /* samples a second; anything when pcmBytes is 0 */
	const uint8_t *pcm;      /* 8-bit unsigned mono samples; may be null when pcmBytes is 0 */
	size_t pcmBytes;         /* 0 (no sound), or 8 or more */
} SmackerEncodeInput;

enum
{
	SMACKER_ENCODE_DONE = 0,
	SMACKER_ENCODE_BAD_CALL,      /* no input, or nowhere to put the answer */
	SMACKER_ENCODE_BAD_SIZE,      /* width or height not a multiple of 4, 0, or past the limits */
	SMACKER_ENCODE_BAD_FRAMES,    /* no frames, or more than SMACKER_ENCODE_MOST_FRAMES */
	SMACKER_ENCODE_BAD_FPS,       /* 0, or past SMACKER_ENCODE_MOST_FPS */
	SMACKER_ENCODE_BAD_PIXELS,    /* no pixels, or rgbBytes is not frameCount x width x height x 3 */
	SMACKER_ENCODE_BAD_SOUND,     /* samples given with a rate out of range, fewer than 8 samples, or null samples */
	SMACKER_ENCODE_TOO_BIG,       /* the file would pass 4 GB, or a frame what a frame size can say */
	SMACKER_ENCODE_NO_MEMORY      /* the memory for the work could not be had */
};

/* Encodes the video. On SMACKER_ENCODE_DONE *smk is the file's bytes (free them with Smacker_FreeEncoded) and *smkBytes how many;
 * on anything else *smk is null and *smkBytes 0. No C++ exception leaves the library. */
int Smacker_Encode(const SmackerEncodeInput *in, uint8_t **smk, size_t *smkBytes);

/* Frees what Smacker_Encode gave; null is nothing to free */
void Smacker_FreeEncoded(uint8_t *smk);

/* What a result means, in words: static text, never null */
const char *Smacker_EncodeWords(int result);

#ifdef __cplusplus
}
#endif

#endif
