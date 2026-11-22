# vcpkg's own x64-windows triplet, the HOST triplet, with one line added. Written 2026-11-22 by Melanikus (Frost) for the AvP port, Bishop 2's order.
#
# vcpkg builds the tools it runs during a build (vcpkg-cmake, vcpkg-cmake-config, and any port's build-time helper) for the machine that builds,
# x64, with this triplet; nothing built with it ships (the game, the editor and their libraries are 32-bit: x86-windows.cmake). vcpkg-configuration.json
# names this folder as overlay-triplets, so this file takes the place of vcpkg's own. The first four lines are vcpkg's, as its triplets/x64-windows.cmake
# has them (a triplet of ours replaces vcpkg's whole, so they have to be here too).
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_PROVIDED_FORTRAN ON)

# THE ONE COMPILER for the host too (win32/msvc-pin.props says why there is one): without this line vcpkg took the newest MSVC the machine has for the
# host triplet. The alpha.4 release build's log (2026-11-22): "Detecting compiler hash for triplet x86-windows... Compiler found:
# ...\MSVC\14.51.36231\bin\Hostx64\x86\cl.exe", then "Detecting compiler hash for triplet x64-windows... Compiler found: ...\MSVC\14.52.37025\bin\
# Hostx64\x64\cl.exe", the image's default. It must name the same version as msvc-pin.props: tools/install-msvc-pin.ps1 stops a build where it doesn't.
set(VCPKG_PLATFORM_TOOLSET_VERSION "14.51.36231")
