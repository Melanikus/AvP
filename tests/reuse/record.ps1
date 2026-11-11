# Writes the records a later run may reuse: one for each test that really ran here, with its key, every part of the key with its own
# hash, the files the compiler read for it, and how it went. Written 2026-11-11 by Melanikus for the AvP port, as step 4 of the CI plan.
#
#   tests/reuse/record.ps1 -Programs <folder build.ps1 built into> -Work <folder for the reuse files> [-Compare]
#
# The records go in <work>\records.json, which the workflow keeps as an artifact. They're only believed later once a job that runs none
# of this code has attested them (tests.yml, "Record the tests' results"), and lookup.ps1 has checked that, and worked every key out
# again. A test whose result was reused here isn't recorded: only a result where the test really ran is ever a source.
#
# -Compare is the nightly run's check (it reuses nothing): every test lookup.ps1 -Dry said it WOULD have reused must have passed here.
# If one didn't, the key missed something that changes a result, and this fails, naming the test, so the key can be fixed.

param(
    [Parameter(Mandatory = $true)][string]$Programs,
    [Parameter(Mandatory = $true)][string]$Work,
    [switch]$Compare
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Reuse.psm1') -Force
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$results = Get-Content -LiteralPath (Join-Path $Work 'results.json') -Raw | ConvertFrom-Json

if ($Compare)
{
    $plan = Get-Content -LiteralPath (Join-Path $Work 'plan.json') -Raw | ConvertFrom-Json
    $wrong = @(foreach ($one in @($plan.wouldReuse)) {
        $fresh = $results.PSObject.Properties[$one.test]
        if (-not $fresh -or -not $fresh.Value.passed) { "$($one.test) (its record from run $($one.run) said it passed; tonight it $(if ($fresh) { 'failed' } else { "didn't run" }))" }
    })
    "Tonight's run against what would have been reused: $(@($plan.wouldReuse).Count) tests would have been, and $($wrong.Count) of them didn't pass here"
    if ($wrong.Count -gt 0) { throw "A reused result would have been wrong, so a key missed something: $($wrong -join '; '). Open a card and fix the key; nothing is hidden." }
}

$shared = Get-SharedParts $root
$records = [System.Collections.Generic.List[object]]::new()
foreach ($property in $results.PSObject.Properties)
{
    $test = $property.Name
    $includesFile = Join-Path $Programs "$test.includes.txt"
    if (-not (Test-Path -LiteralPath $includesFile)) { "Not recorded: $test, as the compiler's list of what it read isn't there"; continue }
    $includes = [string[]]@([System.IO.File]::ReadAllLines($includesFile) | Where-Object { $_ })
    if (@($includes | Where-Object { $_.StartsWith('outside:') }).Count -gt 0) { "Not recorded: $test read a file outside the repository, Visual Studio, the SDK and vcpkg_installed"; continue }
    try { $key = Get-ReuseKey $root $test 'unit' $includes $shared }
    catch { "Not recorded: $test, as its key couldn't be worked out: $_"; continue }
    $records.Add([ordered]@{
        test = $test; kind = 'unit'; class = $key.class; key = $key.key
        parts = @($key.parts | ForEach-Object { , @([string]$_[0], [string]$_[1]) })
        includes = $includes
        result = $(if ($property.Value.passed) { 'passed' } else { 'failed' }); minutes = $property.Value.minutes; status = 'ran'
    })
}

$file = [ordered]@{
    schema = 1
    run = [ordered]@{ id = "$env:GITHUB_RUN_ID"; attempt = "$env:GITHUB_RUN_ATTEMPT"; sha = "$env:GITHUB_SHA"; ref = "$env:GITHUB_REF"; event = "$env:GITHUB_EVENT_NAME" }
    machine = $shared.machineSays
    records = $records
}
[System.IO.File]::WriteAllText((Join-Path $Work 'records.json'), ($file | ConvertTo-Json -Depth 8 -Compress))
"$($records.Count) records written: class A $(@($records | Where-Object { $_.class -ceq 'A' }).Count), B $(@($records | Where-Object { $_.class -ceq 'B' }).Count), C $(@($records | Where-Object { $_.class -ceq 'C' }).Count) (C is never reused)"
