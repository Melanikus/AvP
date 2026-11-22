# vcpkg's own x86-windows triplet, which says how to build every library for the game, with our lines added. Written 2026-11-07 by Melanikus for the AvP port.
#
# The first four lines are vcpkg's, as its triplets/x86-windows.cmake has them. A triplet of ours replaces vcpkg's whole, so they have to be here too.
# The linker's flags are ours. Each library vcpkg builds records where its symbols (.pdb) are, and without it that's the whole folder it was built in, which on
# a developer's machine is C:\Users\<their name>\..., and would go out with the game for anyone to read. /PDBALTPATH:%_PDB% has the linker record only the
# .pdb's name. vcpkg puts VCPKG_LINKER_FLAGS into the link of every library it builds with CMake (scripts/toolchains/windows.cmake), and hands it to
# pthreads' nmake build through the _LINK_ variable (scripts/cmake/vcpkg_build_nmake.cmake). tools/check-package-imports.ps1 refuses a package that
# names any such folder.
# /Brepro (Frost 2026-11-22, Bishop 2's order; Dietrich found vcpkg linked the libraries without it): the linker takes each library's time stamp, its
# debug entries' time stamps and its .pdb's GUID from the library's own contents, not the clock, so two builds of the same commit give the same bytes
# (the game's and the editor's own programs link with it already). Measured before it: the same commit built twice on GitHub (editor PR #98's
# rebuild check and the alpha.4 release run, 2026-11-22) gave fmt.dll and openal32.dll that differed in exactly those fields and nothing else
# (the editor's tools/check-binaries.js same --allow-clock).
set(VCPKG_TARGET_ARCHITECTURE x86)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_PROVIDED_FORTRAN ON)

set(VCPKG_LINKER_FLAGS "/PDBALTPATH:%_PDB% /Brepro")

# THE ONE COMPILER (Frost 2026-11-21, Bishop for the owner after alpha.3; win32/msvc-pin.props says why): vcpkg builds every library with this
# exact MSVC, the one the game and its own DLLs are built with, never the newest a machine happens to have. It must name the same version as
# msvc-pin.props: tools/install-msvc-pin.ps1 stops a build where it doesn't.
set(VCPKG_PLATFORM_TOOLSET_VERSION "14.51.36231")
