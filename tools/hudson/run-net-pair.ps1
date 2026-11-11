#Requires -Version 4.0
<#
Runs two copies of the game on this machine, one hosting a network game and one joining it, and says how far the join got. Written 2026-11-06 by Melanikus for the AvP port.

  powershell -ExecutionPolicy Bypass -File tools\hudson\run-net-pair.ps1 -HostFolder C:\Games\AvP-net-a -JoinFolder C:\Games\AvP-net-b -Level jockey -Frames 3000 -Seconds 120

Each copy needs its own folder, because a run keeps its log, its result and its film beside the game, and the next run clears what's there. run-hudson.ps1 does each run, and keeps each one's files in that folder's runs\ as usual.

WHY IT WATCHES THE SOCKETS: the game's network code isn't safe yet to have open to other machines, so a run with net= only uses this machine's own address, 127.0.0.1. The game's code says so, and this checks what it really did. Once a second, it asks Windows what each game has open. If either has anything on another address, it ends both, and says so. That's exit code 110. A connection a game makes itself (the relays' list, with -Relay) counts by the machine at its far end, which has to be this one; the rule is Test-SocketIsOffThisMachine, below.

-RemoteRelay 203.0.113.7 -ServersLine "203.0.113.7 <fingerprint> New York" (server #14 through a relay server on another machine): both games go through that one server, its list at port 443 and its relay, and through nothing else. The address is the server's servers.cfg line's, written plainly, so nothing is looked up and nothing on this machine (its hosts file) has to change. pair.txt records it, with this machine's address towards it. The watch then lets through exactly the games' UDP sockets on that address of this machine's, and HTTPS to the server's own address at port 443. (Modified 2026-11-10 by Melanikus for the AvP port, #175: the server's name and its lookup, -RelayListPin, -RelayServerName, -RelayKey and -LocalRelayPin are gone: the line's fingerprint proves the server.)

-Relay -ServersLine "127.0.0.1:8443:27500 <fingerprint> Hudson": the same through the server on this machine. Each game's line goes in the servers.cfg beside its bin folder, the only place the game reads one, and only in a copy made for Hudson's runs, which has a file called hudson-run-copy.txt beside bin (relay-words.ps1). A run never writes over a servers.cfg it didn't write.

WHY SOMEONE SHOULD WATCH THE FIRST ONE: Windows can stop and ask whether to let a program on the network, and nothing here can see that question. Nothing should cause one, because of the above. This compares Windows' firewall rules for these two programs before and after, which shows a question that was answered, but only someone at the screen would see one that wasn't. A question appearing means stop and find out why.

It won't start without a claude process, so that nothing is left starting games with nobody here.

THE SMOKE TEST: every game starts by turning to the other player and checking it can see them, before anything else it does. pair.txt says how each one went in its smoke_host=, smoke_join= and smoke_third= lines, and so does what this prints, first. A run whose smoke test failed doesn't count as a pass, whatever else it did.

Exit code: 0 if both runs ended with 0, or the first that didn't. 100 means it didn't start, 110 means a game had something open on another address, and 111 means every run ended with 0 but a smoke test failed. 112 means every run ended with 0 and the smoke tests passed, but a game's log has a "DirectX Error!" line: pair.txt's d3d_errors_ and d3d_first_ lines say which game, how many and the first (#203).

CHECKING THE WATCH ITSELF: -CheckTheWatch starts no game. It puts the watch's rule (Test-SocketIsOffThisMachine, below) through every kind of socket it has to tell apart, and says which, if any, it got wrong. Exit code 0 if none, 1 if any.

  powershell -ExecutionPolicy Bypass -File tools\hudson\run-net-pair.ps1 -CheckTheWatch
#>
[CmdletBinding(DefaultParameterSetName = 'Run')]
param(
    [Parameter(Mandatory = $true, ParameterSetName = 'Run')][string]$HostFolder,
    [Parameter(Mandatory = $true, ParameterSetName = 'Run')][string]$JoinFolder,
    [Parameter(Mandatory = $true, ParameterSetName = 'CheckTheWatch')][switch]$CheckTheWatch,
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
    [switch]$Play,
    [switch]$Hostile,
    [switch]$Drain,
    [string]$ThirdFolder = '',
    [switch]$Relay,              # server #14: the host hosts through the relay server -ServersLine names, and the joiner joins through it
    # Modified 2026-11-10 by Melanikus for the AvP port (#175): the server is one servers.cfg line (relay-words.ps1)
    [string]$ServersLine = '',   # that line, for both games: "<address>[:<list port>:<relay port>] <fingerprint> <name>"
    [string]$RemoteRelay = '',   # server #14 beyond this machine: the line's address, one IPv4 address, given on purpose. It means -Relay too
    # Through the relay (version 2 for every game since #175 PR 3a, 2026-11-10), the joiner is the second copy (-secondcopy), so the two
    # have their own online identity each, and are two players, not one (added 2026-11-09 by Melanikus (Fifth) for the AvP port)

    [ValidateRange(0, 600)][int]$HostSpareSeconds = 30,  # the host plays this much longer than the joiner, so the joiner ends first (0: the same)
    [switch]$NoFilm,
    [int]$CoopEnd = 0,
    [int]$HostDieAt = 0,
    [int]$JoinDieAt = 0,
    [string]$Difficulty = '',
    [switch]$Meets,
    [switch]$LevelFilms,
    [ValidateSet('', 'yes', 'no')][string]$Look = '',
    [string]$Siege = '',
    [int]$HostGun = 0,      # with -Play, the slot of the gun each Hudson is given and holds, from 1 to 14: 10 is the shotgun
    [int]$JoinGun = 0,
    [int]$StepSeconds = 0,  # with -Play, how long one step may take before he's moved to it (a logged cheat), 10 to 600: for a film that should move on. 0 is 180
    [int]$HostFilmSkip = 0,
    [int]$JoinFilmSkip = 0,
    # Added 2026-11-10 by Melanikus (Third) for the AvP port: the host answers a player asking to join on the page's own rows, and the joiner
    # finds the host through Find Games, as players do (run-hudson.ps1's -Answer, -AnswerAfter and -FindGames, Hudson's answer=,
    # answerafter= and findgames=). Without them the host lets every joiner in, and the joiner joins by its own way, as before
    [string]$HostAnswer = '',     # letin or no
    [int]$HostAnswerAfter = 0,    # with -HostAnswer: seconds before the host answers, 1 to 15. Left out, Hudson's own 3
    [switch]$JoinFindGames,
    # Added 2026-11-10 by Melanikus (Third) for the AvP port, for the owner's showcase: both games' films start at the menus, cut into a
    # clip for each page they picture (run-hudson.ps1's -FilmMenus and -PageHold, Hudson's film=menus and pagehold=)
    [switch]$FilmMenus,
    [int]$PageHold = 0,           # with -FilmMenus: seconds each pictured page is held first, 1 to 10
    # Added 2026-11-10 by Melanikus (Third) for the AvP port: the host hosts through the Multiplayer page's rows, online
    # (run-hudson.ps1's -HostRow, Hudson's hostrow=)
    [string]$HostRow = ''
)

$ErrorActionPreference = 'Stop'

function Stop-Here([string]$why)
{
    Write-Host "run-net-pair: not starting: $why"
    exit 100
}

# THE WATCH'S RULE: whether one socket a game has open could be reached from another machine, or reaches one. Changed 2026-11-09 by Melanikus for the AvP port (server #14).
# This machine's own addresses are 127.anything (IPv4), ::1 (IPv6), and ::ffff:127.anything (an IPv4 address written the IPv6 way). Nothing outside the machine can reach those or be reached through them.
#  - UDP, the game's own network sockets: fine only on one of this machine's own addresses. One on 0.0.0.0 or :: hears every network the machine is on, so it's a finding.
#  - TCP that is listening (waiting for others to connect to it): the same, because a listener on any other address could be reached from another machine.
#  - TCP that isn't listening: a connection this game made itself, which here is the relays' list, over HTTPS to localhost. While it's still connecting, Windows shows its own end as :: or 0.0.0.0, because it hasn't
#    been given an address yet. That isn't a listener and hears nothing, so what decides is the far end, the machine it's connecting to: it has to be this one.
#  - TCP that is only "Bound", with no far end (0.0.0.0 or ::): fine. Windows gives the connection a port a moment before it starts connecting, and the watch can look in that moment (server #14's second run,
#    2026-11-09: "tcp Bound :::64861 to :::0"). A bound socket can't take a connection: only a listening one can, and one that went on to listen would show as listening the next second.
#  - With -RemoteRelay (server #14 through a relay on another machine), exactly two things more are fine: a UDP socket on this machine's address towards that relay (route), where the games put their own
#    (never 0.0.0.0), and a TCP connection, not a listener, to the relay's own address at port 443: its list, over HTTPS. Anything else beyond this machine is still a finding. The games drop whatever reaches
#    their sockets from anywhere but that relay (networking.cpp), which nothing out here can see; this checks what they have open.

# -RemoteRelay's and -ServersLine's own rules, and the servers.cfg a run writes, the same in run-hudson.ps1 (relay-words.ps1)
. (Join-Path $PSScriptRoot 'relay-words.ps1')

function Test-SocketIsOffThisMachine([string]$protocol, [string]$state, [string]$localAddress, [string]$remoteAddress, [string]$remotePort = '', [string]$allowedRelay = '', [string]$allowedRoute = '')
{
    $thisMachine = '^(127\.\d{1,3}\.\d{1,3}\.\d{1,3}|::1|::ffff:127\.\d{1,3}\.\d{1,3}\.\d{1,3})$'
    $noAddress = '^(0\.0\.0\.0|::)$'

    # (PowerShell's names don't care about capitals, so nothing here is called relay or route: those would be -Relay's switch)
    if ($allowedRelay -and $allowedRoute)
    {
        if ($protocol -eq 'udp' -and $localAddress -eq $allowedRoute)
        {
            return $false
        }

        if ($protocol -eq 'tcp' -and $state -ne 'Listen' -and $remoteAddress -eq $allowedRelay -and $remotePort -eq '443')
        {
            return $false
        }
    }

    if ($protocol -eq 'tcp' -and $state -eq 'Bound' -and $remoteAddress -match $noAddress)
    {
        return $false
    }

    if ($protocol -eq 'tcp' -and $state -ne 'Listen')
    {
        return $remoteAddress -notmatch $thisMachine
    }

    return $localAddress -notmatch $thisMachine
}

if ($CheckTheWatch)
{
    # Each: what the socket is, and whether the watch must call it a finding. The first of the TCP ones that connect, and the first Bound one, are what ended server #14's first two runs on 2026-11-09: the game
    # connecting to the relays' list.
    $cases = @(
        @('udp', '', '127.0.0.1', '', $false), @('udp', '', '127.5.6.7', '', $false), @('udp', '', '::1', '', $false), @('udp', '', '::ffff:127.0.0.1', '', $false),
        @('udp', '', '0.0.0.0', '', $true), @('udp', '', '::', '', $true), @('udp', '', '192.168.1.5', '', $true), @('udp', '', '::10', '', $true), @('udp', '', '1127.0.0.1', '', $true), @('udp', '', '127.0.0.1.5', '', $true),
        @('tcp', 'Listen', '127.0.0.1', '0.0.0.0', $false), @('tcp', 'Listen', '::1', '::', $false),
        @('tcp', 'Listen', '0.0.0.0', '0.0.0.0', $true), @('tcp', 'Listen', '::', '::', $true), @('tcp', 'Listen', '192.168.1.5', '0.0.0.0', $true),
        @('tcp', 'SynSent', '::', '::1', $false), @('tcp', 'SynSent', '0.0.0.0', '127.0.0.1', $false), @('tcp', 'Established', '::1', '::1', $false), @('tcp', 'Established', '127.0.0.1', '127.0.0.1', $false),
        @('tcp', 'TimeWait', '::1', '::1', $false), @('tcp', 'Established', '::ffff:127.0.0.1', '::ffff:127.0.0.1', $false),
        @('tcp', 'Bound', '::', '::', $false), @('tcp', 'Bound', '0.0.0.0', '0.0.0.0', $false), @('tcp', 'Bound', '::1', '::', $false),
        @('tcp', 'SynSent', '0.0.0.0', '203.0.113.7', $true), @('tcp', 'SynSent', '::', '2001:db8::7', $true), @('tcp', 'Established', '192.168.1.5', '203.0.113.7', $true),
        @('tcp', 'Established', '127.0.0.1', '::10', $true), @('tcp', 'Bound', '::', '203.0.113.7', $true), @('tcp', 'SynSent', '::', '::', $true), @('tcp', 'Established', '::', '::', $true),
        @('tcp', 'Bound', '::', '::10', $true)
    )
    # -RemoteRelay's: the same rule with one relay beyond this machine, at 203.0.113.50, and this machine's address towards it, 192.168.1.10. Each: what the socket is, its far end's port, and whether the
    # watch must call it a finding. (These addresses are made up: 203.0.113.x is one the internet keeps for examples.)
    $exampleRelay = '203.0.113.50'
    $exampleRoute = '192.168.1.10'
    $remoteCases = @(
        @('udp', '', $exampleRoute, '', '', $false), @('udp', '', '127.0.0.1', '', '', $false),
        @('udp', '', '192.168.1.11', '', '', $true), @('udp', '', '0.0.0.0', '', '', $true), @('udp', '', '::', '', '', $true), @('udp', '', $exampleRelay, '', '', $true),
        @('tcp', 'SynSent', $exampleRoute, $exampleRelay, '443', $false), @('tcp', 'Established', $exampleRoute, $exampleRelay, '443', $false), @('tcp', 'SynSent', '0.0.0.0', $exampleRelay, '443', $false),
        @('tcp', 'TimeWait', $exampleRoute, $exampleRelay, '443', $false), @('tcp', 'Bound', $exampleRoute, '0.0.0.0', '0', $false), @('tcp', 'SynSent', '::', '::1', '8443', $false),
        @('tcp', 'Established', $exampleRoute, $exampleRelay, '8443', $true), @('tcp', 'Established', $exampleRoute, '203.0.113.51', '443', $true), @('tcp', 'Established', $exampleRoute, '192.168.1.1', '443', $true),
        @('tcp', 'Listen', $exampleRoute, '0.0.0.0', '0', $true), @('tcp', 'Listen', '0.0.0.0', '0.0.0.0', '0', $true), @('tcp', 'Listen', $exampleRelay, '0.0.0.0', '0', $true)
    )
    $wrong = @()
    foreach ($case in $cases)
    {
        $said = Test-SocketIsOffThisMachine $case[0] $case[1] $case[2] $case[3]
        if ($said -ne $case[4]) { $wrong += "$($case[0]) $($case[1]) $($case[2]) to $($case[3]): the watch said finding=$said, and it should be $($case[4])" }
    }
    foreach ($case in $remoteCases)
    {
        $said = Test-SocketIsOffThisMachine $case[0] $case[1] $case[2] $case[3] $case[4] $exampleRelay $exampleRoute
        if ($said -ne $case[5]) { $wrong += "-RemoteRelay: $($case[0]) $($case[1]) $($case[2]) to $($case[3]):$($case[4]): the watch said finding=$said, and it should be $($case[5])" }
    }
    # And with no -RemoteRelay, the address towards a relay is just another address
    if (-not (Test-SocketIsOffThisMachine 'udp' '' $exampleRoute '')) { $wrong += "with no -RemoteRelay, udp $exampleRoute should be a finding" }
    if (-not (Test-SocketIsOffThisMachine 'tcp' 'Established' $exampleRoute $exampleRelay '443')) { $wrong += "with no -RemoteRelay, tcp to $($exampleRelay):443 should be a finding" }

    # -RemoteRelay and -ServersLine (relay-words.ps1; modified 2026-11-10 by Melanikus for the AvP port, #175, in place of -RelayListPin,
    # -RelayServerName, -RelayKey and -LocalRelayPin): each is taken as it should be, or refused with its own reason
    $notAnAddress = "-RemoteRelay is one plain IPv4 address, as a servers.cfg line gives it, and never a name"
    $notOnAnother = "-RemoteRelay's address isn't one a relay on another machine can have"
    $remoteWordCases = @(
        @('', ''), @($exampleRelay, ''), @('192.168.1.20', ''),
        @('127.0.0.1', $notOnAnother), @('0.1.2.3', $notOnAnother), @('224.0.0.1', $notOnAnother), @('255.255.255.255', $notOnAnother), @('169.254.1.1', $notOnAnother),
        @('nyc3.relay.avpx.test', $notAnAddress), @('203.0.113.050', $notAnAddress), @('203.0.113.256', $notAnAddress), @('203.0.113', $notAnAddress),
        @('203.0.113.50.1', $notAnAddress), @('203.0.113.50.', $notAnAddress), @("203.0.113.50`n", $notAnAddress), @(' 203.0.113.50', $notAnAddress),
        @('203.0.113.50:443', $notAnAddress), @('::1', $notAnAddress)
    )
    foreach ($case in $remoteWordCases)
    {
        $said = Get-HudsonRemoteRelayRefusal $case[0]
        if ($said -cne $case[1]) { $wrong += "-RemoteRelay [$($case[0])]: refused with [$said], and it should be [$($case[1])]" }
    }

    # nyc3's own fingerprint (CRYPTO.md 12.2), as a line would hold it
    $fingerprint = 'bb0e2958dade79033a90f34811c84837b404708cc004c5b1d37d9a26f28d663e'
    $empty = "-ServersLine is empty"
    $shape = "-ServersLine is three things with one space between: the address, the fingerprint and the name"
    $badFingerprint = "-ServersLine's fingerprint is 64 small hex digits, and nothing else"
    $badName = "-ServersLine's name is 1 to 31 letters, digits and ' ( ) - . , with single spaces between words"
    $badAddress = "-ServersLine's address is one plain IPv4 address, or that and :<list port>:<relay port>"
    $badPort = "-ServersLine's ports are 1 to 65535"
    $needsRemote = "-ServersLine names another machine, so it needs -RemoteRelay with the same address: a run leaves this machine only when asked to"
    $notRemotes = "-ServersLine's address isn't -RemoteRelay's"
    $not443 = "-ServersLine's list beyond this machine is at port 443, the only HTTPS port run-net-pair's watch lets through"
    $lineCases = @(
        @("127.0.0.1:8443:27500 $fingerprint Hudson", '', ''), @("127.0.0.1 $fingerprint Hudson", '', ''), @("$exampleRelay $fingerprint New York (Mel's server)", $exampleRelay, ''),
        @("$($exampleRelay):443:27501 $fingerprint a", $exampleRelay, ''), @("127.0.0.1 $fingerprint $('x' * 31)", '', ''), @("127.0.0.1:65535:1 $fingerprint Hudson", '', ''),
        @('', '', $empty),
        @("127.0.0.1  $fingerprint Hudson", '', $shape), @("127.0.0.1 $fingerprint", '', $shape), @("127.0.0.1`t$fingerprint Hudson", '', $shape),
        @(" 127.0.0.1 $fingerprint Hudson", '', $shape), @("127.0.0.1 $fingerprint  Hudson", '', $shape), @("127.0.0.1 $fingerprint a`nb", '', $shape),
        @("127.0.0.1 $($fingerprint.Substring(1)) Hudson", '', $badFingerprint), @("127.0.0.1 $($fingerprint)0 Hudson", '', $badFingerprint),
        @("127.0.0.1 $($fingerprint.ToUpperInvariant()) Hudson", '', $badFingerprint), @("127.0.0.1 $($fingerprint.Substring(1))g Hudson", '', $badFingerprint),
        @("127.0.0.1 $fingerprint $('x' * 32)", '', $badName), @("127.0.0.1 $fingerprint New `"York`"", '', $badName), @("127.0.0.1 $fingerprint a;b", '', $badName),
        @("127.0.0.1 $fingerprint São Paulo", '', $badName), @("127.0.0.1 $fingerprint New  York", '', $badName), @("127.0.0.1 $fingerprint Hudson ", '', $badName),
        @("127.0.0.1 $fingerprint #Hudson", '', $badName),
        @("localhost $fingerprint Hudson", '', $badAddress), @("127.0.0.01 $fingerprint Hudson", '', $badAddress), @("127.0.0.1:8443 $fingerprint Hudson", '', $badAddress),
        @("127.0.0.1:08443:27500 $fingerprint Hudson", '', $badAddress), @("127.0.0.1:0:27500 $fingerprint Hudson", '', $badAddress),
        @("127.0.0.1:8443:27500:1 $fingerprint Hudson", '', $badAddress),
        @("127.0.0.1:65536:27500 $fingerprint Hudson", '', $badPort), @("127.0.0.1:8443:99999 $fingerprint Hudson", '', $badPort),
        @("$exampleRelay $fingerprint New York", '', $needsRemote), @("127.0.0.2 $fingerprint Hudson", '', $needsRemote),
        @("203.0.113.51 $fingerprint New York", $exampleRelay, $notRemotes), @("127.0.0.1 $fingerprint Hudson", $exampleRelay, $notRemotes),
        @("$($exampleRelay):8443:27500 $fingerprint New York", $exampleRelay, $not443)
    )
    foreach ($case in $lineCases)
    {
        $said = Get-HudsonServersLineRefusal $case[0] $case[1]
        if ($said -cne $case[2]) { $wrong += "-ServersLine [$($case[0])] with -RemoteRelay [$($case[1])]: refused with [$said], and it should be [$($case[2])]" }
    }

    # The servers.cfg a run writes (relay-words.ps1, added 2026-11-10 by Melanikus for the AvP port, #175), tried in a folder made for the
    # purpose under this user's temporary folder, with a name nothing else has, and taken away after. Each: what's tried, and whether it
    # went as it should: written only into a copy made for Hudson's runs, and never over a servers.cfg a run didn't write.
    $fileChecks = New-Object System.Collections.Generic.List[object]
    $trial = Join-Path ([System.IO.Path]::GetTempPath()) ('hudson-servers-cfg-check-' + [System.Guid]::NewGuid().ToString('N'))
    $trialBin = Join-Path $trial 'bin'
    $trialFile = Join-Path $trial 'servers.cfg'
    $trialMark = Join-Path $trial 'hudson-run-copy.txt'
    $trialLine = "127.0.0.1:8443:27500 $fingerprint Hudson"
    $written = "$hudsonServersFileMark`n$trialLine`n"
    $read = { [System.IO.File]::ReadAllText($trialFile, [System.Text.Encoding]::ASCII) }

    try
    {
        New-Item -ItemType Directory -Force $trialBin | Out-Null

        # A game that isn't in a folder called bin reads no servers.cfg, so a line can't go anywhere for it
        $fileChecks.Add(@('a game not in a folder called bin is refused a line', ((Set-HudsonServersFile $trial $trialLine) -ceq "-ServersLine needs the game in a folder called bin: the game reads servers.cfg only beside its bin folder")))
        $fileChecks.Add(@('and, given none, it goes ahead', ((Set-HudsonServersFile $trial '') -ceq '')))

        # No hudson-run-copy.txt beside bin: not a copy made for Hudson's runs, so nothing is written
        $fileChecks.Add(@('a folder without the mark is refused a line', ((Set-HudsonServersFile $trialBin $trialLine) -clike "*has no hudson-run-copy.txt, so it isn't a copy made for Hudson's runs*")))
        $fileChecks.Add(@('and nothing is written there', (-not (Test-Path -LiteralPath $trialFile))))
        $fileChecks.Add(@('and, given no line and holding no servers.cfg, it goes ahead', ((Set-HudsonServersFile $trialBin '') -ceq '')))

        # The mark: the line is written, after the run's own note
        [System.IO.File]::WriteAllText($trialMark, "A copy made for Hudson's runs.`n", [System.Text.Encoding]::ASCII)
        $fileChecks.Add(@('with the mark, the line is written', ((Set-HudsonServersFile $trialBin $trialLine) -ceq '' -and (& $read) -ceq $written)))
        $fileChecks.Add(@('and given the same again, it stays as it is', ((Set-HudsonServersFile $trialBin $trialLine) -ceq '' -and (& $read) -ceq $written)))
        $fileChecks.Add(@('another line takes its place', ((Set-HudsonServersFile $trialBin "127.0.0.1 $fingerprint Other") -ceq '' -and (& $read) -ceq "$hudsonServersFileMark`n127.0.0.1 $fingerprint Other`n")))
        $fileChecks.Add(@('a run with no line takes the run''s own away', ((Set-HudsonServersFile $trialBin '') -ceq '' -and -not (Test-Path -LiteralPath $trialFile))))

        # One left half-made by a run that was cut short goes first
        [System.IO.File]::WriteAllText("$trialFile.hudson-new", 'half', [System.Text.Encoding]::ASCII)
        $fileChecks.Add(@('a half-made one left by a run goes, and the line is written whole', ((Set-HudsonServersFile $trialBin $trialLine) -ceq '' -and (& $read) -ceq $written -and -not (Test-Path -LiteralPath "$trialFile.hudson-new"))))

        # The run's own, with the mark gone: no longer a copy made for Hudson's runs, so it's left, and a run with no line doesn't start
        Remove-Item -LiteralPath $trialMark -Force
        $fileChecks.Add(@('the run''s own, in a folder that lost its mark, is left, and a run with no line is refused', ((Set-HudsonServersFile $trialBin '') -clike '*names a server, and*has no hudson-run-copy.txt*' -and (& $read) -ceq $written)))
        [System.IO.File]::WriteAllText($trialMark, "A copy made for Hudson's runs.`n", [System.Text.Encoding]::ASCII)

        # Someone else's that names a server: never written over or taken away, and no run starts beside it
        [System.IO.File]::WriteAllText($trialFile, "203.0.113.7 $fingerprint New York`r`n", [System.Text.Encoding]::ASCII)
        $fileChecks.Add(@('someone else''s naming a server is never written over', ((Set-HudsonServersFile $trialBin $trialLine) -clike "*names a server, and a Hudson run didn't write it*" -and (& $read) -ceq "203.0.113.7 $fingerprint New York`r`n")))
        $fileChecks.Add(@('nor taken away, and a run with no line is refused beside it', ((Set-HudsonServersFile $trialBin '') -clike "*names a server, and a Hudson run didn't write it*" -and (Test-Path -LiteralPath $trialFile))))

        # Someone else's that names none, as a release's does before it ships a line: left, and only a run with no line goes ahead
        [System.IO.File]::WriteAllText($trialFile, "# no server yet`r`n`r`n   `t`r`n", [System.Text.Encoding]::ASCII)
        $fileChecks.Add(@('someone else''s with notes only is left, and a run with no line goes ahead', ((Set-HudsonServersFile $trialBin '') -ceq '' -and (& $read) -ceq "# no server yet`r`n`r`n   `t`r`n")))
        $fileChecks.Add(@('but a line never goes over it', ((Set-HudsonServersFile $trialBin $trialLine) -clike "*is there, and a Hudson run didn't write it*" -and (& $read) -ceq "# no server yet`r`n`r`n   `t`r`n")))

        # Bigger than any servers.cfg can be, or not a file at all: left as it is
        [System.IO.File]::WriteAllText($trialFile, ('#' * 16385), [System.Text.Encoding]::ASCII)
        $fileChecks.Add(@('one bigger than any servers.cfg can be is left', ((Set-HudsonServersFile $trialBin '') -clike '*is bigger than any servers.cfg can be*')))
        Remove-Item -LiteralPath $trialFile -Force
        New-Item -ItemType Directory -Force $trialFile | Out-Null
        $fileChecks.Add(@('a folder called servers.cfg is left', ((Set-HudsonServersFile $trialBin $trialLine) -clike "*isn't a plain file*" -and (Test-Path -LiteralPath $trialFile -PathType Container))))
    }
    catch
    {
        $fileChecks.Add(@("trying the servers.cfg a run writes stopped: $($_.Exception.Message)", $false))
    }
    finally
    {
        if (Test-Path -LiteralPath $trial) { Remove-Item -LiteralPath $trial -Recurse -Force }
    }

    foreach ($check in $fileChecks)
    {
        if (-not $check[1]) { $wrong += "servers.cfg: $($check[0])" }
    }

    if ($wrong.Count -gt 0)
    {
        $wrong | ForEach-Object { Write-Host "run-net-pair: the watch is wrong: $_" }
        exit 1
    }
    Write-Host "run-net-pair: the watch's rule is right on all $($cases.Count + $remoteCases.Count + 2) kinds of socket, the relay's words on all $($remoteWordCases.Count + $lineCases.Count) cases, and the servers.cfg a run writes on all $($fileChecks.Count)"
    exit 0
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

# -RemoteRelay (server #14 through a relay on another machine, 2026-11-09): the relay server's one address, and this machine's address
# towards it. The watch lets through exactly those, and nothing else. Modified 2026-11-10 by Melanikus for the AvP port (#175): the address
# is the server's servers.cfg line's, written plainly, so nothing is looked up.
$remoteRelayAddress = ''
$remoteRelayRoute = ''

# -RemoteRelay and -ServersLine, by the rules run-hudson.ps1 uses too (relay-words.ps1), which checks them again for each game
$relayWordsRefusal = Get-HudsonRemoteRelayRefusal $RemoteRelay
if ($relayWordsRefusal) { Stop-Here $relayWordsRefusal }
if ($RemoteRelay) { $Relay = [switch]::Present }
if ($Relay -and -not $ServersLine) { Stop-Here "-Relay needs -ServersLine: the games find their server only in servers.cfg, and each run writes that one line there" }
if ($ServersLine -and -not $Relay) { Stop-Here "-ServersLine is the server -Relay goes through, so it needs -Relay" }
$serversLineRefusal = if ($ServersLine) { Get-HudsonServersLineRefusal $ServersLine $RemoteRelay } else { '' }
if ($serversLineRefusal) { Stop-Here $serversLineRefusal }
if ($Relay -and $ThirdFolder) { Stop-Here "a pair through the relay is two players, the host and the second copy, and -ThirdFolder would be a third copy with no identity of its own" }
# The host's answer and the joiner's Find Games (added 2026-11-10 by Melanikus (Third)): to Hudson's own words and ranges, and run-hudson.ps1
# checks them again for each game. -cmatch, because Hudson's words are small letters and PowerShell's -match ignores case
if ($HostAnswer -and $HostAnswer -cnotmatch '\A(letin|no)\z') { Stop-Here "-HostAnswer is letin or no" }
if ($HostAnswerAfter -ne 0 -and -not $HostAnswer) { Stop-Here "-HostAnswerAfter says when the host answers, so it needs -HostAnswer" }
if ($HostAnswerAfter -ne 0 -and ($HostAnswerAfter -lt 1 -or $HostAnswerAfter -gt 15)) { Stop-Here "-HostAnswerAfter is from 1 to 15 seconds, well inside the joiner's 20 s wait" }
if ($FilmMenus -and $NoFilm) { Stop-Here "-FilmMenus asks for films, and -NoFilm says there are none" }
if ($PageHold -ne 0 -and -not $FilmMenus) { Stop-Here "-PageHold holds the menus' pages for the film, so it needs -FilmMenus" }
if ($PageHold -ne 0 -and ($PageHold -lt 1 -or $PageHold -gt 10)) { Stop-Here "-PageHold is from 1 to 10 seconds" }
# The host through the Multiplayer page's rows (added 2026-11-10 by Melanikus (Third)). Modified 2026-11-10 by Melanikus for the AvP port
# (the owner's ruling for beta.2: "Everything over nyc3"): online only, as the game at home and its joiner by a typed address are gone
if ($HostRow -and $HostRow -cnotmatch '\A(online)\z') { Stop-Here "-HostRow is online: beta.2 hosts online only, through a server's relay" }
if ($HostRow -and -not $Relay) { Stop-Here "-HostRow hosts through the server servers.cfg names, so it needs -Relay and -ServersLine" }

# A hostile run sends the other game bad messages on purpose. Beyond this machine the other end could be anybody's game, so no hostile kind
# ever goes through -RemoteRelay (Primary's read of this line, 2026-11-09). The game's own plan reader refuses the same.
if ($RemoteRelay -and ($Hostile -or $Drain -or $ThirdFolder)) { Stop-Here "-Hostile, -Drain and -ThirdFolder send bad messages on purpose, so they never go through -RemoteRelay, beyond this machine" }

if ($RemoteRelay)
{
    $remoteRelayAddress = $RemoteRelay
    $remoteRelayRoute = @(Find-NetRoute -RemoteIPAddress $remoteRelayAddress -ErrorAction SilentlyContinue | Where-Object { $_.PSObject.Properties['IPAddress'] -and $_.IPAddress } | ForEach-Object { "$($_.IPAddress)" }) | Select-Object -First 1

    if (-not $remoteRelayRoute -or $remoteRelayRoute -notmatch '^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$' -or $remoteRelayRoute -match '^(0|127)\.') { Stop-Here "no address of this machine's towards -RemoteRelay's was found" }
}

# Each game's servers.cfg, written here once before either starts, so the two runs (which write it too) find it as wanted and never write it
# at the same moment. Added 2026-11-10 by Melanikus for the AvP port (#175).
foreach ($gameFolder in @(@($hostFull, $joinFull, $thirdFull) | Where-Object { $_ } | Select-Object -Unique))
{
    $serversRefusal = Set-HudsonServersFile $gameFolder $ServersLine
    if ($serversRefusal) { Stop-Here $serversRefusal }
}

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
$lines = @("host=$hostFull", "join=$joinFull", "level=$Level", "frames=$Frames", "seconds=$Seconds", "host_spare_seconds=$HostSpareSeconds", "firewall_rules_before=$($rulesBefore.Count)")

# Which relay beyond this machine the watch let through, and at which address: the user's own server's, never a player's
if ($RemoteRelay) { $lines += @("remote_relay=$RemoteRelay", "remote_relay_address=$remoteRelayAddress", "remote_relay_route=$remoteRelayRoute") }
if ($ServersLine) { $lines += "servers_line=$ServersLine" }   # public: the server's address, fingerprint and name

function Start-OneRun([string]$folder, [string]$word, [string]$partner, [string]$outputName, [string]$kind = '')
{
    # The host plays a little longer than the joiner, which starts later. Then the joiner ends its own run, rather than being ended by the host leaving: server #14's first run through the relay, 2026-11-09,
    # had the joiner 670 frames behind when the host ended, and the joiner's game ended with the host's (exit 37). With no cap on the frames each second, 300 a second is allowed for.
    $thisFrames = $Frames
    $thisSeconds = $Seconds
    if ($word -eq 'host' -and $HostSpareSeconds -gt 0)
    {
        $thisFrames = $Frames + $HostSpareSeconds * $(if ($Uncapped) { 300 } else { $Fps })
        $thisSeconds = $Seconds + $HostSpareSeconds
    }
    $words = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$runHudson`"", '-GameFolder', "`"$folder`"", '-Level', $Level, '-Frames', $thisFrames, '-Seconds', $thisSeconds, '-Exe', $Exe, '-TimingsDontMatter', '-WhileInUse', '-Net', $word, '-NetPartner', "`"$partner`"")
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
    # Two Hudsons playing a level: the host does its list, and the one who joins follows him
    if ($Play) { $words += '-Play' }
    if ($word -eq 'host' -and $HostGun -gt 0) { $words += @('-PlayGun', $HostGun) }
    if ($word -eq 'join' -and $JoinGun -gt 0) { $words += @('-PlayGun', $JoinGun) }
    if ($StepSeconds -gt 0) { $words += @('-StepSeconds', $StepSeconds) }
    if ($Hostile) { $words += '-Hostile' }
    if ($kind) { $words += @('-HostileKind', $kind) }
    if ($kind -eq 'flood') { $words += @('-NetSecondPartner', "`"$joinFull`"") }
    if ($Relay) { $words += '-Relay' }
    if ($RemoteRelay) { $words += @('-RemoteRelay', $RemoteRelay) }
    # The line holds spaces, and Start-Process joins its words with nothing but spaces, so it goes in quotes (its rule has no quote in it)
    if ($ServersLine) { $words += @('-ServersLine', "`"$ServersLine`"") }
    # Through the relay, the joiner is the second copy (added 2026-11-09 by Melanikus (Fifth) for the AvP port)
    if ($Relay -and $word -eq 'join') { $words += '-SecondCopy' }
    # The host's answer and the joiner's way in through Find Games (added 2026-11-10 by Melanikus (Third) for the AvP port)
    if ($word -eq 'host' -and $HostAnswer) { $words += @('-Answer', $HostAnswer) }
    if ($word -eq 'host' -and $HostAnswerAfter -ne 0) { $words += @('-AnswerAfter', "$HostAnswerAfter") }
    if ($word -eq 'join' -and $JoinFindGames) { $words += '-FindGames' }
    if ($word -eq 'host' -and $HostRow) { $words += @('-HostRow', $HostRow) }
    if ($FilmMenus) { $words += '-FilmMenus' }
    if ($PageHold -ne 0) { $words += @('-PageHold', "$PageHold") }
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
        # Each socket as the record writes it, and whether the watch's rule (above) calls it a finding
        $sockets = @(Get-NetUDPEndpoint -OwningProcess $game.Id -ErrorAction SilentlyContinue | ForEach-Object {
            [pscustomobject]@{ Text = "udp $($_.LocalAddress):$($_.LocalPort)"; Off = (Test-SocketIsOffThisMachine 'udp' '' "$($_.LocalAddress)" '' '' $remoteRelayAddress $remoteRelayRoute) }
        })
        $sockets += @(Get-NetTCPConnection -OwningProcess $game.Id -ErrorAction SilentlyContinue | ForEach-Object {
            [pscustomobject]@{ Text = "tcp $($_.State) $($_.LocalAddress):$($_.LocalPort) to $($_.RemoteAddress):$($_.RemotePort)"; Off = (Test-SocketIsOffThisMachine 'tcp' "$($_.State)" "$($_.LocalAddress)" "$($_.RemoteAddress)" "$($_.RemotePort)" $remoteRelayAddress $remoteRelayRoute) }
        })

        foreach ($socket in $sockets)
        {
            $seenSockets["$($game.Path) $($socket.Text)"] = $true

            if ($socket.Off)
            {
                $outside += "$($game.Path) $($socket.Text)"
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
$limit = (Get-Date).AddSeconds($SecondsForTheHostToBeReady + 2 * $Seconds + $HostSpareSeconds + 180)
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

# #203, with Primary (#202): a DirectX call that fails is said once in that game's own log ("DirectX Error! : ..."), and nothing else
# shows it: a join once lost a frame that way on every run, unseen. So each game's log is read for those lines, wherever its run was
# kept (its output's "Kept in" line), and pair.txt says how many there were and the first, word for word. Any at all fails the pair
# (exit 112), even when everything else passed. Added 2026-11-10 by Melanikus (Third) for the AvP port.
$directXErrors = 0

foreach ($game in @('host', 'join', 'third'))
{
    $file = Join-Path $kept "$game-output.txt"

    if (-not (Test-Path -LiteralPath $file)) { continue }

    $keptLine = @(Get-Content -LiteralPath $file | Where-Object { $_ -match '^run-hudson: exit code .*Kept in (.+)$' }) | Select-Object -First 1
    $runFolder = if ($keptLine -and $keptLine -match 'Kept in (.+)$') { $Matches[1].Trim() } else { '' }
    $gameLog = if ($runFolder) { Join-Path $runFolder 'avp_log.txt' } else { '' }

    if (-not $gameLog -or -not (Test-Path -LiteralPath $gameLog -PathType Leaf))
    {
        $lines += "d3d_errors_$game=unknown: its log wasn't found"
        continue
    }

    $errors = @(Get-Content -LiteralPath $gameLog | Where-Object { $_ -like '*DirectX Error!*' })
    $lines += "d3d_errors_$game=$($errors.Count)"

    if ($errors.Count -gt 0)
    {
        $lines += "d3d_first_$game=$($errors[0].Trim())"
        Write-Host "run-net-pair: FINDING: the $game game's log has $($errors.Count) DirectX error line(s), the first: $($errors[0].Trim())"
        $directXErrors += $errors.Count
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
if ($directXErrors -gt 0) { exit 112 }
exit 0
