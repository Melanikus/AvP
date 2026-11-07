vcpkg_from_github(OUT_SOURCE_PATH SOURCE_PATH
    REPO  "lsalzman/enet"
    REF "v${VERSION}"
    HEAD_REF master
    SHA512 a0d2fa8c957704dd49e00a726284ac5ca034b50b00d2b20a94fa1bbfbb80841467834bfdc84aa0ed0d6aab894608fd6c86c3b94eee46343f0e6d9c22e391dbf9
    PATCHES
        fix-export.patch
        # Our own fixes. These have only run on this machine's own address, never over a real network: that waits for card NET-2.
        001-remove-dead-channel-check.patch
        002-keep-packet-header-in-host.patch
        003-keep-send-lists-off-the-stack.patch
        # The game only sends reliable messages, so ENet refuses the other three kinds, and a missing size check goes in as well.
        004-refuse-what-this-game-never-sends.patch
        # From our review of ENet as a reader of hostile packets: limits on what one peer can make it hold or do. tests/enet_hostile_test.cpp has a row for each.
        005-limits-on-what-a-peer-can-make-enet-hold.patch
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    # OPTIONS -DUSE_THIS_IN_ALL_BUILDS=1 -DUSE_THIS_TOO=2
    # OPTIONS_RELEASE -DOPTIMIZE=1
    # OPTIONS_DEBUG -DDEBUGGABLE=1
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME unofficial-enet CONFIG_PATH share/unofficial-enet)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_copy_pdbs()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
