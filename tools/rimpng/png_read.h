// Reads a PNG, for tools/rimpng. Written 2026-11-05 by Melanikus for the AvP port.
//
// PNGs here come from image generators and the internet, so nothing in one is trusted. Every length is checked against what's really there, and the unpacking can't write past the room it
// was given. Each reason for refusing a file is words of its own, so a test can check for that exact reason. This doesn't read every PNG there is, only the kinds a normal map comes in, and
// it says so about the others.

#ifndef PNG_READ_H
#define PNG_READ_H

#include <stdint.h>
#include <string.h>

#include <vector>

// An image bigger than this is far more than any map needs, and would take too much memory in a 32-bit program.
uint32_t const kPngLargestSide = 8192;
uint64_t const kPngMostPixels = 16 * 1024 * 1024;

struct PngPicture
{
	uint32_t width, height;
	std::vector<uint8_t> rgb;    // three bytes a pixel
};

// ---- Unpacking: the deflate format, which is what's inside a PNG. Short sequences of bits stand for bytes, or for "copy this many bytes from that far back".

class PngInflate
{
	public:
		PngInflate(uint8_t const *from, size_t fromBytes, uint8_t *to, size_t toBytes)
			: in(from), inSize(fromBytes), inPlace(0), bitBuffer(0), bitCount(0), out(to), outSize(toBytes), outPlace(0) {}

		// Gives back 0 if it worked, or words saying why it didn't
		char const *Run()
		{
			for (;;)
			{
				int const last = Bits(1);
				int const kind = Bits(2);

				if (last < 0 || kind < 0)
				{
					return "the packed part ends too soon";
				}

				char const *problem = (kind == 0) ? Stored() : (kind == 1) ? FixedCodes() : (kind == 2) ? DynamicCodes() : "the packed part uses a kind of block that doesn't exist";

				if (problem)
				{
					return problem;
				}

				if (last)
				{
					return 0;
				}
			}
		}

		size_t Written() const { return outPlace; }
		size_t BytesUsed() const { return inPlace; }

	private:
		struct Codes
		{
			uint16_t count[16];     // how many codes there are of each length
			uint16_t symbol[320];   // what they stand for, in order
		};

		int Bits(int need)
		{
			while (bitCount < need)
			{
				if (inPlace >= inSize)
				{
					return -1;
				}

				bitBuffer |= (uint32_t)in[inPlace++] << bitCount;
				bitCount += 8;
			}

			int const value = (int)(bitBuffer & ((1u << need) - 1));
			bitBuffer >>= need;
			bitCount -= need;
			return value;
		}

		char const *Stored()
		{
			bitBuffer = 0;
			bitCount = 0;

			if (inSize - inPlace < 4)
			{
				return "the packed part ends too soon";
			}

			uint32_t const length = in[inPlace] | ((uint32_t)in[inPlace + 1] << 8);
			uint32_t const check = in[inPlace + 2] | ((uint32_t)in[inPlace + 3] << 8);
			inPlace += 4;

			if ((length ^ 0xFFFF) != check)
			{
				return "a stored block's length is damaged";
			}

			if (inSize - inPlace < length)
			{
				return "the packed part ends too soon";
			}

			if (outSize - outPlace < length)
			{
				return "it unpacks to more than the image needs";
			}

			memcpy(out + outPlace, in + inPlace, length);
			inPlace += length;
			outPlace += length;
			return 0;
		}

		// How many codes are left unused: 0 for a complete set, more for one with gaps, and below zero for a set that's impossible
		static int MakeCodes(Codes &codes, uint16_t const *lengths, int howMany)
		{
			uint16_t places[16];

			memset(codes.count, 0, sizeof(codes.count));

			for (int i = 0; i < howMany; i++)
			{
				codes.count[lengths[i]]++;
			}

			int left = 1;

			for (int length = 1; length <= 15; length++)
			{
				left = left * 2 - codes.count[length];

				if (left < 0)
				{
					return left;
				}
			}

			places[1] = 0;

			for (int length = 1; length < 15; length++)
			{
				places[length + 1] = (uint16_t)(places[length] + codes.count[length]);
			}

			for (int i = 0; i < howMany; i++)
			{
				if (lengths[i] != 0)
				{
					codes.symbol[places[lengths[i]]++] = (uint16_t)i;
				}
			}

			return left;
		}

		int NextSymbol(Codes const &codes)
		{
			int code = 0, first = 0, index = 0;

			for (int length = 1; length <= 15; length++)
			{
				int const bit = Bits(1);

				if (bit < 0)
				{
					return -1;
				}

				code |= bit;

				int const count = codes.count[length];

				if (code - count < first)
				{
					return codes.symbol[index + (code - first)];
				}

				index += count;
				first = (first + count) * 2;
				code *= 2;
			}

			return -2;
		}

		char const *UseCodes(Codes const &lengthCodes, Codes const &distanceCodes)
		{
			static uint16_t const lengthBase[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
			static uint8_t const lengthExtra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
			static uint16_t const distanceBase[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
			static uint8_t const distanceExtra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

			for (;;)
			{
				int const symbol = NextSymbol(lengthCodes);

				if (symbol < 0)
				{
					return (symbol == -1) ? "the packed part ends too soon" : "the packed part has a code that means nothing";
				}

				if (symbol < 256)
				{
					if (outPlace >= outSize)
					{
						return "it unpacks to more than the image needs";
					}

					out[outPlace++] = (uint8_t)symbol;
				}
				else if (symbol == 256)
				{
					return 0;
				}
				else
				{
					int const lengthCode = symbol - 257;

					if (lengthCode >= 29)
					{
						return "the packed part has a length that doesn't exist";
					}

					int const lengthBits = Bits(lengthExtra[lengthCode]);
					int const distanceCode = (lengthBits < 0) ? -1 : NextSymbol(distanceCodes);

					if (lengthBits < 0 || distanceCode == -1)
					{
						return "the packed part ends too soon";
					}

					if (distanceCode < 0 || distanceCode >= 30)
					{
						return "the packed part has a distance that doesn't exist";
					}

					int const distanceBits = Bits(distanceExtra[distanceCode]);

					if (distanceBits < 0)
					{
						return "the packed part ends too soon";
					}

					size_t const length = (size_t)lengthBase[lengthCode] + lengthBits;
					size_t const distance = (size_t)distanceBase[distanceCode] + distanceBits;

					if (distance > outPlace)
					{
						return "the packed part copies from before its start";
					}

					if (outSize - outPlace < length)
					{
						return "it unpacks to more than the image needs";
					}

					// one byte at a time, because a copy can overlap what it's writing, and that's how a run of one byte is made
					for (size_t i = 0; i < length; i++)
					{
						out[outPlace] = out[outPlace - distance];
						outPlace++;
					}
				}
			}
		}

		char const *FixedCodes()
		{
			static Codes lengthCodes, distanceCodes;
			static bool made = false;

			if (!made)
			{
				uint16_t lengths[288 + 30];
				int symbol = 0;

				for (; symbol < 144; symbol++) lengths[symbol] = 8;
				for (; symbol < 256; symbol++) lengths[symbol] = 9;
				for (; symbol < 280; symbol++) lengths[symbol] = 7;
				for (; symbol < 288; symbol++) lengths[symbol] = 8;

				MakeCodes(lengthCodes, lengths, 288);

				for (symbol = 0; symbol < 30; symbol++) lengths[symbol] = 5;

				MakeCodes(distanceCodes, lengths, 30);
				made = true;
			}

			return UseCodes(lengthCodes, distanceCodes);
		}

		char const *DynamicCodes()
		{
			static uint8_t const order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

			int const numLengths = Bits(5);
			int const numDistances = Bits(5);
			int const numCodeLengths = Bits(4);

			if (numLengths < 0 || numDistances < 0 || numCodeLengths < 0)
			{
				return "the packed part ends too soon";
			}

			int const lengthsWanted = numLengths + 257;
			int const distancesWanted = numDistances + 1;

			if (lengthsWanted > 286 || distancesWanted > 30)
			{
				return "the packed part asks for more codes than there are";
			}

			uint16_t lengths[320];
			int place = 0;

			for (; place < numCodeLengths + 4; place++)
			{
				int const bits = Bits(3);

				if (bits < 0)
				{
					return "the packed part ends too soon";
				}

				lengths[order[place]] = (uint16_t)bits;
			}

			for (; place < 19; place++)
			{
				lengths[order[place]] = 0;
			}

			Codes codeLengthCodes;

			if (MakeCodes(codeLengthCodes, lengths, 19) != 0)
			{
				return "the packed part's codes are impossible";
			}

			place = 0;

			while (place < lengthsWanted + distancesWanted)
			{
				int const symbol = NextSymbol(codeLengthCodes);

				if (symbol < 0)
				{
					return (symbol == -1) ? "the packed part ends too soon" : "the packed part has a code that means nothing";
				}

				if (symbol < 16)
				{
					lengths[place++] = (uint16_t)symbol;
					continue;
				}

				int repeat;
				uint16_t value = 0;

				if (symbol == 16)
				{
					if (place == 0)
					{
						return "the packed part repeats a length before there is one";
					}

					value = lengths[place - 1];
					repeat = Bits(2);
					repeat = (repeat < 0) ? -1 : repeat + 3;
				}
				else if (symbol == 17)
				{
					repeat = Bits(3);
					repeat = (repeat < 0) ? -1 : repeat + 3;
				}
				else
				{
					repeat = Bits(7);
					repeat = (repeat < 0) ? -1 : repeat + 11;
				}

				if (repeat < 0)
				{
					return "the packed part ends too soon";
				}

				if (place + repeat > lengthsWanted + distancesWanted)
				{
					return "the packed part repeats a length too many times";
				}

				for (int i = 0; i < repeat; i++)
				{
					lengths[place++] = value;
				}
			}

			if (lengths[256] == 0)
			{
				return "the packed part has no way of saying where it ends";
			}

			Codes lengthCodes, distanceCodes;
			int const lengthsLeft = MakeCodes(lengthCodes, lengths, lengthsWanted);

			// a set with gaps is allowed only if it's a single code, which is how the format writes "nothing else to say"
			if (lengthsLeft < 0 || (lengthsLeft > 0 && lengthsWanted - lengthCodes.count[0] != 1))
			{
				return "the packed part's codes are impossible";
			}

			int const distancesLeft = MakeCodes(distanceCodes, lengths + lengthsWanted, distancesWanted);

			if (distancesLeft < 0 || (distancesLeft > 0 && distancesWanted - distanceCodes.count[0] != 1))
			{
				return "the packed part's codes are impossible";
			}

			return UseCodes(lengthCodes, distanceCodes);
		}

		uint8_t const *in;
		size_t inSize, inPlace;
		uint32_t bitBuffer;
		int bitCount;
		uint8_t *out;
		size_t outSize, outPlace;
};

// ---- The numbers that say a part isn't damaged

inline uint32_t PngRead_Crc(uint8_t const *bytes, size_t size)
{
	static uint32_t table[256];
	static bool made = false;

	if (!made)
	{
		for (uint32_t n = 0; n < 256; n++)
		{
			uint32_t c = n;

			for (int bit = 0; bit < 8; bit++)
			{
				c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
			}

			table[n] = c;
		}

		made = true;
	}

	uint32_t crc = 0xFFFFFFFFu;

	for (size_t i = 0; i < size; i++)
	{
		crc = table[(crc ^ bytes[i]) & 255] ^ (crc >> 8);
	}

	return crc ^ 0xFFFFFFFFu;
}

inline uint32_t PngRead_Big32(uint8_t const *bytes)
{
	return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

// ---- Undoing the filters. Each row of a PNG says which sum was taken off it to make it pack better. Each is worked from the pixel to the left and the row above.

inline uint8_t PngRead_Paeth(int left, int up, int upLeft)
{
	int const p = left + up - upLeft;
	int const toLeft = (p > left) ? p - left : left - p;
	int const toUp = (p > up) ? p - up : up - p;
	int const toUpLeft = (p > upLeft) ? p - upLeft : upLeft - p;

	return (uint8_t)((toLeft <= toUp && toLeft <= toUpLeft) ? left : (toUp <= toUpLeft) ? up : upLeft);
}

inline char const *PngRead_Unfilter(uint8_t *rows, uint32_t height, size_t rowBytes, uint32_t bytesPerPixel)
{
	for (uint32_t y = 0; y < height; y++)
	{
		uint8_t *row = rows + (size_t)y * (rowBytes + 1);
		uint8_t const filter = row[0];
		uint8_t const *above = (y > 0) ? row - rowBytes : 0;

		row++;

		if (filter > 4)
		{
			return "a row says it was filtered in a way that doesn't exist";
		}

		for (size_t i = 0; i < rowBytes; i++)
		{
			int const left = (i >= bytesPerPixel) ? row[i - bytesPerPixel] : 0;
			int const up = above ? above[i] : 0;
			int const upLeft = (above && i >= bytesPerPixel) ? above[i - bytesPerPixel] : 0;
			int add = 0;

			switch (filter)
			{
				case 1: add = left; break;
				case 2: add = up; break;
				case 3: add = (left + up) / 2; break;
				case 4: add = PngRead_Paeth(left, up, upLeft); break;
				default: break;
			}

			row[i] = (uint8_t)(row[i] + add);
		}
	}

	return 0;
}

// ---- A whole file. Gives back 0 if it worked, or words saying why it didn't.

inline char const *Png_Read(uint8_t const *file, size_t size, PngPicture *picture)
{
	static uint8_t const signature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };

	if (size < 8 || memcmp(file, signature, 8) != 0)
	{
		return "it doesn't start the way a PNG does";
	}

	uint32_t width = 0, height = 0, depth = 0, colourType = 0;
	bool haveHeading = false, haveEnd = false;
	uint8_t const *palette = 0;
	uint32_t paletteColours = 0;
	std::vector<uint8_t> packed;
	size_t place = 8;

	while (!haveEnd)
	{
		if (size - place < 12)
		{
			return "the file ends in the middle of a part";
		}

		uint32_t const length = PngRead_Big32(file + place);
		uint8_t const *type = file + place + 4;

		if (length > size - place - 12)
		{
			return "a part says it's longer than the file";
		}

		if (PngRead_Crc(type, (size_t)length + 4) != PngRead_Big32(file + place + 8 + length))
		{
			return "a part is damaged: its check number doesn't match";
		}

		uint8_t const *body = file + place + 8;
		bool const isHeading = memcmp(type, "IHDR", 4) == 0;

		if (isHeading != !haveHeading)
		{
			return isHeading ? "it has two headings" : "it doesn't start with its heading";
		}

		if (isHeading)
		{
			if (length != 13)
			{
				return "its heading is the wrong size";
			}

			width = PngRead_Big32(body);
			height = PngRead_Big32(body + 4);
			depth = body[8];
			colourType = body[9];

			if (width == 0 || height == 0 || width > kPngLargestSide || height > kPngLargestSide || (uint64_t)width * height > kPngMostPixels)
			{
				return "the image is too big, or says it has no size";
			}

			if (colourType == 0 || colourType == 4)
			{
				return "it's grey, which is a height or depth picture, not a normal map";
			}

			if ((colourType != 2 && colourType != 3 && colourType != 6) || (colourType == 3 ? depth != 8 : (depth != 8 && depth != 16)))
			{
				return "it's a kind of PNG this doesn't read: use 8 or 16 bits a colour, red, green and blue";
			}

			if (body[10] != 0 || body[11] != 0)
			{
				return "it's packed or filtered in a way that doesn't exist";
			}

			if (body[12] != 0)
			{
				return "it's interlaced, which this doesn't read";
			}

			haveHeading = true;
		}
		else if (memcmp(type, "PLTE", 4) == 0)
		{
			if (length == 0 || length % 3 != 0 || length > 256 * 3)
			{
				return "its colour list is the wrong size";
			}

			palette = body;
			paletteColours = length / 3;
		}
		else if (memcmp(type, "IDAT", 4) == 0)
		{
			packed.insert(packed.end(), body, body + length);
		}
		else if (memcmp(type, "IEND", 4) == 0)
		{
			haveEnd = true;
		}

		place += (size_t)length + 12;
	}

	if (packed.size() < 6)
	{
		return "it has no pixels in it";
	}

	if (colourType == 3 && !palette)
	{
		return "it uses a colour list it doesn't have";
	}

	// The wrapper round the packed part: two bytes at the start, and at the end a number that checks everything unpacked
	if ((packed[0] & 15) != 8 || (packed[0] >> 4) > 7 || (packed[1] & 32) != 0 || ((uint32_t)packed[0] * 256 + packed[1]) % 31 != 0)
	{
		return "the packed part doesn't start as it should";
	}

	uint32_t const channels = (colourType == 2) ? 3 : (colourType == 3) ? 1 : 4;
	uint32_t const bytesPerPixel = channels * (depth / 8);
	size_t const rowBytes = (size_t)width * bytesPerPixel;
	size_t const allBytes = (rowBytes + 1) * height;

	std::vector<uint8_t> rows(allBytes);
	PngInflate inflate(&packed[2], packed.size() - 2, &rows[0], allBytes);
	char const *problem = inflate.Run();

	if (problem)
	{
		return problem;
	}

	if (inflate.Written() != allBytes)
	{
		return "it unpacks to less than the image needs";
	}

	if (packed.size() - 2 - inflate.BytesUsed() < 4)
	{
		return "the packed part ends too soon";
	}

	uint32_t a = 1, b = 0;

	for (size_t i = 0; i < allBytes; i++)
	{
		a = (a + rows[i]) % 65521;
		b = (b + a) % 65521;
	}

	if (((b << 16) | a) != PngRead_Big32(&packed[2 + inflate.BytesUsed()]))
	{
		return "the unpacked image is damaged: its check number doesn't match";
	}

	problem = PngRead_Unfilter(&rows[0], height, rowBytes, bytesPerPixel);

	if (problem)
	{
		return problem;
	}

	picture->width = width;
	picture->height = height;
	picture->rgb.resize((size_t)width * height * 3);

	uint8_t *to = &picture->rgb[0];

	for (uint32_t y = 0; y < height; y++)
	{
		uint8_t const *from = &rows[(size_t)y * (rowBytes + 1) + 1];

		for (uint32_t x = 0; x < width; x++, from += bytesPerPixel, to += 3)
		{
			if (colourType == 3)
			{
				if (from[0] >= paletteColours)
				{
					return "a pixel uses a colour past the end of its list";
				}

				memcpy(to, palette + (size_t)from[0] * 3, 3);
			}
			else
			{
				// with 16 bits a colour, the first byte of each is the one that matters
				uint32_t const step = depth / 8;
				to[0] = from[0];
				to[1] = from[step];
				to[2] = from[step * 2];
			}
		}
	}

	return 0;
}

#endif
