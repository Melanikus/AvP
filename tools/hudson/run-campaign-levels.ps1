<#
Compares each single player level with the same level in the mode that plays as single player does.

For each level it makes two runs, one at a time. One is real single player, as that level's species. The other
is a skirmish started the way the level row does it (listplace=). Each run's log says what the player meets when
the level starts: the file, the species, the start, the room, health and armour, every gun and all its ammunition,
and how many of each kind of thing the level has. Then he dies, a key is pressed, and it says the same again once
the level has started again. A lone player in the mode should go through exactly what single player does. This
writes out which of those lines differ, level by level. "Exactly the same" means every level has nothing listed
under it.

  powershell -ExecutionPolicy Bypass -File tools\hudson\run-campaign-levels.ps1 -GameFolder C:\Games\AvP-test -Output C:\somewhere\campaign-levels.txt

  -Levels derelict,invasion_p       only these
  -SkipSinglePlayer                 only the skirmish runs, when all that's needed is that the levels start
  -WhileInUse                       the runs don't wait for the machine to be idle (machine=shared)
  -Difficulty hard                  easy, medium or hard, in both kinds of run. Medium if not asked.
  -Fps 120                          no run goes faster than this many frames a second. 60 if not asked.
  -Uncapped                         as fast as they can, by name.

It starts games, so it won't start one without a claude process, or with the game already running, and it checks
that again before each one. That way nothing is left starting games with nobody here. Delete nothing by hand while
it runs: each run keeps its own folder under the game folder's runs.
#>
param(
    [Parameter(Mandatory = $true)][string]$GameFolder,
    [Parameter(Mandatory = $true)][string]$Output,
    [string[]]$Levels = @(),
    [string]$Exe = 'AvPx.exe',
    [int]$Frames = 100000,   # a run ends by itself once the level has started again, so this only has to be plenty
    [int]$Seconds = 300,
    [int]$DieAt = 2,
    [string]$Difficulty = '',
    [int]$Fps = 60,
    [switch]$Uncapped,
    [switch]$SkipSinglePlayer,
    [switch]$WhileInUse
)
$ErrorActionPreference = 'Stop'

# The three campaigns, in the order they're on the level row. The first is at place 13 of the co-op list, which is
# the 13 levels made for co-op, numbered from 0. See 3dc/avp/win95/frontend/level_lists.h.
$firstPlace = 13
$campaign = @(
    @('derelict', 'marine'), @('genshd1', 'marine'), @('invasion', 'marine'), @('odobenus', 'marine'), @('sulaco', 'marine'), @('hangar', 'marine'),
    @('temple_m', 'marine'), @('vaults_m', 'marine'), @('nost03_m', 'marine'), @('stat101_m', 'marine'), @('fall_m', 'marine'),
    @('fall', 'predator'), @('area52', 'predator'), @('vaults', 'predator'), @('furyall', 'predator'), @('caverns', 'predator'), @('battle', 'predator'),
    @('invasion_p', 'predator'), @('escape_p', 'predator'), @('temple_p', 'predator'), @('breakout_p', 'predator'), @('sulaco_p', 'predator'),
    @('temple', 'alien'), @('escape', 'alien'), @('nost03', 'alien'), @('stat101', 'alien'), @('breakout', 'alien'),
    @('invasion_a', 'alien'), @('derelict_a', 'alien'), @('sulaco_a', 'alien'), @('caverns_a', 'alien'), @('furyall_a', 'alien')
)

if ($campaign.Count -ne 32) { Write-Host "run-campaign-levels: the list here has $($campaign.Count) levels, not 32"; exit 2 }

foreach ($asked in $Levels)
{
    if (-not ($campaign | Where-Object { $_[0] -ceq $asked })) { Write-Host "run-campaign-levels: $asked isn't one of the 32"; exit 2 }
}

$runHudson = Join-Path $PSScriptRoot 'run-hudson.ps1'

# Starts one run, and gives back the log's "meets" lines, or $null if it has no log to read.
function Invoke-OneRun([string[]]$extra, [string]$what)
{
    if (@(Get-Process -Name 'claude' -ErrorAction SilentlyContinue).Count -eq 0) { Write-Host "run-campaign-levels: there's no claude process, so this stops"; exit 3 }
    if (@(Get-Process -Name ($Exe -replace '\.exe$', '') -ErrorAction SilentlyContinue).Count -ne 0) { Write-Host "run-campaign-levels: the game is already running, so this stops"; exit 3 }

    $arguments = @('-ExecutionPolicy', 'Bypass', '-File', $runHudson, '-GameFolder', $GameFolder, '-Exe', $Exe, '-Frames', $Frames, '-Seconds', $Seconds, '-DieAt', $DieAt, '-TimingsDontMatter') + $extra
    if ($WhileInUse) { $arguments += '-WhileInUse' }
    if ($Difficulty) { $arguments += @('-Difficulty', $Difficulty) }
    if ($Uncapped) { $arguments += '-Uncapped' } else { $arguments += @('-Fps', $Fps) }

    $said = & powershell @arguments 2>&1 | ForEach-Object { $_.ToString() }
    $code = $LASTEXITCODE
    $keptLine = $said | Where-Object { $_ -match 'Kept in (.+)$' } | Select-Object -Last 1

    if (-not $keptLine -or $keptLine -notmatch 'Kept in (.+)$')
    {
        Write-Host "run-campaign-levels: $what kept nothing, exit code $code"
        return @{ Code = $code; Lines = $null; Folder = '' }
    }

    $kept = $Matches[1].Trim()
    $log = Join-Path $kept 'avp_log.txt'
    $lines = if (Test-Path -LiteralPath $log) { @(Get-Content -LiteralPath $log | Where-Object { $_ -match '(meets: |meets-after-dying: |meets-only-here: |the level row reads |campaign: [0-9]+ places)' } | ForEach-Object { $_ -replace '^.*?(meets: |meets-after-dying: |meets-only-here: |hudson: the level row reads |campaign: [0-9]+ places)', '$1' }) } else { $null }
    return @{ Code = $code; Lines = $lines; Folder = $kept }
}

$report = New-Object System.Collections.ArrayList
$levelsWithDifferences = 0
$levelsThatDidNotStart = 0

for ($i = 0; $i -lt $campaign.Count; $i++)
{
    $level = $campaign[$i][0]
    $species = $campaign[$i][1]
    $place = $firstPlace + $i

    if ($Levels.Count -gt 0 -and $Levels -cnotcontains $level) { continue }

    Write-Host "run-campaign-levels: $level, as $species, place $place"
    [void]$report.Add("== $level, as $species, place $place on the co-op list")

    $mode = Invoke-OneRun @('-Level', $level, '-ListPlace', $place) "the skirmish on $level"
    [void]$report.Add("   the skirmish: exit code $($mode.Code), kept in $($mode.Folder)")

    if ($null -eq $mode.Lines -or @($mode.Lines | Where-Object { $_ -like 'meets: level*' }).Count -eq 0)
    {
        [void]$report.Add('   the skirmish said nothing of what he meets, so it did not start properly')
        $levelsThatDidNotStart++
        continue
    }

    foreach ($line in @($mode.Lines | Where-Object { $_ -notlike 'meets: *' -and $_ -notlike 'meets-after-dying: *' })) { [void]$report.Add("   the skirmish: $line") }

    if ($SkipSinglePlayer)
    {
        foreach ($line in @($mode.Lines | Where-Object { $_ -like 'meets: *' })) { [void]$report.Add("   $line") }
        continue
    }

    $singleExtra = @('-Level', $level, '-Meets')
    if ($species -ne 'marine') { $singleExtra += @('-Species', $species) }
    $single = Invoke-OneRun $singleExtra "single player on $level"
    [void]$report.Add("   single player: exit code $($single.Code), kept in $($single.Folder)")

    if ($null -eq $single.Lines -or @($single.Lines | Where-Object { $_ -like 'meets: level*' }).Count -eq 0)
    {
        [void]$report.Add('   single player said nothing of what he meets, so there is nothing to compare')
        $levelsThatDidNotStart++
        continue
    }

    foreach ($line in @($single.Lines | Where-Object { $_ -notlike 'meets: *' -and $_ -notlike 'meets-after-dying: *' })) { [void]$report.Add("   single player: $line") }

    $modeFacts = @($mode.Lines | Where-Object { $_ -like 'meets: *' -or $_ -like 'meets-after-dying: *' })
    $singleFacts = @($single.Lines | Where-Object { $_ -like 'meets: *' -or $_ -like 'meets-after-dying: *' })
    $onlyInSingle = @($singleFacts | Where-Object { $modeFacts -cnotcontains $_ })
    $onlyInMode = @($modeFacts | Where-Object { $singleFacts -cnotcontains $_ })

    [void]$report.Add("   $($singleFacts.Count) facts in single player, $($modeFacts.Count) in the skirmish, $($onlyInSingle.Count + $onlyInMode.Count) that differ")
    foreach ($line in $onlyInSingle) { [void]$report.Add("   only in single player: $line") }
    foreach ($line in $onlyInMode) { [void]$report.Add("   only in the skirmish: $line") }

    if ($onlyInSingle.Count + $onlyInMode.Count -gt 0) { $levelsWithDifferences++ }

    # Written after each level, so that a stop part of the way through still leaves what was found
    Set-Content -LiteralPath $Output -Encoding UTF8 -Value $report
}

[void]$report.Add('')
[void]$report.Add("$levelsThatDidNotStart levels didn't start, $levelsWithDifferences levels had a difference")
Set-Content -LiteralPath $Output -Encoding UTF8 -Value $report
Write-Host "run-campaign-levels: $levelsThatDidNotStart levels didn't start, $levelsWithDifferences levels had a difference. See $Output"
exit $(if ($levelsThatDidNotStart + $levelsWithDifferences -eq 0) { 0 } else { 1 })
