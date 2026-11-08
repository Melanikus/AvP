vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcat/openal-soft
    REF ${VERSION}
    SHA512 d3790aedce0bdb0b348b35f917dec0626de2486f5967c9160cf2d50d5c2a00ff264ff73133ac6a0aa9207e8e5042e5d4fcc663c1e733df8e733647e843c39b25
    HEAD_REF master
    PATCHES
        001-devendor-fmt.patch
        002-fix-non-blocking.patch
        003-fix-fmt-header.patch
        004-fix-libcxx-compatibility.patch
        # A fix from upstream's master branch, after the 1.25.2 release. The file names its
        # upstream commit. https://github.com/kcat/openal-soft/commit/<hash in the file name>
        005-upstream-c41281da-config-fs-ifstream.patch
        # Our own fixes.
        006-config-file-hardening.patch
        008-excommon-only-with-examples.patch
        # A dated notice at the top of each file the patches above change, saying what was changed, by whom
        # and when, as the Library GPL asks (2(b)). Last, so the others apply to the files as upstream has them.
        009-change-notices.patch
)

# Delete source folders that are never built, so that nothing we do not ship stays in the tree,
# and no code sits where a scanner that follows the build could not reach it.
# - examples/: sample programs, only used under ALSOFT_EXAMPLES, which is OFF below. Patch 008
#   moves the one helper library that was declared outside that check.
# - utils/: command-line tools, only used under ALSOFT_UTILS, which is OFF below.
# - tests/: only used under ALSOFT_TESTS, which is OFF below.
# - registry/: the API lists and the script that makes headers from them, run by hand. No build
#   file mentions this folder.
# - fmt-11.2.0/: a bundled copy of fmt. 001-devendor-fmt.patch uses the fmt port instead.
file(REMOVE_RECURSE
    "${SOURCE_PATH}/examples"
    "${SOURCE_PATH}/utils"
    "${SOURCE_PATH}/tests"
    "${SOURCE_PATH}/registry"
    "${SOURCE_PATH}/fmt-11.2.0"
)

# Delete the library code that this port never compiles on any platform.
# - modules/, alsoft-modules/ and the other .cppm files: C++ modules, only used under
#   HAVE_CXXMODULES, which needs ALSOFT_ENABLE_MODULES, which is OFF below.
# - router/: only used under ALSOFT_BUILD_ROUTER, which is OFF below.
# - common/win_main_utf8.h: only the deleted examples and tools included it.
file(REMOVE_RECURSE
    "${SOURCE_PATH}/modules"
    "${SOURCE_PATH}/alsoft-modules"
    "${SOURCE_PATH}/alc/context.cppm"
    "${SOURCE_PATH}/common/alformattypes.cppm"
    "${SOURCE_PATH}/core/logging.cppm"
    "${SOURCE_PATH}/router"
    "${SOURCE_PATH}/common/win_main_utf8.h"
)

# On x86 Windows, the only build we ship, also delete what only other platforms compile. Other
# platforms do need these files, so they stay there.
# - The audio backends for other systems. Each is only used if its ALSOFT_BACKEND_ option is on,
#   and the options below (or if(NOT WIN32) in CMakeLists.txt) turn all of these off. We keep
#   base, loopback, null, dsound, wasapi and wave.
# - core/rtkit: Linux realtime support, only used under if(NOT WIN32).
# - core/mixer/mixer_neon.cpp: for ARM processors, only used under HAVE_NEON.
# - common/ghc_filesystem.h: a stand-in for compilers with no std::filesystem. Ours has it, so
#   common/filesystem.h never includes this. If that ever changed, the build would stop with an
#   error, so this cannot go wrong quietly.
# - The code in common/almalloc.cpp: all of it is for macOS before 10.13, inside
#   #if defined(__APPLE__), so here the file compiles to nothing. CMake's source list needs the
#   file to exist, so it stays, without that code.
if(VCPKG_TARGET_IS_WINDOWS AND NOT VCPKG_TARGET_IS_UWP AND VCPKG_TARGET_ARCHITECTURE STREQUAL "x86")
    foreach(backend IN ITEMS alsa coreaudio jack oboe opensl oss pipewire portaudio pulseaudio sdl2
            sdl3 sndio solaris winmm)
        file(REMOVE
            "${SOURCE_PATH}/alc/backends/${backend}.cpp"
            "${SOURCE_PATH}/alc/backends/${backend}.h"
            "${SOURCE_PATH}/alc/backends/${backend}.hpp"
        )
    endforeach()
    file(REMOVE
        "${SOURCE_PATH}/core/rtkit.cpp"
        "${SOURCE_PATH}/core/rtkit.h"
        "${SOURCE_PATH}/core/mixer/mixer_neon.cpp"
        "${SOURCE_PATH}/common/ghc_filesystem.h"
    )
    file(WRITE "${SOURCE_PATH}/common/almalloc.cpp" [=[
// Modified 2026-11-03 by Melanikus for the AvP port:
//   removed the memory functions for macOS before 10.13, which were all this file held. Windows never
//   compiled them.

#include "almalloc.h"
]=])
endif()

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        pipewire    ALSOFT_BACKEND_PIPEWIRE
        pipewire    ALSOFT_REQUIRE_PIPEWIRE
        pulseaudio  ALSOFT_BACKEND_PULSEAUDIO
        pulseaudio  ALSOFT_REQUIRE_PULSEAUDIO
)

if(VCPKG_LIBRARY_LINKAGE STREQUAL "dynamic")
    set(OPENAL_LIBTYPE "SHARED")
else()
    set(OPENAL_LIBTYPE "STATIC")
endif()

set(ALSOFT_REQUIRE_LINUX OFF)
set(ALSOFT_REQUIRE_WINDOWS OFF)
set(ALSOFT_REQUIRE_WINDOWS_NOT_UWP OFF)
set(ALSOFT_REQUIRE_APPLE OFF)
set(ALSOFT_CPUEXT_NEON OFF)

if(VCPKG_TARGET_IS_LINUX)
    set(ALSOFT_REQUIRE_LINUX ON)
endif()
if(VCPKG_TARGET_IS_WINDOWS)
    set(ALSOFT_REQUIRE_WINDOWS ON)
    if(NOT VCPKG_TARGET_IS_UWP)
        set(ALSOFT_REQUIRE_WINDOWS_NOT_UWP ON)
    endif()
endif()
if(VCPKG_TARGET_IS_OSX OR VCPKG_TARGET_IS_IOS)
    set(ALSOFT_REQUIRE_APPLE ON)
endif()
if(VCPKG_TARGET_ARCHITECTURE STREQUAL "arm64")
    set(ALSOFT_CPUEXT_NEON ON)
endif()

vcpkg_find_acquire_program(PKGCONFIG)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        -DALSOFT_ENABLE_MODULES=OFF
        -DALSOFT_CPUEXT_NEON=${ALSOFT_CPUEXT_NEON}
        -DALSOFT_EXAMPLES=OFF
        -DALSOFT_INSTALL_AMBDEC_PRESETS=OFF
        -DALSOFT_INSTALL_CONFIG=OFF
        -DALSOFT_INSTALL_HRTF_DATA=OFF
        -DALSOFT_NO_CONFIG_UTIL=ON
        -DALSOFT_TESTS=OFF
        -DALSOFT_UPDATE_BUILD_VERSION=OFF
        -DALSOFT_UTILS=OFF
        -DLIBTYPE=${OPENAL_LIBTYPE}
        "-DPKG_CONFIG_EXECUTABLE=${PKGCONFIG}"
        # order by CMakeLists.txt
        -DALSOFT_BACKEND_ALSA=${ALSOFT_REQUIRE_LINUX}
        -DALSOFT_REQUIRE_ALSA=${ALSOFT_REQUIRE_LINUX}
        -DALSOFT_BACKEND_OSS=OFF
        -DALSOFT_BACKEND_SOLARIS=OFF
        -DALSOFT_BACKEND_SNDIO=OFF
        -DALSOFT_BACKEND_WINMM=OFF
        -DALSOFT_BACKEND_DSOUND=${ALSOFT_REQUIRE_WINDOWS_NOT_UWP}
        -DALSOFT_REQUIRE_DSOUND=${ALSOFT_REQUIRE_WINDOWS_NOT_UWP}
        -DALSOFT_BACKEND_WASAPI=${ALSOFT_REQUIRE_WINDOWS}
        -DALSOFT_REQUIRE_WASAPI=${ALSOFT_REQUIRE_WINDOWS}
        -DALSOFT_BACKEND_JACK=OFF
        -DALSOFT_BACKEND_COREAUDIO=${ALSOFT_REQUIRE_APPLE}
        -DALSOFT_REQUIRE_COREAUDIO=${ALSOFT_REQUIRE_APPLE}
        -DALSOFT_BACKEND_OBOE=OFF
        -DALSOFT_BACKEND_OPENSL=${VCPKG_TARGET_IS_ANDROID}
        -DALSOFT_REQUIRE_OPENSL=${VCPKG_TARGET_IS_ANDROID}
        -DALSOFT_BACKEND_PORTAUDIO=OFF
        -DALSOFT_BACKEND_SDL3=OFF
        -DALSOFT_BACKEND_SDL2=OFF
        -DALSOFT_BACKEND_WAVE=ON
        -DALSOFT_BUILD_ROUTER=OFF
        -DALSOFT_RTKIT=OFF
    MAYBE_UNUSED_VARIABLES
        # NOT WIN32
        ALSOFT_BACKEND_ALSA
        ALSOFT_REQUIRE_ALSA
        ALSOFT_BACKEND_OSS
        ALSOFT_BACKEND_SOLARIS
        ALSOFT_BACKEND_SNDIO
        ALSOFT_RTKIT
        # WIN32
        ALSOFT_BUILD_ROUTER
        ALSOFT_BACKEND_WINMM
        ALSOFT_BACKEND_DSOUND
        ALSOFT_REQUIRE_DSOUND
        ALSOFT_BACKEND_WASAPI
        ALSOFT_REQUIRE_WASAPI
)

vcpkg_cmake_install()
vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(CONFIG_PATH "lib/cmake/OpenAL")
vcpkg_fixup_pkgconfig()

if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    foreach(HEADER IN ITEMS al.h alc.h)
        vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/include/AL/${HEADER}" "defined(AL_LIBTYPE_STATIC)" "1")
    endforeach()
endif()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST
    "${SOURCE_PATH}/COPYING"
    "${SOURCE_PATH}/BSD-3Clause"
    "${SOURCE_PATH}/LICENSE-pffft"
    "${SOURCE_PATH}/core/bs2b.h"
    "${SOURCE_PATH}/gsl/LICENSE"
)
