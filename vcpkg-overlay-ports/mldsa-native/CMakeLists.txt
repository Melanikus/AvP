# How mldsa-native is built for the game. Written 2026-11-09 by Melanikus for the AvP port.
#
# portfile.cmake copies this file into the project's source, which has no CMake build of its own, and vcpkg builds it from there. It
# makes one static library, mldsa_native.lib, from the project's own single file, mldsa/mldsa_native.c, and installs it with the headers
# the game reads.

cmake_minimum_required(VERSION 3.20)
project(avpx_mldsa_native LANGUAGES C)

add_library(mldsa_native STATIC mldsa/mldsa_native.c)

# The library reads its settings from the file MLD_CONFIG_FILE names, ours, and never from its own mldsa/mldsa_native_config.h, which
# would make the weakest size, ML-DSA-44, without a word. avpx/ is where ours is.
target_include_directories(mldsa_native PRIVATE avpx)
target_compile_definitions(mldsa_native PRIVATE MLD_CONFIG_FILE="avpx_mldsa_config.h")

# The game's own safety settings (win32/VS2026/AvP.vcxproj): the compiler's extra checks (/sdl), and the marks Windows needs to check
# where every indirect call goes (/guard:cf). Warnings at the game's level (/W3): the library builds with none (checked 2026-11-09
# with Visual Studio 2026, and with the address sanitizer). Not /WX, which would turn any warning into a stop: the sanitizer's own
# warning about missing debug information (C5072) comes from vcpkg's flags, not this code, and would stop a build with nothing wrong.
if(MSVC)
    target_compile_options(mldsa_native PRIVATE /W3 /sdl /guard:cf)
endif()

install(TARGETS mldsa_native ARCHIVE DESTINATION lib)
install(FILES mldsa/mldsa_native.h avpx/avpx_mldsa_config.h avpx/avpx_mldsa.h DESTINATION include/mldsa-native)

# The project's own known answers, for tests/pq_mldsa_test only: a key and signatures made from fixed seeds, for each size. They come
# from the same archive as the library, checked by the same SHA-512, so they can't drift from the code they were made with.
install(FILES test/test_vectors/expected_test_vectors.h DESTINATION include/mldsa-native/upstream-tests)
