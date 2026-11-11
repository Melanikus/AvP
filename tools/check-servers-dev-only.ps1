<#
Says whether a player's game could reach any of our servers: the relay list's server, or a relay. Written 2026-11-09 by Melanikus for
the AvP port.

THE RULING IT HOLDS. The owner, 2026-11-09: "I want the code to test but NOTHING that can be actually accessed by people with the
release", and "don't stop pushing working code just because it's not completed". So the code that talks to our servers comes into the
game as it's written, and stays asleep: it runs only with -dev, which testers type and players don't. This check proves that on every
pull request and before every release, and refuses either if it isn't so. Only the owner lifts this rule; until he does, nothing here
is to be loosened to let a change through.

HOW IT KNOWS. The game's own code is 3dc and win32. It checks:
1. DevMode, which -dev turns on, starts off, and only one line makes it true: the one that reads -dev from the command line.
2. Every call that reaches beyond this PC by name or by web address (WinHTTP's, WinINet's, a name lookup) is in 3dc/relay_list.cpp,
   the relay list's own file, and nowhere else.
3. The game knows of no server of its own. Its server is a line of servers.cfg, beside bin\ (modified 2026-11-10 by Melanikus for the
   AvP port, #175), and the servers.cfg the repository ships, which a release puts there, names none: each of its lines is empty or a
   note. Only the relay list's file reads the game's servers.cfg, besides the reader's own files.
4. Without -dev, Relay_ServerName gives back no name at all, whatever servers.cfg says: the first thing it does is if (!DevMode)
   { return ""; }.
5. The list starts only with a name: with none, it makes nothing and calls nothing. Its web calls go to the line's address alone,
   written in one place from the line's four numbers, so never a name; and the one relay it's told of is at that same address, the only
   relay the game learns of. So with no name, no relay is ever reached either.
Until protocol version 2 is on for players: then tools/check-online-play.ps1 (#175's last part) takes this rule's place.
6. Every setting read from the [Network] section, and every setting anywhere whose name speaks of a relay, a server, a pin, a key or
   a fingerprint, is ignored without -dev, or only read with it.
Anything it can't place is refused with the file and the line, so a new way out is read by a person before it's let through. Playing
by a typed address, or finding games on the same network, isn't ours to stop: those reach the PCs the players choose, not our servers.

It reads the code as text, with the comments taken out, so it's strict about the forms it lets through: code written another way is
refused, never guessed at.
#>
param(
    [string]$Repository = ''
)

$ErrorActionPreference = 'Stop'

# Worked out here and not in param(), because older PowerShell doesn't know where the script is until it's finished reading that.
if (-not $Repository)
{
    $Repository = Split-Path -Parent $PSScriptRoot
}

$Repository = (Resolve-Path -LiteralPath $Repository).Path

function Stop-Check([string]$why)
{
    Write-Host "REFUSED: $why"
    exit 1
}

# The relay list's own file: the one place a call beyond this PC by name may be
$listFile = '3dc/relay_list.cpp'

# ---- Reading the code

$sourceFiles = @()

foreach ($folder in @('3dc', 'win32'))
{
    $path = Join-Path $Repository $folder

    if (Test-Path -LiteralPath $path)
    {
        $sourceFiles += @(Get-ChildItem -LiteralPath $path -Recurse -File | Where-Object { $_.Extension -match '^\.(c|cc|cpp|cxx|h|hpp|inl)$' })
    }
}

# One pass finds every comment, string and character in the order they come, so that a // inside a string isn't taken for a comment, and
# a quote inside a comment isn't taken for a string. Each is kept as spaces the same size, with its line ends, so line numbers stay true.
$commentOrString = [regex]'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\\n])*"|''(?:\\.|[^''\\\n])*'''
$blankComments = [System.Text.RegularExpressions.MatchEvaluator]{ param($m) if ($m.Value.StartsWith('/')) { $m.Value -replace '[^\n]', ' ' } else { $m.Value } }
$blankAll = [System.Text.RegularExpressions.MatchEvaluator]{ param($m) $m.Value.Substring(0, 1) + ($m.Value.Substring(1) -replace '[^\n]', ' ') }

$code = @{}        # each file's code with comments and strings blanked
$withWords = @{}   # the same with the strings kept, for the rules that read words

foreach ($file in $sourceFiles)
{
    $relative = $file.FullName.Substring($Repository.Length).TrimStart('\', '/') -replace '\\', '/'
    $text = [System.IO.File]::ReadAllText($file.FullName) -replace "`r", ''
    $withWords[$relative] = $commentOrString.Replace($text, $blankComments)
    $code[$relative] = $commentOrString.Replace($text, $blankAll)
}

function Get-LineNumber([string]$text, [int]$at)
{
    return ($text.Substring(0, $at).Split("`n")).Count
}

# The body of a function or block, given where its { is: up to the } that closes it. The code read has no comments or strings, so every
# brace in it is a real one.
function Get-Block([string]$text, [int]$open)
{
    $depth = 0

    for ($i = $open; $i -lt $text.Length; $i++)
    {
        if ($text[$i] -eq '{') { $depth++ }
        elseif ($text[$i] -eq '}')
        {
            $depth--
            if ($depth -eq 0) { return $text.Substring($open + 1, $i - $open - 1) }
        }
    }

    return $null
}

# Every definition of a function by this name: the file, its line, its body read from the code, and the same with its words
function Get-Definitions([string]$name)
{
    $found = @()
    # A function in a namespace is indented, so a definition may start after spaces or tabs
    $pattern = [regex]('(?m)^[ \t]*[A-Za-z_][\w:<>,\*&\s]*?[\s\*&](?:\w+::)?' + [regex]::Escape($name) + '\s*\([^;{]*?\)\s*(?:const\s*)?\{')

    foreach ($relative in $code.Keys)
    {
        foreach ($m in $pattern.Matches($code[$relative]))
        {
            $open = $m.Index + $m.Length - 1
            $found += [pscustomobject]@{ File = $relative; Line = (Get-LineNumber $code[$relative] $m.Index); Body = (Get-Block $code[$relative] $open); WithWords = (Get-Block $withWords[$relative] $open) }
        }
    }

    return $found
}

function Get-Flat([string]$text)
{
    return ($text -replace '\s+', ' ').Trim()
}

# ---- What there is to check

# A call that reaches beyond this PC by a name or a web address
$outwardCall = [regex]'\b(WinHttp(Open|Connect|OpenRequest|SendRequest|ReceiveResponse|ReadData|WriteData)|Internet(Open|Connect|OpenUrl)[AW]?|HttpOpenRequest[AW]?|HttpSendRequest[AW]?|URLDownloadToFile[AW]?|URLOpen(Blocking)?Stream[AW]?|getaddrinfo|GetAddrInfo(W|Ex[AW]?)?|gethostbyname|WSAAsyncGetHostByName|DnsQuery(_A|_W|_UTF8|Ex)?|enet_address_set_host)\s*\('

$outward = @()

foreach ($relative in $code.Keys)
{
    foreach ($m in $outwardCall.Matches($code[$relative]))
    {
        $outward += [pscustomobject]@{ File = $relative; Line = (Get-LineNumber $code[$relative] $m.Index); Name = $m.Groups[1].Value }
    }
}

# Every setting read, with its section and its name as written
$settingRead = [regex]'\bConfig_Get\w*\s*\(\s*"([^"]*)"\s*,\s*"([^"]*)"'
$reads = @()

foreach ($relative in $withWords.Keys)
{
    foreach ($m in $settingRead.Matches($withWords[$relative]))
    {
        $reads += [pscustomobject]@{ File = $relative; Line = (Get-LineNumber $withWords[$relative] $m.Index); At = $m.Index; Section = $m.Groups[1].Value; Key = $m.Groups[2].Value }
    }
}

$serverReads = @($reads | Where-Object { $_.Section -eq '[Network]' -or $_.Key -match '(?i)relay|server|pin|key|fingerprint' })
# Every file that reads the game's servers.cfg, or its text, with the reader's own (servers_cfg.h, servers_cfg_file.h). Added 2026-11-10 by
# Melanikus for the AvP port (#175).
$serversCfgReader = [regex]'\bServersCfg_Read(TheGames|Folder)?\s*\('
$serversCfgOwnFiles = @('3dc/servers_cfg.h', '3dc/servers_cfg.cpp', '3dc/servers_cfg_file.h', '3dc/servers_cfg_file.cpp')
$serversCfgUsers = @($code.Keys | Where-Object { $serversCfgReader.IsMatch($code[$_]) })

if ($outward.Count -eq 0 -and $serverReads.Count -eq 0 -and $serversCfgUsers.Count -eq 0)
{
    Write-Host "None of the game's server code is in this commit, so a player's game can't reach a server."
    exit 0
}

# ---- Rule 1: DevMode starts off, and only -dev on the command line turns it on

$devStarts = @()
$devWrites = @()

foreach ($relative in $code.Keys)
{
    $text = $code[$relative]

    foreach ($m in ([regex]'(?m)^\s*(?:static\s+)?(?:int|bool|BOOL)\s+DevMode\b[^;]*;').Matches($text)) { $devStarts += [pscustomobject]@{ File = $relative; Line = (Get-LineNumber $text $m.Index); Text = (Get-Flat $m.Value) } }

    foreach ($m in ([regex]'\bDevMode\s*(?:=(?!=)|\+\+|--|[-+*/|&^]=)|(?:\+\+|--)\s*DevMode\b|(?<!&)&\s*DevMode\b').Matches($text))
    {
        $lineStart = $text.LastIndexOf("`n", [Math]::Max(0, $m.Index - 1)) + 1
        $lineEnd = $text.IndexOf("`n", $m.Index)
        if ($lineEnd -lt 0) { $lineEnd = $text.Length }
        $lineText = (Get-Flat $text.Substring($lineStart, $lineEnd - $lineStart))
        if ($lineText -match '^(?:static\s+)?(?:int|bool|BOOL)\s+DevMode\b') { continue }   # its definition, checked above
        $wordsLine = Get-Flat $withWords[$relative].Substring($lineStart, $lineEnd - $lineStart)
        $devWrites += [pscustomobject]@{ File = $relative; Line = (Get-LineNumber $text $m.Index); Text = $wordsLine }
    }
}

if ($devStarts.Count -ne 1 -or $devStarts[0].Text -notmatch '^(?:static\s+)?int\s+DevMode\s*=\s*0\s*;$')
{
    Stop-Check "DevMode must be defined once, as int DevMode = 0; it is: $(($devStarts | ForEach-Object { "$($_.File) line $($_.Line): $($_.Text)" }) -join '; ')"
}

foreach ($write in $devWrites)
{
    if ($write.Text -cne 'DevMode = CommandLineHasWord(command_line, "-dev") ? 1 : 0;')
    {
        Stop-Check "$($write.File) line $($write.Line) changes DevMode: only the line that reads -dev from the command line may"
    }
}

if ($devWrites.Count -ne 1)
{
    Stop-Check "DevMode must be set exactly once, from -dev on the command line; it's set $($devWrites.Count) times"
}

# ---- Rule 2: every call beyond this PC is in the relay list's file

foreach ($call in $outward)
{
    if ($call.File -ne $listFile)
    {
        Stop-Check "$($call.File) line $($call.Line) calls $($call.Name), which reaches beyond this PC: only $listFile may, where the list's rules hold it"
    }
}

# ---- Rules 3 to 5: no server of the game's own, none without -dev, and the list and its relay need one. Modified 2026-11-10 by Melanikus
# for the AvP port (#175): the server is a servers.cfg line, in place of kRelayBuiltInServer and the -dev setting RelayServer.

foreach ($relative in @($serversCfgUsers | Where-Object { $_ -ne $listFile -and $serversCfgOwnFiles -notcontains $_ }))
{
    Stop-Check "$relative reads servers.cfg: only $listFile may, where the list's rules hold its server"
}

if ($outward.Count -gt 0 -or $serversCfgUsers.Count -gt 0)
{
    if (-not $withWords.ContainsKey($listFile))
    {
        Stop-Check "the game calls beyond this PC or reads servers.cfg, but $listFile isn't there, so this check can't say where the calls go"
    }

    # Rule 3: the servers.cfg the repository ships, the one a release puts beside bin\, names no server. A line is a note if it starts
    # with #, after any spaces or tabs, and nothing else counts as one.
    $shipped = Join-Path $Repository 'servers.cfg'

    if (Test-Path -LiteralPath $shipped -PathType Leaf)
    {
        $lineNumber = 0

        foreach ($fileLine in ([System.IO.File]::ReadAllText($shipped) -split "`n"))
        {
            $lineNumber++
            $bare = $fileLine.Trim([char[]]@(' ', "`t", "`r"))

            if ($bare -and -not $bare.StartsWith('#', [System.StringComparison]::Ordinal))
            {
                Stop-Check "servers.cfg line $lineNumber names a server: until protocol version 2 is on for players, the servers.cfg a release ships names none"
            }
        }
    }

    # Rule 4: without -dev, no name at all, before anything else is done
    $naming = @(Get-Definitions 'Relay_ServerName')

    if ($naming.Count -ne 1 -or $naming[0].File -ne $listFile)
    {
        Stop-Check "Relay_ServerName must be defined once, in $listFile"
    }

    if (-not (Get-Flat $naming[0].WithWords).StartsWith('if (!DevMode) { return ""; }', [System.StringComparison]::Ordinal))
    {
        Stop-Check "Relay_ServerName ($listFile line $($naming[0].Line)) must give back no name without -dev, before anything else: if (!DevMode) { return `"`"; }"
    }

    # Rule 5: the list's web calls go to the line's address alone, and it starts only with a name
    $listCode = $code[$listFile]
    $listFlat = Get-Flat $withWords[$listFile]

    foreach ($m in ([regex]'\bWinHttpConnect\s*\(\s*(\w+)\s*,\s*([^,]+?)\s*,').Matches($listCode))
    {
        if ((Get-Flat $m.Groups[2].Value) -cne 'shared->server')
        {
            Stop-Check "$listFile line $(Get-LineNumber $listCode $m.Index) calls WinHttpConnect to $(Get-Flat $m.Groups[2].Value): the list's calls may only go to shared->server, the line's address"
        }
    }

    # The server is written in one place only, from the line's four numbers, so it's an address and never a name; and it's used in no other
    # way than these: called (above), written there, and handed to networking.cpp as the relay's address in words
    $serverWrite = 'swprintf(made->server, sizeof(made->server) / sizeof(made->server[0]), L"%u.%u.%u.%u", (line->address >> 24) & 0xFFu, (line->address >> 16) & 0xFFu, (line->address >> 8) & 0xFFu, line->address & 0xFFu);'
    $serverUses = @('WinHttpConnect(session, shared->server,', $serverWrite, 'snprintf(relays[0].host, sizeof(relays[0].host), "%ls", shared->server);')

    if (([regex]::Matches($listFlat, [regex]::Escape($serverWrite))).Count -ne 1)
    {
        Stop-Check "$listFile must write the list's server in one place only, from the line's four numbers: $serverWrite"
    }

    $leftOver = $listFlat
    foreach ($use in $serverUses) { $leftOver = $leftOver.Replace($use, '') }

    if ($leftOver -match '(->|\.)server\b')
    {
        Stop-Check "$listFile uses the list's server in a way this check doesn't know: only $($serverUses -join ' or ')"
    }

    # The relay is at the line's address too, written in one place
    $relayWrites = @([regex]::Matches($listFlat, '->relayAddress\s*=(?!=)[^;]*;'))

    if ($relayWrites.Count -ne 1 -or $relayWrites[0].Value -cne '->relayAddress = htonl(line->address);')
    {
        Stop-Check "$listFile must write the relay's address in one place only, from the line's address: made->relayAddress = htonl(line->address);"
    }

    $starts = @(Get-Definitions 'Start' | Where-Object { $_.File -eq $listFile -and $_.Body.Contains('made->server') })

    if ($starts.Count -ne 1)
    {
        Stop-Check "$listFile must start the list in one function, the one that writes its server"
    }

    $start = Get-Flat $starts[0].WithWords
    $refuseAt = $start.IndexOf('if (!Relay_ServerName()[0]) { return false; }')
    $lineAt = $start.IndexOf('const ServersCfgServer *const line = Server();')
    $writeAt = $start.IndexOf($serverWrite)
    $relayAt = $start.IndexOf('made->relayAddress = htonl(line->address);')

    if ($refuseAt -lt 0 -or $lineAt -lt $refuseAt -or $writeAt -lt $lineAt -or $relayAt -lt $lineAt)
    {
        Stop-Check "the list's start ($listFile line $($starts[0].Line)) must make nothing when Relay_ServerName gives no name: if (!Relay_ServerName()[0]) { return false; }, then const ServersCfgServer *const line = Server();, before it writes its server and its relay"
    }
}

# ---- Rule 6: every server setting is ignored without -dev, or read only with it

foreach ($read in $serverReads)
{
    $text = $withWords[$read.File]
    $flatAfter = Get-Flat $text.Substring($read.At, [Math]::Min(1200, $text.Length - $read.At))

    # Form 1 and 2: read into a string named setting, then at once: nothing for an empty one, and without -dev it's left unread
    $form = [regex]::Match($flatAfter, '^Config_Get\w*\("[^"]*", "[^"]*", "[^"]*"\); (if \(setting\.empty\(\)\) \{ \} else if \(!DevMode\) \{|if \(!DevMode\) \{)')
    $readsIntoSetting = [regex]::IsMatch((Get-Flat $text.Substring([Math]::Max(0, $read.At - 40), [Math]::Min(40, $read.At))), 'std::string const setting =$')

    if ($form.Success -and $readsIntoSetting)
    {
        $open = $text.IndexOf('{', $read.At + ($text.Substring($read.At).IndexOf('DevMode')))
        $ignored = Get-Block $text $open

        # Without -dev the setting may only be asked whether it's empty, to say so in the log
        if ($null -ne $ignored -and ($ignored -replace 'setting\.empty\(\)', '') -notmatch '\bsetting\b')
        {
            continue
        }
    }

    # Form 3: read only inside a block that runs with -dev: the innermost block round the read starts if (DevMode)
    $depth = 0
    $blockStart = -1

    for ($i = $read.At - 1; $i -ge 0; $i--)
    {
        if ($code[$read.File][$i] -eq '}') { $depth++ }
        elseif ($code[$read.File][$i] -eq '{')
        {
            if ($depth -eq 0) { $blockStart = $i; break }
            $depth--
        }
    }

    if ($blockStart -ge 0)
    {
        $before = $code[$read.File].Substring([Math]::Max(0, $blockStart - 200), [Math]::Min(200, $blockStart))
        if ((Get-Flat $before) -match 'if \(DevMode\)$') { continue }
    }

    Stop-Check "$($read.File) line $($read.Line) reads the setting $($read.Key) from $($read.Section), and a player's game would use it: a server's setting must be ignored without -dev, or read only with it"
}

Write-Host "A player's game can't reach a server: servers.cfg names none, Relay_ServerName gives none without -dev, every call beyond this PC is in $listFile and needs one, and $($serverReads.Count) server settings are read only with -dev."

# Said outright, because GitHub ends a step by exiting with whatever the last program to run left behind, whatever this script thought.
exit 0
