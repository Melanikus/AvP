// The post-quantum half the game is built with until the real one has been read (net_pq.h): it refuses everything. Written 2026-11-09 by
// Melanikus for the AvP port.
//
// Version 2 needs both halves, so with this one nothing of version 2 can ever run: every call is refused, with every output wiped, and
// NetPq_Available() says so. There's deliberately nothing here that could be mistaken for a working key agreement, so a missing library
// can only ever stop a game, never weaken one.

#include "net_pq.h"
#include "net_crypto.h"

bool NetPq_Available()
{
	return false;
}

const char *NetPq_Name()
{
	return "none: this game has no post-quantum half yet, so version 2 can't be used";
}

bool NetPq_KemKeypair(uint8_t publicOut[kNetPqKemPublicBytes], uint8_t privateOut[kNetPqKemPrivateBytes])
{
	NetCrypto_Wipe(publicOut, publicOut ? kNetPqKemPublicBytes : 0);
	NetCrypto_Wipe(privateOut, privateOut ? kNetPqKemPrivateBytes : 0);
	return false;
}

bool NetPq_KemEncapsulate(const uint8_t *, uint8_t ciphertextOut[kNetPqKemCiphertextBytes], uint8_t secretOut[kNetPqKemSecretBytes])
{
	NetCrypto_Wipe(ciphertextOut, ciphertextOut ? kNetPqKemCiphertextBytes : 0);
	NetCrypto_Wipe(secretOut, secretOut ? kNetPqKemSecretBytes : 0);
	return false;
}

bool NetPq_KemDecapsulate(const uint8_t *, const uint8_t *, uint8_t secretOut[kNetPqKemSecretBytes])
{
	NetCrypto_Wipe(secretOut, secretOut ? kNetPqKemSecretBytes : 0);
	return false;
}

bool NetPq_DsaVerify(const uint8_t *, const uint8_t *, size_t, const char *, const uint8_t *)
{
	return false;
}
