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

# Why -RemoteRelay can't be used as given, or '' if it can (or isn't given)
function Get-HudsonRemoteRelayRefusal([string]$remoteRelay)
{
    if (-not $remoteRelay) { return '' }
    if (-not (Test-HudsonRelayAddress $remoteRelay)) { return "-RemoteRelay is one plain IPv4 address, as a servers.cfg line gives it, and never a name" }

    # Not this machine (that's -Relay alone), nor "any address", nor one that's everyone's at once
    if ($remoteRelay -match '\A(0|127|22[4-9]|2[3-5]\d)\.' -or $remoteRelay -match '\A169\.254\.') { return "-RemoteRelay's address isn't one a relay on another machine can have" }
    return ''
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

    # Where it may go: this machine, or the one address beyond it the run was given on purpose
    if (-not $remoteRelay -and $address -ne '127.0.0.1') { return "-ServersLine names another machine, so it needs -RemoteRelay with the same address: a run leaves this machine only when asked to" }
    if ($remoteRelay -and $address -ne $remoteRelay) { return "-ServersLine's address isn't -RemoteRelay's" }
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

# -ShippedServers (added 2026-11-11 by Melanikus (Third) for the AvP port, for the owner's picture of a New York and a Frankfurt game): the
# copy keeps the build's own servers.cfg, every line it ships, and the run writes nothing into it. Returns '' when the file beside bin\ in
# $folder is that one, or why not: it has to be a plain file (not a link), no bigger than any servers.cfg can be, not one a run wrote, and
# have exactly the SHA-256 $sha (the build's own file's, given by whoever made the copy). With $remoteRelay, one of its lines has to start
# with that address, so the one relay beyond this machine a net= run may use is one the file ships. The game reads the file again by its
# own rules, which are the last word.
function Get-HudsonShippedServersRefusal([string]$folder, [string]$sha, [string]$remoteRelay)
{
    $path = Get-HudsonServersFilePath $folder
    if (-not $path) { return "-ShippedServers needs the game in a folder called bin: the game reads servers.cfg only beside its bin folder" }
    if (-not (Test-Path -LiteralPath $path)) { return "$path isn't there, so there are no shipped servers to keep" }

    $item = Get-Item -LiteralPath $path -Force
    if ($item.PSIsContainer -or ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) { return "$path isn't a plain file" }
    if ($item.Length -gt 16384) { return "$path is bigger than any servers.cfg can be" }

    # Read once, and both the hash and the text are of those same bytes
    $bytes = [System.IO.File]::ReadAllBytes($path)
    $hasher = [System.Security.Cryptography.SHA256]::Create()
    $found = ([System.BitConverter]::ToString($hasher.ComputeHash($bytes)) -replace '-', '').ToLowerInvariant()
    $hasher.Dispose()
    if ($found -cne $sha) { return "$path has the SHA-256 $found, not the build's own $sha" }

    $text = [System.Text.Encoding]::ASCII.GetString($bytes)
    if ($text.StartsWith("$hudsonServersFileMark`n", [System.StringComparison]::Ordinal)) { return "$path was written by a Hudson run, so it isn't the build's own" }

    if ($remoteRelay)
    {
        $lineThere = $false
        foreach ($fileLine in ($text -split "`n"))
        {
            $bare = $fileLine.Trim([char[]]@(' ', "`t", "`r"))
            foreach ($after in @(' ', "`t", ':'))
            {
                if ($bare.StartsWith("$remoteRelay$after", [System.StringComparison]::Ordinal)) { $lineThere = $true }
            }
        }
        if (-not $lineThere) { return "-RemoteRelay $remoteRelay isn't the address of any line in $path" }
    }

    return ''
}
