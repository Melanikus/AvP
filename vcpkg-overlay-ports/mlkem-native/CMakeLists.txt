# How mlkem-native is built for the game. Written 2026-11-09 by Melanikus for the AvP port.
#
# portfile.cmake copies this file into the project's source, which has no CMake build of its own, and vcpkg builds it from there. It
# makes one static library, mlkem_native.lib, from the project's own single file, mlkem/mlkem_native.c, and installs it with the headers
# the game reads.

cmake_minimum_required(VERSION 3.20)
project(avpx_mlkem_native LANGUAGES C)

add_library(mlkem_native STATIC mlkem/mlkem_native.c)

# The library reads its settings from the file MLK_CONFIG_FILE names, ours, and never from its own mlkem/mlkem_native_config.h, which
# would make a weaker ML-KEM-768 without a word. avpx/ is where ours is.
target_include_directories(mlkem_native PRIVATE avpx)
target_compile_definitions(mlkem_native PRIVATE MLK_CONFIG_FILE="avpx_mlkem_config.h")

# The game's own safety settings (win32/VS2026/AvP.vcxproj): the compiler's extra checks (/sdl), and the marks Windows needs to check
# where every indirect call goes (/guard:cf). Warnings at the game's level (/W3): the library builds with none (checked 2026-11-09
# with Visual Studio 2026, and with the address sanitizer). Not /WX, which would turn any warning into a stop: the sanitizer's own
# warning about missing debug information (C5072) comes from vcpkg's flags, not this code, and would stop a build with nothing wrong.
if(MSVC)
    target_compile_options(mlkem_native PRIVATE /W3 /sdl /guard:cf)
endif()

install(TARGETS mlkem_native ARCHIVE DESTINATION lib)
install(FILES mlkem/mlkem_native.h avpx/avpx_mlkem_config.h avpx/avpx_mlkem.h DESTINATION include/mlkem-native)

# The project's own known answers, for tests/pq_mlkem_test only: keys and a ciphertext made from fixed seeds, for each size. They come
# from the same archive as the library, checked by the same SHA-512, so they can't drift from the code they were made with.
# They go under share/, not include/ (Fourth's cold read, finding 4, 2026-11-09): include/ is on the game's own include path, and the
# game must never be able to read a test's fixed keys by mistake. Only the test's own .args adds share/mlkem-native to its path.
install(FILES test/test_vectors/expected_test_vectors.h DESTINATION share/mlkem-native/upstream-tests)
