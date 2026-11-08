# The libraries the tests link, built with the address sanitizer, for the sanitizer's own run of every test. Nothing that ships is built
# with this. Written 2026-11-08 by Melanikus for the AvP port (card SANITIZE-RUN-1).
#
# The sanitizer watches every read and write a program makes, but only in code compiled with it. A test that links a library built the
# ordinary way runs that library's code unwatched, and ENet's code, which reads what comes in from the network, is the code that most
# needs watching. So .github/workflows/tests.yml has fetch-test-libraries build the tests' libraries with this triplet as well, into
# vcpkg_installed/x86-windows-asan, and tests/takeouts/sanitize.ps1 links a test against that copy.
#
# Everything else is the game's own triplet, read in whole here, so the two can't drift apart. That includes the C runtime as DLLs
# (VCPKG_CRT_LINKAGE dynamic), which the tests use too, with /MD. vcpkg tells one build of a library from another by a hash of the triplet
# file, and a file read in like this isn't part of it. VCPKG_HASH_ADDITIONAL_FILES adds it, so a change to the game's triplet means a new
# build here too, never an old one kept from before.
#
# Modified 2026-11-08 by Melanikus for the AvP port:
#   named x86-windows-asan, and built on the game's x86-windows, after a day as x86-windows-static-asan on the static triplet (STATIC-1).
#   Static linking is undone: Bitdefender flagged the game built that way (#111), and the user ruled it out (STATIC-UNDO-1, #125)
include("${CMAKE_CURRENT_LIST_DIR}/x86-windows.cmake")
set(VCPKG_HASH_ADDITIONAL_FILES "${CMAKE_CURRENT_LIST_DIR}/x86-windows.cmake")

# Only the release build. vcpkg's debug flags have /RTC1, the run-time checks, and the compiler refuses them with the sanitizer. The tests
# link the release build anyway.
set(VCPKG_BUILD_TYPE release)

# The sanitizer, on every C and C++ file the library is built from
set(VCPKG_C_FLAGS "/fsanitize=address")
set(VCPKG_CXX_FLAGS "/fsanitize=address")
