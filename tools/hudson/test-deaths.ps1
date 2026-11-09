<#
Proves that a death ends one of Hudson's walks, and says so, and that a rescue doesn't cause one. Written 2026-11-05 by Melanikus for the AvP port.

Why this exists: in one run the player died falling off a high walkway, and the marine, who can't be hurt in a run, went on walking with the
dead player's camera behind him. That run reported "it finished". So each death now has its own reason and exit code, and this script makes
each one happen on purpose and checks for those exact words and that exact code. A run that ended any other way fails, however well it went.

It starts the game three times, in a test copy, using run-hudson.ps1, which refuses a Steam folder. Each run takes a minute or two:

  powershell -ExecutionPolicy Bypass -File tools\hudson\test-deaths.ps1 -GameFolder C:\Games\AvP-test

What it needs: Derelict in that game folder, and nothing else running. Add -WhileInUse -TimingsDontMatter to run while the machine's in use.
What it leaves: the routes it uses, in hudson-routes beside the game, and each run's folder in runs\ as usual.
Exit code: 0 if they all ended exactly as they should, 1 if not.
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

# What makes each death happen, and what the game has to say about it. These words are the ones in hudson_nav_plan.cpp, and tests\hudson_nav_test.cpp holds them there.
$cases = @(
    @{ Route = 'fall-test'; Ended = 'he fell further than a marine can survive'; Exit = 41; Log = 'HE FELL TOO FAR' },
    @{ Route = 'player-dies-test'; Ended = 'the player died'; Exit = 42; Log = 'THE PLAYER DIED' },
    @{ Route = 'rescue-test'; Ended = "he came to the end of the route, and the level didn't end"; Exit = 43; Log = 'IN THE AIR'; Also = ' nav_moved_on=0 ' }
)

$routes = Join-Path $GameFolder 'hudson-routes'
New-Item -ItemType Directory -Force $routes | Out-Null
$failed = 0

foreach ($case in $cases)
{
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "test-routes\$($case.Route).txt") -Destination $routes -Force

    $arguments = @('-ExecutionPolicy', 'Bypass', '-File', (Join-Path $PSScriptRoot 'run-hudson.ps1'), '-GameFolder', $GameFolder, '-Level', 'derelict', '-Frames', $Frames, '-Seconds', $Seconds, '-IdleSeconds', $IdleSeconds, '-Rooms', 1, '-Nav', 1000, '-Route', $case.Route, '-Exe', $Exe)
    if ($Size) { $arguments += @('-Size', $Size) }
    if ($TimingsDontMatter) { $arguments += '-TimingsDontMatter' }
    if ($WhileInUse) { $arguments += '-WhileInUse' }

    $startedAt = Get-Date
    & powershell @arguments
    $exitCode = $LASTEXITCODE

    # This run's own folder, which can't be an older one: a run that wrote nothing has to fail, not pass on what's left from before
    $kept = Get-ChildItem -LiteralPath (Join-Path $GameFolder 'runs') -Directory | Where-Object { $_.CreationTime -ge $startedAt } | Sort-Object CreationTime | Select-Object -Last 1
    $problems = @()

    if ($exitCode -ne $case.Exit) { $problems += "the exit code is $exitCode, and it should be $($case.Exit)" }

    if (-not $kept)
    {
        $problems += 'it left no folder of its own'
    }
    else
    {
        $resultPath = Join-Path $kept.FullName 'hudson-result.txt'
        $logPath = Join-Path $kept.FullName 'avp_log.txt'
        $result = if (Test-Path -LiteralPath $resultPath) { @(Get-Content -LiteralPath $resultPath) } else { @() }
        $wanted = " ended=$($case.Ended) exit=$($case.Exit)"

        if ($result.Count -eq 0 -or -not $result[0].EndsWith($wanted)) { $problems += "the result's first line doesn't end with '$wanted'" }
        if (-not ($result | Where-Object { $_.Contains('nav_crossed=no') })) { $problems += "the result doesn't say nav_crossed=no" }
        if ($case.Also -and -not ($result | Where-Object { $_.StartsWith('nav_crossed=') -and $_.Contains($case.Also) })) { $problems += "the result's nav_crossed line doesn't say '$($case.Also.Trim())'" }
        if (-not (Test-Path -LiteralPath $logPath) -or -not (Select-String -LiteralPath $logPath -SimpleMatch $case.Log -Quiet)) { $problems += "the log doesn't have '$($case.Log)'" }
    }

    if ($problems.Count -eq 0)
    {
        Write-Host "test-deaths: PASSED $($case.Route): it ended '$($case.Ended)', exit $($case.Exit)"
    }
    else
    {
        $failed++
        foreach ($problem in $problems) { Write-Host "test-deaths: FAILED $($case.Route): $problem" }
    }
}

if ($failed -gt 0)
{
    Write-Host "test-deaths: FAILED, $failed of $($cases.Count)"
    exit 1
}

Write-Host "test-deaths: passed, each one ended the run with its own reason"
exit 0
