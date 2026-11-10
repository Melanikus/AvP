# Published known answers for ML-KEM-1024 and ML-DSA-87

**What these are:** the test cases the people who wrote each standard published, for the two post-quantum primitives the game uses in
network protocol version 2:
- **ML-KEM-1024** (FIPS 203), the key exchange, in `ml-kem-1024/`;
- **ML-DSA-87** (FIPS 204), the signature, in `ml-dsa-87/`.

**Why:** a test written by us and code written by us can share a mistake, and then every test passes on a primitive that's wrong. A
known answer from NIST, or one of Wycheproof's cases built to catch known mistakes, can't share it. `tests/pq_mlkem_test.cpp` and
`tests/pq_mldsa_test.cpp` run every case here through the game's code (`3dc/pq_key_exchange.cpp`, `3dc/pq_signature_check.cpp`) and the
libraries under it (`vcpkg-overlay-ports/mlkem-native`, `vcpkg-overlay-ports/mldsa-native`).

## Where they're from

Each folder's `PROVENANCE.md` names, for every file: the source and its address, the commit or release, the SHA-256 and size of the
file the cases were taken from, its licence, and which cases were taken. In short:

| Folder | NIST (ACVP, release v1.1.0.43) | Wycheproof (commit d17cc89f) |
|---|---|---|
| ml-kem-1024 | keyGen, encapDecap | mlkem_1024_keygen_seed_test, mlkem_1024_encaps_test, mlkem_1024_test, mlkem_1024_semi_expanded_decaps_test |
| ml-dsa-87 | keyGen, sigVer, sigGen | mldsa_87_verify_test |

They were made for the AVPx Dedicated Server, whose server checks the same primitives against the same cases, and copied here byte for
byte on 2026-11-09 from its repository (github.com/Melanikus/AVPx-Dedicated-Server, folder `test-vectors/crypto-kat`, commit
0c4755cf37d11801655c544b3754eae42ed30b74). The tool that made them, which the `PROVENANCE.md` files call `java/tools/MakeCryptoKat.java`,
and the `crypto-kat-sources/` folder it read, are in that repository, not this one. Each file here, as it was copied (with its lines
ending in a single line feed, as git keeps them):

| SHA-256 | Bytes | File |
|---|---|---|
| 58d1e17ffe5109a7ae296caafcadfdbe6a7d176f0bc4ab01e12a689b0499d8bd | 11357 | LICENSE-wycheproof.txt |
| a809b1d51d8e6d6f3fb58b2e622bf1c90f7eb831f87d8c9a41b169e3c99bae2d | 5143 | ml-kem-1024/PROVENANCE.md |
| c6bb9f126f704c0b77dfa370734bb526c14fd14d184711d945c955ee3a3c89a1 | 243481 | ml-kem-1024/nist-acvp-keygen.json |
| ed4d2cf8b5fbf5b3fcb8179423786c8c8d6cedf27eaf512f1f4fa535c747fed3 | 358782 | ml-kem-1024/nist-acvp-encapdecap.json |
| 7e0ee2def2fed3db5da5d84453fc87fdf3174138fc97e4b03517b5c643007f44 | 979821 | ml-kem-1024/wycheproof-keygen-seed.json |
| 2b756a05a8e019035e41244da32c654b1cfbcf43c55170747710b26dae6e7e4d | 1313426 | ml-kem-1024/wycheproof-encaps.json |
| 9a4cea32d8aaf796d2a9fa8336d3222c5e9e46c9151e62b4e8e7ccd7bfa8ba78 | 1261684 | ml-kem-1024/wycheproof-decaps.json |
| f5503f960fe9bd4e55ee05e20af2b570b37f51cf6648db1a6f928dba7b55471d | 116226 | ml-kem-1024/wycheproof-decaps-checked.json |
| 2e417c9b2d5e8d2ba1e90ee8dd54d018c4e8bacdec7e40ffbf2c8ed1d95fcc7a | 4604 | ml-dsa-87/PROVENANCE.md |
| 57ad144d5aeb9cb4d0198901535e675258d56251c9534cc4924132061e5e9cda | 379177 | ml-dsa-87/nist-acvp-keygen.json |
| e74f39003cd39f64036391ee92fb40954d28476dd3fc17d8b048a2dd88e787c7 | 349067 | ml-dsa-87/nist-acvp-sigver.json |
| b4e5a6620f9099118682767999c70d81a9fa14020adb9ae692e17f5dcfa84b4e | 839788 | ml-dsa-87/nist-acvp-siggen.json |
| 1b8176de8d9a7c536a48d9b9fa14264bb4b7d7332a41414e2ef02a17f0058751 | 3554624 | ml-dsa-87/wycheproof-verify.json |

**Licences:** Wycheproof's is Apache-2.0, in `LICENSE-wycheproof.txt`. NIST's work isn't subject to copyright in the United States.

## The shape of every file

```
{ "primitive": "...", "source": "...", "about": "what each case means, and what was taken", "count": N,
  "cases": [ { "id": "...", <the case's inputs and answers>, "result": "valid" } ] }
```

- **Bytes** are hex in small letters, and `""` when there are none.
- **Numbers** are whole numbers. `count` is how many cases there are.
- **`result`:** `"valid"` means the answer must come out exactly as given; `"invalid"` means it must be refused.
- **No `true`, `false` or `null`.** Every string is printable ASCII, with no escape but `\"` and `\\`.
- **Wycheproof's flags** are one string, split by commas (`"ValidSignature,ManySteps"`).
- **Each file's `about`** says what its fields are and what each `result` means.

`tests/crypto_kat_reader.h` reads exactly this and refuses anything else, naming what it found, so a damaged file fails the test rather
than skipping cases.

## What the tests do with them

- Every case is run. A "valid" one must give exactly its answer, and an "invalid" one must be refused.
- A case the game's code has no way to take is counted and named in the test's output, never left out without a word:
  - ML-DSA-87's keyGen and sigGen cases: the game only checks signatures, and its library is built with no key making or signing;
  - Wycheproof's ML-KEM cases that give a seed of the wrong length: the game never makes a key from a seed it's given.
