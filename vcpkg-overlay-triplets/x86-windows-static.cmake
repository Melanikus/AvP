# How every library the game uses is built: for 32-bit Windows, with the C runtime built in, so a player needs no Visual C++ install.
# Written 2026-11-08 by Melanikus for the AvP port (card STATIC-1).
#
# "Static" here means two things, and vcpkg asks for each one by itself:
# - VCPKG_CRT_LINKAGE static: Microsoft's C and C++ runtime goes inside each program and library (the /MT switch), rather than coming from
#   vcruntime140.dll and msvcp140.dll, which a player would have to install.
# - VCPKG_LIBRARY_LINKAGE static: a library goes inside the game itself (a .lib), rather than beside it as a .dll.
# The first lines are vcpkg's own, as its triplets/x86-windows-static.cmake has them. A triplet of ours replaces vcpkg's whole.
set(VCPKG_TARGET_ARCHITECTURE x86)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)

# Two libraries stay beside the game as DLLs, each for its own reason. vcpkg reads this file once for each library, with PORT set to its name.
# - OpenAL Soft, the sound: its licence is the GNU Library GPL, which lets a player swap the library for a build of their own, and that
#   needs it to be a file of its own. It still has the C runtime built in, so it needs no Visual C++ install either.
# - Microsoft's D3DX (dxsdk-d3dx): these are Microsoft's own DLLs, copied as they come. There's nothing to build, so they're left as they are.
if(PORT STREQUAL "openal-soft")
    set(VCPKG_LIBRARY_LINKAGE dynamic)
endif()

if(PORT STREQUAL "dxsdk-d3dx")
    set(VCPKG_CRT_LINKAGE dynamic)
    set(VCPKG_LIBRARY_LINKAGE dynamic)
endif()

# Each library vcpkg links records where its symbols (.pdb) are, and without this that's the whole folder it was built in, which on a
# developer's machine is C:\Users\<their name>\..., and would go out with the game for anyone to read. /PDBALTPATH:%_PDB% has the linker
# record only the .pdb's name. With static libraries, only OpenAL Soft's DLL is linked here. The game's own link sets the same switch.
# tools/check-package-imports.ps1 refuses a package that names any such folder.
set(VCPKG_LINKER_FLAGS "/PDBALTPATH:%_PDB%")
