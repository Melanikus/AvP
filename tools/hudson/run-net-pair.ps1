#Requires -Version 4.0
<#
Runs two copies of the game on this machine, one hosting a network game and one joining it, and says how far the join got. Written 2026-11-06 by Melanikus for the AvP port.

  powershell -ExecutionPolicy Bypass -File tools\hudson\run-net-pair.ps1 -HostFolder C:\Games\AvP-net-a -JoinFolder C:\Games\AvP-net-b -Level jockey -Frames 3000 -Seconds 120

Each copy needs its own folder, because a run keeps its log, its result and its film beside the game, and the next run clears what's there. run-hudson.ps1 does each run, and keeps each one's files in that folder's runs\ as usual.

WHY IT WATCHES THE SOCKETS: the game's network code isn't safe yet to have open to other machines, so a run with net= only uses this machine's own address, 127.0.0.1. The game's code says so, and this checks what it really did. Once a second, it asks Windows what each game has open. If either has anything on another address, it ends both, and says so. That's exit code 110.

WHY SOMEONE SHOULD WATCH THE FIRST ONE: Windows can stop and ask whether to let a program on the network, and nothing here can see that question. Nothing should cause one, because of the above. This compares Windows' firewall rules for these two programs before and after, which shows a question that was answered, but only someone at the screen would see one that wasn't. A question appearing means stop and find out why.

It won't start without a claude process, so that nothing is left starting games with nobody here.

THE SMOKE TEST: every game starts by turning to the other player and checking it can see them, before anything else it does. pair.txt says how each one went in its smoke_host=, smoke_join= and smoke_third= lines, and so does what this prints, first. A run whose smoke test failed doesn't count as a pass, whatever else it did.

Exit code: 0 if both runs ended with 0, or the first that didn't. 100 means it didn't start, 110 means a game had something open on another address, and 111 means every run ended with 0 but a smoke test failed.
#>
param(
    [Parameter(Mandatory = $true)][string]$HostFolder,
    [Parameter(Mandatory = $true)][string]$JoinFolder,
    [string]$Level = '',
    [switch]$Stranded,
    [string]$SiegeFrom = '',
    [string]$SiegeFar = '',
    [string]$Lighting = '', # gouraud, perpixel, normals or relief, for both games
    [string]$Shadows = '',  # on or off, for both games
    [int]$Frames = 3000,
    [int]$Seconds = 120,
    [string]$Exe = 'AvPx.exe',
    [string]$Size = '',
    [string]$HostSize = '',
    [string]$JoinSize = '',
    [ValidateRange(-1, 1)][int]$WindowBig = -1,
    [ValidateSet('second', 'main')][string]$Screen = 'second',
    [int]$SecondsForTheHostToBeReady = 120,
    [switch]$Coop,
    [string]$BotFight = '',
    [switch]$NoPictures,
    [string]$KillKind = '',
    [string]$KillAt = '',
    [string]$Killer = '',
    [switch]$BotStay,
    [switch]$BotOwn,
    [switch]$GhostLog,
    [string]$BotWeapon = '',
    [int]$DeathWay = -1,
    [int]$Fps = 60,
    [switch]$Uncapped,
    [switch]$NoFriendlyFire,
    [switch]$CoopWalk,
    [switch]$Hostile,
    [switch]$Drain,
    [string]$ThirdFolder = '',
    [switch]$Typed,
    [switch]$NoFilm,
    [int]$CoopEnd = 0,
    [int]$HostDieAt = 0,
    [int]$JoinDieAt = 0,
    [string]$Difficulty = '',
    [switch]$Meets,
    [switch]$LevelFilms,
    [ValidateSet('', 'yes', 'no')][string]$Look = '',
    [string]$Siege = '',
    [int]$HostFilmSkip = 0,
    [int]$JoinFilmSkip = 0
)

$ErrorActionPreference = 'Stop'

function Stop-Here([string]$why)
{
    Write-Host "run-net-pair: not starting: $why"
    exit 100
}

# The Stranded test, the user's own standing test, as one word so that it's the same each time: on the open ground beside the bunker on Stranded, near the mouth of the canyon, with two sentry guns in front of the host and the
# joiner a little behind it, so that the joiner's window has the host's animations in it. Enemies of every kind come through the canyon. Each game makes a film, and logs what it knows of each NPC.
if ($Stranded)
{
    $Level = 'kens-co-op'
    $Coop = [switch]::Present
    $GhostLog = [switch]::Present
    $Siege = 'grd2'
    $SiegeFrom = 'canyon_01'
    $SiegeFar = 'canyon_02'
    if (-not $Size) { $Size = '1280x720' }
    # The joiner is the one behind the host's minigun, so its window is the one to watch: it draws and films at 2560x1440, the user's ask, and has the big part of the second screen. The host's is small beside it.
    # The window itself is fitted to its part, 1280x720 in the game's own numbers, which is about 1920x1080 real pixels on that screen. That doesn't change what's drawn or filmed.
    if (-not $JoinSize) { $JoinSize = '2560x1440' }
    if (-not $PSBoundParameters.ContainsKey('WindowBig')) { $WindowBig = 1 }
    if (-not $PSBoundParameters.ContainsKey('Frames')) { $Frames = 900000 }
    if (-not $PSBoundParameters.ContainsKey('Seconds')) { $Seconds = 240 }
}

if (-not $Level) { Stop-Here "-Level says which level to play, or -Stranded runs the Stranded test" }
if (-not (Test-Path -LiteralPath $HostFolder -PathType Container)) { Stop-Here "there's no folder $HostFolder" }
if (-not (Test-Path -LiteralPath $JoinFolder -PathType Container)) { Stop-Here "there's no folder $JoinFolder" }

$hostFull = (Get-Item -LiteralPath $HostFolder).FullName
$joinFull = (Get-Item -LiteralPath $JoinFolder).FullName
$thirdFull = if ($ThirdFolder) { (Get-Item -LiteralPath $ThirdFolder).FullName } else { '' }

if ($thirdFull -match 'steamapps') { Stop-Here 'the third folder looks like a Steam folder' }
if ($thirdFull -and ($thirdFull -eq $hostFull -or $thirdFull -eq $joinFull)) { Stop-Here 'the third game needs a folder of its own' }

if ($hostFull -eq $joinFull) { Stop-Here 'the two games need a folder each, because each keeps its log and its result there' }
if ($hostFull -match 'steamapps' -or $joinFull -match 'steamapps') { Stop-Here 'one of these looks like a Steam folder' }
if ($Exe -ne (Split-Path -Leaf $Exe)) { Stop-Here "-Exe has to be a file in the game folder, and $Exe is a path" }
if (-not (Test-Path -LiteralPath (Join-Path $hostFull $Exe))) { Stop-Here "there's no $Exe in $hostFull" }
if (-not (Test-Path -LiteralPath (Join-Path $joinFull $Exe))) { Stop-Here "there's no $Exe in $joinFull" }
if (@(Get-Process -Name 'claude' -ErrorAction SilentlyContinue).Count -eq 0) { Stop-Here "there's no claude process, so nobody is here to see what happens" }

$running = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -match '^(AvPx|debug_AvPx|AvP_Classic|AvP)$' })
if ($running.Count -gt 0) { Stop-Here "the game is already running: $($running[0].ProcessName)" }

$runHudson = Join-Path $PSScriptRoot 'run-hudson.ps1'
$powershell = Join-Path $PSHOME 'powershell.exe'
$readyFile = Join-Path $hostFull 'HudsonRun\My Games\Aliens versus Predator\hudson-net-ready.txt'
$kept = Join-Path $hostFull ('runs\net-pair-' + (Get-Date -Format 'yyyy-MM-dd-HHmmss'))
New-Item -ItemType Directory -Force $kept | Out-Null

# Rules that name either program. A question from Windows that someone answered shows up as a new one of these.
function Get-FirewallRulesForThese
{
    return @(Get-NetFirewallApplicationFilter -PolicyStore ActiveStore | Where-Object { $_.Program -like "$hostFull\*" -or $_.Program -like "$joinFull\*" -or ($thirdFull -and $_.Program -like "$thirdFull\*") } | ForEach-Object { $_.Program })
}

$rulesBefore = @(Get-FirewallRulesForThese)
$lines = @("host=$hostFull", "join=$joinFull", "level=$Level", "frames=$Frames", "seconds=$Seconds", "firewall_rules_before=$($rulesBefore.Count)")

function Start-OneRun([string]$folder, [string]$word, [string]$partner, [string]$outputName, [string]$kind = '')
{
    $words = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$runHudson`"", '-GameFolder', "`"$folder`"", '-Level', $Level, '-Frames', $Frames, '-Seconds', $Seconds, '-Exe', $Exe, '-TimingsDontMatter', '-WhileInUse', '-Net', $word, '-NetPartner', "`"$partner`"")
    # Each machine can draw at its own size, and otherwise both draw at -Size
    $thisSize = if ($word -eq 'host' -and $HostSize) { $HostSize } elseif ($word -eq 'join' -and $JoinSize) { $JoinSize } else { $Size }
    if ($thisSize) { $words += @('-Size', $thisSize) }
    # Each game has its own part of the second screen, so that none of them covers another
    $slot = if ($outputName -eq 'host-output') { 0 } elseif ($outputName -eq 'join-output') { 1 } else { 2 }
    $words += @('-Screen', $Screen, '-WindowSlot', $slot, '-WindowsTogether', $(if ($thirdFull) { 3 } else { 2 }))
    if ($WindowBig -ge 0 -and -not $thirdFull) { $words += @('-WindowBig', $WindowBig) }
    if ($Coop) { $words += '-Coop' }
    if ($BotFight) { $words += @('-BotFight', $BotFight) }
    if ($NoPictures) { $words += '-NoPictures' }
    if ($KillKind) { $words += @('-KillKind', $KillKind, '-KillAt', $KillAt, '-Killer', $Killer) }
    if ($BotStay) { $words += '-BotStay' }
    if ($BotOwn) { $words += '-BotOwn' }
    if ($GhostLog) { $words += '-GhostLog' }
    if ($BotWeapon) { $words += @('-BotWeapon', $BotWeapon) }
    if ($DeathWay -ge 0) { $words += @('-DeathWay', $DeathWay) }
    if ($Uncapped) { $words += '-Uncapped' } else { $words += @('-Fps', $Fps) }
    if ($NoFriendlyFire) { $words += '-NoFriendlyFire' }
    if ($word -eq 'host' -and $HostDieAt -gt 0) { $words += @('-DieAt', $HostDieAt) }
    if ($word -eq 'join' -and $JoinDieAt -gt 0) { $words += @('-DieAt', $JoinDieAt) }
    if ($word -eq 'host' -and $Difficulty) { $words += @('-Difficulty', $Difficulty) }
    if ($Meets) { $words += '-Meets' }
    if ($LevelFilms) { $words += '-LevelFilms' }
    if ($Look) { $words += @('-Look', $Look) }
    if ($Siege) { $words += @('-Siege', $Siege) }
    if ($SiegeFrom) { $words += @('-SiegeFrom', $SiegeFrom) }
    if ($SiegeFar) { $words += @('-SiegeFar', $SiegeFar) }
    if ($Lighting) { $words += @('-Lighting', $Lighting) }
    if ($Shadows) { $words += @('-Shadows', $Shadows) }
    if ($word -eq 'host' -and $HostFilmSkip -gt 0) { $words += @('-FilmSkip', $HostFilmSkip) }
    if ($word -eq 'join' -and $JoinFilmSkip -gt 0) { $words += @('-FilmSkip', $JoinFilmSkip) }
    if ($CoopWalk) { $words += '-CoopWalk' }
    if ($Hostile) { $words += '-Hostile' }
    if ($kind) { $words += @('-HostileKind', $kind) }
    if ($kind -eq 'flood') { $words += @('-NetSecondPartner', "`"$joinFull`"") }
    if ($Typed) { $words += '-Typed' }
    if ($NoFilm) { $words += '-NoFilm' }
    if ($CoopEnd -gt 0) { $words += @('-CoopEnd', "$CoopEnd") }

    $process = Start-Process -FilePath $powershell -ArgumentList $words -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $kept "$outputName.txt") -RedirectStandardError (Join-Path $kept "$outputName-errors.txt")

    # Without asking for this now, a process that has ended can't say how it ended
    $null = $process.Handle
    return $process
}

# What either game has open anywhere but this machine's own address. Anything new goes in the record, once.
$seenSockets = @{}

function Get-SocketsOffThisMachine
{
    $outside = @()

    foreach ($game in @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -match '^(AvPx|debug_AvPx)$' }))
    {
        $sockets = @(Get-NetUDPEndpoint -OwningProcess $game.Id -ErrorAction SilentlyContinue | ForEach-Object { "udp $($_.LocalAddress):$($_.LocalPort)" })
        $sockets += @(Get-NetTCPConnection -OwningProcess $game.Id -ErrorAction SilentlyContinue | ForEach-Object { "tcp $($_.LocalAddress):$($_.LocalPort)" })

        foreach ($socket in $sockets)
        {
            $seenSockets["$($game.Path) $socket"] = $true

            if ($socket -notmatch '^(udp|tcp) 127\.0\.0\.1:')
            {
                $outside += "$($game.Path) $socket"
            }
        }
    }

    return @($outside)
}

function Stop-TheGames
{
    Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -match '^(AvPx|debug_AvPx)$' -and ($_.Path -like "$hostFull\*" -or $_.Path -like "$joinFull\*" -or ($thirdFull -and $_.Path -like "$thirdFull\*")) } | Stop-Process -Force
}

function Write-Record
{
    $everything = @($script:lines) + @($seenSockets.Keys | Sort-Object | ForEach-Object { "socket=$_" })
    Set-Content -LiteralPath (Join-Path $kept 'pair.txt') -Encoding ASCII -Value $everything
}

# -Drain: the joiner uses up the host's blocks. -ThirdFolder: a third game. The joiner stops answering, and the third floods, so the host holds a queue for a machine that's gone.
$hostKind = if ($Drain -or $thirdFull) { 'watch' } else { '' }
$joinKind = if ($Drain) { 'drain' } elseif ($thirdFull) { 'hold' } else { '' }
$hostRun = Start-OneRun $hostFull 'host' $joinFull 'host-output' $hostKind
$joinRun = $null
$thirdRun = $null
$joinStartedAt = $null
$limit = (Get-Date).AddSeconds($SecondsForTheHostToBeReady + 2 * $Seconds + 180)
$hostWasReady = (Get-Date).AddSeconds($SecondsForTheHostToBeReady)
$offTheMachine = @()

while ((Get-Date) -lt $limit)
{
    Start-Sleep -Seconds 1

    $offTheMachine = @(Get-SocketsOffThisMachine)

    if ($offTheMachine.Count -gt 0)
    {
        break
    }

    if ($null -eq $joinRun)
    {
        if ($hostRun.HasExited)
        {
            Write-Host 'run-net-pair: the host ended before it was ready, so nothing joined'
            break
        }

        if (Test-Path -LiteralPath $readyFile)
        {
            $lines += 'host_ready=yes'
            $joinRun = Start-OneRun $joinFull 'join' $hostFull 'join-output' $joinKind
            $joinStartedAt = Get-Date
        }
        elseif ((Get-Date) -gt $hostWasReady)
        {
            Write-Host "run-net-pair: the host wasn't ready after $SecondsForTheHostToBeReady seconds, so nothing joined"
            Stop-TheGames
            break
        }
    }
    elseif ($thirdFull -and $null -eq $thirdRun -and (Get-Date) -gt $joinStartedAt.AddSeconds(10))
    {
        $thirdRun = Start-OneRun $thirdFull 'join' $hostFull 'third-output' 'flood'
    }
    elseif ($hostRun.HasExited -and $joinRun.HasExited -and ($null -eq $thirdRun -or $thirdRun.HasExited))
    {
        break
    }
}

if ($offTheMachine.Count -gt 0)
{
    Stop-TheGames
    $lines += @($offTheMachine | ForEach-Object { "off_this_machine=$_" })
    Write-Record
    $offTheMachine | ForEach-Object { Write-Host "run-net-pair: FINDING: this was open beyond this machine, so both games have been ended: $_" }
    exit 110
}

if ((Get-Date) -ge $limit)
{
    Write-Host 'run-net-pair: it went past even its own limit, so this is ending both games. That is a finding.'
    Stop-TheGames
}

$hostRun.WaitForExit(120000) | Out-Null
$hostExit = if ($hostRun.HasExited) { $hostRun.ExitCode } else { 101 }
$joinExit = 'never started'

if ($null -ne $joinRun)
{
    $joinRun.WaitForExit(120000) | Out-Null
    $joinExit = if ($joinRun.HasExited) { $joinRun.ExitCode } else { 101 }
}

$rulesAfter = @(Get-FirewallRulesForThese)
$lines += @("firewall_rules_after=$($rulesAfter.Count)", "host_exit=$hostExit", "join_exit=$joinExit")

# What each game's own run said about its smoke test. A game that said nothing failed it.
$smokeFailed = $false

foreach ($game in @('host', 'join', 'third'))
{
    $file = Join-Path $kept "$game-output.txt"

    if (Test-Path -LiteralPath $file)
    {
        $said = @(Get-Content -LiteralPath $file | Where-Object { $_ -like 'run-hudson: smoke=*' }) | Select-Object -First 1
        $verdict = if ($said) { $said.Substring('run-hudson: smoke='.Length) } else { 'failed: this game said nothing about it' }
        $lines += "smoke_$game=$verdict"
        Write-Host "run-net-pair: the smoke test on the $game game: $verdict"
        if ($verdict -notlike 'passed*') { $smokeFailed = $true }
    }
}

if ($rulesAfter.Count -ne $rulesBefore.Count)
{
    Write-Host "run-net-pair: FINDING: the firewall has new rules for these programs, so Windows asked and someone answered: $($rulesAfter.Count - $rulesBefore.Count)"
}

Write-Record

foreach ($name in 'host-output', 'join-output')
{
    $file = Join-Path $kept "$name.txt"
    if (Test-Path -LiteralPath $file) { Get-Content -LiteralPath $file | Where-Object { $_ -match '^run-hudson: (net=|film=|commit=|exit code)' } | ForEach-Object { Write-Host "${name}: $_" } }
}

Write-Host "run-net-pair: the host's exit code was $hostExit, and the joiner's was $joinExit. Kept in $kept"

if ($hostExit -ne 0) { exit $hostExit }
if ($joinExit -is [int] -and $joinExit -ne 0) { exit $joinExit }
if ($null -eq $joinRun) { exit 100 }
if ($smokeFailed) { exit 111 }
exit 0
