// Handshake B of protocol version 2: a joiner and its host prove themselves to each other and agree keys, end to end, through the relay,
// which only passes the bytes on and can't read them (CRYPTO.md section 8, and 5.1 points 17 to 20, in the AVPx-Dedicated-Server
// repository). Written 2026-11-09 by Melanikus for the AvP port.
//
// WHAT IT DOES, in plain words. Before a joiner and a host trade a single game byte, they swap five messages, B1 to B5. By the end:
//   - the joiner knows it's talking to the host the list named (the host's keys match the fingerprint in the listing), and nobody else;
//   - the host knows the joiner holds the keys it showed;
//   - both have two fresh keys, one for each direction, that only they know, made from classical (P-384) and post-quantum (ML-KEM-1024)
//     secrets together, so the keys hold unless both kinds are broken;
//   - both can show the same 6-digit "check code", which the two players can read to each other to be sure nobody sat in the middle.
//
// THE FIVE MESSAGES (Noise pattern XX with post-quantum tokens: "pqXX"). Each token is one step on both sides (net_noise.h says what
// MixHash, MixKey, EncryptAndHash and Split are):
//   B1  joiner -> host  e, then a promise    the joiner's fresh P-384 key and fresh ML-KEM key, then SHA-384(nJ): nJ is 32 random bytes
//                                            the joiner keeps hidden until B5, its share of the check code (the "commitment")
//   B2  host -> joiner  e, ee, ekem, s, es   the host's fresh P-384 key; the two fresh keys' ECDH; a secret encapsulated to the joiner's
//                                            fresh ML-KEM key; the host's own keys, sealed; the joiner's fresh key with the host's own
//                                            key; then a tag
//   B3  joiner -> host  skem, s, se          a secret encapsulated to the host's own ML-KEM key; the joiner's own keys, sealed; the
//                                            joiner's own key with the host's fresh key; then a tag
//   B4  host -> joiner  skem, nH             a secret encapsulated to the joiner's own ML-KEM key; then nH, the host's 32 random bytes for
//                                            the check code, sealed: this tag proves the host holds every key it claimed
//   B5  joiner -> host  nJ                   nJ, sealed: this tag proves the joiner holds every key it claimed, and nJ must be what B1
//                                            promised, or the host ends it (refusal 21)
// Before any message, both sides mix in the "prologue": the region's name and a 0 byte, the joiner's relay port (2 bytes, little-endian)
// and the host's fingerprint as the listing gives it (5.1 point 17). So a B made for one relay, port or host can't be used for another.
//
// WHY THE PROMISE (5.1 point 19). The check code is made from the transcript. Without B1's promise, someone in the middle could try a
// million fresh values before sending its last message, until both screens happened to show the same 6 digits. With it, every choice it
// could make is fixed before the value that decides each code shows up, so one handshake is one try, at one in a million.
//
// WHAT THIS FILE DOESN'T DO:
//   - draw randomness. Every fresh key, encapsulation and random byte is handed in by the caller, in the order 5.1 point 14 sets, so the
//     game draws for real and the test hands in the test vectors' values. That's why each side's turn is a "read" and then a "write":
//     B3's and B4's encapsulations go to keys the caller only learns from the message before (HostKemPublic, JoinerKemPublic);
//   - send, cut into datagrams, resend or time anything: net_peer_v2.h does that, with net_v2_fragments.h.
// It's game-free, as net_noise.h is, so its test (tests/net_v2b_test.cpp) can try every case.
//
// HOW IT ANSWERS. Every call gives back true or false. False ends the handshake for good: Why() says why, with CRYPTO.md section 16's
// number where it has one, every secret is wiped, and every output is wiped too, so half a message can never go out by mistake. A call
// made out of turn is refused the same way.
//
// SECRETS. The fresh P-384 keys live inside CNG and are destroyed the moment they've done their last ECDH. Every ECDH result, ML-KEM
// secret, nJ, nH and the Noise state are wiped as soon as they're used. Nothing here writes anything to the log.

#ifndef NET_HANDSHAKE_B_H
#define NET_HANDSHAKE_B_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "net_crypto.h"
#include "net_identity.h"
#include "net_noise.h"
#include "net_pq.h"

// The sizes, in bytes (CRYPTO.md section 8's table)
size_t const kNetHandshakeBNonceBytes = 32;                                          // nJ and nH
size_t const kNetHandshakeBCommitmentBytes = kNetCryptoHashBytes;                    // SHA-384(nJ)
size_t const kNetHandshakeBSealedStaticsBytes = kNetIdentityStaticBytes + kNetCryptoTagBytes;   // `s`: 1,681
size_t const kNetHandshakeBB1Bytes = kNetCryptoP384PublicBytes + kNetPqKemPublicBytes + kNetHandshakeBCommitmentBytes;   // 1,713
size_t const kNetHandshakeBB2Bytes = kNetCryptoP384PublicBytes + kNetPqKemCiphertextBytes + kNetHandshakeBSealedStaticsBytes
	+ kNetCryptoTagBytes;                                                            // 3,362
size_t const kNetHandshakeBB3Bytes = kNetPqKemCiphertextBytes + kNetHandshakeBSealedStaticsBytes + kNetCryptoTagBytes;      // 3,265
size_t const kNetHandshakeBB4Bytes = kNetPqKemCiphertextBytes + kNetHandshakeBNonceBytes + kNetCryptoTagBytes;              // 1,616
size_t const kNetHandshakeBB5Bytes = kNetHandshakeBNonceBytes + kNetCryptoTagBytes;                                         // 48
size_t const kNetHandshakeBMostRegionBytes = 16;     // a region is 1 to 16 small letters, digits and -, as the list says (relay_list_text)
size_t const kNetHandshakeBCheckCodeBytes = 7;       // 6 digits, then the 0 that ends the text

static const char kNetHandshakeBCheckCodeInfo[] = "AVPX check code v2";

// Why a handshake B ended. The numbers up to 21 are CRYPTO.md section 16's. Section 16 has no number for the others, so they're 100 and up.
int const kNetHandshakeBFine = 0;
int const kNetHandshakeBWrongSize = 1;        // a message of the wrong size
int const kNetHandshakeBBadTag = 8;           // a tag that fails: changed on the way, or made with other keys
int const kNetHandshakeBWrongHost = 10;       // the host's keys aren't the ones the listing's fingerprint names ("this isn't the host the
                                              // list named")
int const kNetHandshakeBBrokenPromise = 21;   // B5's nJ isn't what B1's commitment promised
int const kNetHandshakeBNotAKey = 100;        // a P-384 key that isn't on the curve, or that agrees to nothing (or CNG refused the ECDH)
int const kNetHandshakeBOutOfTurn = 101;      // a call that isn't the next one in B's order
int const kNetHandshakeBBadStart = 102;       // no identity, a region that isn't 1 to 16 small letters, digits and -, or a port of 0
int const kNetHandshakeBCouldnt = 103;        // the cryptography itself failed (a hash, HKDF, a seal or a decapsulation)
int const kNetHandshakeBGivenUp = 104;        // the caller ended it (Abandon), such as when its encapsulation was refused

// What both sides have when B is done
struct NetHandshakeBKeys
{
	NetNoiseSplit split;                                       // joiner-to-host is Split's "initiator to responder"; host-to-joiner
	                                                           // is "responder to initiator"
	uint8_t transcript[kNetCryptoHashBytes];                   // h after B5 (5.1 point 15): what the check code is made from
	uint8_t theirFingerprint[kNetIdentityFingerprintBytes];    // the other side's: the host's for the joiner, the joiner's for the host
};

inline void NetHandshakeB_WipeKeys(NetHandshakeBKeys *keys)
{
	NetCrypto_Wipe(keys, sizeof(*keys));
}

// Two runs of bytes the same? It looks at every byte whatever it finds, so how long it takes says nothing about where they differ.
inline bool NetHandshakeB_Same(const uint8_t *a, const uint8_t *b, size_t size)
{
	uint8_t differ = 0;

	for (size_t i = 0; i < size; i++)
	{
		differ = static_cast<uint8_t>(differ | (a[i] ^ b[i]));
	}

	return differ == 0;
}

// How long a region's name is, if it's one: 1 to 16 small letters, digits and -. 0 if it isn't one, or is NULL.
inline size_t NetHandshakeB_RegionLength(const char *region)
{
	if (!region)
	{
		return 0;
	}

	size_t n = 0;

	while (region[n] != 0)
	{
		char const c = region[n];
		bool const allowed = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';

		if (!allowed || n == kNetHandshakeBMostRegionBytes)
		{
			return 0;
		}

		n++;
	}

	return n;
}

// The start, the same on both sides: h = SHA-384 of the protocol's name, ck = h, then the prologue's three MixHashes (5.1 point 17)
inline bool NetHandshakeB_Start(NetNoiseState *noise, const char *region, uint16_t relayPort, const uint8_t hostFingerprint[kNetIdentityFingerprintBytes])
{
	size_t const regionBytes = NetHandshakeB_RegionLength(region);
	uint8_t named[kNetHandshakeBMostRegionBytes + 1];
	memcpy(named, region, regionBytes);
	named[regionBytes] = 0;
	uint8_t const port[2] = { static_cast<uint8_t>(relayPort & 0xFF), static_cast<uint8_t>(relayPort >> 8) };

	return NetNoise_Start(noise, kNetNoiseNamePeers)
		&& NetNoise_MixHash(noise, named, regionBytes + 1)
		&& NetNoise_MixHash(noise, port, sizeof(port))
		&& NetNoise_MixHash(noise, hostFingerprint, kNetIdentityFingerprintBytes);
}

// One ECDH, mixed into the chain at once, and wiped: ee, es and se
inline int NetHandshakeB_MixEcdh(NetNoiseState *noise, NetEcdhKey *mine, const uint8_t theirs[kNetCryptoP384PublicBytes])
{
	uint8_t secret[kNetCryptoP384ScalarBytes];

	if (!NetCrypto_EcdhAgree(mine, theirs, secret))
	{
		return kNetHandshakeBNotAKey;
	}

	bool const mixed = NetNoise_MixKey(noise, secret, sizeof(secret));
	NetCrypto_Wipe(secret, sizeof(secret));
	return mixed ? kNetHandshakeBFine : kNetHandshakeBCouldnt;
}

// One ML-KEM token, as handshake A does it: the ciphertext into h as it goes, then its secret into the chain (ekem and skem)
inline bool NetHandshakeB_MixKem(NetNoiseState *noise, const uint8_t ciphertext[kNetPqKemCiphertextBytes], const uint8_t secret[kNetPqKemSecretBytes])
{
	return NetNoise_MixHash(noise, ciphertext, kNetPqKemCiphertextBytes) && NetNoise_MixKey(noise, secret, kNetPqKemSecretBytes);
}

// The same, for the side that decapsulates: the secret comes from its private key, and is wiped once mixed
inline bool NetHandshakeB_MixDecapsulated(NetNoiseState *noise, const uint8_t privateKey[kNetPqKemPrivateBytes], const uint8_t ciphertext[kNetPqKemCiphertextBytes])
{
	uint8_t secret[kNetPqKemSecretBytes];
	bool const mixed = NetNoise_MixHash(noise, ciphertext, kNetPqKemCiphertextBytes)
		&& NetPq_KemDecapsulate(privateKey, ciphertext, secret)
		&& NetNoise_MixKey(noise, secret, sizeof(secret));
	NetCrypto_Wipe(secret, sizeof(secret));
	return mixed;
}

// A tag on nothing (EncryptAndHash of no words), into tagOut: B2's, B3's tags
inline bool NetHandshakeB_SealNothing(NetNoiseState *noise, uint8_t tagOut[kNetCryptoTagBytes])
{
	static const uint8_t kNothing[1] = { 0 };
	size_t written = 0;
	return NetNoise_EncryptAndHash(noise, kNothing, 0, tagOut, kNetCryptoTagBytes, &written) && written == kNetCryptoTagBytes;
}

// And checking one
inline bool NetHandshakeB_OpenNothing(NetNoiseState *noise, const uint8_t tag[kNetCryptoTagBytes])
{
	uint8_t nothing[1];
	size_t written = 1;
	return NetNoise_DecryptAndHash(noise, tag, kNetCryptoTagBytes, nothing, 0, &written) && written == 0;
}

// The end, the same on both sides: the keys, the transcript and the other side's fingerprint
inline bool NetHandshakeB_Finish(const NetNoiseState *noise, const uint8_t theirFingerprint[kNetIdentityFingerprintBytes], NetHandshakeBKeys *keysOut)
{
	if (!NetNoise_Split(noise, &keysOut->split))
	{
		NetHandshakeB_WipeKeys(keysOut);
		return false;
	}

	memcpy(keysOut->transcript, noise->h, sizeof(keysOut->transcript));
	memcpy(keysOut->theirFingerprint, theirFingerprint, sizeof(keysOut->theirFingerprint));
	return true;
}

// The 6 digits both screens show (5.1 point 19): HKDF-SHA-384 with the transcript as its salt, no input and the info "AVPX check code
// v2", 8 bytes, read as a number little-endian (the first byte is the smallest part), the remainder after dividing by 1,000,000, written
// as 6 digits with 0s in front. False, with out empty, only if HKDF failed.
inline bool NetHandshakeB_CheckCode(const uint8_t transcript[kNetCryptoHashBytes], char out[kNetHandshakeBCheckCodeBytes])
{
	uint8_t bytes[8];

	if (!NetCrypto_HkdfSha384(transcript, kNetCryptoHashBytes, NULL, 0, reinterpret_cast<const uint8_t *>(kNetHandshakeBCheckCodeInfo),
		sizeof(kNetHandshakeBCheckCodeInfo) - 1, bytes, sizeof(bytes)))
	{
		out[0] = 0;
		return false;
	}

	uint64_t number = 0;

	for (size_t i = 8; i > 0; i--)
	{
		number = (number << 8) | bytes[i - 1];
	}

	uint64_t code = number % 1000000;

	for (size_t i = 6; i > 0; i--)
	{
		out[i - 1] = static_cast<char>('0' + code % 10);
		code /= 10;
	}

	out[6] = 0;
	NetCrypto_Wipe(bytes, sizeof(bytes));
	return true;
}

// ----- The joiner: Noise's initiator. Its order: WriteB1, ReadB2, WriteB3, ReadB4.
//
// Its draws, in 5.1 point 14's order: its fresh P-384 key and its fresh ML-KEM key pair, nJ (all three for WriteB1), then, once ReadB2
// says who the host is, an encapsulation to HostKemPublic() (for WriteB3).
class NetHandshakeBJoiner
{
public:
	NetHandshakeBJoiner() : me(NULL), e(NULL), step(kNothing), why(kNetHandshakeBFine)
	{
		Clear();
	}

	~NetHandshakeBJoiner()
	{
		Clear();
	}

	NetHandshakeBJoiner(const NetHandshakeBJoiner &) = delete;   // it holds a CNG key, so it's never copied
	NetHandshakeBJoiner &operator=(const NetHandshakeBJoiner &) = delete;

	// B1, to the host the listing named (by its fingerprint), on this joiner's port of that region's relay. me must last until the
	// handshake is over. It always takes ephemeral (and destroys it when done), even when it refuses. ephemeralKemPrivate is copied in;
	// the caller wipes its own copy, and nJ's.
	bool WriteB1(const char *region, uint16_t relayPort, const uint8_t hostFingerprint[kNetIdentityFingerprintBytes], const NetIdentity *identity,
		NetEcdhKey *ephemeral, const uint8_t ephemeralPublic[kNetCryptoP384PublicBytes], const uint8_t ephemeralKemPublic[kNetPqKemPublicBytes],
		const uint8_t ephemeralKemPrivate[kNetPqKemPrivateBytes], const uint8_t nJ[kNetHandshakeBNonceBytes], uint8_t b1Out[kNetHandshakeBB1Bytes])
	{
		if (step != kNothing)
		{
			NetCrypto_EcdhFree(ephemeral);
			return Fail(kNetHandshakeBOutOfTurn, b1Out, kNetHandshakeBB1Bytes);
		}

		e = ephemeral;

		if (!identity || !identity->made || !e || relayPort == 0 || NetHandshakeB_RegionLength(region) == 0)
		{
			return Fail(kNetHandshakeBBadStart, b1Out, kNetHandshakeBB1Bytes);
		}

		me = identity;
		memcpy(expectedHost, hostFingerprint, sizeof(expectedHost));
		memcpy(ekPrivate, ephemeralKemPrivate, sizeof(ekPrivate));
		memcpy(promised, nJ, sizeof(promised));

		uint8_t *const commitment = b1Out + kNetCryptoP384PublicBytes + kNetPqKemPublicBytes;
		NetCryptoPiece const piece = { nJ, kNetHandshakeBNonceBytes };
		memcpy(b1Out, ephemeralPublic, kNetCryptoP384PublicBytes);
		memcpy(b1Out + kNetCryptoP384PublicBytes, ephemeralKemPublic, kNetPqKemPublicBytes);

		bool const written = NetHandshakeB_Start(&noise, region, relayPort, expectedHost)
			&& NetCrypto_Sha384(&piece, 1, commitment)
			&& NetNoise_MixHash(&noise, ephemeralPublic, kNetCryptoP384PublicBytes)            // e
			&& NetNoise_MixHash(&noise, ephemeralKemPublic, kNetPqKemPublicBytes)
			&& NetNoise_MixHash(&noise, commitment, kNetHandshakeBCommitmentBytes);           // the promise

		if (!written)
		{
			return Fail(kNetHandshakeBCouldnt, b1Out, kNetHandshakeBB1Bytes);
		}

		step = kWroteB1;
		return true;
	}

	// B2 read: the host is proved, and must be the one the listing named. Then HostKemPublic() is the key for WriteB3's encapsulation.
	bool ReadB2(const uint8_t *b2, size_t size)
	{
		if (step != kWroteB1)
		{
			return Fail(kNetHandshakeBOutOfTurn, NULL, 0);
		}

		if (!b2 || size != kNetHandshakeBB2Bytes)
		{
			return Fail(kNetHandshakeBWrongSize, NULL, 0);
		}

		const uint8_t *const theirE = b2;
		const uint8_t *const ciphertext = theirE + kNetCryptoP384PublicBytes;
		const uint8_t *const sealedStatics = ciphertext + kNetPqKemCiphertextBytes;
		const uint8_t *const tag = sealedStatics + kNetHandshakeBSealedStaticsBytes;

		// e, ee: the host's fresh key, and the two fresh keys' secret
		int mixed = NetNoise_MixHash(&noise, theirE, kNetCryptoP384PublicBytes) ? NetHandshakeB_MixEcdh(&noise, e, theirE) : kNetHandshakeBCouldnt;

		if (mixed != kNetHandshakeBFine)
		{
			return Fail(mixed, NULL, 0);
		}

		// ekem: the secret the host encapsulated to the joiner's fresh ML-KEM key. That key has done its work, so it's wiped now.
		bool const decapsulated = NetHandshakeB_MixDecapsulated(&noise, ekPrivate, ciphertext);
		NetCrypto_Wipe(ekPrivate, sizeof(ekPrivate));

		if (!decapsulated)
		{
			return Fail(kNetHandshakeBCouldnt, NULL, 0);
		}

		// s: the host's own keys, which must be the ones whose fingerprint the listing gave (refusal 10)
		uint8_t statics[kNetIdentityStaticBytes];
		size_t written = 0;

		if (!NetNoise_DecryptAndHash(&noise, sealedStatics, kNetHandshakeBSealedStaticsBytes, statics, sizeof(statics), &written)
			|| written != sizeof(statics))
		{
			return Fail(kNetHandshakeBBadTag, NULL, 0);
		}

		uint8_t theirFingerprint[kNetIdentityFingerprintBytes];

		if (!NetIdentity_Fingerprint(statics, statics + kNetCryptoP384PublicBytes, theirFingerprint))
		{
			return Fail(kNetHandshakeBCouldnt, NULL, 0);
		}

		if (!NetHandshakeB_Same(theirFingerprint, expectedHost, sizeof(expectedHost)))
		{
			return Fail(kNetHandshakeBWrongHost, NULL, 0);
		}

		// es: the joiner's fresh key with the host's own key. Then the fresh key has done everything it's for, and is destroyed.
		mixed = NetHandshakeB_MixEcdh(&noise, e, statics);
		NetCrypto_EcdhFree(e);
		e = NULL;

		if (mixed != kNetHandshakeBFine)
		{
			return Fail(mixed, NULL, 0);
		}

		// B2's tag
		if (!NetHandshakeB_OpenNothing(&noise, tag))
		{
			return Fail(kNetHandshakeBBadTag, NULL, 0);
		}

		memcpy(hostEphemeral, theirE, sizeof(hostEphemeral));
		memcpy(hostKem, statics + kNetCryptoP384PublicBytes, sizeof(hostKem));
		step = kReadB2;
		return true;
	}

	// The host's own ML-KEM key, once ReadB2 has taken B2: what WriteB3's encapsulation is to. NULL before.
	const uint8_t *HostKemPublic() const
	{
		return step == kReadB2 ? hostKem : NULL;
	}

	// B3, with the caller's encapsulation to HostKemPublic(): its ciphertext and its secret (the caller wipes its own copy)
	bool WriteB3(const uint8_t ciphertext[kNetPqKemCiphertextBytes], const uint8_t secret[kNetPqKemSecretBytes], uint8_t b3Out[kNetHandshakeBB3Bytes])
	{
		if (step != kReadB2)
		{
			return Fail(kNetHandshakeBOutOfTurn, b3Out, kNetHandshakeBB3Bytes);
		}

		uint8_t *const sealedStatics = b3Out + kNetPqKemCiphertextBytes;
		uint8_t *const tag = sealedStatics + kNetHandshakeBSealedStaticsBytes;
		size_t written = 0;
		memcpy(b3Out, ciphertext, kNetPqKemCiphertextBytes);

		// skem, s
		bool const sealed = NetHandshakeB_MixKem(&noise, ciphertext, secret)
			&& NetNoise_EncryptAndHash(&noise, me->statics, kNetIdentityStaticBytes, sealedStatics, kNetHandshakeBSealedStaticsBytes, &written)
			&& written == kNetHandshakeBSealedStaticsBytes;

		if (!sealed)
		{
			return Fail(kNetHandshakeBCouldnt, b3Out, kNetHandshakeBB3Bytes);
		}

		// se: the joiner's own key with the host's fresh key; then B3's tag
		int const mixed = NetHandshakeB_MixEcdh(&noise, me->p384, hostEphemeral);

		if (mixed != kNetHandshakeBFine)
		{
			return Fail(mixed, b3Out, kNetHandshakeBB3Bytes);
		}

		if (!NetHandshakeB_SealNothing(&noise, tag))
		{
			return Fail(kNetHandshakeBCouldnt, b3Out, kNetHandshakeBB3Bytes);
		}

		step = kWroteB3;
		return true;
	}

	// B4 read (the host's key confirmation, and nH), then B5 made, revealing nJ, and the keys. Either way, the handshake is over.
	bool ReadB4(const uint8_t *b4, size_t size, uint8_t b5Out[kNetHandshakeBB5Bytes], NetHandshakeBKeys *keysOut)
	{
		NetHandshakeB_WipeKeys(keysOut);

		if (step != kWroteB3)
		{
			return Fail(kNetHandshakeBOutOfTurn, b5Out, kNetHandshakeBB5Bytes);
		}

		if (!b4 || size != kNetHandshakeBB4Bytes)
		{
			return Fail(kNetHandshakeBWrongSize, b5Out, kNetHandshakeBB5Bytes);
		}

		// skem: the secret the host encapsulated to the joiner's own ML-KEM key
		if (!NetHandshakeB_MixDecapsulated(&noise, me->kemPrivate, b4))
		{
			return Fail(kNetHandshakeBCouldnt, b5Out, kNetHandshakeBB5Bytes);
		}

		// nH, the host's share of the check code. Its tag is the host's key confirmation. Its bytes are in h now, which is all it's for.
		uint8_t nH[kNetHandshakeBNonceBytes];
		size_t written = 0;
		bool const opened = NetNoise_DecryptAndHash(&noise, b4 + kNetPqKemCiphertextBytes, kNetHandshakeBNonceBytes + kNetCryptoTagBytes, nH,
			sizeof(nH), &written) && written == sizeof(nH);
		NetCrypto_Wipe(nH, sizeof(nH));

		if (!opened)
		{
			return Fail(kNetHandshakeBBadTag, b5Out, kNetHandshakeBB5Bytes);
		}

		// B5: nJ, as B1 promised
		bool const done = NetNoise_EncryptAndHash(&noise, promised, sizeof(promised), b5Out, kNetHandshakeBB5Bytes, &written)
			&& written == kNetHandshakeBB5Bytes
			&& NetHandshakeB_Finish(&noise, expectedHost, keysOut);

		if (!done)
		{
			NetHandshakeB_WipeKeys(keysOut);
			return Fail(kNetHandshakeBCouldnt, b5Out, kNetHandshakeBB5Bytes);
		}

		Clear();
		step = kDone;
		return true;
	}

	// The caller ends it: its own draw was refused (such as an encapsulation to a key FIPS 203 refuses), or it gave up
	void Abandon(int reason)
	{
		Fail(reason == kNetHandshakeBFine ? kNetHandshakeBGivenUp : reason, NULL, 0);
	}

	int Why() const
	{
		return why;
	}

	bool Done() const
	{
		return step == kDone;
	}

private:
	static int const kNothing = 0;
	static int const kWroteB1 = 1;
	static int const kReadB2 = 2;
	static int const kWroteB3 = 3;
	static int const kDone = 4;
	static int const kFailed = 5;

	// Every secret and every key gone; the state as it was before WriteB1, but for why
	void Clear()
	{
		NetCrypto_EcdhFree(e);
		e = NULL;
		me = NULL;
		NetNoise_Wipe(&noise);
		NetCrypto_Wipe(ekPrivate, sizeof(ekPrivate));
		NetCrypto_Wipe(promised, sizeof(promised));
		NetCrypto_Wipe(expectedHost, sizeof(expectedHost));
		NetCrypto_Wipe(hostEphemeral, sizeof(hostEphemeral));
		NetCrypto_Wipe(hostKem, sizeof(hostKem));
	}

	bool Fail(int reason, uint8_t *out, size_t outSize)
	{
		if (out)
		{
			NetCrypto_Wipe(out, outSize);
		}

		Clear();
		why = reason;
		step = kFailed;
		return false;
	}

	NetNoiseState noise;
	const NetIdentity *me;
	NetEcdhKey *e;                                          // the joiner's fresh P-384 key, until es
	uint8_t ekPrivate[kNetPqKemPrivateBytes];               // the joiner's fresh ML-KEM private key, until ekem
	uint8_t promised[kNetHandshakeBNonceBytes];             // nJ, until B5
	uint8_t expectedHost[kNetIdentityFingerprintBytes];     // the listing's fingerprint
	uint8_t hostEphemeral[kNetCryptoP384PublicBytes];       // the host's fresh P-384 key, for se
	uint8_t hostKem[kNetPqKemPublicBytes];                  // the host's own ML-KEM key, for B3's encapsulation
	int step;
	int why;
};

// ----- The host: Noise's responder. Its order: ReadB1, WriteB2, ReadB3, WriteB4, ReadB5.
//
// Its draws, in 5.1 point 14's order: its fresh P-384 key and an encapsulation to JoinerEphemeralKemPublic() (both for WriteB2), then an
// encapsulation to JoinerKemPublic() and nH (both for WriteB4).
class NetHandshakeBHost
{
public:
	NetHandshakeBHost() : me(NULL), re(NULL), step(kNothing), why(kNetHandshakeBFine)
	{
		Clear();
	}

	~NetHandshakeBHost()
	{
		Clear();
	}

	NetHandshakeBHost(const NetHandshakeBHost &) = delete;
	NetHandshakeBHost &operator=(const NetHandshakeBHost &) = delete;

	// B1 read, on this joiner's port of the host's relay. me must last until the handshake is over. Then JoinerEphemeralKemPublic() is the
	// key for WriteB2's encapsulation.
	bool ReadB1(const char *region, uint16_t relayPort, const NetIdentity *identity, const uint8_t *b1, size_t size)
	{
		if (step != kNothing)
		{
			return Fail(kNetHandshakeBOutOfTurn, NULL, 0);
		}

		if (!identity || !identity->made || relayPort == 0 || NetHandshakeB_RegionLength(region) == 0)
		{
			return Fail(kNetHandshakeBBadStart, NULL, 0);
		}

		if (!b1 || size != kNetHandshakeBB1Bytes)
		{
			return Fail(kNetHandshakeBWrongSize, NULL, 0);
		}

		me = identity;
		memcpy(joinerEphemeral, b1, sizeof(joinerEphemeral));
		memcpy(joinerEphemeralKem, b1 + kNetCryptoP384PublicBytes, sizeof(joinerEphemeralKem));
		memcpy(commitment, b1 + kNetCryptoP384PublicBytes + kNetPqKemPublicBytes, sizeof(commitment));

		// The prologue names this host's own fingerprint: a joiner that expected another host has a different h from here on
		bool const read = NetHandshakeB_Start(&noise, region, relayPort, me->fingerprint)
			&& NetNoise_MixHash(&noise, joinerEphemeral, sizeof(joinerEphemeral))       // e
			&& NetNoise_MixHash(&noise, joinerEphemeralKem, sizeof(joinerEphemeralKem))
			&& NetNoise_MixHash(&noise, commitment, sizeof(commitment));                // the promise

		if (!read)
		{
			return Fail(kNetHandshakeBCouldnt, NULL, 0);
		}

		step = kReadB1;
		return true;
	}

	// The joiner's fresh ML-KEM key, once ReadB1 has taken B1: what WriteB2's encapsulation is to. NULL before.
	const uint8_t *JoinerEphemeralKemPublic() const
	{
		return step == kReadB1 ? joinerEphemeralKem : NULL;
	}

	// B2, with the host's fresh P-384 key (always taken, and destroyed when done) and the caller's encapsulation to
	// JoinerEphemeralKemPublic() (the caller wipes its own secret)
	bool WriteB2(NetEcdhKey *ephemeral, const uint8_t ephemeralPublic[kNetCryptoP384PublicBytes], const uint8_t ciphertext[kNetPqKemCiphertextBytes],
		const uint8_t secret[kNetPqKemSecretBytes], uint8_t b2Out[kNetHandshakeBB2Bytes])
	{
		if (step != kReadB1)
		{
			NetCrypto_EcdhFree(ephemeral);
			return Fail(kNetHandshakeBOutOfTurn, b2Out, kNetHandshakeBB2Bytes);
		}

		re = ephemeral;

		if (!re)
		{
			return Fail(kNetHandshakeBBadStart, b2Out, kNetHandshakeBB2Bytes);
		}

		uint8_t *const sealedStatics = b2Out + kNetCryptoP384PublicBytes + kNetPqKemCiphertextBytes;
		uint8_t *const tag = sealedStatics + kNetHandshakeBSealedStaticsBytes;
		memcpy(b2Out, ephemeralPublic, kNetCryptoP384PublicBytes);
		memcpy(b2Out + kNetCryptoP384PublicBytes, ciphertext, kNetPqKemCiphertextBytes);

		// e, ee
		int mixed = NetNoise_MixHash(&noise, ephemeralPublic, kNetCryptoP384PublicBytes) ? NetHandshakeB_MixEcdh(&noise, re, joinerEphemeral)
			: kNetHandshakeBCouldnt;

		if (mixed != kNetHandshakeBFine)
		{
			return Fail(mixed, b2Out, kNetHandshakeBB2Bytes);
		}

		// ekem, s
		size_t written = 0;
		bool const sealed = NetHandshakeB_MixKem(&noise, ciphertext, secret)
			&& NetNoise_EncryptAndHash(&noise, me->statics, kNetIdentityStaticBytes, sealedStatics, kNetHandshakeBSealedStaticsBytes, &written)
			&& written == kNetHandshakeBSealedStaticsBytes;

		if (!sealed)
		{
			return Fail(kNetHandshakeBCouldnt, b2Out, kNetHandshakeBB2Bytes);
		}

		// es: the host's own key with the joiner's fresh key; then B2's tag
		mixed = NetHandshakeB_MixEcdh(&noise, me->p384, joinerEphemeral);

		if (mixed != kNetHandshakeBFine)
		{
			return Fail(mixed, b2Out, kNetHandshakeBB2Bytes);
		}

		if (!NetHandshakeB_SealNothing(&noise, tag))
		{
			return Fail(kNetHandshakeBCouldnt, b2Out, kNetHandshakeBB2Bytes);
		}

		step = kWroteB2;
		return true;
	}

	// B3 read: the joiner's own keys, proved classically (se, and B3's tag). Then JoinerKemPublic() is the key for WriteB4's encapsulation,
	// which proves them post-quantum.
	bool ReadB3(const uint8_t *b3, size_t size)
	{
		if (step != kWroteB2)
		{
			return Fail(kNetHandshakeBOutOfTurn, NULL, 0);
		}

		if (!b3 || size != kNetHandshakeBB3Bytes)
		{
			return Fail(kNetHandshakeBWrongSize, NULL, 0);
		}

		const uint8_t *const sealedStatics = b3 + kNetPqKemCiphertextBytes;
		const uint8_t *const tag = sealedStatics + kNetHandshakeBSealedStaticsBytes;

		// skem: the secret the joiner encapsulated to the host's own ML-KEM key
		if (!NetHandshakeB_MixDecapsulated(&noise, me->kemPrivate, b3))
		{
			return Fail(kNetHandshakeBCouldnt, NULL, 0);
		}

		// s: the joiner's own keys
		uint8_t statics[kNetIdentityStaticBytes];
		size_t written = 0;

		if (!NetNoise_DecryptAndHash(&noise, sealedStatics, kNetHandshakeBSealedStaticsBytes, statics, sizeof(statics), &written)
			|| written != sizeof(statics))
		{
			return Fail(kNetHandshakeBBadTag, NULL, 0);
		}

		// se: the host's fresh key with the joiner's own key. Then the fresh key has done everything it's for, and is destroyed.
		int const mixed = NetHandshakeB_MixEcdh(&noise, re, statics);
		NetCrypto_EcdhFree(re);
		re = NULL;

		if (mixed != kNetHandshakeBFine)
		{
			return Fail(mixed, NULL, 0);
		}

		// B3's tag
		if (!NetHandshakeB_OpenNothing(&noise, tag))
		{
			return Fail(kNetHandshakeBBadTag, NULL, 0);
		}

		if (!NetIdentity_Fingerprint(statics, statics + kNetCryptoP384PublicBytes, joinerFingerprint))
		{
			return Fail(kNetHandshakeBCouldnt, NULL, 0);
		}

		memcpy(joinerKem, statics + kNetCryptoP384PublicBytes, sizeof(joinerKem));
		step = kReadB3;
		return true;
	}

	// The joiner's own ML-KEM key, once ReadB3 has taken B3: what WriteB4's encapsulation is to. NULL before.
	const uint8_t *JoinerKemPublic() const
	{
		return step == kReadB3 ? joinerKem : NULL;
	}

	// B4, with the caller's encapsulation to JoinerKemPublic() and nH (the caller wipes its own copies of both)
	bool WriteB4(const uint8_t ciphertext[kNetPqKemCiphertextBytes], const uint8_t secret[kNetPqKemSecretBytes], const uint8_t nH[kNetHandshakeBNonceBytes],
		uint8_t b4Out[kNetHandshakeBB4Bytes])
	{
		if (step != kReadB3)
		{
			return Fail(kNetHandshakeBOutOfTurn, b4Out, kNetHandshakeBB4Bytes);
		}

		size_t written = 0;
		memcpy(b4Out, ciphertext, kNetPqKemCiphertextBytes);

		// skem, then nH sealed: its tag is the host's key confirmation
		bool const sealed = NetHandshakeB_MixKem(&noise, ciphertext, secret)
			&& NetNoise_EncryptAndHash(&noise, nH, kNetHandshakeBNonceBytes, b4Out + kNetPqKemCiphertextBytes, kNetHandshakeBNonceBytes + kNetCryptoTagBytes,
				&written)
			&& written == kNetHandshakeBNonceBytes + kNetCryptoTagBytes;

		if (!sealed)
		{
			return Fail(kNetHandshakeBCouldnt, b4Out, kNetHandshakeBB4Bytes);
		}

		step = kWroteB4;
		return true;
	}

	// B5 read: the joiner's key confirmation, and nJ, which must be what B1 promised (refusal 21). Then the keys. Either way, it's over.
	bool ReadB5(const uint8_t *b5, size_t size, NetHandshakeBKeys *keysOut)
	{
		NetHandshakeB_WipeKeys(keysOut);

		if (step != kWroteB4)
		{
			return Fail(kNetHandshakeBOutOfTurn, NULL, 0);
		}

		if (!b5 || size != kNetHandshakeBB5Bytes)
		{
			return Fail(kNetHandshakeBWrongSize, NULL, 0);
		}

		uint8_t nJ[kNetHandshakeBNonceBytes];
		size_t written = 0;

		if (!NetNoise_DecryptAndHash(&noise, b5, size, nJ, sizeof(nJ), &written) || written != sizeof(nJ))
		{
			return Fail(kNetHandshakeBBadTag, NULL, 0);
		}

		uint8_t promise[kNetHandshakeBCommitmentBytes];
		NetCryptoPiece const piece = { nJ, sizeof(nJ) };
		bool const hashed = NetCrypto_Sha384(&piece, 1, promise);
		NetCrypto_Wipe(nJ, sizeof(nJ));

		if (!hashed)
		{
			return Fail(kNetHandshakeBCouldnt, NULL, 0);
		}

		if (!NetHandshakeB_Same(promise, commitment, sizeof(commitment)))
		{
			return Fail(kNetHandshakeBBrokenPromise, NULL, 0);
		}

		if (!NetHandshakeB_Finish(&noise, joinerFingerprint, keysOut))
		{
			return Fail(kNetHandshakeBCouldnt, NULL, 0);
		}

		Clear();
		step = kDone;
		return true;
	}

	// The caller ends it: its own draw was refused (such as an encapsulation to a key FIPS 203 refuses), or it gave up
	void Abandon(int reason)
	{
		Fail(reason == kNetHandshakeBFine ? kNetHandshakeBGivenUp : reason, NULL, 0);
	}

	int Why() const
	{
		return why;
	}

	bool Done() const
	{
		return step == kDone;
	}

private:
	static int const kNothing = 0;
	static int const kReadB1 = 1;
	static int const kWroteB2 = 2;
	static int const kReadB3 = 3;
	static int const kWroteB4 = 4;
	static int const kDone = 5;
	static int const kFailed = 6;

	void Clear()
	{
		NetCrypto_EcdhFree(re);
		re = NULL;
		me = NULL;
		NetNoise_Wipe(&noise);
		NetCrypto_Wipe(joinerEphemeral, sizeof(joinerEphemeral));
		NetCrypto_Wipe(joinerEphemeralKem, sizeof(joinerEphemeralKem));
		NetCrypto_Wipe(commitment, sizeof(commitment));
		NetCrypto_Wipe(joinerKem, sizeof(joinerKem));
		NetCrypto_Wipe(joinerFingerprint, sizeof(joinerFingerprint));
	}

	bool Fail(int reason, uint8_t *out, size_t outSize)
	{
		if (out)
		{
			NetCrypto_Wipe(out, outSize);
		}

		Clear();
		why = reason;
		step = kFailed;
		return false;
	}

	NetNoiseState noise;
	const NetIdentity *me;
	NetEcdhKey *re;                                            // the host's fresh P-384 key, until se
	uint8_t joinerEphemeral[kNetCryptoP384PublicBytes];        // the joiner's fresh P-384 key, for ee and es
	uint8_t joinerEphemeralKem[kNetPqKemPublicBytes];          // the joiner's fresh ML-KEM key, for B2's encapsulation
	uint8_t commitment[kNetHandshakeBCommitmentBytes];         // B1's promise, SHA-384(nJ), checked in B5
	uint8_t joinerKem[kNetPqKemPublicBytes];                   // the joiner's own ML-KEM key, for B4's encapsulation
	uint8_t joinerFingerprint[kNetIdentityFingerprintBytes];
	int step;
	int why;
};

#endif
