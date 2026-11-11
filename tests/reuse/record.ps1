# Writes the records a later run may reuse: one for each test that really ran, with its key, every part of the key with its own hash, the
# files the compiler read for it, the program's imports, and how it went. Written 2026-11-11 by Melanikus for the AvP port, as step 4 of
# the CI plan; modified the same day after Secondary's attacker's readings (C1, C2, R1).
#
#   record.ps1 -Root <a fresh checkout of the commit> -Lists <the lists kept before the tests> -Results <the tests' results.json>
#              -Work <folder for records.json>
#
# WHERE THIS RUNS: in a job of its own, "Work the tests' records out", on a machine no test has run on (PO, after Secondary's R1: anything
# run after the tests on their own machine can be steered by them). It has a fresh checkout of the commit, runs with -NoProfile, and is
# given only data from the Unit tests job:
#   - the lists kept straight after the build, before any test ran: what each test's compiler read (<test>.includes.txt), what its
#     program imports (<test>.imports.txt), and the machine's words (machine.txt);
#   - the tests' results.json: which passed, and anything left running.
# A test runs with every right on its machine, so one test could make ANY test's result in its job say "passed" (Secondary's T2). So
# every key from this job also holds every test program that ran in it (Reuse.psm1, "ran-with"): an edit to any of them, and none of
# this job's results is reused, so reuse adds nothing a test couldn't do without it. Every file is read from HEAD (Reuse.psm1), and
# HEAD must be the commit the workflow is for.
#   - if a test left a process running (run-tests.ps1's "left behind"), it writes no records;
#   - if the empty test (reuse_empty_probe_test) imports anything tests/reuse/imports-allowed.txt hasn't got, no test can be class A, and
#     it says so loudly, so the list is read again.

param(
    [Parameter(Mandatory = $true)][string]$Root,
    [Parameter(Mandatory = $true)][string]$Lists,
    [Parameter(Mandatory = $true)][string]$Results,
    [Parameter(Mandatory = $true)][string]$Work
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Reuse.psm1') -Force
New-Item -ItemType Directory -Force $Work | Out-Null

function Write-Records([object[]]$records, [string]$why)
{
    $file = [ordered]@{
        schema = 4
        run = [ordered]@{ id = "$env:GITHUB_RUN_ID"; attempt = "$env:GITHUB_RUN_ATTEMPT"; sha = "$env:GITHUB_SHA"; ref = "$env:GITHUB_REF"; event = "$env:GITHUB_EVENT_NAME" }
        none = $why
        records = @($records)
    }
    [System.IO.File]::WriteAllText((Join-Path $Work 'records.json'), ($file | ConvertTo-Json -Depth 8 -Compress))
}

$head = "$(& git -C $Root rev-parse HEAD 2>$null)".Trim()
if ($head -cne "$env:GITHUB_SHA") { throw "The checkout is at $head, not at the commit this run is for ($env:GITHUB_SHA)" }
# (PowerShell reads $Results and $results as one name, so what it holds is called $outcome)
$outcome = Get-Content -LiteralPath $Results -Raw | ConvertFrom-Json
$left = @($outcome.leftBehind | Where-Object { $_ })
if ($left.Count -gt 0) { "No records: a test left something running ($(@($left | Select-Object -First 3) -join '; '))"; Write-Records @() 'a test left something running'; exit 0 }

$machine = ([System.IO.File]::ReadAllText((Join-Path $Lists 'machine.txt'))).Trim()
$rules = Get-ImportRules $Root
$emptyImports = Join-Path $Lists 'reuse_empty_probe_test.imports.txt'
if (Test-Path -LiteralPath $emptyImports)
{
    $floor = @([System.IO.File]::ReadAllLines($emptyImports) | Where-Object { $_ -and -not $rules.allowed.Contains($_) })
    if ($floor.Count -gt 0) { "WARNING: the empty test imports what tests/reuse/imports-allowed.txt hasn't got ($($floor -join ', ')): the compiler has changed, so no test is class A until the list is read again" }
}

$shared = Get-SharedParts $Root $machine
# For each key's "ran-with" part: every test the commit has, from this fresh checkout. That holds every test that ran in the job, and
# unlike results.json no test can change it (one could leave itself out of results.json). A name in the results that isn't one of the
# commit's tests means the results aren't the runner's, and nothing is recorded.
$ranWith = Get-AllTests $Root
$strangers = @($outcome.tests.PSObject.Properties | ForEach-Object { $_.Name } | Where-Object { $ranWith -cnotcontains $_ })
if ($strangers.Count -gt 0) { "No records: the results name what isn't one of this commit's tests"; Write-Records @() 'the results name what is not one of the commit tests'; exit 0 }
$records = [System.Collections.Generic.List[object]]::new()
foreach ($property in $outcome.tests.PSObject.Properties)
{
    $test = $property.Name   # (every name was checked above)
    # Only a true or false that agrees with the program's exit code, and a number of minutes from 0 to 600, are believed (Secondary's T4)
    $passed = $property.Value.passed
    $exitCode = $property.Value.exit
    $minutes = $property.Value.minutes
    if ($passed -isnot [bool] -or -not ($exitCode -is [int] -or $exitCode -is [long]) -or $passed -ne ($exitCode -eq 0) -or -not (Test-IsMinutes $minutes)) { "Not recorded: $test, as its result isn't a true or false that agrees with its exit code, and a number of minutes"; continue }
    $includesFile = Join-Path $Lists "$test.includes.txt"
    $importsFile = Join-Path $Lists "$test.imports.txt"
    if (-not (Test-Path -LiteralPath $includesFile) -or -not (Test-Path -LiteralPath $importsFile)) { "Not recorded: $test, as the lists of what it read and what it imports weren't both kept before the tests ran"; continue }
    $includes = [string[]]@([System.IO.File]::ReadAllLines($includesFile) | ForEach-Object { $_.Trim() } | Where-Object { $_ })
    $imports = [string[]]@([System.IO.File]::ReadAllLines($importsFile) | ForEach-Object { $_.Trim() } | Where-Object { $_ })
    if (@($includes | Where-Object { $_.StartsWith('outside:') }).Count -gt 0) { "Not recorded: $test read a file outside the repository, Visual Studio, the SDK and vcpkg_installed, or its list couldn't be read"; continue }
    try { $key = Get-ReuseKey $Root $test 'unit' $includes $imports $shared $rules $ranWith }
    catch { "Not recorded: $test, as its key couldn't be worked out: $_"; continue }
    $records.Add([ordered]@{
        test = $test; kind = 'unit'; class = $key.class; textClass = $key.textClass; importClass = $key.importClass; key = $key.key
        parts = @($key.parts | ForEach-Object { , @([string]$_[0], [string]$_[1]) })
        includes = $includes; imports = $key.imports; ranWith = $key.ranWith
        result = $(if ($passed) { 'passed' } else { 'failed' }); minutes = [double]$minutes; status = 'ran'
    })
}

Write-Records $records.ToArray() ''
"$($records.Count) records written: class A $(@($records | Where-Object { $_.class -ceq 'A' }).Count), B $(@($records | Where-Object { $_.class -ceq 'B' }).Count), C $(@($records | Where-Object { $_.class -ceq 'C' }).Count) (C is never reused)"
