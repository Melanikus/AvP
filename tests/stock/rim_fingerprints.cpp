// Prints a fingerprint of every pixel of every picture in a folder, through the real RimLoader, so that two versions of the loader can be compared picture by picture. Written 2026-11-04 by
// Melanikus for the AvP port. It reads only what it's given. README.md, beside this file, says how to run it.
//
// Usage: rim_fingerprints <folder of .RIM pictures>

#include <stdio.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <corecrt_io.h>

#include "RimLoader.h"
#include "FastFile.h"

// FileStream asks whether a file is in a fastfile. These copies never are.
FastFileHandle *FF_Find(const std::string &) { return 0; }

const std::string &FF_GetFastFileName(int32_t)
{
	static std::string const none;
	return none;
}

namespace Util
{
	void FtoBslash(std::string &text)
	{
		for (size_t i = 0; i < text.size(); i++)
		{
			text[i] = (text[i] == '/') ? '\\' : text[i];
		}
	}
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("usage: rim-hash folder\n");
		return 1;
	}

	std::string const folder = argv[1];
	_finddata_t found;
	intptr_t const search = _findfirst((folder + "/*.rim").c_str(), &found);

	if (search == -1)
	{
		printf("no pictures\n");
		return 1;
	}

	int pictures = 0, failed = 0;

	do
	{
		RimLoader picture;

		if (!picture.Open(folder + "/" + found.name))
		{
			printf("%s REFUSED\n", found.name);
			failed++;
			continue;
		}

		uint32_t width = 0, height = 0;
		picture.GetDimensions(width, height);
		std::vector<uint8_t> pixels((size_t)width * height * 4 + 1);
		picture.Decode(&pixels[0], width * 4);

		uint32_t fingerprint = 2166136261u;

		for (size_t i = 0; i + 1 < pixels.size(); i++)
		{
			fingerprint = (fingerprint ^ pixels[i]) * 16777619u;
		}

		printf("%s %u %u %08x\n", found.name, width, height, fingerprint);
		pictures++;
	}
	while (_findnext(search, &found) == 0);

	_findclose(search);
	fprintf(stderr, "%d pictures, %d refused\n", pictures, failed);
	return 0;
}
