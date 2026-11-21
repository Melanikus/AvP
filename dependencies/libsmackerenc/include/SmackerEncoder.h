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
 * the game's own player plays it. It writes: Smacker 2 ("SMK2"), one palette for the whole film (in frame 0 only, as AvP's own films
 * have it), every 4 x 4 block FULL, and 8-bit mono sound compressed as his DecodeAudio reads it (his decoder plays only compressed
 * sound: raw sound is skipped, SM-P14).
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
 * THE PICTURE: the film gets one palette of at most 255 colours (Smacker's palette holds 64 levels a colour, smk_pal), chosen by
 * median cut when the film has more. A film of 255 colours or fewer in all, each on those levels, comes back exactly.
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
	SMACKER_ENCODE_NO_MEMORY,     /* the memory for the work could not be had */
	SMACKER_ENCODE_FILE           /* the stream's file, or one of its own files beside it, could not be made, written or read */
};

/* Encodes the video. On SMACKER_ENCODE_DONE *smk is the file's bytes (free them with Smacker_FreeEncoded) and *smkBytes how many;
 * on anything else *smk is null and *smkBytes 0. No C++ exception leaves the library. */
int Smacker_Encode(const SmackerEncodeInput *in, uint8_t **smk, size_t *smkBytes);

/* Frees what Smacker_Encode gave; null is nothing to free */
void Smacker_FreeEncoded(uint8_t *smk);

/* What a result means, in words: static text, never null */
const char *Smacker_EncodeWords(int result);

/* THE STREAM: a film of any length, a frame at a time, in memory that does not grow with it (Smacker_Encode holds the whole film).
 *
 *   SmackerStreamSettings s = {};
 *   s.width = 128; s.height = 96; s.fps = 15; s.sampleRate = 22050;   // sampleRate 0: no sound
 *   SmackerStream *stream = 0;
 *   int r = Smacker_Begin(&s, L"C:\\...\\message101.smk", &stream);
 *   for each frame: r = Smacker_AddFrame(stream, rgb, 128 * 96 * 3, samples, sampleCount);   // any number of samples with each frame, 0 too
 *   r = Smacker_End(stream);                  // writes the file; the stream is gone after it, whatever it gives
 *   (or Smacker_Abandon(stream): nothing written, the file taken away)
 *
 * The file is the very same as Smacker_Encode would make of the same frames and sound (one palette for the whole film, in frame 0, as AvP's
 * own films have it): so the frames are kept until End in a file of the stream's own beside the film (the film's name with ".frames.tmp"),
 * and the sound in another (".sound.tmp"), and End reads them three times. Those two files are made new (never over a file that is there)
 * and Windows takes them away when the stream closes them, after a crash too. They take frames x width x height x 3 bytes on the disk. */
typedef struct SmackerStream SmackerStream;

typedef struct SmackerStreamSettings
{
	uint32_t width;          /* pixels, a multiple of 4 */
	uint32_t height;         /* pixels, a multiple of 4 */
	uint32_t fps;            /* frames a second, 1 to SMACKER_ENCODE_MOST_FPS */
	uint32_t sampleRate;     /* 0 for a film with no sound, else SMACKER_ENCODE_LEAST_RATE to SMACKER_ENCODE_MOST_RATE */
} SmackerStreamSettings;

/* Starts a stream that will write the film to path (made, or made again: a film there already is replaced). On anything but
 * SMACKER_ENCODE_DONE *stream is null, nothing is made, and a file already at path is left as it was. */
int Smacker_Begin(const SmackerStreamSettings *settings, const wchar_t *path, SmackerStream **stream);

/* The next frame (width x height pixels, R G B, rgbBytes exactly width x height x 3) and the sound that goes on from the last (8-bit unsigned
 * mono; pcm may be null when pcmBytes is 0). A refused call adds nothing, and the stream goes on; a file that cannot be written fails the
 * stream (End then gives SMACKER_ENCODE_FILE). */
int Smacker_AddFrame(SmackerStream *stream, const uint8_t *rgb, size_t rgbBytes, const uint8_t *pcm, size_t pcmBytes);

/* Writes the film and ends the stream. SMACKER_ENCODE_DONE: the film is written. Anything else says why not (no frame, fewer than 8 samples,
 * a file error, ...), and the film's file is taken away. The stream is freed either way. */
int Smacker_End(SmackerStream *stream);

/* Ends the stream with nothing written: the film's file is taken away. Null is nothing to end. */
void Smacker_Abandon(SmackerStream *stream);

#ifdef __cplusplus
}
#endif

#endif
