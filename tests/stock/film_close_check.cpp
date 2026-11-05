// Opens, decodes and closes every film in a folder through the real decoders, alone and two at once, as the menu background and the logos film do. Written 2026-11-04 by Melanikus for the AvP port.
//
// It's built with the address sanitizer, so that any read or write outside what the decoders were given stops it. It reads only what it's given. README.md, beside this file, says how to run it.
//
// Usage: film_close_check <folder of films>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <corecrt_io.h>

#include "BinkDecoder.h"
#include "SmackerDecoder.h"

static int const kFramesEach = 40;

static uint32_t Decode(BinkHandle &film, int frames)
{
	uint32_t total = 0;
	uint32_t const tracks = Bink_GetNumAudioTracks(film);
	std::vector<std::vector<int16_t>> sound(tracks);

	for (uint32_t t = 0; t < tracks; t++)
	{
		sound[t].resize(Bink_GetAudioTrackDetails(film, t).idealBufferSize / 2 + 16);
	}

	for (int f = 0; f < frames && (uint32_t)f < Bink_GetNumFrames(film); f++)
	{
		YUVbuffer yuv;
		memset(yuv, 0, sizeof(yuv));
		Bink_GetNextFrame(film, yuv);

		for (uint32_t t = 0; t < tracks; t++)
		{
			total += Bink_GetAudioData(film, t, &sound[t][0]);
		}
	}

	return total;
}

static uint32_t Decode(SmackerHandle &film, int frames)
{
	uint32_t total = 0;
	uint32_t width = 0, height = 0;
	Smacker_GetFrameSize(film, width, height);
	std::vector<uint8_t> picture((size_t)width * height + 16);
	uint8_t palette[768];
	std::vector<int16_t> sound;

	if (Smacker_GetNumAudioTracks(film))
	{
		sound.resize(Smacker_GetAudioTrackDetails(film, 0).idealBufferSize / 2 + 16);
	}

	for (int f = 0; f < frames && (uint32_t)f < Smacker_GetNumFrames(film); f++)
	{
		Smacker_GetNextFrame(film);
		Smacker_GetPalette(film, palette);
		Smacker_GetFrame(film, &picture[0]);

		if (!sound.empty())
		{
			total += Smacker_GetAudioData(film, 0, &sound[0]);
		}
	}

	return total;
}

template <class Handle, class Open, class Close>
static bool Exercise(std::string const &path, Open open, Close close, uint64_t &soundBytes)
{
	// alone, then again in the place the first one left free
	for (int round = 0; round < 2; round++)
	{
		Handle film = open(path.c_str());

		if (!film.isValid)
		{
			return false;
		}

		soundBytes += Decode(film, kFramesEach);
		close(film);

		if (film.isValid)
		{
			return false;
		}
	}

	// two at once, closing the first while the second goes on, as happens when a film starts over the menu background
	Handle first = open(path.c_str());
	Handle second = open(path.c_str());

	if (!first.isValid || !second.isValid || first.instanceIndex == second.instanceIndex)
	{
		return false;
	}

	soundBytes += Decode(first, 10);
	soundBytes += Decode(second, 10);
	close(first);
	soundBytes += Decode(second, 10);
	close(second);

	// a closed handle is refused, and closing twice is safe
	close(second);
	return true;
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("usage: film-close-check folder\n");
		return 1;
	}

	std::string const folder = argv[1];
	int bink = 0, smacker = 0, failed = 0;
	uint64_t soundBytes = 0;

	_finddata_t found;
	intptr_t const search = _findfirst((folder + "/*.*").c_str(), &found);

	if (search == -1)
	{
		printf("no files\n");
		return 1;
	}

	do
	{
		std::string const name = found.name;
		std::string const path = folder + "/" + name;
		std::string lower = name;

		for (size_t i = 0; i < lower.size(); i++)
		{
			lower[i] = (char)tolower((unsigned char)lower[i]);
		}

		bool passed = true;

		if (lower.size() > 4 && lower.compare(lower.size() - 4, 4, ".bik") == 0)
		{
			bink++;
			passed = Exercise<BinkHandle>(path, Bink_Open, Bink_Close, soundBytes);
		}
		else if (lower.size() > 4 && lower.compare(lower.size() - 4, 4, ".smk") == 0)
		{
			smacker++;
			passed = Exercise<SmackerHandle>(path, Smacker_Open, Smacker_Close, soundBytes);
		}

		if (!passed)
		{
			failed++;
			printf("FAILED %s\n", name.c_str());
		}
	}
	while (_findnext(search, &found) == 0);

	_findclose(search);
	printf("%d Bink films and %d Smacker films opened, decoded and closed, %d failed, %llu bytes of sound decoded\n", bink, smacker, failed, (unsigned long long)soundBytes);
	return failed ? 1 : 0;
}
