vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO xiph/vorbis
    REF v1.3.7
    SHA512 bfb6f5dbfd49ed38b2b08b3667c06d02e68f649068a050f21a3cc7e1e56b27afd546aaa3199c4f6448f03f6e66a82f9a9dc2241c826d3d1d4acbd38339b9e9fb
    HEAD_REF master
    PATCHES
        0001-Dont-export-vorbisenc-functions.patch
        0002-Fixup-pkgconfig-libs.patch
        0003-def-mingw-compat.patch
        0004-ogg-find-dependency.patch
        # Security fixes from upstream's main branch, after the 1.3.7 release. Each file keeps its
        # upstream commit header. https://github.com/xiph/vorbis/commit/<hash in the file name>
        0005-upstream-bb4047de-sharedbook-undefined-shift.patch
        0006-upstream-315da9cc-psy-half-octave-bounds.patch
        0007-upstream-2d79800b-left-shift-of-negative.patch
        0008-upstream-2868f93c-encode-cap-base-setting-delta.patch
        0009-upstream-c9b52fc1-encode-stereo-point-truncation.patch
        0010-upstream-7ef6726d-crosslap-large-alloca.patch
        0011-upstream-28965ede-sharedbook-replace-alloca.patch
        0012-upstream-a629068d-sharedbook-check-allocations.patch
        0013-upstream-1b75110b-page-seek-use-after-free.patch
        # Our own fixes.
        0014-reject-value-mapped-books-with-zero-dimensions.patch
        0015-check-fopen-and-bound-buffer-in-disabled-dumps.patch
        0016-check-codebook-allocations.patch
        0017-limit-codebook-memory-per-file.patch
        0018-keep-a-failed-decoder-setup-failed.patch
        0019-check-serial-list-and-read-buffer.patch
        0020-probe-five-files.patch
)

# Delete source files that are never built, so that they are not scanned as if we shipped them.
# The library is built from the source lists in lib/CMakeLists.txt, and the top-level
# CMakeLists.txt only adds lib/. None of these is in those lists:
# - vq/: command-line tools for designing codebooks;
# - examples/: sample programs;
# - test/: upstream's test programs;
# - contrib/: upstream's fuzz test setup;
# - lib/psytune.c: a command-line tool for tuning the encoder;
# - lib/misc.c: a debug memory tracker, only for builds with DEBUG_MALLOC, which needs pthread.h.
# - lib/barkmel.c, lib/tone.c: small standalone programs, each with its own main().
#
# Before changing what this port patches or deletes, read this. Snyk recognises this library by
# how many of its release files are still here unchanged, and its scan fails if it no longer
# does. The lowest figure known to pass is 0.691, which is 56 of 81 files, and this tree is at
# exactly that figure. Patching a file that a patch here already changes costs nothing. Patching
# or deleting any other file Snyk counts costs one, and nothing below 0.691 has been tested.
file(REMOVE_RECURSE
    "${SOURCE_PATH}/vq"
    "${SOURCE_PATH}/examples"
    "${SOURCE_PATH}/test"
    "${SOURCE_PATH}/contrib"
    "${SOURCE_PATH}/lib/psytune.c"
    "${SOURCE_PATH}/lib/misc.c"
    "${SOURCE_PATH}/lib/barkmel.c"
    "${SOURCE_PATH}/lib/tone.c"
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 # https://github.com/xiph/vorbis/issues/113
    MAYBE_UNUSED_VARIABLES
        CMAKE_POLICY_VERSION_MINIMUM
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME Vorbis CONFIG_PATH "lib/cmake/Vorbis")
vcpkg_fixup_pkgconfig()
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
file(INSTALL "${SOURCE_PATH}/COPYING" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
