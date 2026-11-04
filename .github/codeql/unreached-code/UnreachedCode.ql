/**
 * @name Code no configuration compiled
 * @description Lists every #if, #ifdef, #elif or #else whose code no compile of the game ever used.
 * @kind diagnostic
 * @id avp/diagnostics/unreached-code
 */

/*
 * CodeQL only reads the code a compiler really compiles. Text inside "#if 0", or behind a switch
 * nobody turns on, is skipped, so the file looks scanned while that part of it was never read.
 * This query lists those places, using the real compiles of every configuration: a branch counts
 * as reached if even one compile used it.
 *
 * It doesn't look inside a branch that was already skipped, because nested ones were never tested
 * at all, so each dead block is listed once, at its outermost line.
 */

import cpp

/** Holds if `branch` was tested at least once and was false every time. */
predicate neverTaken(PreprocessorBranch branch) { branch.wasNotTaken() and not branch.wasTaken() }

/**
 * Holds if `e` can never be used because a branch before it in the same chain was true every time,
 * such as the #else of an "#if 1". An #else has no result of its own to check, so this asks the
 * branches before it.
 */
predicate elseNeverTaken(PreprocessorElse e) {
  exists(PreprocessorBranch earlier |
    earlier.getIf() = e.getIf() and
    earlier.getLocation().getStartLine() < e.getLocation().getStartLine() and
    earlier.wasTaken() and
    not earlier.wasNotTaken()
  )
}

/** Gets how many lines lie between `directive` and the next directive of its chain. */
int linesAfter(PreprocessorBranchDirective directive) {
  result = directive.getNext().getLocation().getStartLine() - directive.getLocation().getStartLine() - 1
}

from PreprocessorBranchDirective directive
where
  exists(directive.getFile().getRelativePath()) and
  (neverTaken(directive) or elseNeverTaken(directive))
select directive,
  "No configuration compiled the " + linesAfter(directive).toString() + " lines after " +
    directive.toString().replaceAll("$", "$$"), 1
