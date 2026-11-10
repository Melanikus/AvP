// Protocol version 2's handshake A, the game's half: the game and a relay agree on keys, the relay is proved to the game, and the game
// proves to the relay that it's an approved player. Written 2026-11-09 by Melanikus for the AvP port; made pqIK the same day (server #24).
//
// WHAT IT IS (CRYPTO.md section 6, in the AVPx-Dedicated-Server repository). The game already knows the relay's two static public keys,
// a P-384 one and an ML-KEM-1024 one, checked against the fingerprint the game was given (net_relay_v2.h says where they come from).
// Then three messages:
//   A1, game to relay, 4,930 bytes: the game's ephemeral P-384 key and ML-KEM key, a secret encapsulated to the relay's static ML-KEM
//       key, the player's two static public keys (their identity) sealed, and a 16-byte tag made after ss, the ECDH of the player's static
//       key and the relay's;
//   A2, relay to game, 3,257 bytes: the relay's ephemeral P-384 key, a secret encapsulated to the game's ephemeral ML-KEM key, one
//       encapsulated to the PLAYER's static ML-KEM key, and the relay's 8-byte session id, sealed;
//   A3, game to relay, 16 bytes: the game's key confirmation.
// Only the holder of both of the relay's static private keys can make an A2 whose seal opens, so an A2 that opens proves the relay. Only
// the holder of the player's static P-384 key makes A1's last tag (ss), and only the holder of the player's static ML-KEM key can open
// A2's second encapsulation, and so make A3: the relay takes the game as that player only at A3. The relay answers only an approved
// player's A1 (the owner approves fingerprints: PROTOCOL.md's access requests). The two ephemeral exchanges make the keys forward-secret.
// Every secret, the classical and the post-quantum, goes into one chain (Noise's ck), so the keys hold unless both kinds are broken.
//
// IN NOISE'S TERMS: the pattern IK (the relay's key known first, the player's sent in A1), made post-quantum the way PQNoise does it, and
// hybrid. The relay's Java (java/src/avpx/v2/HandshakeA.java) is the other half, and tests/vectors/crypto-v2.json fixes every byte of
// a whole handshake, every step's h and ck among them. tests/net_v2a_test.cpp holds this file to it.
//
// NO RANDOMNESS IN HERE. The game draws what a handshake needs (NetHandshakeA_Draw), in the order CRYPTO.md 5.1 point 14 sets: e's P-384
// key, e's ML-KEM key, then the encapsulation. They're handed in, so the test can hand in the file's own values instead, and the game the
// players get has no way at all to be told which values to use. The player's static keys are handed in too (NetHandshakeAPlayer).
//
// SECRETS: e's private keys, the shared secrets and the chain are secret. Every one is wiped the moment it's used, and the whole state when
// the handshake ends either way (NetHandshakeA_Wipe). The player's static keys are only borrowed: the identity that owns them (the game's
// net_identity.h) keeps and wipes them, and this file never frees or copies them. EVERYTHING FROM OUTSIDE IS HOSTILE (AvP Tao 3): A2 is
// read only at its one size, and its P-384 key is checked to be on the curve, and not to give an all-zero secret, before it's used.

#ifndef NET_HANDSHAKE_A_H
#define NET_HANDSHAKE_A_H

#include <stddef.h>
#include <stdint.h>

#include "net_crypto.h"
#include "net_noise.h"
#include "net_pq.h"

// The player's two static public keys, one after the other, as A1's s carries them: the P-384 key (97), then the ML-KEM key (1,568)
size_t const kNetHandshakeAStaticBytes = kNetCryptoP384PublicBytes + kNetPqKemPublicBytes;   // 1,665
// e's two keys, skem's ciphertext, s sealed, and the last tag
size_t const kNetHandshakeA1Bytes = kNetCryptoP384PublicBytes + kNetPqKemPublicBytes + kNetPqKemCiphertextBytes + kNetHandshakeAStaticBytes
	+ kNetCryptoTagBytes + kNetCryptoTagBytes;   // 4,930
size_t const kNetHandshakeSessionIdBytes = 8;
// e (P-384 only), ekem's ciphertext, skem's ciphertext (to the player's static key), and the session id sealed
size_t const kNetHandshakeA2Bytes = kNetCryptoP384PublicBytes + kNetPqKemCiphertextBytes + kNetPqKemCiphertextBytes + kNetHandshakeSessionIdBytes
	+ kNetCryptoTagBytes;   // 3,257
size_t const kNetHandshakeA3Bytes = kNetCryptoTagBytes;   // 16
size_t const kNetHandshakeASteps = 21;                    // the steps whose h and ck the test vectors write down
size_t const kNetHandshakeMostRegionLetters = 16;         // a region's name: 1 to 16 small letters, digits and '-', as the relay's

// Why a handshake was refused: CRYPTO.md section 16's numbers where it has one
int const kNetHandshakeAOk = 0;
int const kNetHandshakeAWrongSize = 1;      // A2 isn't 3,257 bytes
int const kNetHandshakeATag = 8;            // A2's seal doesn't open: it isn't from the holder of the relay's static keys
int const kNetHandshakeABadKey = 100;       // A2's P-384 key isn't on the curve, or gives an all-zero secret (no number of its own)
int const kNetHandshakeABadState = 101;     // a call out of order, or a key or draw that isn't one: this game's mistake, not the relay's
int const kNetHandshakeACrypto = 102;       // the operating system's cryptography failed

// The relay's two static public keys, checked against its fingerprint before they're given here
struct NetRelayStaticKeys
{
	uint8_t p384[kNetCryptoP384PublicBytes];
	uint8_t kem[kNetPqKemPublicBytes];
};

// The player, as handshake A uses them: their identity's static keys (the game's net_identity.h), BORROWED for the handshake's life and
// never freed or copied here. The identity must outlive the handshake.
struct NetHandshakeAPlayer
{
	NetEcdhKey *p384;                                  // the static P-384 private key, inside CNG: ss in A1, se in A2
	const uint8_t *statics;                            // kNetHandshakeAStaticBytes: what A1's s sends, sealed
	const uint8_t *kemPrivate;                         // kNetPqKemPrivateBytes: opens A2's encapsulation to the player's static key
};

// What one handshake draws, in the order it's drawn (CRYPTO.md 5.1 point 14)
struct NetHandshakeADraws
{
	NetEcdhKey *e;                                     // e's P-384 private key, inside CNG; the handshake takes it over
	uint8_t ePublic[kNetCryptoP384PublicBytes];
	uint8_t ekPublic[kNetPqKemPublicBytes];            // e's ML-KEM key pair
	uint8_t ekPrivate[kNetPqKemPrivateBytes];
	uint8_t skemCiphertext[kNetPqKemCiphertextBytes];  // the encapsulation to the relay's static ML-KEM key
	uint8_t skemSecret[kNetPqKemSecretBytes];
};

// What a finished handshake gives the game: a key and an IV each way (the game is Noise's initiator), the relay's session id, and the
// transcript, h after A3 (5.1 point 15)
struct NetHandshakeAKeys
{
	NetNoiseSplit split;
	uint8_t sessionId[kNetHandshakeSessionIdBytes];
	uint8_t transcript[kNetCryptoHashBytes];
};

// h and ck after each step, for the test vectors only: the game passes NULL. ck is secret, so a trace is wiped by its test.
struct NetHandshakeATrace
{
	size_t count;
	uint8_t h[kNetHandshakeASteps][kNetCryptoHashBytes];
	uint8_t ck[kNetHandshakeASteps][kNetCryptoHashBytes];
};

// One handshake, from A1 to A3
struct NetHandshakeA
{
	NetNoiseState noise;
	NetEcdhKey *e;
	uint8_t ekPrivate[kNetPqKemPrivateBytes];
	NetHandshakeAPlayer player;         // borrowed, for A2's se and skem: Wipe only forgets it, never frees it
	uint8_t a1[kNetHandshakeA1Bytes];   // kept, to send again until A2 comes: the same bytes every time
	int step;                           // 0 not started, 1 A1 made, 2 done, 3 refused
};

// The draws for real: a fresh P-384 key, a fresh ML-KEM key pair, and a fresh encapsulation to the relay's static ML-KEM key. False, with
// everything wiped, if any of them can't be made, or the relay's ML-KEM key is one FIPS 203 section 7.2 refuses.
bool NetHandshakeA_Draw(const NetRelayStaticKeys *relay, NetHandshakeADraws *out);

// Wipes the draws, destroying e's key inside CNG if they still hold it
void NetHandshakeA_WipeDraws(NetHandshakeADraws *draws);

// A1, for region (the relay's region's name), the relay's static keys and the player, from the draws: into hs->a1. The draws are taken
// over (e and its ML-KEM private key move into hs) and wiped; the player is borrowed until A2 is read. Gives back kNetHandshakeAOk, or why
// not, with hs wiped.
int NetHandshakeA_WriteA1(NetHandshakeA *hs, const char *region, const NetRelayStaticKeys *relay, const NetHandshakeAPlayer *player,
	NetHandshakeADraws *draws, NetHandshakeATrace *trace);

// A2, from the relay: if it's good, A3 into a3Out and the keys into keysOut. Either way the handshake's state is wiped after. Gives back
// kNetHandshakeAOk, or why not, with a3Out and keysOut wiped.
int NetHandshakeA_ReadA2(NetHandshakeA *hs, const uint8_t *a2, size_t size, uint8_t a3Out[kNetHandshakeA3Bytes], NetHandshakeAKeys *keysOut,
	NetHandshakeATrace *trace);

// Wipes everything the handshake holds, destroying e's key inside CNG
void NetHandshakeA_Wipe(NetHandshakeA *hs);

// Wipes a finished handshake's keys, once its session is over
void NetHandshakeA_WipeKeys(NetHandshakeAKeys *keys);

// Whether a region's name is one the relay's can be: 1 to 16 small letters, digits and '-', with nothing after
bool NetHandshakeA_RegionIsGood(const char *region);

#endif
