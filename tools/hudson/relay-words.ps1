# The words that point a Hudson run at a relay server, checked the same way by run-net-pair.ps1 and run-hudson.ps1, which both read this
# file. Written 2026-11-09 by Melanikus (Third) for the AvP port.
# Modified 2026-11-10 by Melanikus for the AvP port (#175): the game finds its server only in servers.cfg, beside bin\ (3dc/servers_cfg.h),
# so a run is given that file's one line, and the -dev settings the run used to write (RelayServer, RelayListPort, RelayListPin,
# RelayServerName and RelayKey) are gone, with -RelayListPort, -RelayListPin, -RelayServerName, -RelayKey and -LocalRelayPin. The line's
# fingerprint proves the list's key and the relay's keys at once, so nothing else is needed to trust the server.
#
#   -ServersLine "<address>[:<list port>:<relay port>] <fingerprint> <name>"
#                                   the one servers.cfg line the run's game reads: the server's address, written plainly, with its ports
#                                   (443 and 27500 unless it says), its fingerprint (64 small hex digits, servers_cfg.h says how it's
#                                   made), and its name as people see it. Without -RemoteRelay its address is 127.0.0.1, the server on
#                                   this machine; with -RemoteRelay it's that address, and its list is at port 443, the only HTTPS port
#                                   run-net-pair's watch lets through
#   -RemoteRelay <one IPv4 address> the one relay server beyond this machine the run may reach: the line's address, given on purpose, so a
#                                   line can never take a run off this machine by itself. Never a name: a servers.cfg line has none
#
# Several servers at once (added 2026-11-11 by Melanikus (Third) for the AvP port; the owner, 2026-11-11: every copy in a run reads the
# same servers.cfg, with its lines in the order the zip ships them). run-hudson.ps1 takes them this way; run-net-pair.ps1 still takes one:
#   -ServersLine "<line> | <line>"  several lines, in the file's order, with "|" between (the spaces around it are left out). The game's
#                                   line in use is the first, unless the run picks another. No line can hold a "|", so splitting there
#                                   never cuts one in two
#   -RemoteRelay "<address>,<address>"
#                                   an address for each line beyond this machine, with "," between and no spaces, each given on purpose
#
# Every word is input from outside (AVP Tao 3), so each is checked to its shape here, before it goes anywhere: into a file, or onto a
# command line. The game reads the line again by its own rules, which are the last word.

# One plain IPv4 address: four numbers 0-255 with dots between, no leading zeros, and nothing before or after (\A and \z: a "$" would
# let a final newline through)
function Test-HudsonRelayAddress([string]$text)
{
    if ($text -cnotmatch '\A(0|[1-9][0-9]{0,2})\.(0|[1-9][0-9]{0,2})\.(0|[1-9][0-9]{0,2})\.(0|[1-9][0-9]{0,2})\z') { return $false }
    foreach ($i in 1..4) { if ([int]$Matches[$i] -gt 255) { return $false } }
    return $true
}

# The most lines, and so addresses, a run may be given: as many as one find asks (3dc/relay_list.h, kRelayListMostVisits)
$hudsonMostServersLines = 8

# -RemoteRelay's addresses, as given: one, or several with "," between
function Get-HudsonRemoteRelays([string]$remoteRelay)
{
    if (-not $remoteRelay) { return @() }
    return @($remoteRelay -split ',')
}

# -ServersLine's lines, in the order given: one, or several with "|" between, each without the spaces around its "|"
function Get-HudsonServersLines([string]$lines)
{
    if (-not $lines) { return @() }
    return @($lines -split '\|' | ForEach-Object { $_.Trim(' ') })
}

# Why -RemoteRelay can't be used as given, or '' if it can (or isn't given). Modified 2026-11-11 by Melanikus (Third) for the AvP port:
# several addresses, each checked as the one used to be
function Get-HudsonRemoteRelayRefusal([string]$remoteRelay)
{
    if (-not $remoteRelay) { return '' }
    $relays = @(Get-HudsonRemoteRelays $remoteRelay)
    if ($relays.Count -gt $hudsonMostServersLines) { return "-RemoteRelay names at most $hudsonMostServersLines addresses, as many lines as one find asks" }

    foreach ($relay in $relays)
    {
        if (-not (Test-HudsonRelayAddress $relay)) { return "-RemoteRelay is plain IPv4 addresses with commas between, as servers.cfg lines give them, and never a name" }

        # Not this machine (that's -Relay alone), nor "any address", nor one that's everyone's at once
        if ($relay -match '\A(0|127|22[4-9]|2[3-5]\d)\.' -or $relay -match '\A169\.254\.') { return "-RemoteRelay's address isn't one a relay on another machine can have" }
    }

    if (@($relays | Select-Object -Unique).Count -ne $relays.Count) { return "-RemoteRelay names an address twice" }
    return ''
}

# Why -ServersLine's lines, with -RemoteRelay's addresses, can't be used as given, or '' if they can (added 2026-11-11 by Melanikus (Third)
# for the AvP port). Each line by Get-HudsonServersLineRefusal; then no server twice, and no address given that no line names, as an
# address nobody uses would only widen what the watch lets through.
function Get-HudsonServersLinesRefusal([string]$lines, [string]$remoteRelay)
{
    $all = @(Get-HudsonServersLines $lines)
    if ($all.Count -eq 0) { return "-ServersLine is empty" }
    if ($all.Count -gt $hudsonMostServersLines) { return "-ServersLine has at most $hudsonMostServersLines lines, as many as one find asks" }
    $addresses = @()

    foreach ($line in $all)
    {
        $refusal = Get-HudsonServersLineRefusal $line $remoteRelay
        if ($refusal) { return $refusal }
        $addresses += ($line -split ' ')[0] -replace ':.*\z', ''
    }

    if (@($addresses | Select-Object -Unique).Count -ne $addresses.Count) { return "-ServersLine names one server's address twice" }

    foreach ($relay in @(Get-HudsonRemoteRelays $remoteRelay))
    {
        if ($addresses -cnotcontains $relay) { return "-RemoteRelay's $relay isn't the address of any -ServersLine line" }
    }

    return ''
}

# The address of the line in use: the first line, or the one the run picks (1 for the first). '' when there's no such line.
function Get-HudsonLineInUseAddress([string]$lines, [int]$pick)
{
    $all = @(Get-HudsonServersLines $lines)
    $place = if ($pick -gt 0) { $pick } else { 1 }
    if ($place -gt $all.Count) { return '' }
    return ($all[$place - 1] -split ' ')[0] -replace ':.*\z', ''
}

# Why -ServersLine can't be used as given, or '' if it can. Hudson's rule is narrower than the game's (servers_cfg.h): one space between the
# fields, and a name of letters, digits, spaces and ' ( ) - . , only, so the line can go onto a command line in quotes as it is. Each reason
# is its own, so a test can tell them apart.
function Get-HudsonServersLineRefusal([string]$line, [string]$remoteRelay)
{
    if (-not $line) { return "-ServersLine is empty" }
    if ($line -cnotmatch '\A([^ ]+) ([^ ]+) ([^ ].*)\z') { return "-ServersLine is three things with one space between: the address, the fingerprint and the name" }
    $where = $Matches[1]
    $fingerprint = $Matches[2]
    $name = $Matches[3]

    if ($fingerprint -cnotmatch '\A[0-9a-f]{64}\z') { return "-ServersLine's fingerprint is 64 small hex digits, and nothing else" }
    if ($name.Length -gt 31 -or $name -cnotmatch "\A[A-Za-z0-9'()\-.,]([A-Za-z0-9 '()\-.,]*[A-Za-z0-9'()\-.,])?\z" -or $name.Contains('  ')) { return "-ServersLine's name is 1 to 31 letters, digits and ' ( ) - . , with single spaces between words" }

    # The address alone, or the address and both ports: 1 to 65535 each, with no leading zeros
    $addressShape = "-ServersLine's address is one plain IPv4 address, or that and :<list port>:<relay port>"
    if ($where -cnotmatch '\A([0-9.]+)(:([1-9][0-9]{0,4}):([1-9][0-9]{0,4}))?\z') { return $addressShape }
    $address = $Matches[1]
    $listPort = if ($Matches[3]) { [int]$Matches[3] } else { 443 }
    $relayPort = if ($Matches[4]) { [int]$Matches[4] } else { 27500 }
    if (-not (Test-HudsonRelayAddress $address)) { return $addressShape }
    if ($listPort -gt 65535 -or $relayPort -gt 65535) { return "-ServersLine's ports are 1 to 65535" }

    # Where it may go: this machine, or an address beyond it the run was given on purpose (one of -RemoteRelay's, modified 2026-11-11 by
    # Melanikus (Third) for the AvP port)
    if (-not $remoteRelay -and $address -ne '127.0.0.1') { return "-ServersLine names another machine, so it needs -RemoteRelay with the same address: a run leaves this machine only when asked to" }
    if ($remoteRelay -and @(Get-HudsonRemoteRelays $remoteRelay) -cnotcontains $address) { return "-ServersLine's address isn't one of -RemoteRelay's" }
    if ($remoteRelay -and $listPort -ne 443) { return "-ServersLine's list beyond this machine is at port 443, the only HTTPS port run-net-pair's watch lets through" }
    return ''
}

# The first line of the servers.cfg a run writes: the game skips it (a note), and the next run knows the file for its own by it
$hudsonServersFileMark = '# Written for a Hudson run by tools\hudson\relay-words.ps1. The next Hudson run replaces it or takes it away; keep nothing else here.'

# The servers.cfg the game in $folder reads: beside its bin folder, or none if the game isn't in a folder called bin (servers_cfg_file.h)
function Get-HudsonServersFilePath([string]$folder)
{
    if ((Split-Path -Leaf $folder) -ne 'bin') { return '' }
    return Join-Path (Split-Path -Parent $folder) 'servers.cfg'
}

# Whether servers.cfg's text names any server: a line that isn't empty and isn't a note. Anything this can't make sense of counts as naming
# one, so a file that would reach a server is never missed.
function Test-HudsonServersTextNamesAServer([string]$text)
{
    foreach ($fileLine in ($text -split "`n"))
    {
        $bare = $fileLine.Trim([char[]]@(' ', "`t", "`r"))
        if ($bare -and -not $bare.StartsWith('#', [System.StringComparison]::Ordinal)) { return $true }
    }
    return $false
}

# The file that says a game folder is a copy made for Hudson's runs: a plain file of this name beside bin\, put there by whoever made the
# copy. A release's folder, a player's and Steam's never have one, so a run never writes a servers.cfg into any of those (PO, 2026-11-10).
$hudsonRunCopyMark = 'hudson-run-copy.txt'

# Why the copy holding bin\ in $folder isn't one a run may write servers.cfg into, or '' if it is: it has the mark, isn't Steam's, and
# neither it nor any folder on the way to it is a link to somewhere else
function Get-HudsonRunCopyRefusal([string]$folder)
{
    $root = Get-Item -LiteralPath (Split-Path -Parent $folder) -Force
    if ($root.FullName -match 'steamapps') { return "$($root.FullName) looks like a Steam folder" }

    for ($place = $root; $place; $place = $place.Parent)
    {
        if ($place.Attributes -band [System.IO.FileAttributes]::ReparsePoint) { return "$($place.FullName) is a link to somewhere else" }
    }

    $mark = Join-Path $root.FullName $hudsonRunCopyMark
    if (-not (Test-Path -LiteralPath $mark -PathType Leaf)) { return "$($root.FullName) has no $hudsonRunCopyMark, so it isn't a copy made for Hudson's runs, and its servers.cfg isn't a run's to write" }
    if ((Get-Item -LiteralPath $mark -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint) { return "$mark is a link to somewhere else" }
    return ''
}

# Makes the servers.cfg the game in $folder reads say exactly what this run was given: $line alone, or, with no line, no server at all.
# Returns '' when it's so, or why it can't be. Before it writes or takes anything away it looks: it writes only into a copy made for
# Hudson's runs (Get-HudsonRunCopyRefusal), and a file that isn't a plain one, or a servers.cfg a run didn't write that names a server, is
# someone else's, and is left as it is (the run doesn't start). One that names no server, such as a release's before it ships a line, is
# left too, and a run with no line can go ahead beside it.
function Set-HudsonServersFile([string]$folder, [string]$line)
{
    $path = Get-HudsonServersFilePath $folder

    if (-not $path)
    {
        if ($line) { return "-ServersLine needs the game in a folder called bin: the game reads servers.cfg only beside its bin folder" }
        return ''
    }

    $wanted = "$hudsonServersFileMark`n$line`n"
    $ours = $false

    if ($line)
    {
        $copyRefusal = Get-HudsonRunCopyRefusal $folder
        if ($copyRefusal) { return $copyRefusal }
    }

    if (Test-Path -LiteralPath $path)
    {
        $item = Get-Item -LiteralPath $path -Force
        if ($item.PSIsContainer -or ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) { return "$path isn't a plain file, so it's left as it is" }
        if ($item.Length -gt 16384) { return "$path is bigger than any servers.cfg can be, so it's left as it is" }
        $text = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::ASCII)
        $ours = $text.StartsWith("$hudsonServersFileMark`n", [System.StringComparison]::Ordinal)

        if (-not $ours)
        {
            if (Test-HudsonServersTextNamesAServer $text) { return "$path names a server, and a Hudson run didn't write it: move it away first, as a run uses only the server it's given" }
            if ($line) { return "$path is there, and a Hudson run didn't write it: move it away first, so this run's line can go there" }
            return ''
        }

        # Already as wanted: written by the other game of a pair a moment ago, so it isn't written again while that game may be reading it
        if ($line -and $text -ceq $wanted) { return '' }
    }

    # A run's own file, and this run has no line: it goes, so this run reaches no server. Only from a copy made for Hudson's runs, as that's
    # the only place a run writes one; anywhere else it names a server nobody here asked for, and the run doesn't start.
    if (-not $line)
    {
        if ($ours)
        {
            $copyRefusal = Get-HudsonRunCopyRefusal $folder
            if ($copyRefusal) { return "$path names a server, and $copyRefusal" }
            Remove-Item -LiteralPath $path -Force
        }
        return ''
    }

    # Written whole beside it, then moved into place, so the game never reads half of it. One left by a run that was cut short goes first,
    # if it's a plain file: writing through a link would write wherever the link points.
    $temporary = "$path.hudson-new"

    if (Test-Path -LiteralPath $temporary)
    {
        $left = Get-Item -LiteralPath $temporary -Force
        if ($left.PSIsContainer -or ($left.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) { return "$temporary isn't a plain file, so it's left as it is" }
        Remove-Item -LiteralPath $temporary -Force
    }

    [System.IO.File]::WriteAllText($temporary, $wanted, [System.Text.Encoding]::ASCII)

    # No copy of the old one is kept. That's [NullString]::Value, not $null: PowerShell hands $null to a .NET text parameter as an empty
    # text, which Replace takes for a path and refuses (found by -CheckTheWatch's first run, 2026-11-10)
    if ($ours) { [System.IO.File]::Replace($temporary, $path, [NullString]::Value) } else { [System.IO.File]::Move($temporary, $path) }
    return ''
}
