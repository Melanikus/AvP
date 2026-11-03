/**
 * @name Code CodeQL could not read
 * @description Lists every place where CodeQL's C/C++ reader hit an error, in every build mode.
 * @kind diagnostic
 * @id avp/diagnostics/read-problems
 */

/*
 * CodeQL reads C and C++ the way a compiler does. When it meets something it can't understand, it
 * carries on with the rest of the file, so what follows may not be analysed at all, and the scan
 * can still look clean. Code nobody read is a blind spot, and a blind spot must fail our gate.
 *
 * CodeQL has its own list of these problems (cpp/diagnostics/extraction-warnings), but that list
 * deliberately leaves out "build mode none", the mode that reads files without building them. Two
 * of our jobs use that mode, so they would hide exactly the problem we care about. This query is
 * the same list without that exception.
 */

import cpp

from Diagnostic problem
where
  // These three are the compiler-error levels; CodeQL's own list uses the same three.
  (
    problem instanceof CompilerDiscretionaryError or
    problem instanceof CompilerError or
    problem instanceof CompilerCatastrophe
  ) and
  // Keep only problems in files we asked to scan. One general "there was an error" message has no
  // file at all, and system headers are outside the folder we scan.
  exists(problem.getFile().getRelativePath())
select problem, "CodeQL could not read this code: " + problem.getFullMessage().replaceAll("$", "$$"), 2
