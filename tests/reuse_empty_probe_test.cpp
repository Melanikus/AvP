// The empty test: no checks, nothing read, nothing called. Written 2026-11-11 by Melanikus for the AvP port, for the CI plan's step 4.
//
// What a test's program calls outside itself (its import table) is part of how tests/reuse decides whether its result can be reused. This
// program calls only what every test does before main and after it, so its imports are the floor every test has: tests/reuse/record.ps1
// checks, in every run, that each of them is on the list tests/reuse/imports-allowed.txt, so the list keeps up with the compiler.

int main()
{
    return 0;
}
