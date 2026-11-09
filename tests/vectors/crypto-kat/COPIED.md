# What the game copied from crypto-kat, and why

Written 2026-11-09 by Melanikus for the AvP port.

**What this folder is.** The published known answers for protocol version 2's classical primitives, copied byte for byte from the relay's repository:
- AVPx-Dedicated-Server, branch crypto-kat-java;
- commit `0c4755cf37d11801655c544b3754eae42ed30b74`;
- folder `test-vectors/crypto-kat/`.

That folder's README.md and each PROVENANCE.md, copied with the files, say where each file came from: NIST CAVP and ACVP, and Wycheproof. Wycheproof's licence is LICENSE-wycheproof.txt.

**Why it's here.** `tests/net_crypto_test.cpp` checks the game's own CNG code (3dc/net_crypto_cng.cpp, net_crypto.h) against every case. Each case is a known answer from the people who wrote the standard, so the test can't share a mistake with the code it tests.

**How it stays honest.** SHA256SUMS lists every copied file's SHA-256. The gate (`.github/workflows/tests.yml`, "Check the copied known answers") checks every one, and that nothing else is in the folder but this note and SHA256SUMS. A copy changed to suit the game's code would prove nothing, so never edit a file here.

**What was copied:**
- aes-256-gcm: nist-cavp-encrypt.json, nist-cavp-decrypt.json, wycheproof.json
- ecdh-p384: nist-cavp.json, nist-cavp-pkv.json, wycheproof-ecpoint.json
- ecdsa-p384-sha384: nist-cavp-sigver.json, nist-cavp-siggen.json, wycheproof-p1363.json. The game only checks signatures; siggen's signatures are checked as valid ones. Since 2026-11-09 no test of the game's reads these: the game has no signature check until the signed relays directory (CRYPTO.md section 12), so its NetCrypto_EcdsaVerify and the test over these files were taken out, and come back with it. The files stay, as the relay's own copy, held to SHA256SUMS like the rest (the relay's Java still checks its ECDSA against them).
- hkdf-sha384: nist-acvp-kda.json, wycheproof.json
- hmac-sha384: wycheproof.json

**What wasn't, and why:**
- ecdh-p384/wycheproof-der.json: public keys wrapped in DER (X.509 SubjectPublicKeyInfo). Version 2 only ever gets a raw point (04, X, Y), so the game has no DER reader to test.
- hkdf-sha384/rfc5869.json: RFC 5869's cases are SHA-256 and SHA-1. The game's HKDF is SHA-384 only.
- ml-kem-1024 and ml-dsa-87: the game's post-quantum half is still the stand-in that refuses everything (3dc/net_pq_none.cpp). These come with the library, once it has been read (CRYPTO.md section 3.1).

**Taking a newer set:**
1. Copy the files from the newer commit, byte for byte.
2. Write SHA256SUMS again (`sha256sum` of each file, the path from this folder), and change the commit above.
3. Run net_crypto_test.
