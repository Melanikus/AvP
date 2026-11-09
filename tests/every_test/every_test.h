// Read first by every C++ file of every test. Written 2026-11-08 by Melanikus for the AvP port (#140, card STL-HARDEN-1), from the
// same guard on the unmerged branch stl-harden (28767cc3, 2026-11-07).
//
// Every test's settings (tests/*.args) say /Itests/every_test /FIevery_test.h, which makes the compiler read this file ahead of each of
// the test's source files, and /D_MSVC_STL_HARDENING=1, which turns on the standard library's own bounds checks. tests/build.ps1
// refuses to build a test whose settings don't say all three.
//
// THE STANDARD LIBRARY'S BOUNDS CHECKS. With them on, looking past the end of a std::vector, a std::string or another of the standard
// library's containers stops the program at once, with exit code 0xC0000409. Without them, it reads whatever is in memory there, and a
// test's check can pass by luck. That's how a take-out row went unnoticed on GitHub's machine: texture_relief_test row 38 took out a
// bound, the test looked one place past the end, and the memory there happened to be zero, which read as "nothing here". So a test
// refuses to build if the checks aren't on, rather than pass quietly. That also catches a setting that turns them off again later on the
// same line, and a compiler too old to know them. tests/stl_bounds_canary_test proves, by running, that they really stop a program.
//
// C files skip this: they don't use the standard library's containers.

#pragma once

#ifdef __cplusplus

#include <vector>   // brings in the standard library's own settings, which say whether its checks are on

#if !defined(_MSVC_STL_HARDENING_VECTOR) || !_MSVC_STL_HARDENING_VECTOR
#error "Every test is built with the standard library's bounds checks: its .args needs /D_MSVC_STL_HARDENING=1 (tests/every_test/every_test.h says why)"
#endif

#endif
