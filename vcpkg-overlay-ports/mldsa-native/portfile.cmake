# mldsa-native, for checking the post-quantum half of every signature the game is given (ML-DSA-87, FIPS 204). Written 2026-11-09 by
# Melanikus for the AvP port.
#
# Where it comes from: release v2.0.0 of the mldsa-native project, as GitHub's own archive of that tag. vcpkg works out the SHA-512 of
# what it downloads and stops before building anything if it isn't the one below, so no other archive can be built in its place.
#
# What we change in their code: nothing. There are no patches, and only the folders the game never builds or runs are deleted from our
# copy (below). It's built from the project's own single file, mldsa/mldsa_native.c, which gathers the whole library for one size of
# key. Everything that's ours is copied in beside their code:
# - CMakeLists.txt says how to build it. The project has no CMake build of its own.
# - avpx/avpx_mldsa_config.h holds the settings: which size of key, checking only (the game signs nothing), no assembly, and the
#   smaller memory use. It replaces the project's own settings file whole.
# - avpx/avpx_mldsa.h is the one header the game reads. It names the settings file first, so the game and the library always agree.

# A static library only: it goes inside the game's own program, so there's no DLL to ship, check or replace.
vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(OUT_SOURCE_PATH SOURCE_PATH
    REPO "pq-code-package/mldsa-native"
    REF "v${VERSION}"
    HEAD_REF main
    SHA512 f6fd4fc0216326f6dc217a3b40a76738b7737fe42534c96b0a767d8723046604e1dc0f40439b7c0a951fb3134d869370859b99cc98913ed93db7517eb2de29a0
)

# Delete what the game never builds or runs, so what's scanned is exactly what we build from, as fmt's and libvorbis's ports do. The owner's
# word, 2026-11-09 (#174): "Yes, delete them". Our CMakeLists.txt builds mldsa/mldsa_native.c alone, and nothing in mldsa/ includes a
# file outside mldsa/: every relative #include in it names a file inside it (checked against the release, 2026-11-09). So none of these
# can be part of the build. Each, and why it's never ours:
# - .github/: the project's own workflows, which GitHub runs for the project, never for us.
# - dev/: the project's working copies of its assembly back ends, which its scripts copy into mldsa/. We build with no assembly
#   (avpx/avpx_mldsa_config.h), and from mldsa/ alone.
# - examples/: example programs, never built. 162 of the release's 163 links are here (the other is in test/src), and 20 of
#   them point at folders, mldsa/src among them (#174, run 42671277153's notices).
# - integration/: how other projects (aws-lc, liboqs and others) build it into theirs.
# - nix/ and scripts/: the project's development shell, and its scripts for tests, formatting, checks and code generation.
# - proofs/: the project's proofs (CBMC, HOL Light and the others), which the project runs, not us. They prove things of the project's
#   own code; deleting our copy of them changes nothing they prove.
# - test/, all but test/test_vectors/: the project's own test programs and tools (ACVP, Wycheproof, bare metal, benchmarks, Zephyr and
#   the rest). Our tests run NIST's and Wycheproof's answers through the game's own code (tests/pq_mldsa_test.cpp).
# - test/test_vectors/expected_test_vectors_multilevel.h: the known answers for a build of several sizes at once; we build one.
# What stays of test/: test/test_vectors/expected_test_vectors.h, the known answers our test reads (CMakeLists.txt installs it).
# A link is removed as a link, never followed into what it points at. That matters here, because examples/ holds links to mldsa/src.
# So the files of mldsa/ are counted before and after (GLOB_RECURSE doesn't follow links either), and the build stops if one went.
file(GLOB_RECURSE AVPX_BUILT_BEFORE LIST_DIRECTORIES false "${SOURCE_PATH}/mldsa/*")
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

# Every file of mldsa/ is still there, and so is the one file of test/ that must stay, or the build stops here
file(GLOB_RECURSE AVPX_BUILT_AFTER LIST_DIRECTORIES false "${SOURCE_PATH}/mldsa/*")
list(LENGTH AVPX_BUILT_BEFORE AVPX_BUILT_BEFORE_COUNT)
list(LENGTH AVPX_BUILT_AFTER AVPX_BUILT_AFTER_COUNT)
if(AVPX_BUILT_BEFORE_COUNT EQUAL 0 OR NOT AVPX_BUILT_BEFORE_COUNT EQUAL AVPX_BUILT_AFTER_COUNT
    OR NOT EXISTS "${SOURCE_PATH}/test/test_vectors/expected_test_vectors.h")
    message(FATAL_ERROR "mldsa-native: the deletion took a file the build needs (mldsa/ had ${AVPX_BUILT_BEFORE_COUNT} files, has ${AVPX_BUILT_AFTER_COUNT})")
endif()

file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/avpx" DESTINATION "${SOURCE_PATH}")

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()

# The headers are the same for both builds, so the debug build's copy goes, as vcpkg asks.
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

# The project's LICENSE offers Apache-2.0, ISC or MIT. The game takes it under MIT: notices/mldsa-native.txt.
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
