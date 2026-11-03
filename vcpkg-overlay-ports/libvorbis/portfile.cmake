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
