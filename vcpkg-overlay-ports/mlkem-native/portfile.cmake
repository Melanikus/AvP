# mlkem-native, for the game's post-quantum key exchange (ML-KEM-1024, FIPS 203). Written 2026-11-09 by Melanikus for the AvP port.
#
# Where it comes from: release v2.0.0 of the mlkem-native project, as GitHub's own archive of that tag. vcpkg works out the SHA-512 of
# what it downloads and stops before building anything if it isn't the one below, so no other archive can be built in its place.
#
# What we change in their code: nothing. There are no patches. It's built from the project's own single file, mlkem/mlkem_native.c,
# which gathers the whole library for one size of key. Everything that's ours is copied in beside their code:
# - CMakeLists.txt says how to build it. The project has no CMake build of its own.
# - avpx/avpx_mlkem_config.h holds the settings: which size of key, no assembly, the check of every new key pair, and where random
#   bytes come from. It replaces the project's own settings file whole.
# - avpx/avpx_mlkem.h is the one header the game reads. It names the settings file first, so the game and the library always agree.

# A static library only: it goes inside the game's own program, so there's no DLL to ship, check or replace.
vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(OUT_SOURCE_PATH SOURCE_PATH
    REPO "pq-code-package/mlkem-native"
    REF "v${VERSION}"
    HEAD_REF main
    SHA512 0f576e21fc0bbe8d14440b88f684ead90ba5c3cfb295595b28febd1261d71198b9a21ed65c6d9db0c3fc8dc21bcd906eb9fc5f9cbc9f82a311b7e41b23a6d790
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/avpx" DESTINATION "${SOURCE_PATH}")

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()

# The headers are the same for both builds, so the debug build's copy goes, as vcpkg asks.
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

# The project's LICENSE offers Apache-2.0, ISC or MIT. The game takes it under MIT: notices/mlkem-native.txt.
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
