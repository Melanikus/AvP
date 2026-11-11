# Runs every test tests/build.ps1 built, every one even after one fails, and writes down how each went. Written 2026-11-11 by Melanikus
# for the AvP port, as step 4 of the CI plan. Until then the Tests check stopped at the first test that failed; now each test's result
# is written to <work>\results.json, for record.ps1, and the step fails at the end, naming each test that didn't pass.
# Modified the same day after Secondary's attacker's reading (C1): after each test, every process it started and left behind is stopped,
# and at the end anything still running from the tests' folder or the repository is written down as left behind. A run with anything
# left behind writes no records (record.ps1), as something may have changed what the records are made from.
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

# Every process whose parent is one of these, and theirs, as far down as it goes (Windows keeps a child's parent's number after the
# parent has ended, so this finds what a finished test left behind)
function Get-Descendants([int[]]$parents)
{
    $all = @(Get-CimInstance Win32_Process | Select-Object ProcessId, ParentProcessId, ExecutablePath, CreationDate)
    $found = [System.Collections.Generic.HashSet[int]]::new()
    $front = [System.Collections.Generic.Queue[int]]::new()
    foreach ($p in $parents) { $front.Enqueue($p) }
    while ($front.Count -gt 0)
    {
        $parent = $front.Dequeue()
        foreach ($child in @($all | Where-Object { $_.ParentProcessId -eq $parent -and $_.ProcessId -ne $PID }))
        {
            if ($found.Add([int]$child.ProcessId)) { $front.Enqueue([int]$child.ProcessId) }
        }
    }
    return [int[]]@($found)
}

$started = Get-Date
$results = [ordered]@{}
foreach ($test in $tests)
{
    if ($test.BaseName -cnotmatch '^[A-Za-z0-9_]+$') { throw "$($test.Name) isn't named as a test is" }
    "Running $($test.Name)"
    $began = Get-Date
    $process = Start-Process -FilePath $test.FullName -NoNewWindow -PassThru
    $handle = $process.Handle   # without asking for this, Windows PowerShell can't read the exit code afterwards
    $process.WaitForExit()
    $code = $process.ExitCode
    # Whatever it started and left running is stopped now, before the next test
    foreach ($left in (Get-Descendants @($process.Id)))
    {
        "  stopping process $left, which $($test.Name) started and left running"
        Stop-Process -Id $left -Force -ErrorAction SilentlyContinue
    }
    $results[$test.BaseName] = [ordered]@{ passed = ($code -eq 0); exit = $code; minutes = [Math]::Round(((Get-Date) - $began).TotalMinutes, 2) }
    if ($code -ne 0) { "FAILED: $($test.Name) gave back $code" }
}

# Anything still running that came from the tests' folder or the repository, started since the tests began
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$folders = @([System.IO.Path]::GetFullPath($Programs).TrimEnd('\') + '\', [System.IO.Path]::GetFullPath($root).TrimEnd('\') + '\')
$leftBehind = @(Get-CimInstance Win32_Process |
    Where-Object { $_.ProcessId -ne $PID -and $_.CreationDate -and $_.CreationDate -ge $started -and $_.ExecutablePath } |
    Where-Object { $exe = "$($_.ExecutablePath)"; @($folders | Where-Object { $exe.StartsWith($_, [StringComparison]::OrdinalIgnoreCase) }).Count -gt 0 } |
    ForEach-Object { "$($_.ProcessId) $($_.ExecutablePath)" })
foreach ($left in $leftBehind) { "LEFT BEHIND: $left" }

[System.IO.File]::WriteAllText((Join-Path $Work 'results.json'), ([ordered]@{ tests = $results; leftBehind = $leftBehind } | ConvertTo-Json -Depth 5))

foreach ($one in $reused) { "Reused: $($one.test), which passed in run $($one.run) at $($one.sha), with every input the same" }
$failed = @($results.Keys | Where-Object { -not $results[$_].passed })
"$($results.Count - $failed.Count) of the $($results.Count) tests that ran here passed; $($reused.Count) reused"
if ($failed.Count -gt 0) { "These didn't pass: $($failed -join ', ')"; exit 1 }
exit 0
