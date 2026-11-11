# The nightly run's check, which reuses nothing: every test lookup.ps1 -Dry said it WOULD have reused must have passed tonight. If one
# didn't, the key missed something that changes a result, and this fails, naming the test, so the key can be fixed: a card, and nothing
# hidden. Written 2026-11-11 by Melanikus for the AvP port, as step 4 of the CI plan (it was record.ps1 -Compare until the records moved to
# a job of their own). It only reads, and can only fail.
#
#   compare.ps1 -Plan <lookup.ps1's plan.json> -Results <run-tests.ps1's results.json>
#
# WHERE THIS RUNS: in "Work the tests' records out", on a machine no test has run on, from its fresh checkout (PO, after Secondary's T3:
# a script run on the tests' own machine after them could be steered by them). The plan is kept with the lists, before any test runs.
# The results still come from the tests' machine, so a test could make them say it passed (Secondary's T2): this check finds a key that
# missed an input, not a test that lies.

param(
    [Parameter(Mandatory = $true)][string]$Plan,
    [Parameter(Mandatory = $true)][string]$Results
)

$ErrorActionPreference = 'Stop'
# (PowerShell reads $Plan and $plan, and $Results and $results, as one name each, so what they hold has other names)
$planned = Get-Content -LiteralPath $Plan -Raw | ConvertFrom-Json
$outcome = if (Test-Path -LiteralPath $Results) { Get-Content -LiteralPath $Results -Raw | ConvertFrom-Json } else { $null }
$wrong = @(foreach ($one in @($planned.wouldReuse | Where-Object { $_ })) {
    $fresh = if ($outcome) { $outcome.tests.PSObject.Properties["$($one.test)"] } else { $null }
    # Only a true is a pass: not the word "True", and not a number (Secondary's T4)
    if (-not $fresh -or $fresh.Value.passed -isnot [bool] -or -not $fresh.Value.passed) { "$($one.test) (its record from run $($one.run) said it passed; tonight it $(if ($fresh) { 'failed' } else { "didn't run" }))" }
})
"Tonight's run against what would have been reused: $(@($planned.wouldReuse | Where-Object { $_ }).Count) tests would have been, and $($wrong.Count) of them didn't pass here"
if ($wrong.Count -gt 0) { throw "A reused result would have been wrong, so a key missed something: $($wrong -join '; '). Open a card and fix the key; nothing is hidden." }
