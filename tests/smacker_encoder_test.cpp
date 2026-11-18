// The test of the Smacker ENCODER (dependencies/libsmackerenc), read back by Barry Duncan's own DECODER (dependencies/libsmackerdec), the one
// the game plays films with. Written 2026-11-18 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// Generated videos are encoded, written to a file, opened with the decoder's own Smacker_Open, and played frame by frame:
//   - an AvP message video's shape (128 x 96, 15 a second, 22050 Hz 8-bit mono, 2 seconds): every frame's pixels, put through the decoder's
//     palette, are the RGB that went in, exactly (each frame has 255 colours or fewer, all on Smacker's 64 levels); every sound byte is the
//     one that went in, exactly; the header reads as AvP's own message videos do (message100.smk: SMK2, frame rate -6666, sound flags
//     0xC0005622, the first frame's sound one second more than the others'); one palette, in frame 0 only; all four trees there; every sound chunk a whole number of 4 samples;
//   - a video with more colours than a palette holds: each frame comes back near (median cut), and palette entry 255 is never used;
//   - a video with no sound: the decoder finds no sound track;
//   - a bigger frame (256 x 192, 3072 blocks: more than one run of the block type), and one of 4 x 4 pixels, one frame;
//   - the refusals: every field out of range gives its reason, and nothing is handed back.
//
// HOW IT RUNS
//
// It is built with the encoder's and the decoder's own sources, and nothing else (in the AvP port: tests/build.ps1, with the list in
// tests/smacker_encoder_test.args). It writes its
// films into a folder of its own in the working folder and takes the folder away after. A non-zero result means a check failed.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "SmackerEncoder.h"
#include "SmackerDecoder.h"

static int checks = 0, checksFailed = 0;

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

// Smacker's 64 levels a colour (the decoder's smk_pal)
static const uint8_t kLevels[64] = {
	0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C,
	0x41, 0x45, 0x49, 0x4D, 0x51, 0x55, 0x59, 0x5D, 0x61, 0x65, 0x69, 0x6D, 0x71, 0x75, 0x79, 0x7D,
	0x82, 0x86, 0x8A, 0x8E, 0x92, 0x96, 0x9A, 0x9E, 0xA2, 0xA6, 0xAA, 0xAE, 0xB2, 0xB6, 0xBA, 0xBE,
	0xC3, 0xC7, 0xCB, 0xCF, 0xD3, 0xD7, 0xDB, 0xDF, 0xE3, 0xE7, 0xEB, 0xEF, 0xF3, 0xF7, 0xFB, 0xFF };

static uint32_t U32At(std::vector<uint8_t> const &b, size_t at)
{
	return at + 4 <= b.size() ? (uint32_t)b[at] | ((uint32_t)b[at + 1] << 8) | ((uint32_t)b[at + 2] << 16) | ((uint32_t)b[at + 3] << 24) : 0;
}

// A frame of at most 129 colours, each on the 64 levels: bands that move with the frame, and a white square that moves across
static void MakeFewColours(uint32_t w, uint32_t h, uint32_t f, uint8_t *rgb)
{
	for (uint32_t y = 0; y < h; y++)
	{
		for (uint32_t x = 0; x < w; x++)
		{
			uint32_t const c = ((x / 8) + (y / 12) * 16 + f) % 128;
			uint8_t *const p = rgb + ((size_t)y * w + x) * 3;
			bool const square = x >= (f * 4) % (w - 16) && x < (f * 4) % (w - 16) + 16 && y >= (f * 2) % (h - 16) && y < (f * 2) % (h - 16) + 16;

			// the same 128 colours (and white) in every frame, moving: the film has 129, so its one palette holds them all
			p[0] = square ? 0xFF : kLevels[c % 64];
			p[1] = square ? 0xFF : kLevels[(c * 7) % 64];
			p[2] = square ? 0xFF : kLevels[(c * 13 + 5) % 64];
		}
	}
}

// A frame of thousands of colours: smooth ramps in all three
static void MakeManyColours(uint32_t w, uint32_t h, uint32_t f, uint8_t *rgb)
{
	for (uint32_t y = 0; y < h; y++)
	{
		for (uint32_t x = 0; x < w; x++)
		{
			uint8_t *const p = rgb + ((size_t)y * w + x) * 3;

			p[0] = (uint8_t)((x * 255) / (w - 1));
			p[1] = (uint8_t)((y * 255) / (h - 1));
			p[2] = (uint8_t)(((x + y) * 2 + f * 9) & 0xFF);
		}
	}
}

static std::vector<uint8_t> MakeSound(size_t count)
{
	std::vector<uint8_t> pcm(count);

	for (size_t i = 0; i < count; i++)
	{
		double const wave = 100.0 * sin(2.0 * 3.14159265358979 * 440.0 * (double)i / 22050.0);
		int const v = 128 + (int)wave + (int)((i * 37) % 7) - 3;

		pcm[i] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
	}

	return pcm;
}

static bool WriteFile(std::string const &path, uint8_t const *bytes, size_t count)
{
	FILE *const out = fopen(path.c_str(), "wb");

	if (!out)
	{
		return false;
	}

	bool const all = fwrite(bytes, 1, count, out) == count;

	return fclose(out) == 0 && all;
}

struct Played
{
	bool opened = false;
	uint32_t frames = 0, width = 0, height = 0, audioTracks = 0;
	float fps = 0.0f;
	SmackerAudioInfo audio = {};
	std::vector<std::vector<uint8_t> > rgb;          // each frame through its palette
	std::vector<std::vector<uint8_t> > indices;      // each frame's palette numbers
	std::vector<uint8_t> sound;                      // every frame's sound, one after the other
	uint32_t firstFrameSound = 0;
	bool soundInFours = true;                        // every frame's sound a whole number of 4 samples (as AvP's own films)
};

// Plays the file with the decoder's own exported calls, as the game's SmackerPlayback does. hasSound: the film was made with sound. The decoder is
// asked about sound only then: an older libsmackerdec says there is one track even when the file has none, and leaves that track's numbers unset.
static Played Play(std::string const &path, bool hasSound)
{
	Played p;
	SmackerHandle handle = Smacker_Open(path.c_str());

	if (!handle.isValid)
	{
		return p;
	}

	p.opened = true;
	p.frames = Smacker_GetNumFrames(handle);
	Smacker_GetFrameSize(handle, p.width, p.height);
	p.fps = Smacker_GetFrameRate(handle);
	if (hasSound)
	{
		p.audioTracks = Smacker_GetNumAudioTracks(handle);
		p.audio = Smacker_GetAudioTrackDetails(handle, 0);
	}

	std::vector<int16_t> audioBuffer((p.audio.idealBufferSize + 1) / 2 + 1);
	std::vector<uint8_t> palette(768), frame((size_t)p.width * p.height);

	for (uint32_t i = 0; i < p.frames; i++)
	{
		Smacker_GetNextFrame(handle);
		Smacker_GetPalette(handle, palette.data());
		Smacker_GetFrame(handle, frame.data());

		std::vector<uint8_t> rgb(frame.size() * 3);

		for (size_t k = 0; k < frame.size(); k++)
		{
			memcpy(&rgb[k * 3], &palette[(size_t)frame[k] * 3], 3);
		}

		p.rgb.push_back(rgb);
		p.indices.push_back(frame);

		if (p.audioTracks)
		{
			uint32_t const got = Smacker_GetAudioData(handle, 0, audioBuffer.data());
			uint8_t const *const bytes = (uint8_t const *)audioBuffer.data();

			if (i == 0)
			{
				p.firstFrameSound = got;
			}

			p.soundInFours = p.soundInFours && got % 4 == 0;

			p.sound.insert(p.sound.end(), bytes, bytes + (got <= p.audio.idealBufferSize ? got : 0));
		}
	}

	Smacker_Close(handle);
	return p;
}

static SmackerEncodeInput InputOf(uint32_t w, uint32_t h, uint32_t frames, uint32_t fps, std::vector<uint8_t> const &rgb, std::vector<uint8_t> const &pcm, uint32_t rate)
{
	SmackerEncodeInput in;

	memset(&in, 0, sizeof(in));
	in.width = w;
	in.height = h;
	in.frameCount = frames;
	in.fps = fps;
	in.rgb = rgb.data();
	in.rgbBytes = rgb.size();
	in.sampleRate = rate;
	in.pcm = pcm.empty() ? nullptr : pcm.data();
	in.pcmBytes = pcm.size();
	return in;
}

static bool Encode(SmackerEncodeInput const &in, std::vector<uint8_t> *out, int *result)
{
	uint8_t *smk = nullptr;
	size_t bytes = 0;

	*result = Smacker_Encode(&in, &smk, &bytes);

	if (*result != SMACKER_ENCODE_DONE || !smk)
	{
		Smacker_FreeEncoded(smk);
		return false;
	}

	out->assign(smk, smk + bytes);
	Smacker_FreeEncoded(smk);
	return true;
}

static void TestMessageVideo(std::string const &folder)
{
	puts("an AvP message video: 128 x 96, 15 a second, 22050 Hz 8-bit mono, 2 seconds");

	uint32_t const w = 128, h = 96, frames = 30;
	size_t const frameBytes = (size_t)w * h * 3;
	std::vector<uint8_t> rgb(frameBytes * frames);

	for (uint32_t f = 0; f < frames; f++)
	{
		MakeFewColours(w, h, f, &rgb[frameBytes * f]);
	}

	std::vector<uint8_t> const pcm = MakeSound(44100);
	std::vector<uint8_t> smk;
	int result = -1;

	bool const encoded = Encode(InputOf(w, h, frames, 15, rgb, pcm, 22050), &smk, &result);

	Check(encoded, std::string("encoded: ") + Smacker_EncodeWords(result) + ", " + std::to_string(smk.size()) + " bytes");

	if (smk.size() < 104)
	{
		return;
	}

	// the header, as message100.smk's
	Check(memcmp(smk.data(), "SMK2", 4) == 0, "the signature is SMK2");
	Check(U32At(smk, 4) == w && U32At(smk, 8) == h && U32At(smk, 12) == frames, "128 x 96, 30 frames");
	Check((int32_t)U32At(smk, 16) == -6666, "the frame rate is -6666 (1/100 ms a frame, as AvP's own: 15 a second)");
	Check(U32At(smk, 20) == 0, "no flags (no ring frame, as AvP's own)");
	Check(U32At(smk, 72) == 0xC0005622u, "sound track 0: compressed, there, 22050 Hz, 8-bit mono (0xC0005622, as AvP's own)");
	Check(U32At(smk, 24) == 16 * 1470, "the largest sound chunk, the first frame's, is 16 frames' worth (23520, as message100.smk)");

	bool restNone = true;

	for (int t = 1; t < 7; t++)
	{
		restNone = restNone && U32At(smk, 24 + 4 * t) == 0 && U32At(smk, 72 + 4 * t) == 0;
	}

	Check(restNone, "sound tracks 1 to 6: none");

	// every frame's size a whole number of 4 bytes, and the sizes add up to the file
	size_t const treeSize = U32At(smk, 52);
	size_t at = 104 + 5 * (size_t)frames + treeSize;
	bool fours = true;

	for (uint32_t f = 0; f < frames; f++)
	{
		uint32_t const size = U32At(smk, 104 + 4 * (size_t)f);

		fours = fours && (size & 3) == 0;
		at += size & ~3u;
	}

	Check(fours && at == smk.size(), "each frame's size is a whole number of 4 bytes, and they end where the file ends");

	uint8_t const flags0 = smk[104 + 4 * (size_t)frames];
	bool laterPalette = false;

	for (uint32_t f = 1; f < frames; f++)
	{
		laterPalette = laterPalette || (smk[104 + 4 * (size_t)frames + f] & 1) != 0;
	}

	size_t const frame0 = 104 + 5 * (size_t)frames + treeSize;

	Check((flags0 & 1) && !laterPalette && frame0 < smk.size() && smk[frame0] * 4 <= 768, "one palette, in frame 0 only, of 768 bytes at most (as AvP's own films)");
	Check(U32At(smk, 56) == 24 && U32At(smk, 60) == 24 && U32At(smk, 64) % 4 == 0 && U32At(smk, 64) > 24 && U32At(smk, 68) % 4 == 0 && U32At(smk, 68) >= 24,
		"all four trees there: MMAP and MCLR of two leaves (24: 3 entries x 4 + 12), FULL and TYPE sized in 4s");
	Check((flags0 & 2) != 0, "frame 0 carries sound");

	std::string const path = folder + "\\message_test.smk";

	Check(WriteFile(path, smk.data(), smk.size()), "written to a file");

	Played const p = Play(path, true);

	Check(p.opened, "Barry's decoder opens it (Smacker_Open)");
	Check(p.frames == frames && p.width == w && p.height == h, "the decoder reads 30 frames of 128 x 96");
	Check(p.fps == 15.0f, "the decoder reads 15 frames a second");
	Check(p.audioTracks == 1 && p.audio.sampleRate == 22050 && p.audio.nChannels == 1 && p.audio.bitsPerSample == 8,
		"the decoder finds one sound track of 22050 Hz, mono, 8-bit");
	Check(p.audio.idealBufferSize == 16 * 1470 && p.firstFrameSound == 16 * 1470, "the first frame gives 16 frames' worth of sound (the player's buffer size)");

	size_t exactFrames = 0;

	for (uint32_t f = 0; f < frames && f < p.rgb.size(); f++)
	{
		exactFrames += memcmp(p.rgb[f].data(), &rgb[frameBytes * f], frameBytes) == 0 ? 1 : 0;
	}

	Check(exactFrames == frames, "every frame, through its palette, is the RGB that went in, exactly (" + std::to_string(exactFrames) + " of 30)");
	Check(p.soundInFours, "every frame's sound is a whole number of 4 samples (as AvP's own: 1468 / 1472)");
	Check(p.sound.size() == pcm.size() && memcmp(p.sound.data(), pcm.data(), pcm.size()) == 0,
		"every sound byte is the one that went in, exactly (" + std::to_string(p.sound.size()) + " of " + std::to_string(pcm.size()) + ")");

	DeleteFileA(path.c_str());
}

static void TestManyColours(std::string const &folder)
{
	puts("more colours than a palette holds: 128 x 96, 4 frames, no sound");

	uint32_t const w = 128, h = 96, frames = 4;
	size_t const frameBytes = (size_t)w * h * 3;
	std::vector<uint8_t> rgb(frameBytes * frames);

	for (uint32_t f = 0; f < frames; f++)
	{
		MakeManyColours(w, h, f, &rgb[frameBytes * f]);
	}

	std::vector<uint8_t> smk;
	int result = -1;

	bool const encoded = Encode(InputOf(w, h, frames, 15, rgb, std::vector<uint8_t>(), 0), &smk, &result);

	Check(encoded, std::string("encoded: ") + Smacker_EncodeWords(result));

	std::string const path = folder + "\\many_colours.smk";

	Check(WriteFile(path, smk.data(), smk.size()), "written to a file");

	Played const p = Play(path, false);
	bool soundInAFrame = false;

	for (uint32_t f = 0; f < frames && smk.size() > 104 + 5 * (size_t)frames; f++)
	{
		soundInAFrame = soundInAFrame || (smk[104 + 4 * (size_t)frames + f] & 2) != 0;
	}

	Check(p.opened && p.frames == frames, "Barry's decoder opens it and reads 4 frames");
	Check(U32At(smk, 24) == 0 && U32At(smk, 72) == 0 && !soundInAFrame, "no sound: the header names no track, and no frame carries sound");

	double worst = 0.0;
	bool no255 = true;

	for (uint32_t f = 0; f < frames && f < p.rgb.size(); f++)
	{
		double sum = 0.0;

		for (size_t k = 0; k < frameBytes; k++)
		{
			sum += fabs((double)p.rgb[f][k] - (double)rgb[frameBytes * f + k]);
		}

		double const mean = sum / (double)frameBytes;

		worst = mean > worst ? mean : worst;

		for (uint8_t index : p.indices[f])
		{
			no255 = no255 && index != 255;
		}
	}

	// one palette for the whole film (as AvP's own films have) over 4 frames of ramps: 6.12 when written (a palette per frame gave 4.36)
	Check(p.rgb.size() == frames && worst < 8.0, "each frame comes back near: the worst frame's mean difference is " + std::to_string(worst) + " of 255 (under 8)");
	Check(no255, "palette entry 255 is never used (255 colours at most)");

	DeleteFileA(path.c_str());
}

static void TestBigAndSmall(std::string const &folder)
{
	puts("a bigger frame (256 x 192: 3072 blocks) and the smallest (4 x 4)");

	{
		uint32_t const w = 256, h = 192, frames = 3;
		size_t const frameBytes = (size_t)w * h * 3;
		std::vector<uint8_t> rgb(frameBytes * frames);

		for (uint32_t f = 0; f < frames; f++)
		{
			MakeFewColours(w, h, f, &rgb[frameBytes * f]);
		}

		std::vector<uint8_t> const pcm = MakeSound(22050 / 10 * 3);
		std::vector<uint8_t> smk;
		int result = -1;

		bool const encoded = Encode(InputOf(w, h, frames, 10, rgb, pcm, 22050), &smk, &result);

		Check(encoded, std::string("256 x 192 encoded: ") + Smacker_EncodeWords(result));

		std::string const path = folder + "\\big.smk";

		WriteFile(path, smk.data(), smk.size());

		Played const p = Play(path, true);
		size_t exact = 0;

		for (uint32_t f = 0; f < frames && f < p.rgb.size(); f++)
		{
			exact += memcmp(p.rgb[f].data(), &rgb[frameBytes * f], frameBytes) == 0 ? 1 : 0;
		}

		Check(p.opened && p.fps == 10.0f && exact == frames, "decoded exactly, every frame (" + std::to_string(exact) + " of 3), 10 a second");
		// 6615 samples: filled out to 6616 with the last one, a whole number of 4
		Check(p.sound.size() == 6616 && memcmp(p.sound.data(), pcm.data(), pcm.size()) == 0 && p.sound.back() == pcm.back(),
			"and its sound exactly, filled out to a whole number of 4 samples with its last (" + std::to_string(p.sound.size()) + ")");
		DeleteFileA(path.c_str());
	}

	{
		std::vector<uint8_t> rgb(4 * 4 * 3);

		for (size_t k = 0; k < rgb.size(); k++)
		{
			rgb[k] = kLevels[(k * 11) % 64];
		}

		std::vector<uint8_t> smk;
		int result = -1;

		bool const encoded = Encode(InputOf(4, 4, 1, 1, rgb, std::vector<uint8_t>(), 0), &smk, &result);

		Check(encoded, std::string("4 x 4, one frame, 1 a second: ") + Smacker_EncodeWords(result));

		std::string const path = folder + "\\small.smk";

		WriteFile(path, smk.data(), smk.size());

		Played const p = Play(path, false);

		Check(p.opened && p.frames == 1 && p.rgb.size() == 1 && memcmp(p.rgb[0].data(), rgb.data(), rgb.size()) == 0, "decoded exactly");
		DeleteFileA(path.c_str());
	}
}

static void TestRefusals()
{
	puts("the refusals");

	uint32_t const w = 8, h = 8;
	std::vector<uint8_t> rgb(w * h * 3 * 2, 0x41);
	std::vector<uint8_t> const pcm(100, 128);

	struct Case
	{
		char const *what;
		int expected;
		SmackerEncodeInput in;
	};

	std::vector<Case> cases;
	SmackerEncodeInput const good = InputOf(w, h, 2, 15, rgb, pcm, 22050);

	{
		Case c = { "a width that is not a multiple of 4", SMACKER_ENCODE_BAD_SIZE, good };
		c.in.width = 6;
		cases.push_back(c);
	}
	{
		Case c = { "a height of 0", SMACKER_ENCODE_BAD_SIZE, good };
		c.in.height = 0;
		cases.push_back(c);
	}
	{
		Case c = { "a width past 4096", SMACKER_ENCODE_BAD_SIZE, good };
		c.in.width = 4100;
		cases.push_back(c);
	}
	{
		Case c = { "a height past 4096", SMACKER_ENCODE_BAD_SIZE, good };
		c.in.width = 4096;
		c.in.height = 4100;
		cases.push_back(c);
	}
	{
		Case c = { "no frames", SMACKER_ENCODE_BAD_FRAMES, good };
		c.in.frameCount = 0;
		cases.push_back(c);
	}
	{
		Case c = { "too many frames", SMACKER_ENCODE_BAD_FRAMES, good };
		c.in.frameCount = SMACKER_ENCODE_MOST_FRAMES + 1;
		cases.push_back(c);
	}
	{
		Case c = { "0 frames a second", SMACKER_ENCODE_BAD_FPS, good };
		c.in.fps = 0;
		cases.push_back(c);
	}
	{
		Case c = { "101 frames a second", SMACKER_ENCODE_BAD_FPS, good };
		c.in.fps = 101;
		cases.push_back(c);
	}
	{
		Case c = { "rgbBytes one short", SMACKER_ENCODE_BAD_PIXELS, good };
		c.in.rgbBytes--;
		cases.push_back(c);
	}
	{
		Case c = { "no pixels", SMACKER_ENCODE_BAD_PIXELS, good };
		c.in.rgb = nullptr;
		cases.push_back(c);
	}
	{
		Case c = { "a sound rate under 1000", SMACKER_ENCODE_BAD_SOUND, good };
		c.in.sampleRate = 999;
		cases.push_back(c);
	}
	{
		Case c = { "a sound rate past 96000", SMACKER_ENCODE_BAD_SOUND, good };
		c.in.sampleRate = 96001;
		cases.push_back(c);
	}
	{
		Case c = { "7 samples", SMACKER_ENCODE_BAD_SOUND, good };
		c.in.pcmBytes = 7;
		cases.push_back(c);
	}
	{
		Case c = { "samples counted but none given", SMACKER_ENCODE_BAD_SOUND, good };
		c.in.pcm = nullptr;
		cases.push_back(c);
	}

	for (Case const &c : cases)
	{
		uint8_t *smk = (uint8_t *)1;
		size_t bytes = 99;
		int const result = Smacker_Encode(&c.in, &smk, &bytes);

		Check(result == c.expected && smk == nullptr && bytes == 0, std::string(c.what) + ": " + Smacker_EncodeWords(result));
	}

	uint8_t *smk = nullptr;
	size_t bytes = 0;

	Check(Smacker_Encode(nullptr, &smk, &bytes) == SMACKER_ENCODE_BAD_CALL && Smacker_Encode(&good, nullptr, &bytes) == SMACKER_ENCODE_BAD_CALL &&
		Smacker_Encode(&good, &smk, nullptr) == SMACKER_ENCODE_BAD_CALL, "no input, or nowhere to put the answer: BAD_CALL");
	Check(Smacker_Encode(&good, &smk, &bytes) == SMACKER_ENCODE_DONE && smk && bytes > 104, "the good one is encoded");
	Smacker_FreeEncoded(smk);
	Smacker_FreeEncoded(nullptr);
	Check(Smacker_EncodeWords(-5) != nullptr && Smacker_EncodeWords(1000) != nullptr, "words for any number, never null");
}

int main()
{
	setvbuf(stdout, 0, _IONBF, 0);

	std::string const folder = "smacker_encoder_test_" + std::to_string(GetCurrentProcessId());

	if (!CreateDirectoryA(folder.c_str(), 0))
	{
		printf("  FAILED  the test couldn't make its folder\n");
		return 1;
	}

	TestMessageVideo(folder);
	TestManyColours(folder);
	TestBigAndSmall(folder);
	TestRefusals();

	RemoveDirectoryA(folder.c_str());
	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
