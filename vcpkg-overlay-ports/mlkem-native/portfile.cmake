# mlkem-native, for the game's post-quantum key exchange (ML-KEM-1024, FIPS 203). Written 2026-11-09 by Melanikus for the AvP port.
#
# Where it comes from: release v2.0.0 of the mlkem-native project, as GitHub's own archive of that tag. vcpkg works out the SHA-512 of
# what it downloads and stops before building anything if it isn't the one below, so no other archive can be built in its place.
#
# What we change in their code: nothing. There are no patches, and only the folders the game never builds or runs are deleted from our
# copy (below). It's built from the project's own single file, mlkem/mlkem_native.c, which gathers the whole library for one size of
# key. Everything that's ours is copied in beside their code:
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

# Delete what the game never builds or runs, so what's scanned is exactly what we build from, as fmt's and libvorbis's ports do. The owner's
# word, 2026-11-09 (#174): "Yes, delete them". Our CMakeLists.txt builds mlkem/mlkem_native.c alone, and nothing in mlkem/ includes a
# file outside mlkem/: every relative #include in it names a file inside it (checked against the release, 2026-11-09). So none of these
# can be part of the build. Each, and why it's never ours:
# - .github/: the project's own workflows, which GitHub runs for the project, never for us.
# - dev/: the project's working copies of its assembly back ends, which its scripts copy into mlkem/. We build with no assembly
#   (avpx/avpx_mlkem_config.h), and from mlkem/ alone.
# - examples/: example programs, never built. All 176 of the release's links are here, and 9 of them point at folders,
#   mlkem/src among them (#174, run 42671277153's notices).
# - integration/: how other projects (aws-lc, liboqs and others) build it into theirs.
# - nix/ and scripts/: the project's development shell, and its scripts for tests, formatting, checks and code generation.
# - proofs/: the project's proofs (CBMC, HOL Light and the others), which the project runs, not us. They prove things of the project's
#   own code; deleting our copy of them changes nothing they prove.
# - test/, all but test/test_vectors/: the project's own test programs and tools (ACVP, Wycheproof, bare metal, benchmarks, Zephyr and
#   the rest). Our tests run NIST's and Wycheproof's answers through the game's own code (tests/pq_mlkem_test.cpp).
# - test/test_vectors/expected_test_vectors_multilevel.h: the known answers for a build of several sizes at once; we build one.
# What stays of test/: test/test_vectors/expected_test_vectors.h, the known answers our test reads (CMakeLists.txt installs it).
# A link is removed as a link, never followed into what it points at. That matters here, because examples/ holds links to mlkem/src.
# So the files of mlkem/ are counted before and after (GLOB_RECURSE doesn't follow links either), and the build stops if one went.
file(GLOB_RECURSE AVPX_BUILT_BEFORE LIST_DIRECTORIES false "${SOURCE_PATH}/mlkem/*")
file(GLOB AVPX_TEST_PARTS LIST_DIRECTORIES true "${SOURCE_PATH}/test/*")
list(FILTER AVPX_TEST_PARTS EXCLUDE REGEX "/test_vectors$")
file(REMOVE_RECURSE
    "${SOURCE_PATH}/.github"
    "${SOURCE_PATH}/dev"
    "${SOURCE_PATH}/examples"
    "${SOURCE_PATH}/integration"
    "${SOURCE_PATH}/nix"
    "${SOURCE_PATH}/scripts"
    "${SOURCE_PATH}/proofs"
    ${AVPX_TEST_PARTS}
    "${SOURCE_PATH}/test/test_vectors/expected_test_vectors_multilevel.h"
)

# Every file of mlkem/ is still there, and so is the one file of test/ that must stay, or the build stops here
file(GLOB_RECURSE AVPX_BUILT_AFTER LIST_DIRECTORIES false "${SOURCE_PATH}/mlkem/*")
list(LENGTH AVPX_BUILT_BEFORE AVPX_BUILT_BEFORE_COUNT)
list(LENGTH AVPX_BUILT_AFTER AVPX_BUILT_AFTER_COUNT)
if(AVPX_BUILT_BEFORE_COUNT EQUAL 0 OR NOT AVPX_BUILT_BEFORE_COUNT EQUAL AVPX_BUILT_AFTER_COUNT
    OR NOT EXISTS "${SOURCE_PATH}/test/test_vectors/expected_test_vectors.h")
    message(FATAL_ERROR "mlkem-native: the deletion took a file the build needs (mlkem/ had ${AVPX_BUILT_BEFORE_COUNT} files, has ${AVPX_BUILT_AFTER_COUNT})")
endif()

# The native back ends go too: mlkem/src/native/ (the arithmetic, in code for AArch64, POWER, RISC-V and x86-64 processors) and
# mlkem/src/fips202/native/ (the same for SHA-3, for AArch64, Armv8.1-M and x86-64). They're the project's fast versions for
# particular processors, and a build uses one only when its settings turn it on, with MLK_CONFIG_USE_NATIVE_BACKEND_ARITH or
# MLK_CONFIG_USE_NATIVE_BACKEND_FIPS202. Ours turn on neither (avpx/avpx_mlkem_config.h). Every #include of these folders is inside
# one of those two settings' #if, in mlkem/mlkem_native.c and in mlkem/src/common.h (checked against the release, 2026-11-09). So the
# game never builds them, and the owner's word above covers them too (PO, 2026-11-09). Snyk Code can't read most of them (#174),
# and what's scanned should be what's built. If a setting ever did turn one on, the build would stop at once on the missing file;
# it could never build something else in its place. The rest of mlkem/ is counted before and after, so nothing else goes with them.
file(GLOB_RECURSE AVPX_PORTABLE_BEFORE LIST_DIRECTORIES false "${SOURCE_PATH}/mlkem/*")
list(FILTER AVPX_PORTABLE_BEFORE EXCLUDE REGEX "/mlkem/src/(fips202/)?native/")
file(REMOVE_RECURSE "${SOURCE_PATH}/mlkem/src/native" "${SOURCE_PATH}/mlkem/src/fips202/native")
file(GLOB_RECURSE AVPX_PORTABLE_AFTER LIST_DIRECTORIES false "${SOURCE_PATH}/mlkem/*")
list(LENGTH AVPX_PORTABLE_BEFORE AVPX_PORTABLE_BEFORE_COUNT)
list(LENGTH AVPX_PORTABLE_AFTER AVPX_PORTABLE_AFTER_COUNT)
if(AVPX_PORTABLE_BEFORE_COUNT EQUAL 0 OR NOT AVPX_PORTABLE_BEFORE_COUNT EQUAL AVPX_PORTABLE_AFTER_COUNT
    OR EXISTS "${SOURCE_PATH}/mlkem/src/native" OR EXISTS "${SOURCE_PATH}/mlkem/src/fips202/native")
    message(FATAL_ERROR "mlkem-native: taking out the native back ends went wrong (mlkem/ without them had ${AVPX_PORTABLE_BEFORE_COUNT} files, has ${AVPX_PORTABLE_AFTER_COUNT})")
endif()

file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/avpx" DESTINATION "${SOURCE_PATH}")

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()

# The headers are the same for both builds, so the debug build's copy goes, as vcpkg asks.
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

# The project's LICENSE offers Apache-2.0, ISC or MIT. The game takes it under MIT: notices/mlkem-native.txt.
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
