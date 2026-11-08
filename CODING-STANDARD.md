# How we write code here

This file is for anyone who changes this code, now or decades from now, person or program. The game is from 1999, and much of
how it worked lived in its authors' heads. We want the next person to find everything they need in the repository itself.

**Every rule here says why it exists and what enforces it.** A rule a machine checks is listed with that check. A rule only a
reviewer can check says so plainly, because a rule nothing enforces gets forgotten. **When you add a rule,** give it the next
number, a plain reason and what enforces it, in the same pull request as the change that taught us the rule.

This is the start of the standard. More rules come as we find them, and the whole tree gets one mechanical formatting pass
later, after the hardening work.

## The rules

### 1. Put `const` first for an enum type, and for a type from inside a template

**Why:** Snyk Code, one of the scanners every change has to pass, can't read a declaration that has the word `enum` and then a
`const` after the type's name. The compiler accepts it, so nothing else would tell you: the scanner just skips the whole file, and
a file it skips is a file nobody checked for security holes.

In C++ the word `enum` isn't needed in front of an enum's name, and `const` reads naturally first, so the plain way is also the
shorter one:

```cpp
const WEAPON_SLOT last = static_cast<WEAPON_SLOT>(MAX_NO_OF_WEAPON_SLOTS - 1);   // yes
enum WEAPON_SLOT const last = (enum WEAPON_SLOT)(MAX_NO_OF_WEAPON_SLOTS - 1);   // no: Snyk Code can't read this file
```

The cast `(enum WEAPON_SLOT)-1` and `static AvP_GameStats const zero = {};` were tested too, and Snyk Code reads both. Only the
declaration form is the problem.

**How we found it:** in November 2026 one test file couldn't be read. Running the scan on copies with parts left out narrowed
it down to one file, then one part of it, then this one form.

**The second form,** found the same month: a type named from inside a template, written with the template's `<...>` in front
of it, such as a map's `std::map<...>::iterator` or `::const_iterator`, with `const` after it. Snyk Code skips a file with that too.
(A name like `std::string::size_type`, with no `<...>` written, is read fine.) Write `const` first, or leave it out:

```cpp
const auto found = pictures.find(name);                                              // yes
std::map<std::string, Picture>::const_iterator found = pictures.find(name);          // yes
std::map<std::string, Picture>::const_iterator const found = pictures.find(name);    // no: Snyk Code can't read this file
```

**How we know:** pull request #56 failed its scan with 650 of 651 C and C++ files read. The one change after that was a single
line in `tests/net_start_test.cpp`, `std::map<...>::iterator const held = ...` written as `const auto held = ...`, and the next scan
read all 651 (commit `93574994`). #58 had hit the same form earlier (`custom_maps_test`, commit `50280bf0`), but that fix changed
two other forms at the same time, so it only agrees with this.

**What doesn't need it:** `const` after a plain type, a pointer or a template itself is read fine, and the repository is full of
it: hundreds of lines such as `size_t const nBytesAsked`, `char *const keyName`, `std::vector<int> const numbers` and
`std::string::size_type const tests`.
Only the two forms above are known to stop the scan, so the rule is only about them.

**What enforces it:** two checks, and a pull request has to pass both.

- The step "Refuse the forms Snyk Code can't read (CODING-STANDARD.md, rule 1)", in the job "Release checks on Ubuntu" in
  `.github/workflows/tests.yml`. It reads every C and C++ file in the repository, line by line, and names each line written
  either way, with its file and line number. It reads lines, not code, so a comment written this way is refused too (say it
  another way), and a declaration split over two lines isn't seen.
- The step "Fail if Snyk Code skipped any C/C++ file" in `.github/workflows/snyk.yml`. It counts the C and C++ files in the
  repository and the files Snyk Code read, and fails if they differ. It catches a file the first check misses, and anything
  else Snyk Code can't read, but it doesn't say which file it was.
