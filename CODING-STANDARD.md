# How we write code here

This file is for anyone who changes this code, now or decades from now, person or program. The game is from 1999, and much of
how it worked lived in its authors' heads. We want the next person to find everything they need in the repository itself.

**Every rule here says why it exists and what enforces it.** A rule a machine checks is listed with that check. A rule only a
reviewer can check says so plainly, because a rule nothing enforces gets forgotten. **When you add a rule,** give it the next
number, a plain reason and what enforces it, in the same pull request as the change that taught us the rule.

This is the start of the standard. More rules come as we find them, and the whole tree gets one mechanical formatting pass
later, after the hardening work.

## The rules

### 1. Write `const WEAPON_SLOT slot = ...`, not `enum WEAPON_SLOT const slot = ...`

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

**What enforces it:** the step "Fail if Snyk Code skipped any C/C++ file" in `.github/workflows/snyk.yml`. It counts the C and
C++ files in the repository and the files Snyk Code read, and fails if they differ. So a file written this way fails that check
and can't be merged. The check doesn't say which file it was, which is why this rule is written down.
