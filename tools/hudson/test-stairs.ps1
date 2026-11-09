<#
Proves that Hudson walks up stairs. Written 2026-11-05 by Melanikus for the AvP port.

Why this exists: the first map took stairs for walls. He went up and down the bottom of a flight for two minutes, until he was picked up and moved on. The route has a lot of stairs.
The map's own test (tests\hudson_nav_graph_test.cpp) checks what the map says about stairs of the same size. This checks what he really does on the game's real ones.

It starts the game twice, in a test copy, using run-hudson.ps1, which refuses a Steam folder. It takes a few minutes:
- stairs-test walks two whole flights.
- stair-lip-test stops him just short of one stair, ten times, a little closer each time, and has him walk over it. Which of those catches his box depends on where he happens to stop, and that's what makes a walk pass on one run and fail on the next.

  powershell -ExecutionPolicy Bypass -File tools\hudson\test-stairs.ps1 -GameFolder C:\Games\AvP-test

What it needs: Derelict in that game folder, and nothing else running. Add -WhileInUse -TimingsDontMatter to run while the machine's in use.
What passes: each gets to the end of its short route, with nobody moving him, and he hasn't walked much further than it should take. The level doesn't end there, so each run ends with exit 43, and that's as it should be.
Exit code: 0 if they both passed, 1 if not.
#>
param(
    [Parameter(Mandatory = $true)][string]$GameFolder,
    [string]$Exe = 'AvPx.exe',
    [int]$Frames = 20000,
    [int]$Seconds = 300,
    [int]$IdleSeconds = 120,
    [string]$Size = '',
    [switch]$TimingsDontMatter,
    [switch]$WhileInUse
)
$ErrorActionPreference = 'Stop'

# How far each should take, along the ground, which is how the game's log measures it. The flights are about 16 metres. Past the most, he's been going back and forth.
$cases = @(
    @{ Route = 'stairs-test'; LeastMetres = 10; MostMetres = 24 },
    @{ Route = 'stair-lip-test'; LeastMetres = 2; MostMetres = 12 }
)
$wantedEnding = " ended=he came to the end of the route, and the level didn't end exit=43"

$routes = Join-Path $GameFolder 'hudson-routes'
New-Item -ItemType Directory -Force $routes | Out-Null
$failed = 0

foreach ($case in $cases)
{
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "test-routes\$($case.Route).txt") -Destination $routes -Force

    $arguments = @('-ExecutionPolicy', 'Bypass', '-File', (Join-Path $PSScriptRoot 'run-hudson.ps1'), '-GameFolder', $GameFolder, '-Level', 'derelict', '-Frames', $Frames, '-Seconds', $Seconds, '-IdleSeconds', $IdleSeconds, '-Rooms', 1, '-Route', $case.Route, '-Exe', $Exe)
    if ($Size) { $arguments += @('-Size', $Size) }
    if ($TimingsDontMatter) { $arguments += '-TimingsDontMatter' }
    if ($WhileInUse) { $arguments += '-WhileInUse' }

    $startedAt = Get-Date
    & powershell @arguments
    $exitCode = $LASTEXITCODE

    # This run's own folder, which can't be an older one: a run that wrote nothing has to fail, not pass on what's left from before
    $kept = Get-ChildItem -LiteralPath (Join-Path $GameFolder 'runs') -Directory | Where-Object { $_.CreationTime -ge $startedAt } | Sort-Object CreationTime | Select-Object -Last 1
    $problems = @()
    $said = ''

    if ($exitCode -ne 43) { $problems += "the exit code is $exitCode, and it should be 43" }

    if (-not $kept)
    {
        $problems += 'it left no folder of its own'
    }
    else
    {
        $resultPath = Join-Path $kept.FullName 'hudson-result.txt'
        $result = if (Test-Path -LiteralPath $resultPath) { @(Get-Content -LiteralPath $resultPath) } else { @() }

        if ($result.Count -eq 0 -or -not $result[0].EndsWith($wantedEnding)) { $problems += "the result's first line doesn't end with '$wantedEnding'" }

        $line = $result | Where-Object { $_.StartsWith('nav_crossed=') } | Select-Object -First 1

        if (-not $line)
        {
            $problems += "the result has no nav_crossed line"
        }
        else
        {
            $said = $line
            if (-not $line.Contains(' nav_moved_on=0 ')) { $problems += "someone had to move him: $line" }

            if ($line -match 'nav_metres=(\d+)')
            {
                $metres = [int]$Matches[1]
                if ($metres -gt $case.MostMetres) { $problems += "he walked $metres metres, more than the $($case.MostMetres) it should take" }
                if ($metres -lt $case.LeastMetres) { $problems += "he only walked $metres metres, and it's longer than that" }
            }
            else
            {
                $problems += "the result doesn't say how far he walked: $line"
            }
        }
    }

    if ($problems.Count -eq 0)
    {
        Write-Host "test-stairs: PASSED $($case.Route): $said"
    }
    else
    {
        $failed++
        foreach ($problem in $problems) { Write-Host "test-stairs: FAILED $($case.Route): $problem" }
    }
}

if ($failed -gt 0)
{
    Write-Host "test-stairs: FAILED, $failed of $($cases.Count)"
    exit 1
}

Write-Host 'test-stairs: passed, both'
exit 0
