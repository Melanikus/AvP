# vcpkg's own x86-windows triplet, which says how to build every library for the game, with one line added. Written 2026-11-07 by Melanikus for the AvP port.
#
# The first four lines are vcpkg's, as its triplets/x86-windows.cmake has them. A triplet of ours replaces vcpkg's whole, so they have to be here too.
# The last line is ours. Each library vcpkg builds records where its symbols (.pdb) are, and without it that's the whole folder it was built in, which on
# a developer's machine is C:\Users\<their name>\..., and would go out with the game for anyone to read. /PDBALTPATH:%_PDB% has the linker record only the
# .pdb's name. vcpkg puts VCPKG_LINKER_FLAGS into the link of every library it builds with CMake (scripts/toolchains/windows.cmake), and hands it to
# pthreads' nmake build through the _LINK_ variable (scripts/cmake/vcpkg_build_nmake.cmake). tools/check-package-imports.ps1 refuses a package that
# names any such folder.
set(VCPKG_TARGET_ARCHITECTURE x86)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_PROVIDED_FORTRAN ON)

set(VCPKG_LINKER_FLAGS "/PDBALTPATH:%_PDB%")
