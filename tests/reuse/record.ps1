# Writes the records a later run may reuse: one for each test that really ran here, with its key, every part of the key with its own
# hash, the files the compiler read for it, the program's imports, and how it went. Written 2026-11-11 by Melanikus for the AvP port, as
# step 4 of the CI plan; modified the same day after Secondary's attacker's reading (C1, C2).
#
#   record.ps1 -Root <the repository> -Programs <folder build.ps1 built into> -Work <folder for the reuse files> [-Compare]
#
# It's run from a copy of tests/reuse taken from the commit (HEAD) into a folder of its own, never from the repository's folder, which the
# tests could have written to; so it's told where the repository is. Every file it reads is read from HEAD (Reuse.psm1), and:
#   - if the run changed anything in the tree (git status), it writes no records;
#   - if a test left a process running (run-tests.ps1's "left behind"), it writes no records;
#   - if the empty test (reuse_empty_probe_test) imports anything tests/reuse/imports-allowed.txt hasn't got, no test can be class A, and
#     it says so loudly, so the list is read again;
# and the workflow keeps records only from a run on avpx_main, started by a push or the nightly schedule, and the job "Record the tests'
# results" checks them against the lists kept straight after the build before it attests them.
#
# -Compare is the nightly run's check (it reuses nothing): every test lookup.ps1 -Dry said it WOULD have reused must have passed here.
# If one didn't, the key missed something that changes a result, and this fails, naming the test, so the key can be fixed.

param(
    [Parameter(Mandatory = $true)][string]$Root,
    [Parameter(Mandatory = $true)][string]$Programs,
    [Parameter(Mandatory = $true)][string]$Work,
    [switch]$Compare
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Reuse.psm1') -Force
$results = Get-Content -LiteralPath (Join-Path $Work 'results.json') -Raw | ConvertFrom-Json

if ($Compare)
{
    $plan = Get-Content -LiteralPath (Join-Path $Work 'plan.json') -Raw | ConvertFrom-Json
    $wrong = @(foreach ($one in @($plan.wouldReuse)) {
        $fresh = $results.tests.PSObject.Properties[$one.test]
        if (-not $fresh -or -not $fresh.Value.passed) { "$($one.test) (its record from run $($one.run) said it passed; tonight it $(if ($fresh) { 'failed' } else { "didn't run" }))" }
    })
    "Tonight's run against what would have been reused: $(@($plan.wouldReuse).Count) tests would have been, and $($wrong.Count) of them didn't pass here"
    if ($wrong.Count -gt 0) { throw "A reused result would have been wrong, so a key missed something: $($wrong -join '; '). Open a card and fix the key; nothing is hidden." }
}

function Write-Records([object[]]$records, [string]$why)
{
    $file = [ordered]@{
        schema = 2
        run = [ordered]@{ id = "$env:GITHUB_RUN_ID"; attempt = "$env:GITHUB_RUN_ATTEMPT"; sha = "$env:GITHUB_SHA"; ref = "$env:GITHUB_REF"; event = "$env:GITHUB_EVENT_NAME" }
        none = $why
        records = @($records)
    }
    [System.IO.File]::WriteAllText((Join-Path $Work 'records.json'), ($file | ConvertTo-Json -Depth 8 -Compress))
}

# Nothing the tests could have changed is believed
$dirty = Get-DirtyFiles $Root
if ($dirty.Count -gt 0) { "No records: the run changed the tree ($(@($dirty | Select-Object -First 5) -join '; '))"; Write-Records @() 'the run changed the tree'; exit 0 }
$left = @($results.leftBehind)
if ($left.Count -gt 0) { "No records: a test left something running ($(@($left | Select-Object -First 3) -join '; '))"; Write-Records @() 'a test left something running'; exit 0 }

$rules = Get-ImportRules $Root
$emptyImports = Join-Path $Programs 'reuse_empty_probe_test.imports.txt'
if (Test-Path -LiteralPath $emptyImports)
{
    $floor = @([System.IO.File]::ReadAllLines($emptyImports) | Where-Object { $_ -and -not $rules.allowed.Contains($_) })
    if ($floor.Count -gt 0) { "WARNING: the empty test imports what tests/reuse/imports-allowed.txt hasn't got ($($floor -join ', ')): the compiler has changed, so no test is class A until the list is read again" }
}

$shared = Get-SharedParts $Root
$records = [System.Collections.Generic.List[object]]::new()
foreach ($property in $results.tests.PSObject.Properties)
{
    $test = $property.Name
    $includesFile = Join-Path $Programs "$test.includes.txt"
    $importsFile = Join-Path $Programs "$test.imports.txt"
    if (-not (Test-Path -LiteralPath $includesFile) -or -not (Test-Path -LiteralPath $importsFile)) { "Not recorded: $test, as the compiler's lists of what it read and what it imports aren't both there"; continue }
    $includes = [string[]]@([System.IO.File]::ReadAllLines($includesFile) | Where-Object { $_ })
    $imports = [string[]]@([System.IO.File]::ReadAllLines($importsFile) | Where-Object { $_ })
    if (@($includes | Where-Object { $_.StartsWith('outside:') }).Count -gt 0) { "Not recorded: $test read a file outside the repository, Visual Studio, the SDK and vcpkg_installed, or its list couldn't be read"; continue }
    try { $key = Get-ReuseKey $Root $test 'unit' $includes $imports $shared $rules }
    catch { "Not recorded: $test, as its key couldn't be worked out: $_"; continue }
    $records.Add([ordered]@{
        test = $test; kind = 'unit'; class = $key.class; textClass = $key.textClass; importClass = $key.importClass; key = $key.key
        parts = @($key.parts | ForEach-Object { , @([string]$_[0], [string]$_[1]) })
        includes = $includes; imports = $key.imports
        result = $(if ($property.Value.passed) { 'passed' } else { 'failed' }); minutes = $property.Value.minutes; status = 'ran'
    })
}

Write-Records $records.ToArray() ''
"$($records.Count) records written: class A $(@($records | Where-Object { $_.class -ceq 'A' }).Count), B $(@($records | Where-Object { $_.class -ceq 'B' }).Count), C $(@($records | Where-Object { $_.class -ceq 'C' }).Count) (C is never reused)"
