// The one header the game reads for ML-DSA-87. Written 2026-11-09 by Melanikus for the AvP port.
//
// It names our settings file, then reads the library's own header. The library was built with the same settings file
// (CMakeLists.txt), so the size of every key and the name of every function the game sees are the ones the library has.
// Include it as <mldsa-native/avpx_mldsa.h>. Reading mldsa_native.h directly instead doesn't compile, because the library's own
// settings file, which it would look for, isn't installed.

#ifndef AVPX_MLDSA_H
#define AVPX_MLDSA_H

#if defined(MLD_CONFIG_FILE)
#error "MLD_CONFIG_FILE is already set: the game reads ML-DSA only with avpx_mldsa_config.h, the settings the library was built with"
#endif

#define MLD_CONFIG_FILE "avpx_mldsa_config.h"
#include "mldsa_native.h"

#if MLD_CONFIG_PARAMETER_SET != 87
#error "The game's ML-DSA is ML-DSA-87: avpx_mldsa_config.h has to say MLD_CONFIG_PARAMETER_SET 87"
#endif

#if !defined(MLD_CONFIG_NO_SIGN_API) || !defined(MLD_CONFIG_NO_KEYPAIR_API)
#error "The game only checks signatures: avpx_mldsa_config.h has to leave out key making and signing"
#endif

#endif
