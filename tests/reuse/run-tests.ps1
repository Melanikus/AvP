# Runs every test tests/build.ps1 built, every one even after one fails, and writes down how each went. Written 2026-11-11 by Melanikus
# for the AvP port, as step 4 of the CI plan. Until then the Tests check stopped at the first test that failed; now each test's result
# is written to <work>\results.json, for record.ps1, and the step fails at the end, naming each test that didn't pass.
#
#   tests/reuse/run-tests.ps1 -Programs <folder build.ps1 built into> -Work <folder for the reuse files> [-Plan <lookup.ps1's plan>]
#
# A test a plan reuses wasn't built, and is said here with the run it really ran in, so the log shows every test either way.

param(
    [Parameter(Mandatory = $true)][string]$Programs,
    [Parameter(Mandatory = $true)][string]$Work,
    [string]$Plan = ''
)

$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force $Work | Out-Null

$tests = @(Get-ChildItem -LiteralPath $Programs -Filter '*.exe' -File | Sort-Object Name)
$reused = @()
if ($Plan -and (Test-Path -LiteralPath $Plan)) { $reused = @((Get-Content -LiteralPath $Plan -Raw | ConvertFrom-Json).reused) }
if ($tests.Count -eq 0 -and $reused.Count -eq 0) { throw 'No tests were built, and none is reused' }

$results = [ordered]@{}
foreach ($test in $tests)
{
    if ($test.BaseName -cnotmatch '^[A-Za-z0-9_]+$') { throw "$($test.Name) isn't named as a test is" }
    "Running $($test.Name)"
    $started = Get-Date
    & $test.FullName
    $code = $LASTEXITCODE
    $results[$test.BaseName] = [ordered]@{ passed = ($code -eq 0); exit = $code; minutes = [Math]::Round(((Get-Date) - $started).TotalMinutes, 2) }
    if ($code -ne 0) { "FAILED: $($test.Name) gave back $code" }
}

[System.IO.File]::WriteAllText((Join-Path $Work 'results.json'), ($results | ConvertTo-Json -Depth 4))

foreach ($one in $reused) { "Reused: $($one.test), which passed in run $($one.run) at $($one.sha), with every input the same" }
$failed = @($results.Keys | Where-Object { -not $results[$_].passed })
"$($results.Count - $failed.Count) of the $($results.Count) tests that ran here passed; $($reused.Count) reused"
if ($failed.Count -gt 0) { "These didn't pass: $($failed -join ', ')"; exit 1 }
exit 0
