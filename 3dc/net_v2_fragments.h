// Protocol version 2's handshake datagrams: every handshake message cut into, and put back together from, datagrams of exactly 1,200
// bytes. Written 2026-11-09 by Melanikus for the AvP port.
//
// WHY. A handshake message is 1.6 to 3.4 KB, more than one datagram should carry. So each one travels as "fragments", and every
// handshake datagram, either way, is exactly 1,200 bytes: the smallest size the internet carries whole (QUIC uses it too). Nothing is
// ever cut in two on the way, and nobody can be sent more than they sent. CRYPTO.md section 5 (in the AVPx-Dedicated-Server repository)
// says it, and its section 5.1 point 10 says how strictly a fragment is read. The relay's Java (java/src/avpx/v2/Fragments.java) reads
// them the very same way.
//
// ONE DATAGRAM:
//   bytes 0-3      'AVPR' (to and from the relay) or 'AVPE' (handshake B, between a joiner and its host)
//   byte  4        the version, 2
//   byte  5        the kind (below)
//   byte  6        this fragment's number, from 0
//   byte  7        how many fragments the message has, 1 to 4
//   bytes 8-15     the handshake's id: 8 random bytes the side that starts it chose
//   bytes 16-31    the relay's cookie, or 16 zero bytes where there's none
//   bytes 32-33    the whole message's length, little-endian
//   bytes 34-35    where this fragment's part starts in the message, little-endian
//   bytes 36-1199  up to 1,164 bytes of the message, then zeros
//
// READ ONLY ONE WAY (point 10), so that no two readers can ever disagree about what a datagram says:
//   - every fragment but the last carries exactly 1,164 bytes of the message, and fragment k starts at k x 1,164;
//   - the count is the length divided by 1,164, rounded up (1 for a message of no bytes), from 1 to 4;
//   - every fragment of one message says the same kind, id, count and length;
//   - every byte after a fragment's part is 0.
// Anything else isn't a fragment at all: refusal 6. Everything that comes in is hostile (AvP Tao 3): nothing is read from a datagram
// before its size and its fixed bytes have been checked, and nothing here allocates or keeps a pointer past the call it was given in.
//
// THE COOKIE ROUND (CRYPTO.md section 6, A0 and A0'): before A1, the game asks the relay for a cookie with a COOKIE_ASK, which is fragment
// 0 of 1 of a message of no bytes, with a zero cookie, padded to 1,200 bytes. The relay answers with 64 bytes: 'AVPR', 2, the kind COOKIE,
// two zeros, the id, the cookie, then zeros. The cookie then goes on every A1 and A3 datagram.
//
// Handshake A's and B's own files (net_handshake_a.h, net_handshake_b.h) use this one; neither writes datagrams any other way.

#ifndef NET_V2_FRAGMENTS_H
#define NET_V2_FRAGMENTS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

size_t const kNetV2DatagramBytes = 1200;                                     // every handshake datagram, exactly
size_t const kNetV2HeaderBytes = 36;
size_t const kNetV2PartBytes = kNetV2DatagramBytes - kNetV2HeaderBytes;      // 1,164: the most of the message one datagram carries
size_t const kNetV2MostFragments = 4;
size_t const kNetV2MostMessageBytes = kNetV2MostFragments * kNetV2PartBytes;  // 4,656
size_t const kNetV2IdBytes = 8;
size_t const kNetV2CookieBytes = 16;
size_t const kNetV2CookieAnswerBytes = 64;                                   // A0', the relay's answer to a COOKIE_ASK
uint8_t const kNetV2Version = 2;

// The magics: to and from the relay, and handshake B's end to end
static const uint8_t kNetV2MagicRelay[4] = { 'A', 'V', 'P', 'R' };
static const uint8_t kNetV2MagicEndToEnd[4] = { 'A', 'V', 'P', 'E' };

// The kinds (CRYPTO.md sections 6 to 8)
uint8_t const kNetV2KindCookieAsk = 1;   // A0, game to relay
uint8_t const kNetV2KindCookie = 2;      // A0', relay to game: 64 bytes, not a fragment
uint8_t const kNetV2KindA1 = 3;
uint8_t const kNetV2KindA2 = 4;
uint8_t const kNetV2KindA3 = 5;
uint8_t const kNetV2KindB1 = 16;         // handshake B's are 16 to 20, always 'AVPE'
uint8_t const kNetV2KindB5 = 20;
uint8_t const kNetV2KindSealed = 32;     // a sealed control message (net_sealed_control.h): 104 bytes, not a fragment

// How many fragments a message of length bytes takes: 1 to 4 for a length the protocol allows
inline size_t NetV2_FragmentCount(size_t length)
{
	return length == 0 ? 1 : (length + kNetV2PartBytes - 1) / kNetV2PartBytes;
}

// Writes fragment number index of message (length bytes) into out, a whole 1,200-byte datagram. False, with out all zeros, for anything
// the protocol can't send: a message over 4,656 bytes, an index past its count, or a missing buffer.
inline bool NetV2_CutFragment(const uint8_t magic[4], uint8_t kind, const uint8_t id[kNetV2IdBytes], const uint8_t cookie[kNetV2CookieBytes],
	const uint8_t *message, size_t length, size_t index, uint8_t out[kNetV2DatagramBytes])
{
	if (!out)
	{
		return false;
	}

	memset(out, 0, kNetV2DatagramBytes);
	size_t const count = NetV2_FragmentCount(length);

	if (!magic || !id || !cookie || (length > 0 && !message) || length > kNetV2MostMessageBytes || index >= count)
	{
		return false;
	}

	size_t const start = index * kNetV2PartBytes;
	size_t const part = (length - start < kNetV2PartBytes) ? length - start : kNetV2PartBytes;
	memcpy(out, magic, 4);
	out[4] = kNetV2Version;
	out[5] = kind;
	out[6] = static_cast<uint8_t>(index);
	out[7] = static_cast<uint8_t>(count);
	memcpy(out + 8, id, kNetV2IdBytes);
	memcpy(out + 16, cookie, kNetV2CookieBytes);
	out[32] = static_cast<uint8_t>(length & 0xFF);
	out[33] = static_cast<uint8_t>((length >> 8) & 0xFF);
	out[34] = static_cast<uint8_t>(start & 0xFF);
	out[35] = static_cast<uint8_t>((start >> 8) & 0xFF);

	if (part > 0)
	{
		memcpy(out + kNetV2HeaderBytes, message + start, part);
	}

	return true;
}

// One fragment, as read from a datagram. part points into that datagram, so it lasts only as long as the datagram does.
struct NetV2Fragment
{
	uint8_t kind;
	size_t index;
	size_t count;
	uint8_t id[kNetV2IdBytes];
	uint8_t cookie[kNetV2CookieBytes];
	size_t length;          // the whole message's
	size_t start;           // where this part goes in it
	size_t partBytes;
	const uint8_t *part;
};

// Reads a datagram from outside as a fragment under magic. False, with *out all zeros, unless it is exactly one, by every rule above.
inline bool NetV2_ReadFragment(const uint8_t magic[4], const uint8_t *datagram, size_t size, NetV2Fragment *out)
{
	if (!out)
	{
		return false;
	}

	memset(out, 0, sizeof(*out));

	if (!magic || !datagram || size != kNetV2DatagramBytes || memcmp(datagram, magic, 4) != 0 || datagram[4] != kNetV2Version)
	{
		return false;
	}

	size_t const index = datagram[6];
	size_t const count = datagram[7];
	size_t const length = static_cast<size_t>(datagram[32]) | (static_cast<size_t>(datagram[33]) << 8);
	size_t const start = static_cast<size_t>(datagram[34]) | (static_cast<size_t>(datagram[35]) << 8);

	if (count < 1 || count > kNetV2MostFragments || index >= count || length > kNetV2MostMessageBytes || count != NetV2_FragmentCount(length) ||
		start != index * kNetV2PartBytes || start > length)
	{
		return false;
	}

	size_t const part = (length - start < kNetV2PartBytes) ? length - start : kNetV2PartBytes;

	// A fragment but the last is full, which the count and the start already make so; every byte after the part must be 0
	for (size_t i = kNetV2HeaderBytes + part; i < kNetV2DatagramBytes; i++)
	{
		if (datagram[i] != 0)
		{
			return false;
		}
	}

	out->kind = datagram[5];
	out->index = index;
	out->count = count;
	memcpy(out->id, datagram + 8, kNetV2IdBytes);
	memcpy(out->cookie, datagram + 16, kNetV2CookieBytes);
	out->length = length;
	out->start = start;
	out->partBytes = part;
	out->part = datagram + kNetV2HeaderBytes;
	return true;
}

// A message being put back together from its fragments, which may come in any order, and more than once. It holds a copy of every
// part, so the datagrams can go as soon as each is added.
struct NetV2Assembly
{
	bool started;
	uint8_t kind;
	uint8_t id[kNetV2IdBytes];
	size_t count;
	size_t length;
	bool have[kNetV2MostFragments];
	size_t haveCount;
	uint8_t message[kNetV2MostMessageBytes];
};

inline void NetV2_AssemblyClear(NetV2Assembly *a)
{
	memset(a, 0, sizeof(*a));
}

// Takes one fragment. The first one fixes the message's kind, id, count and length; any later one must say the same, and one that comes
// again must carry exactly the bytes it carried before. False for a fragment that doesn't belong (refusal 6), and then nothing changes.
inline bool NetV2_AssemblyAdd(NetV2Assembly *a, const NetV2Fragment *f)
{
	// What NetV2_ReadFragment already made so, checked again here, so that no fragment however it was made can write past the message
	if (!a || !f || !f->part || f->index >= f->count || f->count > kNetV2MostFragments || f->start != f->index * kNetV2PartBytes ||
		f->partBytes > kNetV2PartBytes || f->start + f->partBytes > f->length || f->length > kNetV2MostMessageBytes)
	{
		return false;
	}

	if (!a->started)
	{
		NetV2_AssemblyClear(a);
		a->started = true;
		a->kind = f->kind;
		memcpy(a->id, f->id, kNetV2IdBytes);
		a->count = f->count;
		a->length = f->length;
	}
	else if (f->kind != a->kind || memcmp(f->id, a->id, kNetV2IdBytes) != 0 || f->count != a->count || f->length != a->length)
	{
		return false;
	}

	if (a->have[f->index])
	{
		return memcmp(a->message + f->start, f->part, f->partBytes) == 0;
	}

	memcpy(a->message + f->start, f->part, f->partBytes);
	a->have[f->index] = true;
	a->haveCount++;
	return true;
}

// Whether every fragment is in: then a->message holds a->length bytes
inline bool NetV2_AssemblyComplete(const NetV2Assembly *a)
{
	return a && a->started && a->haveCount == a->count;
}

// A0, the COOKIE_ASK: fragment 0 of 1 of a message of no bytes, with a zero cookie (CRYPTO.md section 5.1 point 9)
inline bool NetV2_CookieAsk(const uint8_t id[kNetV2IdBytes], uint8_t out[kNetV2DatagramBytes])
{
	static const uint8_t kNoCookie[kNetV2CookieBytes] = {};
	return NetV2_CutFragment(kNetV2MagicRelay, kNetV2KindCookieAsk, id, kNoCookie, NULL, 0, 0, out);
}

// Reads A0', the relay's 64-byte answer to our COOKIE_ASK for id: 'AVPR', 2, COOKIE, two zeros, our id, the cookie, then 32 zeros. False,
// with cookieOut all zeros, for anything else, an answer for another handshake among them.
inline bool NetV2_ReadCookie(const uint8_t *answer, size_t size, const uint8_t id[kNetV2IdBytes], uint8_t cookieOut[kNetV2CookieBytes])
{
	if (!cookieOut)
	{
		return false;
	}

	memset(cookieOut, 0, kNetV2CookieBytes);

	if (!answer || !id || size != kNetV2CookieAnswerBytes || memcmp(answer, kNetV2MagicRelay, 4) != 0 || answer[4] != kNetV2Version ||
		answer[5] != kNetV2KindCookie || answer[6] != 0 || answer[7] != 0 || memcmp(answer + 8, id, kNetV2IdBytes) != 0)
	{
		return false;
	}

	for (size_t i = 16 + kNetV2CookieBytes; i < kNetV2CookieAnswerBytes; i++)
	{
		if (answer[i] != 0)
		{
			return false;
		}
	}

	memcpy(cookieOut, answer + 16, kNetV2CookieBytes);
	return true;
}

#endif
