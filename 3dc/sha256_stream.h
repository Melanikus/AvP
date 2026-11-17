// SHA-256 (FIPS 180-4), fed in pieces as they arrive. Written 2026-11-17 by Melanikus for the AvP port.
//
// WHY: the level loader (File_Chunk, mishchnk.cpp) sums every byte it reads. MD5 is enough to tell two honest copies of a level apart in a
// log, but the level editor (AVP_OpenInteng) uses the loader's sum to PROVE the loader read exactly the bytes the editor wrote, and for a
// proof MD5 is too weak (anyone can make two files with the same MD5). So the loader also works out SHA-256 of the same reads, and the
// editor compares that. The game's MD5 stays for its log line and its save records (level_file_sum.h, save_level_record.h).
//
// HOW: header-only and self-contained, so the game, the editor and the tests that compile the loader need no other file or library. It
// is the standard's own algorithm, word for word: the eight starting words, the 64 round constants, the message padded with 0x80, zeros
// and the length in bits, big-endian. tests/level_file_sum_test checks it against the standard's own answers and against Windows' CNG.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct Sha256Stream
{
	uint32_t state[8];
	uint64_t bytes;          // how many bytes have been added in all
	uint8_t block[64];       // the bytes of the block being filled
	size_t filled;           // how many of them there are
};

static inline uint32_t Sha256_Rotate(uint32_t x, int by)
{
	return (x >> by) | (x << (32 - by));
}

// One 64-byte block into the state (FIPS 180-4 section 6.2.2)
static inline void Sha256_Block(Sha256Stream *s, const uint8_t *block)
{
	static const uint32_t k[64] =
	{
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
		0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
		0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
		0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
		0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
		0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
		0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
	};
	uint32_t w[64];

	for (int t = 0; t < 16; t++)
	{
		w[t] = ((uint32_t)block[t * 4] << 24) | ((uint32_t)block[t * 4 + 1] << 16) | ((uint32_t)block[t * 4 + 2] << 8) | (uint32_t)block[t * 4 + 3];
	}

	for (int t = 16; t < 64; t++)
	{
		uint32_t const s0 = Sha256_Rotate(w[t - 15], 7) ^ Sha256_Rotate(w[t - 15], 18) ^ (w[t - 15] >> 3);
		uint32_t const s1 = Sha256_Rotate(w[t - 2], 17) ^ Sha256_Rotate(w[t - 2], 19) ^ (w[t - 2] >> 10);
		w[t] = w[t - 16] + s0 + w[t - 7] + s1;
	}

	uint32_t a = s->state[0], b = s->state[1], c = s->state[2], d = s->state[3], e = s->state[4], f = s->state[5], g = s->state[6], h = s->state[7];

	for (int t = 0; t < 64; t++)
	{
		uint32_t const big1 = Sha256_Rotate(e, 6) ^ Sha256_Rotate(e, 11) ^ Sha256_Rotate(e, 25);
		uint32_t const choose = (e & f) ^ (~e & g);
		uint32_t const t1 = h + big1 + choose + k[t] + w[t];
		uint32_t const big0 = Sha256_Rotate(a, 2) ^ Sha256_Rotate(a, 13) ^ Sha256_Rotate(a, 22);
		uint32_t const majority = (a & b) ^ (a & c) ^ (b & c);
		uint32_t const t2 = big0 + majority;
		h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
	}

	s->state[0] += a; s->state[1] += b; s->state[2] += c; s->state[3] += d;
	s->state[4] += e; s->state[5] += f; s->state[6] += g; s->state[7] += h;
}

static inline void Sha256_Start(Sha256Stream *s)
{
	static const uint32_t first[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };

	memcpy(s->state, first, sizeof(first));
	s->bytes = 0;
	s->filled = 0;
}

static inline void Sha256_Add(Sha256Stream *s, const void *bytes, size_t size)
{
	const uint8_t *from = (const uint8_t *)bytes;

	s->bytes += size;

	while (size > 0)
	{
		size_t const take = (64 - s->filled < size) ? 64 - s->filled : size;

		memcpy(s->block + s->filled, from, take);
		s->filled += take;
		from += take;
		size -= take;

		if (s->filled == 64)
		{
			Sha256_Block(s, s->block);
			s->filled = 0;
		}
	}
}

// The sum, 32 bytes, big-endian as the standard writes it. The stream is used up: start it again for another sum.
static inline void Sha256_Finish(Sha256Stream *s, uint8_t out[32])
{
	uint64_t const bits = s->bytes * 8;
	uint8_t const one = 0x80, zero = 0;
	uint8_t length[8];

	Sha256_Add(s, &one, 1);

	while (s->filled != 56)
	{
		Sha256_Add(s, &zero, 1);
	}

	for (int i = 0; i < 8; i++)
	{
		length[i] = (uint8_t)(bits >> (56 - 8 * i));
	}

	Sha256_Add(s, length, 8);

	for (int i = 0; i < 8; i++)
	{
		out[i * 4] = (uint8_t)(s->state[i] >> 24);
		out[i * 4 + 1] = (uint8_t)(s->state[i] >> 16);
		out[i * 4 + 2] = (uint8_t)(s->state[i] >> 8);
		out[i * 4 + 3] = (uint8_t)s->state[i];
	}
}
