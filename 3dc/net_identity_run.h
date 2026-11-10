// This run's player identity, for the screens that show it or replace it (the -dev access page, #21 and #24). Added 2026-11-09 by
// Melanikus for the AvP port.
//
// networking.cpp keeps the identity (RelayIdentityReady), and these three are the screens' only way to it. They're in a header of their
// own, not networking.h, so the many files that include networking.h don't all take in the cryptography's headers too.
//
// The identity is looked for once each time the network starts: the one kept from earlier runs (net_identity_store.h), or, when there's
// none yet, a new one, made and kept. A kept one that can't be used (refused, not opened for this Windows user, or not read) is never
// replaced without the player's yes: until then there's no identity, and nothing goes through a relay, the same as when one can't be made.
#ifndef NET_IDENTITY_RUN_H
#define NET_IDENTITY_RUN_H

#include "net_identity_store.h"

// What looking for the kept identity found this run (it looks first, if it hasn't yet). Loaded, or NoFile (a new one made and kept), means
// there's nothing to ask. Refused, CantUnseal or CantRead means the page asks the player before anything is made. Once the player's yes has
// made a new one (Net_MakeNewIdentity), it's Loaded.
NetIdentityStoreResult Net_KeptIdentityResult(void);

// This run's identity's public halves and fingerprint, made ready first (as above). False, with both wiped, when there's none: a kept one
// that couldn't be used, until the player's yes, or one that couldn't be made or kept. Only public bytes are copied out.
bool Net_OurIdentity(uint8_t statics[kNetIdentityStaticBytes], uint8_t fingerprint[kNetIdentityFingerprintBytes]);

// The player's yes, from the confirmation or from "Make a new identity": the identity in memory, if there is one, is freed, and a new one
// is made and kept in place of the file. False means there's no identity this run, as when one can't be made. It's refused (false, nothing
// changed) while a network game is open, as its handshakes and sealed datagrams belong to the identity they began with.
bool Net_MakeNewIdentity(void);

#endif
