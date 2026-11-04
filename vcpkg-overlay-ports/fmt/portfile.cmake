vcpkg_download_distfile(FMT_BACKPORT_4813_PATCH
    URLS https://github.com/fmtlib/fmt/commit/588b3a0f8f6a8bcf2a959cae882d5b2703e86737.patch?full_index=1
    FILENAME fmt-backport-4813.patch
    SHA512 afda8fdfcdcb4b0dd5df4d4dae96a57a85fb9c4b65d0b49d51258f0913d4aed93ed146ebf96ed7b277490b1dde6c7117f43332013071441a96c3147520de8368
)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO fmtlib/fmt
    REF "${VERSION}"
    SHA512 5ac2ba0f54a484999ed5407d82b77aad170cea49a267decd2c0eedadf3b14413e2a83fcc8e9ca9c16640595e019b8636e160f72314d8be50653324e82ac745eb
    HEAD_REF master
    PATCHES
        "${FMT_BACKPORT_4813_PATCH}"
)

# Delete source folders that are never built, so that nothing we do not ship stays in the tree,
# and no code sits where a scanner that follows the build could not reach it.
# - test/: the tests and the fuzzing programs, only used under FMT_TEST and FMT_FUZZ, which are
#   both OFF below.
# - support/python/: the plugin that builds the documentation, only used under FMT_DOC, which is
#   OFF below. The build still needs the rest of support/.
# - src/fmt.cc: the C++ module form of the library, only used under FMT_MODULE, which is OFF below.
file(REMOVE_RECURSE
    "${SOURCE_PATH}/test"
    "${SOURCE_PATH}/support/python"
    "${SOURCE_PATH}/src/fmt.cc"
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DFMT_CMAKE_DIR=share/fmt
        -DFMT_TEST=OFF
        -DFMT_FUZZ=OFF
        -DFMT_DOC=OFF
        -DFMT_MODULE=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup()
vcpkg_fixup_pkgconfig()
vcpkg_copy_pdbs()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
