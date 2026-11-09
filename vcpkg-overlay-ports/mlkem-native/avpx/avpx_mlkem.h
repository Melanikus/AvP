// The one header the game reads for ML-KEM-1024. Written 2026-11-09 by Melanikus for the AvP port.
//
// It names our settings file, then reads the library's own header. The library was built with the same settings file
// (CMakeLists.txt), so the size of every key and the name of every function the game sees are the ones the library has.
// Include it as <mlkem-native/avpx_mlkem.h>. Reading mlkem_native.h directly instead doesn't compile, because the library's own
// settings file, which it would look for, isn't installed.

#ifndef AVPX_MLKEM_H
#define AVPX_MLKEM_H

#if defined(MLK_CONFIG_FILE)
#error "MLK_CONFIG_FILE is already set: the game reads ML-KEM only with avpx_mlkem_config.h, the settings the library was built with"
#endif

#define MLK_CONFIG_FILE "avpx_mlkem_config.h"
#include "mlkem_native.h"

#if MLK_CONFIG_PARAMETER_SET != 1024
#error "The game's ML-KEM is ML-KEM-1024: avpx_mlkem_config.h has to say MLK_CONFIG_PARAMETER_SET 1024"
#endif

#endif
