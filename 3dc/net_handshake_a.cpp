// Protocol version 2's handshake A, the game's half (net_handshake_a.h says what it is). Written 2026-11-09 by Melanikus for the AvP port.
//
// Each step below is one of CRYPTO.md section 6's, in order, as the relay's Java takes them (java/src/avpx/v2/HandshakeA.java), and as
// tests/vectors/crypto-v2.json writes them down, h and ck after each. A step that fails ends the handshake there: nothing after it runs,
// and everything is wiped.

#include "net_handshake_a.h"

#include <string.h>

// The words of an empty EncryptAndHash: A1's tag and A3 seal nothing. A pointer to no bytes, never NULL, so nothing below has to wonder.
static const uint8_t kNothing[1] = { 0 };

// A handshake's step (a wiped one is 0: not started)
int const kStepA1Made = 1;
int const kStepDone = 2;
int const kStepRefused = 3;

// h and ck as they are now, written into the trace, if there is one (the test vectors' steps)
static void Trace(NetHandshakeATrace *trace, const NetNoiseState *noise)
{
	if (trace && trace->count < kNetHandshakeASteps)
	{
		memcpy(trace->h[trace->count], noise->h, kNetCryptoHashBytes);
		memcpy(trace->ck[trace->count], noise->ck, kNetCryptoHashBytes);
		trace->count++;
	}
}

// MixHash, then the trace
static bool MixHash(NetNoiseState *noise, const uint8_t *data, size_t size, NetHandshakeATrace *trace)
{
	if (!NetNoise_MixHash(noise, data, size))
	{
		return false;
	}

	Trace(trace, noise);
	return true;
}

// MixKey, then the trace
static bool MixKey(NetNoiseState *noise, const uint8_t *secret, size_t size, NetHandshakeATrace *trace)
{
	if (!NetNoise_MixKey(noise, secret, size))
	{
		return false;
	}

	Trace(trace, noise);
	return true;
}

bool NetHandshakeA_RegionIsGood(const char *region)
{
	if (!region)
	{
		return false;
	}

	size_t letters = 0;

	for (; region[letters] != 0; letters++)
	{
		if (letters >= kNetHandshakeMostRegionLetters)
		{
			return false;
		}

		char const c = region[letters];

		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
		{
			return false;
		}
	}

	return letters >= 1;
}

void NetHandshakeA_WipeDraws(NetHandshakeADraws *draws)
{
	if (!draws)
	{
		return;
	}

	NetCrypto_EcdhFree(draws->e);
	NetCrypto_Wipe(draws, sizeof(*draws));
}

bool NetHandshakeA_Draw(const NetRelayStaticKeys *relay, NetHandshakeADraws *out)
{
	if (!out)
	{
		return false;
	}

	NetCrypto_Wipe(out, sizeof(*out));

	if (!relay)
	{
		return false;
	}

	// In 5.1 point 14's order: e's P-384 key, e's ML-KEM key pair, then the encapsulation to the relay's static ML-KEM key
	out->e = NetCrypto_EcdhNew(out->ePublic);

	if (!out->e || !NetPq_KemKeypair(out->ekPublic, out->ekPrivate) || !NetPq_KemEncapsulate(relay->kem, out->skemCiphertext, out->skemSecret))
	{
		NetHandshakeA_WipeDraws(out);
		return false;
	}

	return true;
}

void NetHandshakeA_Wipe(NetHandshakeA *hs)
{
	if (!hs)
	{
		return;
	}

	NetCrypto_EcdhFree(hs->e);
	NetCrypto_Wipe(hs, sizeof(*hs));
}

void NetHandshakeA_WipeKeys(NetHandshakeAKeys *keys)
{
	if (keys)
	{
		NetCrypto_Wipe(keys, sizeof(*keys));
	}
}

int NetHandshakeA_WriteA1(NetHandshakeA *hs, const char *region, const NetRelayStaticKeys *relay, NetHandshakeADraws *draws,
	NetHandshakeATrace *trace)
{
	if (!hs)
	{
		NetHandshakeA_WipeDraws(draws);
		return kNetHandshakeABadState;
	}

	NetHandshakeA_Wipe(hs);

	if (!draws || !draws->e || !relay || !NetHandshakeA_RegionIsGood(region))
	{
		NetHandshakeA_WipeDraws(draws);
		hs->step = kStepRefused;
		return kNetHandshakeABadState;
	}

	// The draws move into the handshake: e's private keys stay here until A2 is read
	hs->e = draws->e;
	draws->e = NULL;
	memcpy(hs->ekPrivate, draws->ekPrivate, sizeof(hs->ekPrivate));

	// The prologue: the region's name, then one 0 byte (5.1 point 4)
	uint8_t prologue[kNetHandshakeMostRegionLetters + 1] = {};
	size_t const letters = strlen(region);
	memcpy(prologue, region, letters);

	uint8_t es[kNetCryptoP384ScalarBytes];
	uint8_t *const at = hs->a1;
	size_t written = 0;
	int why = kNetHandshakeACrypto;
	bool ok = NetNoise_Start(&hs->noise, kNetNoiseNameRelay);

	if (ok)
	{
		Trace(trace, &hs->noise);                                                                    // 1: the start
	}

	ok = ok && MixHash(&hs->noise, prologue, letters + 1, trace);                                    // 2: the prologue
	ok = ok && MixHash(&hs->noise, relay->p384, sizeof(relay->p384), trace);                         // 3: <- s, the relay's P-384 key
	ok = ok && MixHash(&hs->noise, relay->kem, sizeof(relay->kem), trace);                           // 4: <- s, its ML-KEM key
	ok = ok && MixHash(&hs->noise, draws->ePublic, sizeof(draws->ePublic), trace);                   // 5: e, the game's P-384 key
	ok = ok && MixHash(&hs->noise, draws->ekPublic, sizeof(draws->ekPublic), trace);                 // 6: e, the game's ML-KEM key

	// es: ECDH of the game's ephemeral key and the relay's static one. A relay key that isn't on the curve, or gives 0, ends it here.
	if (ok && !NetCrypto_EcdhAgree(hs->e, relay->p384, es))
	{
		ok = false;
		why = kNetHandshakeABadKey;
	}

	ok = ok && MixKey(&hs->noise, es, sizeof(es), trace);                                            // 7: es
	ok = ok && MixHash(&hs->noise, draws->skemCiphertext, sizeof(draws->skemCiphertext), trace);     // 8: skem's ciphertext
	ok = ok && MixKey(&hs->noise, draws->skemSecret, sizeof(draws->skemSecret), trace);              // 9: skem's secret

	// A1: e's two keys, skem's ciphertext, then an empty EncryptAndHash's tag
	if (ok)
	{
		memcpy(at, draws->ePublic, kNetCryptoP384PublicBytes);
		memcpy(at + kNetCryptoP384PublicBytes, draws->ekPublic, kNetPqKemPublicBytes);
		memcpy(at + kNetCryptoP384PublicBytes + kNetPqKemPublicBytes, draws->skemCiphertext, kNetPqKemCiphertextBytes);
		size_t const tagAt = kNetCryptoP384PublicBytes + kNetPqKemPublicBytes + kNetPqKemCiphertextBytes;
		ok = NetNoise_EncryptAndHash(&hs->noise, kNothing, 0, at + tagAt, kNetHandshakeA1Bytes - tagAt, &written) && written == kNetCryptoTagBytes;

		if (ok)
		{
			Trace(trace, &hs->noise);                                                                // 10: A1's tag
		}
	}

	NetCrypto_Wipe(es, sizeof(es));
	NetHandshakeA_WipeDraws(draws);

	if (!ok)
	{
		NetHandshakeA_Wipe(hs);
		hs->step = kStepRefused;
		return why;
	}

	hs->step = kStepA1Made;
	return kNetHandshakeAOk;
}

int NetHandshakeA_ReadA2(NetHandshakeA *hs, const uint8_t *a2, size_t size, uint8_t a3Out[kNetHandshakeA3Bytes], NetHandshakeAKeys *keysOut,
	NetHandshakeATrace *trace)
{
	NetCrypto_Wipe(a3Out, a3Out ? kNetHandshakeA3Bytes : 0);
	NetHandshakeA_WipeKeys(keysOut);

	if (!hs || hs->step != kStepA1Made || !a3Out || !keysOut)
	{
		if (hs)
		{
			NetHandshakeA_Wipe(hs);
			hs->step = kStepRefused;
		}

		return kNetHandshakeABadState;
	}

	if (!a2 || size != kNetHandshakeA2Bytes)
	{
		NetHandshakeA_Wipe(hs);
		hs->step = kStepRefused;
		return kNetHandshakeAWrongSize;
	}

	// A2: the relay's ephemeral P-384 key, the ciphertext to the game's ephemeral ML-KEM key, then the session id sealed with its tag
	const uint8_t *const re = a2;
	const uint8_t *const ciphertext = a2 + kNetCryptoP384PublicBytes;
	const uint8_t *const sealedId = ciphertext + kNetPqKemCiphertextBytes;
	size_t const sealedIdBytes = kNetHandshakeSessionIdBytes + kNetCryptoTagBytes;

	uint8_t ee[kNetCryptoP384ScalarBytes];
	uint8_t ekem[kNetPqKemSecretBytes];
	uint8_t sessionId[kNetHandshakeSessionIdBytes];
	size_t written = 0;
	int why = kNetHandshakeACrypto;
	bool ok = MixHash(&hs->noise, re, kNetCryptoP384PublicBytes, trace);                             // 11: e, the relay's P-384 key

	// ee: ECDH of the two ephemeral keys. A key that isn't on the curve, or gives 0, ends it.
	if (ok && !NetCrypto_EcdhAgree(hs->e, re, ee))
	{
		ok = false;
		why = kNetHandshakeABadKey;
	}

	ok = ok && MixKey(&hs->noise, ee, sizeof(ee), trace);                                            // 12: ee
	ok = ok && MixHash(&hs->noise, ciphertext, kNetPqKemCiphertextBytes, trace);                     // 13: ekem's ciphertext

	// ekem's secret: decapsulation never fails on a changed ciphertext (FIPS 203's implicit rejection: another secret, and the seal next
	// fails); it fails only for a private key that fails FIPS 203's own check, which would be this game's
	ok = ok && NetPq_KemDecapsulate(hs->ekPrivate, ciphertext, ekem);
	ok = ok && MixKey(&hs->noise, ekem, sizeof(ekem), trace);                                        // 14: ekem's secret

	// The session id, sealed: only the holder of both of the relay's static keys can make a seal that opens
	if (ok && (!NetNoise_DecryptAndHash(&hs->noise, sealedId, sealedIdBytes, sessionId, sizeof(sessionId), &written) || written != sizeof(sessionId)))
	{
		ok = false;
		why = kNetHandshakeATag;
	}

	if (ok)
	{
		Trace(trace, &hs->noise);                                                                    // 15: A2's seal opened
	}

	// A3: an empty EncryptAndHash, the game's key confirmation
	ok = ok && NetNoise_EncryptAndHash(&hs->noise, kNothing, 0, a3Out, kNetHandshakeA3Bytes, &written) && written == kNetHandshakeA3Bytes;

	if (ok)
	{
		Trace(trace, &hs->noise);                                                                    // 16: A3; h is now the transcript
	}

	ok = ok && NetNoise_Split(&hs->noise, &keysOut->split);

	if (ok)
	{
		memcpy(keysOut->sessionId, sessionId, sizeof(sessionId));
		memcpy(keysOut->transcript, hs->noise.h, sizeof(keysOut->transcript));
	}

	NetCrypto_Wipe(ee, sizeof(ee));
	NetCrypto_Wipe(ekem, sizeof(ekem));
	NetCrypto_Wipe(sessionId, sizeof(sessionId));
	NetHandshakeA_Wipe(hs);

	if (!ok)
	{
		NetCrypto_Wipe(a3Out, kNetHandshakeA3Bytes);
		NetHandshakeA_WipeKeys(keysOut);
		hs->step = kStepRefused;
		return why;
	}

	hs->step = kStepDone;
	return kNetHandshakeAOk;
}
