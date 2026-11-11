<#
Says whether a player's game plays online only the safe way: only with the servers its servers.cfg names, only by protocol version 2,
only sealed, and plain only at home. Written 2026-11-09 by Melanikus for the AvP port, as check-servers-dev-only.ps1 (beta.1's rule, that
a player's game reached no server at all), rewritten the same night for beta.2 as this, moved to the one short servers.cfg line on
2026-11-10, and made the release's rule the same day (#175 PR 3b), in check-servers-dev-only.ps1's place.

THE RULINGS IT HOLDS. The owner, 2026-11-09 night: "We want people from all over the world to connect and play with as little hurdle
as possible while safe", "Keep approval always", "Keep plain LAN at home", "Trust the pin alone", and, on why the game itself must hold
every rule: "20 years from now, we won't be there to enforce tao 1, our keys and tokens will have expired". 2026-11-10: one file ("10
years from now someone may deploy it and say 'Ok replace the original v1.x config with new server ips'"), and a short line ("Must be
something simple we can easily instruct ppl to switch/replace as there is no server discovery"). So the game reaches only the servers
written in servers.cfg, beside bin\, each one line: its IPv4 address (with its two ports, or the usual ones), its fingerprint (one
SHA-256 over the list's key and the relay's id: the server's CRYPTO.md 12.2), and its name (#175). It reaches them only by protocol
version 2, only sealed, and -dev weakens none of it. A plain game stays, between addresses on the home network only. This check proves
that on every pull request and before every release, and refuses either if it isn't so. Only the owner changes these rules. Until he
does, nothing here is loosened to let a change through.

HOW IT KNOWS. The game's own code is 3dc and win32. It reads the code as text, with the comments taken out, and it's strict about forms:
code written another way is refused, never guessed at. The forms are the ones the code has (AvP-docs' beta2-online-rule.md and
issue-175-servers-cfg.md say why), and each refusal names its rule:
D. -dev decides nothing online: DevMode isn't named in the network's own files (networking, relay_*, net_*, servers_cfg*).
N. No name is looked up: no call that turns a name into an address, anywhere. Web calls are only in 3dc/relay_list.cpp, and they go
   only to the list's server the list was started with.
C. No server comes from the settings file: nothing is read from AliensVsPredator.cfg's [Network] section, nor any setting whose name
   has Relay, Server, Pin or Fingerprint in it.
F. servers.cfg, at the repository's root and packaged beside bin, is the list of the servers the game ships with. It's held to the
   game's own rules (3dc/servers_cfg.h) and stricter ones, since it's ours: plain line ends, no mark at the start, no empty line, no
   space at a line's ends, and no server at a home network's, this machine's or a shared provider's address. The servers it ships are
   printed, for the release's text: the owner's approval of the release is their review.
R. The route: only the list's file reads servers.cfg (besides the reader's own files); the list's server is written in one place, from
   the line's four numbers, so it's an address and never a name; the relay is at the line's address too; and the list starts only for a
   line, making nothing without one.
K. The key: the server is trusted by its line's fingerprint alone. Every web call goes through CallChecked, which sends nothing to a
   server whose keys didn't match and nothing but the first call before they're checked; the list's key is read and compared just
   before the first byte goes, every byte of it, and a call whose key doesn't match is closed unsent; a body goes only after that; and
   the key that matched is kept only when the list's key and the relay's keys make the line's fingerprint.
Y. The player's yes (#175's last part, added 2026-11-10): the list starts only for a line the player has said yes to, before it makes
   anything; the one way past that is a Hudson run, which this file only reads (tools/check-hudson-asleep.ps1 proves a player's game
   never sets it); a yes is the line's own hash among the kept ones, and only the player's yes adds one; the list says a line without
   it needs it; and an online host isn't hosted before it.
V. Version 2 only: every switch the v2 draft had is gone, and beta.1's version 1 relay path with it (the plain relay messages, the echo
   picker, the commit word), so nothing can choose version 1 or a plain game after a relay. The network version is 116 or later, and the
   relay's messages are read only once the channel has opened them.
S. Sealed or nothing: an online host goes through the relay, sealing what it sends, or it isn't hosted at all; a lost relay ends the
   game, which hears nothing after; a joiner through a relay makes handshake B before it connects, or doesn't join; and the search port,
   which makes a game plain on the network, opens only for a home game or a search.
L. Plain only at home: a typed address is asked only if it's on this network (NetAddress_IsThisNetwork); a direct join goes only to an
   address on this network; every home host, and every joiner not through a relay, starts with HomeIntercept, which drops anything
   from outside the network before ENet reads it; nothing clears a host's intercept; and a search's answer from outside the network is
   ignored.
WHAT IT CAN'T PROVE: how the code behaves, as opposed to how it's written. That's net_start_test's and relay_list_test's, with their
take-out rows; NetAddress_IsThisNetwork's ranges are proved by net_checks_test, and the fingerprint's sum by net_relay_keys_test.
Anything this check can't place is refused with the file and the line, so a new way out is read by a person before it's let through.
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

# The relay list's own file: the one place a web call may be, and where rules R and K live. The network's own file: where V, S and L live.
$listFile = '3dc/relay_list.cpp'
$netFile = '3dc/networking.cpp'

# The network's own files, where -dev may decide nothing (rule D). Modified 2026-11-10 by Melanikus for the AvP port (beta.2): and the
# online pages, the access page, the yes page, the online screens and the Servers page (#175 2b), now that they're there for every player whose servers.cfg names a
# server, so no word typed on the command line can open or close one
$onlineFile = '^3dc/(networking\.(cpp|h)|relay_[\w]+\.(cpp|h)|net_[\w]+\.(cpp|h)|servers_cfg[\w]*\.(cpp|h)|avp/win95/frontend/(access_page|server_yes|online_screens|servers_page)[\w]*\.(cpp|h))$'

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
# a quote inside a comment isn't taken for a string. Each is kept the same size, with its line ends, so places and line numbers stay true.
# The code that's read keeps no comment at all, and of a string or a character only its two quotes, so a brace or a name inside one is
# never taken for code. The words kept read the same with the strings as they are, for the rules that need what they say.
$commentOrString = [regex]'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\\n])*"|''(?:\\.|[^''\\\n])*'''
$blankComments = [System.Text.RegularExpressions.MatchEvaluator]{ param($m) if ($m.Value.StartsWith('/')) { $m.Value -replace '[^\n]', ' ' } else { $m.Value } }
$blankAll = [System.Text.RegularExpressions.MatchEvaluator]{
    param($m)
    if ($m.Value.StartsWith('/')) { return ($m.Value -replace '[^\n]', ' ') }
    $quote = $m.Value.Substring(0, 1)
    return $quote + ($m.Value.Substring(1, $m.Value.Length - 2) -replace '.', ' ') + $quote
}

$code = @{}        # each file's code with comments taken out and strings emptied
$withWords = @{}   # the same with the strings kept

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

function Get-Flat([string]$text)
{
    return ($text -replace '\s+', ' ').Trim()
}

# Where the } is that closes the block opening at the { at $open. The code read has no comments and no string's insides, so every brace
# in it is a real one.
function Get-Close([string]$text, [int]$open)
{
    $depth = 0

    for ($i = $open; $i -lt $text.Length; $i++)
    {
        if ($text[$i] -eq '{') { $depth++ }
        elseif ($text[$i] -eq '}')
        {
            $depth--
            if ($depth -eq 0) { return $i }
        }
    }

    return -1
}

# The body of a block, given where its { is: up to the } that closes it
function Get-Block([string]$text, [int]$open)
{
    $close = Get-Close $text $open
    if ($close -lt 0) { return $null }
    return $text.Substring($open + 1, $close - $open - 1)
}

# What comes before a block's {: back to the end of the statement or the block before it, flattened, such as "if (x)" or "namespace"
function Get-Header([string]$text, [int]$open)
{
    $j = $open - 1
    while ($j -ge 0 -and $text[$j] -ne ';' -and $text[$j] -ne '{' -and $text[$j] -ne '}') { $j-- }
    return (Get-Flat $text.Substring($j + 1, $open - $j - 1))
}

# The blocks round a place in the code, innermost first: where each one's { is
function Get-Enclosing([string]$text, [int]$at)
{
    $opens = @()
    $depth = 0

    for ($i = $at - 1; $i -ge 0; $i--)
    {
        if ($text[$i] -eq '}') { $depth++ }
        elseif ($text[$i] -eq '{')
        {
            if ($depth -gt 0) { $depth-- } else { $opens += $i }
        }
    }

    return ,$opens
}

# The flattened line a place in the code is on
function Get-LineText([string]$text, [int]$at)
{
    $lineStart = $text.LastIndexOf("`n", [Math]::Max(0, $at - 1)) + 1
    $lineEnd = $text.IndexOf("`n", $at)
    if ($lineEnd -lt 0) { $lineEnd = $text.Length }
    return (Get-Flat $text.Substring($lineStart, $lineEnd - $lineStart))
}

# Every definition of a function by this name: the file, its line, where its body opens and closes, its body read from the code, flattened,
# and the same with its words
function Get-Definitions([string]$name)
{
    $found = @()
    # A function in a namespace is indented, so a definition may start after spaces or tabs
    $pattern = [regex]('(?m)^[ \t]*[A-Za-z_][\w:<>,\*&\s]*?[\s\*&](?:\w+::)?' + [regex]::Escape($name) + '\s*\([^;{]*?\)\s*(?:const\s*)?\{')

    foreach ($relative in ($code.Keys | Sort-Object))
    {
        foreach ($m in $pattern.Matches($code[$relative]))
        {
            $open = $m.Index + $m.Length - 1
            $close = Get-Close $code[$relative] $open
            if ($close -lt 0) { Stop-Check "$relative line $(Get-LineNumber $code[$relative] $m.Index): $name's body never closes, so this check can't read it" }
            $body = $code[$relative].Substring($open + 1, $close - $open - 1)
            # The words are cut at the same places: both readings are the same length, and the code's braces are the real ones
            $words = $withWords[$relative].Substring($open + 1, $close - $open - 1)
            $found += [pscustomobject]@{ File = $relative; Line = (Get-LineNumber $code[$relative] $m.Index); Open = $open; Close = $close; Body = $body;
                Flat = (Get-Flat $body); Words = (Get-Flat $words) }
        }
    }

    return $found
}

# The one definition of a function, or the check stops: $rule names the rule that needs it, $where the file it must be in
function Get-OneDefinition([string]$name, [string]$rule, [string]$where, [string]$why)
{
    $all = @(Get-Definitions $name)

    if ($all.Count -ne 1 -or $all[0].File -ne $where)
    {
        Stop-Check "$($rule): $name must be defined once, in $where ($why); it's defined $($all.Count) times"
    }

    return $all[0]
}

# The words that can come just before a call, and so never make a line a definition: "else Open();" and "return Add(x);" are calls
$wordsBeforeACall = @('return', 'else', 'do', 'case', 'goto', 'throw', 'new', 'delete', 'sizeof', 'co_return', 'co_await', 'co_yield')

# Every call of a function by name, in every file: the file, where, and its line. A line that defines or declares it isn't a call: one
# that starts with type words, the last of them just before the name, such as "static void Open();" or "NetResult Net_HostGame(char
# *name)". A keyword in that place ("else Open();") makes it a call, which is checked like any other.
function Get-Calls([string]$name)
{
    $found = @()
    $pattern = [regex]('\b' + [regex]::Escape($name) + '\s*\(')
    $definition = [regex]('^((?:[A-Za-z_][\w:<>]*[\s\*&]+)+)' + [regex]::Escape($name) + '\s*\(')

    foreach ($relative in ($code.Keys | Sort-Object))
    {
        foreach ($m in $pattern.Matches($code[$relative]))
        {
            $defined = $definition.Match((Get-LineText $code[$relative] $m.Index))

            if ($defined.Success)
            {
                $lastWord = ([regex]::Matches($defined.Groups[1].Value, '[A-Za-z_][\w:<>]*') | Select-Object -Last 1).Value

                if ($wordsBeforeACall -notcontains $lastWord)
                {
                    continue
                }
            }

            $found += [pscustomobject]@{ File = $relative; At = $m.Index; Line = (Get-LineNumber $code[$relative] $m.Index) }
        }
    }

    return $found
}

# Whether a call is inside a function's definition
function Test-Inside($call, $definition)
{
    return ($call.File -eq $definition.File -and $call.At -gt $definition.Open -and $call.At -lt $definition.Close)
}

# Where a name appears anywhere in the code (comments and strings aside): the first place, or $null
function Find-Name([string]$name, [string]$filePattern = '')
{
    foreach ($relative in ($code.Keys | Sort-Object))
    {
        if ($filePattern -and $relative -notmatch $filePattern) { continue }
        $m = [regex]::Match($code[$relative], '\b' + [regex]::Escape($name) + '\b')
        if ($m.Success) { return [pscustomobject]@{ File = $relative; Line = (Get-LineNumber $code[$relative] $m.Index) } }
    }

    return $null
}

# How many times a text is in another, exactly
function Get-Count([string]$text, [string]$part)
{
    return ([regex]::Matches($text, [regex]::Escape($part))).Count
}

# The block that follows a form in flattened code: the form ends with its {, and must be there exactly once. $null otherwise.
function Get-FormBlock([string]$flat, [string]$form)
{
    if ((Get-Count $flat $form) -ne 1) { return $null }
    $at = $flat.IndexOf($form, [System.StringComparison]::Ordinal)
    return (Get-Block $flat ($at + $form.Length - 1))
}

# Whether the block after a form holds every one of $musts
function Test-FormBlock([string]$flat, [string]$form, [string[]]$musts)
{
    $block = Get-FormBlock $flat $form
    if ($null -eq $block) { return $false }
    foreach ($must in $musts) { if (-not $block.Contains($must)) { return $false } }
    return $true
}

# Whether $first is in a text once, and $second comes after it
function Test-Before([string]$flat, [string]$first, [string]$second)
{
    $a = $flat.IndexOf($first, [System.StringComparison]::Ordinal)
    $b = $flat.IndexOf($second, [System.StringComparison]::Ordinal)
    return ((Get-Count $flat $first) -eq 1 -and $a -ge 0 -and $b -gt $a)
}

# ---- Is there any network code at all?

$webCall = [regex]'\b(WinHttp(Open|Connect|OpenRequest|SendRequest|ReceiveResponse|ReadData|WriteData)|Internet(Open|Connect|OpenUrl)[AW]?|HttpOpenRequest[AW]?|HttpSendRequest[AW]?|URLDownloadToFile[AW]?|URLOpen(Blocking)?Stream[AW]?)\s*\('
$nameLookup = [regex]'\b(getaddrinfo|GetAddrInfo(W|Ex[AW]?)?|gethostbyname|WSAAsyncGetHostByName|DnsQuery(_A|_W|_UTF8|Ex)?|enet_address_set_host)\s*\('

$anyWeb = @($code.Keys | Where-Object { $webCall.IsMatch($code[$_]) -or $nameLookup.IsMatch($code[$_]) })

if ($anyWeb.Count -eq 0 -and -not $code.ContainsKey($netFile) -and -not $code.ContainsKey($listFile))
{
    Write-Host "None of the game's network code is in this commit, so a player's game can't play online at all."
    exit 0
}

if (-not $code.ContainsKey($netFile) -or -not $code.ContainsKey($listFile))
{
    Stop-Check "the game has network code, but $netFile or $listFile isn't there, so this check can't say how it plays online"
}

# ---- D: -dev decides nothing online

foreach ($relative in ($code.Keys | Sort-Object))
{
    if ($relative -notmatch $onlineFile) { continue }
    $m = [regex]::Match($code[$relative], '\bDevMode\b')

    if ($m.Success)
    {
        Stop-Check "D: $relative line $(Get-LineNumber $code[$relative] $m.Index) names DevMode: in the network's own files -dev may decide nothing, as a word anyone can be told to type must never bring back a weaker way"
    }
}

# ---- N: no name is looked up, and web calls stay in the list's file, to the list's server

foreach ($relative in ($code.Keys | Sort-Object))
{
    $m = $nameLookup.Match($code[$relative])

    if ($m.Success)
    {
        Stop-Check "N: $relative line $(Get-LineNumber $code[$relative] $m.Index) calls $($m.Groups[1].Value), which turns a name into an address: a server is reached only at the IPv4 address its line gives, as whoever holds a name can aim it anywhere"
    }

    if ($relative -ne $listFile)
    {
        $w = $webCall.Match($code[$relative])

        if ($w.Success)
        {
            Stop-Check "N: $relative line $(Get-LineNumber $code[$relative] $w.Index) calls $($w.Groups[1].Value), which reaches beyond this PC: only $listFile may"
        }
    }
}

# Every call goes to its own server, the one it was given: s, a server's state made from its servers.cfg line alone (NewServer, rule R),
# whose key every call is held to (rule K). Modified 2026-11-11 by Melanikus for the AvP port (all servers' games, the owner's "same code,
# for 2 or more servers"): every line takes this one way, the line in use and each one visited.
foreach ($m in ([regex]'\bWinHttpConnect\s*\(\s*(\w+)\s*,\s*([^,]+?)\s*,').Matches($code[$listFile]))
{
    if ((Get-Flat $m.Groups[2].Value) -cne 's->server')
    {
        Stop-Check "N: $listFile line $(Get-LineNumber $code[$listFile] $m.Index) calls WinHttpConnect to $(Get-Flat $m.Groups[2].Value): the list's calls may only go to s->server, the call's own server, made from its line"
    }
}

# ---- C: no server comes from the settings file

$settingRead = [regex]'\bConfig_Get\w*\s*\(\s*"([^"]*)"\s*,\s*"([^"]*)"'

foreach ($relative in ($withWords.Keys | Sort-Object))
{
    foreach ($m in $settingRead.Matches($withWords[$relative]))
    {
        $section = $m.Groups[1].Value
        $key = $m.Groups[2].Value

        # Names matched with their capitals, so a setting such as [Video]'s "RenderingKey" (a hotkey) isn't taken for a server's
        if ($section -eq '[Network]' -or $key -cmatch 'Relay|Server|Pin|Fingerprint')
        {
            Stop-Check "C: $relative line $(Get-LineNumber $withWords[$relative] $m.Index) reads $key from $section in the settings file: every server comes from servers.cfg only, never from a file people share and edit"
        }
    }
}

# ---- F: the shipped servers.cfg, the list of our servers, keeps the game's rules (3dc/servers_cfg.h), and stricter ones, since it's ours

$shippedFile = 'servers.cfg'

# One IPv4 address, alone or with its two ports (the list's, then the relay's), as the game reads a line: four numbers with no leading
# zeros, ports of 1 to 65535, and an address a server can have on the internet. That isn't this machine (127/8), nothing (0/8), a group
# or a reserved one (224/4 and up), a home network's (10/8, 172.16/12, 192.168/16), the cable's own (169.254/16), or one an internet
# provider shares among its customers (100.64/10): a server we ship is for everyone, so it has a public address.
function Test-ServerAddress([string]$text)
{
    $m = [regex]::Match($text, '\A(0|[1-9][0-9]{0,2})\.(0|[1-9][0-9]{0,2})\.(0|[1-9][0-9]{0,2})\.(0|[1-9][0-9]{0,2})(:([1-9][0-9]{0,4}):([1-9][0-9]{0,4}))?\z')
    if (-not $m.Success) { return $false }
    $a = @(1..4 | ForEach-Object { [int]$m.Groups[$_].Value })
    if (@($a | Where-Object { $_ -gt 255 }).Count -gt 0) { return $false }
    if ($m.Groups[5].Success -and ([int]$m.Groups[6].Value -gt 65535 -or [int]$m.Groups[7].Value -gt 65535)) { return $false }
    if ($a[0] -eq 0 -or $a[0] -eq 127 -or $a[0] -ge 224 -or $a[0] -eq 10) { return $false }
    if (($a[0] -eq 172 -and $a[1] -ge 16 -and $a[1] -le 31) -or ($a[0] -eq 192 -and $a[1] -eq 168) -or ($a[0] -eq 169 -and $a[1] -eq 254)) { return $false }
    if ($a[0] -eq 100 -and $a[1] -ge 64 -and $a[1] -le 127) { return $false }
    return $true
}

$shippedPath = Join-Path $Repository $shippedFile

if (-not (Test-Path -LiteralPath $shippedPath -PathType Leaf)) { Stop-Check "F: $shippedFile isn't there: it's the list of the servers the game ships with, packaged beside bin" }

$shippedBytes = [System.IO.File]::ReadAllBytes($shippedPath)

if ($shippedBytes.Length -gt 16384) { Stop-Check "F: $shippedFile is over 16 KB, more than the game reads" }

# Ours has plain line ends and plain letters only: no carriage return, and no mark at the start (its bytes are past 126). The game would
# take both, from a player's Notepad, but the file we ship is kept the one way (.gitattributes keeps its line ends plain).
foreach ($b in $shippedBytes)
{
    if (-not ($b -eq 9 -or $b -eq 10 -or ($b -ge 32 -and $b -le 126)))
    {
        Stop-Check "F: $shippedFile holds a byte ($b) that isn't a printable character, a space, a tab or a newline: the one we ship is plain text with plain line ends"
    }
}

$shippedText = [System.Text.Encoding]::ASCII.GetString($shippedBytes)
$lines = @($shippedText.Split("`n"))
if ($lines.Count -gt 0 -and $lines[-1] -eq '') { $lines = @($lines | Select-Object -First ($lines.Count - 1)) }

if ($lines.Count -gt 64) { Stop-Check "F: $shippedFile has more than 64 lines, more than the game reads" }

# Each line a note (from #) or a server: address, fingerprint, then the name as the rest of the line, as the game reads it. Ours has no
# empty line and no space at a line's ends, which the game would skip, so every line we ship is one the game uses as it's written.
$expected = @()
$names = @{}
$addresses = @{}

for ($n = 0; $n -lt $lines.Count; $n++)
{
    $line = $lines[$n]
    if ($line.StartsWith('#', [System.StringComparison]::Ordinal)) { continue }

    if ($line -cmatch '\A[ \t]|[ \t]\z' -or $line -eq '')
    {
        Stop-Check "F: $shippedFile line $($n + 1) is empty or has a space at an end: the one we ship has neither"
    }

    $m = [regex]::Match($line, '\A([^ \t]+)[ \t]+([^ \t]+)[ \t]+([^ \t].*)\z')

    if (-not $m.Success) { Stop-Check "F: $shippedFile line $($n + 1) isn't an address, a fingerprint and a name: the game would refuse it, so it isn't shipped (#175)" }

    $address = $m.Groups[1].Value
    $fingerprint = $m.Groups[2].Value
    $name = [regex]::Replace($m.Groups[3].Value, '[ \t]+', ' ')

    $bad = if (-not (Test-ServerAddress $address)) { 'an address that isn''t one public IPv4 address, alone or with its two ports' }
        elseif ($fingerprint -cnotmatch '\A[0-9a-f]{64}\z') { 'a fingerprint that isn''t 64 small hex digits' }
        elseif ($name.Length -gt 31 -or $name.StartsWith('#', [System.StringComparison]::Ordinal)) { 'a name over 31 letters, or starting with #' }
        else { '' }

    if ($bad)
    {
        Stop-Check "F: $shippedFile line $($n + 1) has $($bad): the game would refuse it, so it isn't shipped (#175)"
    }

    # The game refuses both lines of a name or an address two lines share, so neither would be used
    $nameKey = $name.ToLowerInvariant()
    $addressKey = $address.Split(':')[0]
    if ($names.ContainsKey($nameKey)) { Stop-Check "F: $shippedFile line $($n + 1) has the name of line $($names[$nameKey]): the game would use neither" }
    if ($addresses.ContainsKey($addressKey)) { Stop-Check "F: $shippedFile line $($n + 1) has the address of line $($addresses[$addressKey]): the game would use neither" }
    $names[$nameKey] = $n + 1
    $addresses[$addressKey] = $n + 1
    $expected += [pscustomobject]@{ Name = $name; Address = $address; Fingerprint = $fingerprint }
}

# ---- R: the route. Only the list's file reads servers.cfg, and the list and its relay go only to the line's address.

$serversCfgReader = [regex]'\bServersCfg_Read(TheGames|Folder)?\s*\('
$serversCfgOwnFiles = @('3dc/servers_cfg.h', '3dc/servers_cfg.cpp', '3dc/servers_cfg_file.h', '3dc/servers_cfg_file.cpp')

foreach ($relative in ($code.Keys | Sort-Object))
{
    if ($relative -ne $listFile -and $serversCfgOwnFiles -notcontains $relative -and $serversCfgReader.IsMatch($code[$relative]))
    {
        Stop-Check "R: $relative reads servers.cfg: only $listFile may, where the list's rules hold its server"
    }
}

$listFlat = Get-Flat $withWords[$listFile]

# EVERY SERVER THE SAME WAY (modified 2026-11-11 by Melanikus for the AvP port: all servers' games, the owner's "same code, for 2 or
# more servers"). A server's state is made in one place only, NewServer, from its servers.cfg line alone: for the line in use (Start) and
# for each line a find visits (VisitEveryLine). Nothing else makes one. So whatever holds for one line holds for every line.
$newServer = Get-OneDefinition 'NewServer' 'R' $listFile 'a server''s state, made from its line alone'
$madeHere = 'Shared *made = new (std::nothrow) Shared;'

if ((Get-Count $listFlat 'new (std::nothrow) Shared') -ne 1 -or -not $newServer.Words.StartsWith($madeHere, [System.StringComparison]::Ordinal) -or
    -not $newServer.Words.EndsWith('return made;', [System.StringComparison]::Ordinal))
{
    Stop-Check "R: a server's state must be made in one place only, NewServer ($listFile line $($newServer.Line)), from its line: $madeHere, ... return made;"
}

# NOTHING ELSE MAKES OR COPIES ONE (added 2026-11-11 by Melanikus for the AvP port, all servers' games). Rule K holds each server's trust
# in its state: the key that matched, and its line's fingerprint. So a state that came any other way than NewServer, or one copied whole
# from another server's, could carry one server's trust over to another. There's one new of a state, whatever its form, and no state kept
# as a plain variable, which could be copied by assignment. No state is copied whole: not by assignment through a pointer, not by the
# size of a whole state (only made and visit are sized, to be cleared), and no copy call is handed a whole state. (This reads names, not
# types: a state is reached by the names the file gives it, listed in $stateNames.)
$listCode = Get-Flat $code[$listFile]
$stateNames = 'shared|visit|made|s|server|owner|hostServer'
$byValue = [regex]::Match($listCode, '\bShared\b(\s+const)?\s+\w+\s*[;=\[({,)]')
$wholeSizes = @([regex]::Matches($listCode, '[^;{}]*\bsizeof\s*\(\s*(\*\s*\w+|Shared)\s*\)[^;{}]*[;{]') | ForEach-Object { $_.Value.Trim() } |
    Where-Object { $_ -cmatch "sizeof\s*\(\s*(\*\s*($stateNames)|Shared)\s*\)" })
$knownSizes = @('memset(made, 0, sizeof(*made));', 'SecureZeroMemory(visit, sizeof(*visit));')
$copies = @([regex]::Matches($listCode, '\b(memcpy|memmove|memcpy_s|memmove_s|CopyMemory|MoveMemory|RtlCopyMemory|RtlMoveMemory|copy)\s*\(([^;]*);') |
    Where-Object { $_.Groups[2].Value -cmatch "(?<![\w.>])\b($stateNames)\b(?!\s*(->|\.))" } | ForEach-Object { $_.Value })

if (([regex]::Matches($listCode, '\bnew\b[^;]*\bShared\b')).Count -ne 1 -or $byValue.Success -or
    @($wholeSizes | Where-Object { $knownSizes -cnotcontains $_ }).Count -ne 0 -or $copies.Count -ne 0 -or
    $listCode -cmatch "=\s*\*\s*\(?\s*($stateNames)\b\s*\)?\s*;" -or $listCode -cmatch "(^|[;{}(]\s*)\*\s*($stateNames)\b\s*=(?!=)")
{
    Stop-Check "R: $listFile must make a server's state only by NewServer's one new, keep none as a plain variable, and copy none whole (by assignment, by its whole size, or by a copy call handed one)$(if ($byValue.Success) { ': ' + $byValue.Value })$(if ($copies.Count) { ': ' + ($copies -join ' / ') })"
}

# The two states that last (modified 2026-11-11 by Melanikus for the AvP port, one model for every line): the list's own (shared), set
# in one place, Start, to the one it made or back to nothing; and the host's line's (hostServer), set only by UseTheHostLine, to one made
# from the line the room is on, and back to nothing by ForgetTheHostLine. Each starts as nothing, and nothing else is ever called by its
# name (a variable or a parameter of that name would hide it).
$lasting = @(
    @{ Name = 'shared'; Sets = @{ 'made' = 'Start'; 'NULL' = 'Start' } },
    @{ Name = 'hostServer'; Sets = @{ 'NewServer(line)' = 'UseTheHostLine'; 'NULL' = 'ForgetTheHostLine' } }
)

foreach ($state in $lasting)
{
    $name = $state.Name
    $named = [regex]('\b(?!(return|delete|else|case|sizeof|throw|new|goto)\b)[A-Za-z_][\w:<>]*\s*(\*\s*|&\s*|\s)(const\s*)?(\*\s*)?' + $name + '\s*[;=,)\[]')

    if ((Get-Count $listCode "Shared *$name = NULL;") -ne 1 -or $named.Matches($listCode).Count -ne 1)
    {
        Stop-Check "R: $listFile must start $name as nothing, once, and call nothing else ${name}: Shared *$name = NULL;"
    }

    foreach ($m in [regex]::Matches($code[$listFile], '\b' + $name + '\s*=(?!=)\s*([^;]*);'))
    {
        if ($code[$listFile].Substring(0, $m.Index) -cmatch 'Shared\s*\*\s*$') { continue }
        $setAt = [pscustomobject]@{ File = $listFile; At = $m.Index; Line = (Get-LineNumber $code[$listFile] $m.Index) }
        $to = Get-Flat $m.Groups[1].Value
        $where = @(if ($state.Sets.ContainsKey($to)) { Get-Definitions $state.Sets[$to] | Where-Object { $_.File -eq $listFile } })

        if ($where.Count -ne 1 -or -not (Test-Inside $setAt $where[0]))
        {
            Stop-Check "R: $listFile line $($setAt.Line) sets $name to $($to): only $(($state.Sets.Keys | Sort-Object | ForEach-Object { "$($state.Sets[$_]) may set it to $_" }) -join ', and ')"
        }
    }
}

# The list's own Start, in its file (another file may have a Start of its own)
$startDefs = @(Get-Definitions 'Start' | Where-Object { $_.File -eq $listFile })

if ($startDefs.Count -ne 1)
{
    Stop-Check "R: $listFile must start the list in one function, Start; it has $($startDefs.Count)"
}

$startDef = $startDefs[0]
$visits = Get-OneDefinition 'VisitEveryLine' 'R' $listFile 'every line a find visits'

# THE MAKERS (modified 2026-11-11 by Melanikus for the AvP port, one model for every line). A server's state is made in five places,
# each once, in its own one form, each from a line whose yes rule Y proves: the list's own (Start, the line in use), a find's visits
# (VisitEveryLine), a relay's keys and an access request (each a visit of its own, to the line it's for), and the host's line
# (UseTheHostLine, kept for a room's life). The three that take a line are each called once, from the list's thread, with the line the
# main thread copied after its yes (rule Y).
$makers = [ordered]@{
    'Start' = 'Shared *made = NewServer(*line);'
    'VisitEveryLine' = 'Shared *const visit = place >= 0 ? NewServer(lines[v]) : NULL;'
    'ReadTheRelayKeysOn' = 'Shared *const visit = NewServer(line);'
    'AskForAccessOn' = 'Shared *const visit = NewServer(line);'
    'UseTheHostLine' = 'hostServer = NewServer(line);'
}
$makerParts = @{ 'Start' = $startDef; 'VisitEveryLine' = $visits }

foreach ($maker in $makers.Keys)
{
    if (-not $makerParts.ContainsKey($maker)) { $makerParts[$maker] = Get-OneDefinition $maker 'R' $listFile 'a server''s state, made from one line' }

    if ((Get-Count $makerParts[$maker].Flat $makers[$maker]) -ne 1 -or (Get-Count $makerParts[$maker].Flat 'NewServer(') -ne 1)
    {
        Stop-Check "R: $maker ($listFile line $($makerParts[$maker].Line)) must make its server's state once, in one form: $($makers[$maker])"
    }
}

foreach ($making in (Get-Calls 'NewServer'))
{
    if (@($makers.Keys | Where-Object { Test-Inside $making $makerParts[$_] }).Count -ne 1)
    {
        Stop-Check "R: $($making.File) line $($making.Line) makes a server's state: only $($makers.Keys -join ', ') may"
    }
}

$listThread = Get-OneDefinition 'ListThread' 'R' $listFile 'the list''s thread'
$lineTakers = [ordered]@{
    'ReadTheRelayKeysOn' = 'ReadTheRelayKeysOn(keysPlace, keysLine, keysRegion, keysAsking);'
    'AskForAccessOn' = 'AskForAccessOn(place, accessLines[place], accessBody, accessFingerprint);'
    'UseTheHostLine' = 'UseTheHostLine(hostPlace, hostLine);'
}

foreach ($taker in $lineTakers.Keys)
{
    $takes = @(Get-Calls $taker)

    if ($takes.Count -ne 1 -or -not (Test-Inside $takes[0] $listThread) -or (Get-Count $listThread.Flat $lineTakers[$taker]) -ne 1)
    {
        Stop-Check "R: $taker must be called once, from ListThread ($listFile line $($listThread.Line)), with the line the main thread copied after its yes: $($lineTakers[$taker])"
    }
}

# The server is written in one place only, there, from the line's four numbers, so it's an address and never a name; and it's used in no
# other way than these: called (rule N), written there, and handed to networking.cpp as the relay's address in words
$serverWrite = 'swprintf(made->server, sizeof(made->server) / sizeof(made->server[0]), L"%u.%u.%u.%u", (line.address >> 24) & 0xFFu, (line.address >> 16) & 0xFFu, (line.address >> 8) & 0xFFu, line.address & 0xFFu);'
$serverUses = @('WinHttpConnect(session, s->server,', $serverWrite, 'snprintf(relays[0].host, sizeof(relays[0].host), "%ls", shared->server);')

if ((Get-Count $listFlat $serverWrite) -ne 1 -or -not $newServer.Words.Contains($serverWrite))
{
    Stop-Check "R: $listFile must write a server in one place only, NewServer, from the line's four numbers: $serverWrite"
}

$leftOver = $listFlat
foreach ($use in $serverUses) { $leftOver = $leftOver.Replace($use, '') }

if ($leftOver -match '(->|\.)server\b')
{
    Stop-Check "R: $listFile uses the list's server in a way this check doesn't know: only $($serverUses -join ' or ')"
}

# The relay is at the line's address too, written in one place, there
$relayWrites = @([regex]::Matches($listFlat, '->relayAddress\s*=(?!=)[^;]*;'))

if ($relayWrites.Count -ne 1 -or $relayWrites[0].Value -cne '->relayAddress = htonl(line.address);' -or -not $newServer.Words.Contains('made->relayAddress = htonl(line.address);'))
{
    Stop-Check "R: $listFile must write the relay's address in one place only, NewServer, from the line's address: made->relayAddress = htonl(line.address);"
}

# The list starts only for a line: with none, it makes nothing
$start = $startDef.Words
$startLine = $startDef.Line
$refuseAt = $start.IndexOf('if (!Relay_ServerName()[0]) { return false; }', [System.StringComparison]::Ordinal)
$lineAt = $start.IndexOf('const ServersCfgServer *const line = Server();', [System.StringComparison]::Ordinal)
$makeAt = $start.IndexOf('Shared *made = NewServer(*line);', [System.StringComparison]::Ordinal)

if ($refuseAt -lt 0 -or $lineAt -lt $refuseAt -or $makeAt -lt $lineAt)
{
    Stop-Check "R: the list's start ($listFile line $startLine) must make nothing when there's no line: if (!Relay_ServerName()[0]) { return false; }, then const ServersCfgServer *const line = Server();, before Shared *made = NewServer(*line);"
}

# Each visit is made from a line the find chose (rule Y), by the same NewServer, and only those lines are visited
if (-not (Test-Before $visits.Flat 'memcpy(lines, shared->visitLines, sizeof(lines));' 'Shared *const visit = place >= 0 ? NewServer(lines[v]) : NULL;'))
{
    Stop-Check "R: VisitEveryLine ($listFile line $($visits.Line)) must make each visit by NewServer from the lines the find chose: memcpy(lines, shared->visitLines, sizeof(lines)); before Shared *const visit = place >= 0 ? NewServer(lines[v]) : NULL;"
}

# ---- K: the key. The server is trusted by its line's fingerprint alone.

# Modified 2026-11-11 by Melanikus for the AvP port (all servers' games, the owner's "same code, for 2 or more servers"): each call is
# held to its OWN server's key. A server's state (Shared, made by NewServer from its line, rule R) carries its own two key checks and its
# own answers, so nothing of one server's check or answer can be read for another's, and nothing is kept between one line and the next.
$callChecked = Get-OneDefinition 'CallChecked' 'K' $listFile 'every web call to the list'

foreach ($name in @('WinHttpConnect', 'WinHttpOpenRequest', 'WinHttpSendRequest', 'WinHttpWriteData', 'WinHttpReceiveResponse'))
{
    foreach ($call in (Get-Calls $name))
    {
        if (-not (Test-Inside $call $callChecked))
        {
            Stop-Check "K: $($call.File) line $($call.Line) calls $name outside CallChecked: every web call goes through its key check"
        }
    }
}

$checked = $callChecked.Flat

# Modified 2026-11-11 by Melanikus for the AvP port (one model for every line): a server's key is read under its lock (KeyStateOf), as the
# main thread can turn the list's own state from "no line yet" to "not checked" with the player's yes (rule Y). A state with no line
# (kKeyNoLine) sends nothing at all, so the list's thread can run before the line in use has its yes.
$keyNowFirst = [regex]'^Answer a = \{ false, [^;{}]*\}; check\.owner = s; int const keyNow = KeyStateOf\(s\); if \(keyNow == kKeyNoLine\) \{'
$keyNowRefusal = 'if (keyNow == kKeyRefused || (!check.keepIt && keyNow != kKeyGood)) {'

if ((Get-Count (Get-Flat $code[$listFile]) 'Answer CallChecked(Shared *s, ') -ne 1 -or -not $keyNowFirst.IsMatch($checked) -or
    -not (Test-FormBlock $checked 'if (keyNow == kKeyNoLine) {' @('return a;')) -or -not (Test-FormBlock $checked $keyNowRefusal @('return a;')) -or
    -not (Test-Before $checked 'if (keyNow == kKeyNoLine) {' $keyNowRefusal) -or -not (Test-Before $checked $keyNowRefusal 'WinHttpOpen(') -or
    (Get-Count $checked 'KeyStateOf(') -ne 1 -or $checked -cmatch '\bkeyState\b')
{
    Stop-Check "K: CallChecked ($listFile line $($callChecked.Line)) must hold its key check to its own server, then send nothing through a state with no line, nothing to a server whose keys didn't match, and nothing but the first call before they're checked: Answer CallChecked(Shared *s, ...), then check.owner = s; int const keyNow = KeyStateOf(s); if (keyNow == kKeyNoLine) { ... return a; } $keyNowRefusal ... return a; }"
}

$keyNowOf = Get-OneDefinition 'KeyStateOf' 'K' $listFile 'where a server''s key stands'

if ($keyNowOf.Flat -cne 'EnterCriticalSection(&s->lock); int const state = s->keyState; LeaveCriticalSection(&s->lock); return state;')
{
    Stop-Check "K: KeyStateOf ($listFile line $($keyNowOf.Line)) must only read its own server's key state under its lock: EnterCriticalSection(&s->lock); int const state = s->keyState; LeaveCriticalSection(&s->lock); return state;"
}

# (The request is sent from one statement, with a body's headers or without, so WinHttpSendRequest is written twice: the check comes after
# the last.)
$lastSend = $checked.LastIndexOf('WinHttpSendRequest(', [System.StringComparison]::Ordinal)
$matchedAt = $checked.IndexOf('if (!check.matched) {', [System.StringComparison]::Ordinal)

if (-not (Test-Before $checked 'WinHttpSetStatusCallback(request, CheckTheKey, WINHTTP_CALLBACK_FLAG_SEND_REQUEST, 0)' 'WinHttpSendRequest(') -or
    $lastSend -lt 0 -or $matchedAt -lt $lastSend -or
    -not (Test-FormBlock $checked 'if (!check.matched) {' @('return a;')) -or
    -not (Test-Before $checked 'if (!check.matched) {' 'WinHttpWriteData(') -or
    -not (Test-Before $checked 'if (!check.matched) {' 'WinHttpReceiveResponse('))
{
    Stop-Check "K: CallChecked ($listFile line $($callChecked.Line)) must set CheckTheKey going before the request is sent, and write the body and read the answer only after if (!check.matched) { ... return a; }"
}

$keyCheck = Get-OneDefinition 'CheckTheKey' 'K' $listFile 'the list''s key, read and compared just before the first byte goes'

foreach ($must in @('|| !check->owner) { return; }', 'Shared const *const server = check->owner;', 'if (check->keepIt) { check->matched = read; }',
    'bool const sameSize = read && check->shownSize == server->goodKeySize;', 'differences |= static_cast<uint8_t>(check->shown[i] ^ server->goodKey[i]);',
    'check->matched = sameSize && differences == 0;', 'if (!check->matched) { check->closed = true; WinHttpCloseHandle(request); }'))
{
    if (-not $keyCheck.Flat.Contains($must))
    {
        Stop-Check "K: CheckTheKey ($listFile line $($keyCheck.Line)) must compare every byte of the list's key with the one that matched for the call's own server (check->owner), and close a call whose key doesn't match before anything of it goes: $must"
    }
}

$serverCheck = Get-OneDefinition 'CheckTheServer' 'K' $listFile 'the first call, and the server held to its line'
$call = Get-OneDefinition 'Call' 'K' $listFile 'every call after the first'

foreach ($caller in (Get-Calls 'CallChecked'))
{
    if (-not (Test-Inside $caller $serverCheck) -and -not (Test-Inside $caller $call))
    {
        Stop-Check "K: $($caller.File) line $($caller.Line) calls CallChecked: only CheckTheServer (the first call) and Call (every other) may"
    }
}

if (-not (Test-Before $serverCheck.Flat 'KeyCheck &first = s->firstCheck; memset(&first, 0, sizeof(first)); first.keepIt = true;' 'CallChecked(s, ') -or
    -not $serverCheck.Flat.Contains('bool const good = NetServerFingerprint_IsTheLines(first.shown, first.shownSize, &keys, s->lineFingerprint);') -or
    -not (Test-FormBlock $serverCheck.Flat 'if (good) {' @('memcpy(s->goodKey, first.shown, first.shownSize);')) -or
    -not $serverCheck.Flat.Contains('s->keyState = good ? kKeyGood : kKeyRefused;'))
{
    Stop-Check "K: CheckTheServer ($listFile line $($serverCheck.Line)) must make its server's own first check, and keep the list's key only when it and the relay's keys make that server's line's fingerprint: KeyCheck &first = s->firstCheck; memset(&first, 0, sizeof(first)); first.keepIt = true;, NetServerFingerprint_IsTheLines(first.shown, first.shownSize, &keys, s->lineFingerprint), then if (good) { memcpy(s->goodKey, ...) }"
}

if (-not (Test-Before $call.Flat 'const char *const unchecked = CheckTheServer(s);' 'KeyCheck &check = s->laterCheck; memset(&check, 0, sizeof(check)); check.keepIt = false;') -or
    -not (Test-FormBlock $call.Flat 'if (unchecked) {' @('return none;')) -or
    -not (Test-Before $call.Flat 'check.keepIt = false;' 'return CallChecked(s, '))
{
    Stop-Check "K: Call ($listFile line $($call.Line)) must hold its server to its line first, and send nothing if that fails, before a call of its own server's that goes only to the key that matched: CheckTheServer(s), then KeyCheck &check = s->laterCheck; memset(&check, 0, sizeof(check)); check.keepIt = false;"
}

# The key that matched is written there alone, and a server is called good there alone. Modified 2026-11-11 by Melanikus for the AvP port
# (one model for every line): a server's key state is written in four statements, each once, each in its one place. Called good or refused
# by its own check (CheckTheServer); not checked yet when it's made (NewServer); with no line when the list's own state is made before the
# line in use has its yes (Start, inside if (!lineAgreed)); and from no line to not checked yet only by the player's yes to that line
# (RelayList_AgreeToTheLineAt, inside if (shared->keyState == kKeyNoLine)).
$stateWrites = @([regex]::Matches($listCode, '[^;{}]*\bkeyState\s*=(?!=)[^;]*;') | ForEach-Object { $_.Value.Trim() })
$writePlaces = [ordered]@{
    's->keyState = good ? kKeyGood : kKeyRefused;' = @('CheckTheServer', '')
    'made->keyState = kKeyNotChecked;' = @('NewServer', '')
    'made->keyState = kKeyNoLine;' = @('Start', 'if (!lineAgreed) {')
    'shared->keyState = kKeyNotChecked;' = @('RelayList_AgreeToTheLineAt', 'if (shared->keyState == kKeyNoLine) {')
}

if ((Get-Count $listCode 'goodKey,') -ne 1 -or (Get-Count $listCode 'memcpy(s->goodKey,') -ne 1 -or
    @($stateWrites | Where-Object { -not $writePlaces.Contains($_) }).Count -ne 0 -or $stateWrites.Count -ne $writePlaces.Count)
{
    Stop-Check "K: $listFile must write the key that matched in CheckTheServer alone, and a server's key state only in $($writePlaces.Keys -join ' / '), each once"
}

foreach ($write in $writePlaces.Keys)
{
    # (The list's own Start, rule R's: other files have a Start of their own)
    $in = if ($writePlaces[$write][0] -eq 'Start') { $startDef } else { Get-OneDefinition $writePlaces[$write][0] 'K' $listFile 'where a server''s key state is written' }
    $form = $writePlaces[$write][1]

    if ((Get-Count $in.Flat $write) -ne 1 -or ($form -and -not (Test-FormBlock $in.Flat $form @($write))))
    {
        Stop-Check "K: $($writePlaces[$write][0]) ($listFile line $($in.Line)) must write $write$(if ($form) { " inside $form ... }" })"
    }
}

# A server's trust lives in a few parts of its state: the key that matched and its size, its line's fingerprint, its relay's address and
# its two key checks. Each is used only in the statements below (added 2026-11-11 by Melanikus for the AvP port, all servers' games), so
# none can be written from anything but its own server's check or its own line, or read across from another server. A new use fails
# this check until it's read and added here: a check that names what it allows can't be passed by a shape it didn't think of.
$trustUses = [ordered]@{
    'goodKey' = @('uint8_t goodKey[kNetServerFingerprintMostKeyBytes];', 'differences |= static_cast<uint8_t>(check->shown[i] ^ server->goodKey[i]);',
        'memcpy(s->goodKey, first.shown, first.shownSize);')
    'goodKeySize' = @('size_t goodKeySize;', 'bool const sameSize = read && check->shownSize == server->goodKeySize;', 's->goodKeySize = first.shownSize;')
    'lineFingerprint' = @('uint8_t lineFingerprint[kNetServerFingerprintBytes];',
        'bool const good = NetServerFingerprint_IsTheLines(first.shown, first.shownSize, &keys, s->lineFingerprint);',
        'memcpy(made->lineFingerprint, line.fingerprint, sizeof(made->lineFingerprint));')
    'relayAddress' = @('uint32_t relayAddress;', 'made->relayAddress = htonl(line.address);', 'relays[0].address = shared->relayAddress;')
    'firstCheck' = @('KeyCheck firstCheck;', 'KeyCheck &first = s->firstCheck;')
    'laterCheck' = @('KeyCheck laterCheck;', 'KeyCheck &check = s->laterCheck;')
    'keyState' = @('int keyState;', 'int const state = s->keyState;', 's->keyState = good ? kKeyGood : kKeyRefused;', 'made->keyState = kKeyNotChecked;',
        'made->keyState = kKeyNoLine;', 'bool const known = shared->keyState == kKeyGood;', 'if (shared->keyState == kKeyGood) {',
        'else if (shared->keyState == kKeyRefused) {', 'if (shared->keyState == kKeyNoLine) {', 'shared->keyState = kKeyNotChecked;')
}

foreach ($member in $trustUses.Keys)
{
    $uses = @([regex]::Matches($listCode, '[^;{}]*\b' + $member + '\b[^;{}]*[;{]') | ForEach-Object { $_.Value.Trim() })
    $unknown = @($uses | Where-Object { $trustUses[$member] -cnotcontains $_ })

    if ($unknown.Count -ne 0)
    {
        Stop-Check "K: $listFile uses a server's $member in a way this check doesn't know: only $($trustUses[$member] -join ' or ') ($($unknown -join ' / '))"
    }
}

if (-not $newServer.Flat.Contains('memcpy(made->lineFingerprint, line.fingerprint, sizeof(made->lineFingerprint));'))
{
    Stop-Check "K: NewServer ($listFile line $($newServer.Line)) must take its server's fingerprint from its own line: memcpy(made->lineFingerprint, line.fingerprint, sizeof(made->lineFingerprint));"
}

# NOTHING CARRIED FROM ONE SERVER TO ANOTHER. No key check is kept by the file, and the functions a call goes through touch only the server
# they were given: no static of their own, and never the line in use's state (shared) in place of the one they were given.
if ($listCode -cmatch '\bstatic\s+KeyCheck\b')
{
    Stop-Check "K: $listFile keeps a KeyCheck for the whole file: each server's state holds its own (firstCheck and laterCheck), so no check is shared between servers"
}

$chain = @('Tell', 'CantUse', 'KeyStateOf', 'CallChecked', 'CheckTheKey', 'CheckTheServer', 'Call', 'VisitTheLine', 'NewServer', 'ForgetTheVisit')
$chainParts = @{}

foreach ($name in $chain)
{
    $part = Get-OneDefinition $name 'K' $listFile 'a call to one server, held to that server alone'
    $chainParts[$name] = $part

    if ($part.Flat -cmatch '\bstatic\s' -or $part.Flat -cmatch '\bshared\b')
    {
        Stop-Check "K: $name ($listFile line $($part.Line)) must keep nothing of its own between calls (no static) and touch only the server it was given, never shared"
    }
}

# And each of them hands on the server it was given, never another: inside the chain a call's first word is s (VisitTheLine's, visit);
# in a function that makes a visit it's visit, the line it's visiting; anywhere else it's shared, the list's own, or hostServer, the
# host's line's (modified 2026-11-11 by Melanikus for the AvP port, one model for every line)
$visitMakers = @('VisitEveryLine', 'ReadTheRelayKeysOn', 'AskForAccessOn')

foreach ($name in @('Tell', 'CantUse', 'KeyStateOf', 'CallChecked', 'CheckTheServer', 'Call', 'VisitTheLine'))
{
    foreach ($use in (Get-Calls $name))
    {
        if ($use.File -ne $listFile)
        {
            Stop-Check "K: $($use.File) line $($use.Line) calls ${name}: only $listFile's own calls may"
        }

        $given = [regex]::Match($code[$listFile].Substring($use.At), '^\w+\s*\(\s*(\w+)\s*[,)]')
        $inside = @($chain | Where-Object { Test-Inside $use $chainParts[$_] })
        $inAVisit = @($visitMakers | Where-Object { Test-Inside $use $makerParts[$_] }).Count -gt 0
        $wanted = if ($inside -contains 'VisitTheLine') { @('visit') } elseif ($inside.Count -gt 0) { @('s') } elseif ($inAVisit) { @('visit') } else { @('shared', 'hostServer') }

        if (-not $given.Success -or $wanted -cnotcontains $given.Groups[1].Value)
        {
            Stop-Check "K: $listFile line $($use.Line) hands $name another server than its own: inside a call it must be $($wanted -join ' or ')"
        }
    }
}

# A visit is forgotten whole, wiped and given back, and nothing of it is kept: between its making and its end there's no way out, and
# it isn't stored anywhere
$forget = Get-OneDefinition 'ForgetTheVisit' 'K' $listFile 'a visit''s end'

if ($forget.Flat -cne 'DeleteCriticalSection(&visit->lock); SecureZeroMemory(visit, sizeof(*visit)); delete visit;')
{
    Stop-Check "K: ForgetTheVisit ($listFile line $($forget.Line)) must wipe the whole visit and give it back, and do nothing else: DeleteCriticalSection(&visit->lock); SecureZeroMemory(visit, sizeof(*visit)); delete visit;"
}

# Only a visit is forgotten, where it was made, and the host's line's state where it's let go: forgetting the list's own state would
# leave it pointing at freed memory
$forgetters = @{ 'ForgetTheHostLine' = (Get-OneDefinition 'ForgetTheHostLine' 'K' $listFile 'the host''s line let go') }
foreach ($maker in $visitMakers) { $forgetters[$maker] = $makerParts[$maker] }

foreach ($forgetting in (Get-Calls 'ForgetTheVisit'))
{
    if (@($forgetters.Keys | Where-Object { Test-Inside $forgetting $forgetters[$_] }).Count -ne 1)
    {
        Stop-Check "K: $($forgetting.File) line $($forgetting.Line) calls ForgetTheVisit: only $($visitMakers -join ', ') may, for the visit each made, and ForgetTheHostLine, for the host's line's"
    }
}

# The form of the innermost block round a place in flattened code, such as "if (visit) {". The code read has no comments and no string's
# insides, so every brace is a real one.
function Get-FormRound([string]$flat, [int]$at)
{
    $depth = 0

    for ($i = $at - 1; $i -ge 0; $i--)
    {
        if ($flat[$i] -eq '}') { $depth++ }
        elseif ($flat[$i] -eq '{')
        {
            if ($depth -eq 0) { return ([regex]::Match($flat.Substring(0, $i + 1), '[^;{}]*\{$')).Value.Trim() }
            $depth--
        }
    }

    return ''
}

# Each visit ends the one way: made once, then forgotten once, inside if (visit) { }, with no way out between, nothing of it kept, and
# nothing of it touched after
foreach ($maker in $visitMakers)
{
    $part = $makerParts[$maker]
    $madeAt = $part.Flat.IndexOf($makers[$maker], [System.StringComparison]::Ordinal)
    $endAt = $part.Flat.IndexOf('ForgetTheVisit(visit);', [System.StringComparison]::Ordinal)
    $between = if ($madeAt -ge 0 -and $endAt -gt $madeAt) { $part.Flat.Substring($madeAt, $endAt - $madeAt) } else { '' }
    $after = if ($endAt -ge 0) { $part.Flat.Substring($endAt + 'ForgetTheVisit(visit);'.Length) } else { '' }

    if ($madeAt -lt 0 -or $endAt -lt $madeAt -or (Get-Count $part.Flat 'ForgetTheVisit(visit);') -ne 1 -or (Get-FormRound $part.Flat $endAt) -cne 'if (visit) {' -or
        $between -cmatch '\b(return|break|continue|goto|throw)\b' -or $part.Flat -cmatch '=\s*visit\s*[;,)]' -or $after -cmatch '\bvisit\b')
    {
        Stop-Check "K: $maker ($listFile line $($part.Line)) must end every visit it makes with ForgetTheVisit(visit), with no way out before it, and keep nothing of it: $($makers[$maker]) then if (visit) { ... ForgetTheVisit(visit); }"
    }
}

# The host's line's state: let go whole before another is made, and made only from the line the room is on
$hostForms = [ordered]@{
    'ForgetTheHostLine' = 'if (hostServer) { ForgetTheVisit(hostServer); } hostServer = NULL; hostServerPlace = -1;'
    'UseTheHostLine' = 'if (hostServer && hostServerPlace == place) { return; } ForgetTheHostLine(); hostServer = NewServer(line); hostServerPlace = hostServer ? place : -1;'
}

foreach ($name in $hostForms.Keys)
{
    $part = if ($name -eq 'UseTheHostLine') { $makerParts[$name] } else { $forgetters[$name] }

    if ($part.Flat -cne $hostForms[$name])
    {
        Stop-Check "K: $name ($listFile line $($part.Line)) must be exactly: $($hostForms[$name])"
    }
}

# ---- Y: the player's yes (#175's last part). Added 2026-11-10 by Melanikus for the AvP port.

# Modified 2026-11-11 by Melanikus for the AvP port (all servers' games): every line has the same gate. The line in use is refused at
# the list's start, before its server's state is made; and a find visits only the lines that passed the same question on the main thread,
# where the yeses are, and nothing else puts a line in the list of lines to visit.
#
# Modified again 2026-11-11 by Melanikus for the AvP port (one model for every line): the list's thread runs for every line that's had its
# yes, so Start no longer refuses the line in use without one. It asks the yes before it makes its state, and a state made without the yes
# has no line (kKeyNoLine): rule K proves CallChecked sends nothing through it, and only the player's yes to that line turns it into one
# not checked yet. Every other call to a line is a visit of its own, asked for on the main thread only after that line's yes (below).
$theGate = 'const ServersCfgServer *const line = Server(); if (!line) { return false; } bool const lineAgreed = TheLineIsAgreed(*line); Shared *made = NewServer(*line); if (!made) { return false; } if (!lineAgreed) { made->keyState = kKeyNoLine; }'
$gateAt = $start.IndexOf($theGate, [System.StringComparison]::Ordinal)

if ($gateAt -lt 0 -or (Get-Count $start $theGate) -ne 1 -or $gateAt -gt $start.IndexOf('shared = made;', [System.StringComparison]::Ordinal))
{
    Stop-Check "Y: the list's start ($listFile line $startLine) must give a line the player hasn't said yes to no server, before its state is shared: $theGate, before shared = made;"
}

# Each call to a line other than a find's is asked for on the main thread, where the yeses are, and only once that line has its yes:
# the line is copied for the list's thread only after that refusal. The host's line is the one RelayList_HostLine gives, which is a line
# with its yes and its approval (below), or none.
$asks = [ordered]@{
    'RelayList_AskForRelayKeysOn' = @('if (!TheLineIsAgreed(theFile.servers[place]) || !Start()) {', 'return false;', 'shared->keysLine = theFile.servers[place];')
    'RelayList_AskForAccessOn' = @('if (!TheLineIsAgreed(theFile.servers[place])) {', 'return false;', 'shared->accessLines[place] = theFile.servers[place];')
    'RelayList_HostRoomOpened' = @('int const place = RelayList_HostLine(); if (place < 0) {', 'return;', 'shared->hostLine = theFile.servers[place];')
}

foreach ($ask in $asks.Keys)
{
    $part = Get-OneDefinition $ask 'Y' $listFile 'a call to one line, asked for after its yes'
    $refusal, $out, $copy = $asks[$ask]

    if (-not (Test-FormBlock $part.Flat $refusal @($out)) -or -not (Test-Before $part.Flat $refusal $copy) -or (Get-Count $part.Flat $copy) -ne 1)
    {
        Stop-Check "Y: $ask ($listFile line $($part.Line)) must refuse a line the player hasn't said yes to before it hands it to the list's thread: $refusal ... $out } before $copy"
    }
}

# The lines handed over are used only as they're copied there, taken by the list's thread, and given to the one call each is for (rule R)
$handedLines = [ordered]@{
    'keysLine' = @('ServersCfgServer keysLine;', 'ServersCfgServer const keysLine = shared->keysLine;', 'ReadTheRelayKeysOn(keysPlace, keysLine, keysRegion, keysAsking);',
        'shared->keysLine = theFile.servers[place];')
    'accessLines' = @('ServersCfgServer accessLines[kServersCfgMostLines];', 'ServersCfgServer accessLines[kServersCfgMostLines];',
        'memcpy(accessLines, shared->accessLines, sizeof(accessLines));', 'AskForAccessOn(place, accessLines[place], accessBody, accessFingerprint);',
        'shared->accessLines[place] = theFile.servers[place];')
    'hostLine' = @('ServersCfgServer hostLine;', 'ServersCfgServer const hostLine = shared->hostLine;', 'UseTheHostLine(hostPlace, hostLine);',
        'shared->hostLine = theFile.servers[place];')
}

foreach ($handed in $handedLines.Keys)
{
    $uses = @([regex]::Matches($listCode, '[^;{}]*\b' + $handed + '\b[^;{}]*[;{]') | ForEach-Object { $_.Value.Trim() })
    $unknown = @($uses | Where-Object { $handedLines[$handed] -cnotcontains $_ })

    if ($unknown.Count -ne 0 -or $uses.Count -ne $handedLines[$handed].Count)
    {
        Stop-Check "Y: $listFile uses $handed in a way this check doesn't know: only $($handedLines[$handed] -join ' / ')$(if ($unknown.Count) { ' (' + ($unknown -join ' / ') + ')' })"
    }
}

# The host's line: chosen only once it has its yes (RelayList_HostOn), given only while it has its yes and its approval, or for the
# room already open on it (RelayList_HostLine), and the room's line set only there
$hostOn = Get-OneDefinition 'RelayList_HostOn' 'Y' $listFile 'the player''s choice of the line to host on'
$hostOnRefusal = 'if (place < 0 || place >= theFile.serverCount || !TheLineIsAgreed(theFile.servers[place])) {'

if (-not (Test-FormBlock $hostOn.Flat $hostOnRefusal @('return false;')) -or -not (Test-Before $hostOn.Flat $hostOnRefusal 'hostChoice = place;'))
{
    Stop-Check "Y: RelayList_HostOn ($listFile line $($hostOn.Line)) must refuse a line the player hasn't said yes to before it's chosen: $hostOnRefusal ... return false; } before hostChoice = place;"
}

$hostLineNow = Get-OneDefinition 'RelayList_HostLine' 'Y' $listFile 'the line to host on'
$hostLineForm = 'if (hostRoomLine >= 0) { return hostRoomLine; } const char *why = NULL; if (hostChoice >= 0 && (hostChoice >= theFile.serverCount || !TheLineIsAgreed(theFile.servers[hostChoice]) || AccessOn(hostChoice, &why) != kRelayAccessIsApproved)) { hostChoice = -1; } return hostChoice;'

if ($hostLineNow.Flat -cne $hostLineForm)
{
    Stop-Check "Y: RelayList_HostLine ($listFile line $($hostLineNow.Line)) must give the room's line, or a chosen line only while it has its yes and its approval: $hostLineForm"
}

$hostKnown = [ordered]@{
    'hostChoice' = @('int hostChoice = -1;', 'hostChoice = -1;', 'hostChoice = place;', 'hostChoice = -1;', 'return hostChoice;',
        'if (hostChoice >= 0 && (hostChoice >= theFile.serverCount || !TheLineIsAgreed(theFile.servers[hostChoice]) || AccessOn(hostChoice, &why) != kRelayAccessIsApproved)) {')
    'hostRoomLine' = @('int hostRoomLine = -1;', 'hostRoomLine = place;', 'hostRoomLine = -1;', 'if (hostRoomLine >= 0) {', 'return hostRoomLine;')
}

foreach ($member in $hostKnown.Keys)
{
    $uses = @([regex]::Matches($listCode, '[^;{}]*\b' + $member + '\b[^;{}]*[;{]') | ForEach-Object { $_.Value.Trim() })
    $unknown = @($uses | Where-Object { $hostKnown[$member] -cnotcontains $_ })

    if ($unknown.Count -ne 0 -or $uses.Count -ne $hostKnown[$member].Count)
    {
        Stop-Check "Y: $listFile uses $member in a way this check doesn't know: only $($hostKnown[$member] -join ' / ')$(if ($unknown.Count) { ' (' + ($unknown -join ' / ') + ')' })"
    }
}

$roomOpened = Get-OneDefinition 'RelayList_HostRoomOpened' 'Y' $listFile 'a room put on its line''s list'

if (-not (Test-Before $roomOpened.Flat 'int const place = RelayList_HostLine(); if (place < 0) {' 'hostRoomLine = place;'))
{
    Stop-Check "Y: RelayList_HostRoomOpened ($listFile line $($roomOpened.Line)) must set the room's line only from RelayList_HostLine, once it gave one: int const place = RelayList_HostLine(); if (place < 0) { ... } before hostRoomLine = place;"
}

# Modified 2026-11-11 by Melanikus for the AvP port (one model for every line): a find asks each line's yes, in its place, and hands the
# answers to RelayList_ChooseVisits (in the list's text file, a plain function, so its tests can run it with 1, 2 and 9 lines). Only the
# places it chose become the lines to visit. It never chooses a line without its yes, and never more than kRelayListMostVisits.
$finding = Get-OneDefinition 'RelayList_FindGames' 'Y' $listFile 'a find, and the lines it visits'
$findSteps = @(
    'int const count = theFile.serverCount < kServersCfgMostLines ? theFile.serverCount : kServersCfgMostLines; bool agreed[kServersCfgMostLines] = {}; for (int place = 0; place < count; place++) { agreed[place] = TheLineIsAgreed(theFile.servers[place]); }',
    'int const visits = RelayList_ChooseVisits(agreed, count, theServer.inUse, places, states);',
    'for (int v = 0; v < visits; v++) { lines[v] = theFile.servers[places[v]]; }',
    'memcpy(shared->visitLines, lines, ')
$stepAt = -1
$inOrder = $true

foreach ($step in $findSteps)
{
    $at = $finding.Flat.IndexOf($step, [System.StringComparison]::Ordinal)
    if ($at -le $stepAt -or (Get-Count $finding.Flat $step) -ne 1) { $inOrder = $false }
    $stepAt = $at
}

if (-not $inOrder -or ([regex]::Matches($finding.Flat, '\bagreed\b')).Count -ne 3 -or ([regex]::Matches($finding.Flat, '\b(lines|places)\s*\[[^\]]*\]\s*=(?!=)')).Count -ne 1)
{
    Stop-Check "Y: RelayList_FindGames ($listFile line $($finding.Line)) must ask each line's yes, let RelayList_ChooseVisits choose from those answers alone, and hand the list's thread only the lines it chose: $($findSteps -join ' ... ')"
}

$textFile = '3dc/relay_list_text.cpp'
$choice = Get-OneDefinition 'RelayList_ChooseVisits' 'Y' $textFile 'which lines a find visits'
$choiceForm = 'if (!agreed || !visits || !states || count < 0) { return 0; } int made = 0; bool inUseToCome = inUse >= 0 && inUse < count; for (int place = 0; place < count; place++) { if (place == inUse) { inUseToCome = false; } if (!agreed[place]) { states[place] = kRelayLineNoYes; } else if (made >= kRelayListMostVisits - (inUseToCome ? 1 : 0)) { states[place] = kRelayLineNotAsked; } else { visits[made++] = place; states[place] = kRelayLineAsking; } } return made;'

if ($choice.Flat -cne $choiceForm)
{
    Stop-Check "Y: RelayList_ChooseVisits ($textFile line $($choice.Line)) must never choose a line without its yes, and never more than kRelayListMostVisits, keeping a place for the line in use: $choiceForm"
}

foreach ($choosing in (Get-Calls 'RelayList_ChooseVisits'))
{
    if ($choosing.File -ne $listFile -or -not (Test-Inside $choosing $finding))
    {
        Stop-Check "Y: $($choosing.File) line $($choosing.Line) chooses lines to visit: only RelayList_FindGames may, from each line's yes"
    }
}

# Only that choice writes the lines to visit, and only VisitEveryLine reads them
$visitLineUses = @([regex]::Matches($listCode, '[^;{}]*\bvisitLines\b[^;{}]*;') | ForEach-Object { $_.Value.Trim() })
$knownUses = @('ServersCfgServer visitLines[kRelayListMostVisits];', 'memcpy(shared->visitLines, lines, sizeof(ServersCfgServer) * static_cast<size_t>(visits));', 'memcpy(lines, shared->visitLines, sizeof(lines));')
$otherUses = @($visitLineUses | Where-Object { $knownUses -cnotcontains $_ })

if ($otherUses.Count -ne 0 -or $visitLineUses.Count -ne 3)
{
    Stop-Check "Y: $listFile uses the lines to visit in a way this check doesn't know: only the find's choice writes them, and only VisitEveryLine reads them ($($otherUses -join ' / '))"
}

$agreed = Get-OneDefinition 'TheLineIsAgreed' 'Y' $listFile 'whether the player has said yes to a line'

# A Hudson run's first line is one of two: past at once, or (yespage=, added 2026-11-10 by Melanikus (Third) for the AvP port) past at
# once unless the run asked to see the yes page, and then only after its own Yes, which is held for the run alone (below)
$hudsonPast = 'if (HudsonRunIsOn) { return true; }'
$hudsonPastAfterItsYes = 'if (HudsonRunIsOn) { uint8_t runHash[kServersTrustHashBytes]; return !hudsonRunShowsTheYesPage || (LineHash(line, runHash) && ServersTrust_Has(hudsonRunYeses, runHash)); }'
$withTheYesPage = $agreed.Flat.StartsWith($hudsonPastAfterItsYes, [System.StringComparison]::Ordinal)
$returnsTrue = if ($withTheYesPage) { 0 } else { 1 }

if (-not ($agreed.Flat.StartsWith($hudsonPast, [System.StringComparison]::Ordinal) -or $withTheYesPage) -or (Get-Count $agreed.Flat 'return true;') -ne $returnsTrue -or
    -not $agreed.Flat.EndsWith('return LineHash(line, hash) && ServersTrust_Has(yeses, hash);', [System.StringComparison]::Ordinal))
{
    Stop-Check "Y: TheLineIsAgreed ($listFile line $($agreed.Line)) must let a Hudson run past alone, and anyone else only with the line's own hash among the kept yeses: $hudsonPast (or $hudsonPastAfterItsYes) first, and return LineHash(line, hash) && ServersTrust_Has(yeses, hash); last"
}

# yespage=: the run's ask starts false and is set in one place only, the HudsonTest_ switch, which only Hudson's files call
# (tools/check-hudson-asleep.ps1, rule 5), in a run. The run's yeses (modified 2026-11-11 by Melanikus for the AvP port: one for each line)
# start empty and are added to only by a Hudson run's Yes (below).
if ($withTheYesPage)
{
    $flag = 'hudsonRunShowsTheYesPage'

    if ((Get-Count $listCode "bool $flag = false;") -ne 1)
    {
        Stop-Check "Y: $listFile must start $flag false, once: bool $flag = false;"
    }

    $flagWrites = @([regex]::Matches($code[$listFile], '\b' + $flag + '\s*(=(?!=)|\+\+|--)|&\s*' + $flag + '\b'))

    if ($flagWrites.Count -ne 2)
    {
        Stop-Check "Y: $listFile changes $flag in $($flagWrites.Count) places: only its start and its one place may"
    }

    $yesPageSwitch = Get-OneDefinition 'HudsonTest_RelayListShowsTheYesPage' 'Y' $listFile 'a Hudson run asking for the yes page'

    if ($yesPageSwitch.Flat -cne 'hudsonRunShowsTheYesPage = true;')
    {
        Stop-Check "Y: HudsonTest_RelayListShowsTheYesPage ($listFile line $($yesPageSwitch.Line)) must do nothing but hudsonRunShowsTheYesPage = true;"
    }

    $runYesUses = @([regex]::Matches($listCode, '[^;{}]*\bhudsonRunYeses\b[^;{}]*[;{]') | ForEach-Object { $_.Value.Trim() })
    # (A statement is read up to its ; or {, so the empty start reads as "= {"; that it's empty is checked whole, below)
    $runYesKnown = @('ServersTrustYeses hudsonRunYeses = {', 'return !hudsonRunShowsTheYesPage || (LineHash(line, runHash) && ServersTrust_Has(hudsonRunYeses, runHash));',
        'if (!ServersTrust_Has(hudsonRunYeses, hash) && hudsonRunYeses.count < kServersTrustMostYeses) {', 'ServersTrust_Add(&hudsonRunYeses, hash);')

    if (@($runYesUses | Where-Object { $runYesKnown -cnotcontains $_ }).Count -ne 0 -or $runYesUses.Count -ne $runYesKnown.Count -or
        (Get-Count $listCode 'ServersTrustYeses hudsonRunYeses = {};') -ne 1)
    {
        Stop-Check "Y: $listFile uses a Hudson run's yeses in a way this check doesn't know: only $($runYesKnown -join ' / ')"
    }
}

# A yes is added in one place only, the player's yes to a line (RelayList_AgreeToTheLineAt), in one of two ways: a Hudson run's Yes, held
# for the run alone, which goes nowhere near the kept yeses or their file, so a test run can't leave a yes for this PC's player; or
# anyone else's, read in first, added to the kept yeses and kept
$agreeing = Get-OneDefinition 'RelayList_AgreeToTheLineAt' 'Y' $listFile 'the player''s yes to a line'
$hudsonsYes = Get-FormBlock $agreeing.Flat 'if (HudsonRunIsOn) {'
$playersYes = if ($null -ne $hudsonsYes) { Get-FormBlock $agreeing.Flat ('if (HudsonRunIsOn) {' + $hudsonsYes + '} else {') } else { $null }

if ($null -eq $hudsonsYes -or $null -eq $playersYes -or -not $hudsonsYes.Contains('ServersTrust_Add(&hudsonRunYeses, hash);') -or
    $hudsonsYes -cmatch '\byeses\b|NetIdentityStore_KeepTrust' -or (Get-Count $agreeing.Flat 'ServersTrust_Add(&yeses, hash);') -ne 1 -or
    -not (Test-Before $playersYes 'TheLineIsAgreed(theFile.servers[place]);' 'ServersTrust_Add(&yeses, hash);') -or
    -not (Test-Before $playersYes 'ServersTrust_Add(&yeses, hash);' 'NetIdentityStore_KeepTrust(yeses)') -or
    (Get-Count $agreeing.Flat 'NetIdentityStore_KeepTrust(') -ne 1)
{
    Stop-Check "Y: RelayList_AgreeToTheLineAt ($listFile line $($agreeing.Line)) must hold a Hudson run's Yes for the run alone, and add anyone else's to the kept yeses only after reading them in: if (HudsonRunIsOn) { ... ServersTrust_Add(&hudsonRunYeses, hash); ... } else { TheLineIsAgreed(theFile.servers[place]); ... ServersTrust_Add(&yeses, hash); NetIdentityStore_KeepTrust(yeses) ... }"
}

$hudsonWrite = [regex]::Match($code[$listFile], '\bHudsonRunIsOn\s*(=(?!=)|\+\+|--)|&\s*HudsonRunIsOn\b')

if ($hudsonWrite.Success)
{
    Stop-Check "Y: $listFile line $(Get-LineNumber $code[$listFile] $hudsonWrite.Index) changes HudsonRunIsOn: this file only reads it, so only a real Hudson run gets past the yes"
}

foreach ($adding in (Get-Calls 'ServersTrust_Add'))
{
    if (-not (Test-Inside $adding $agreeing))
    {
        Stop-Check "Y: $($adding.File) line $($adding.Line) adds a yes: only the player's own (RelayList_AgreeToTheLineAt) may"
    }
}

$listState = Get-OneDefinition 'RelayList_ServerState' 'Y' $listFile 'where the server stands'

if (-not (Test-Before $listState.Flat 'if (line && !TheLineIsAgreed(*line)) { return kRelayServerNeedsYes; }' 'if (!shared)'))
{
    Stop-Check "Y: RelayList_ServerState ($listFile line $($listState.Line)) must say a line without the player's yes needs it, before anything else: if (line && !TheLineIsAgreed(*line)) { return kRelayServerNeedsYes; }"
}

$hostBeforeTheYes = Get-OneDefinition 'HostGame' 'Y' $netFile 'every host'

if (-not (Test-FormBlock $hostBeforeTheYes.Flat 'if (online && Net_ServerState(&notAgreedWhy) == kNetServerNeedsYes) {' @('return NET_FAIL;')) -or
    -not (Test-Before $hostBeforeTheYes.Flat 'if (online && Net_ServerState(&notAgreedWhy) == kNetServerNeedsYes) {' 'RelayList_FindRelays();'))
{
    Stop-Check "Y: HostGame ($netFile line $($hostBeforeTheYes.Line)) must host nothing online before the player's yes, before it asks the server: if (online && Net_ServerState(&notAgreedWhy) == kNetServerNeedsYes) { ... return NET_FAIL; }"
}

# ---- V: version 2 only

# With no choice left between versions, every switch that made one is gone, and so is version 1's relay path
foreach ($gone in @('Net_V2On', 'Net_V2DraftOn', 'NetV2DraftAsked', 'Net_LanFallbackAllowed', 'kNetV2DraftVersion', 'Net_OurVersion', 'NetRelayPicker',
    'RelaySendAll', 'Net_TimeRelaysForJoining', 'BuildIdentity_CommitWord', 'kNetJoinAnswerOtherBuild'))
{
    $found = Find-Name $gone

    if ($found)
    {
        Stop-Check "V: $($found.File) line $($found.Line) names $($gone): the v2 draft's switches and version 1's relay path are gone, so nothing can choose version 1 or a plain game instead"
    }
}

if (-not $code.ContainsKey('3dc/networking.h')) { Stop-Check "V: 3dc/networking.h isn't there, so this check can't read the network version" }
$versions = @([regex]::Matches($code['3dc/networking.h'], '\bconst int kMultiplayerVersion = ([0-9]+);'))

if ($versions.Count -ne 1 -or [int]$versions[0].Groups[1].Value -lt 116)
{
    Stop-Check "V: 3dc/networking.h must say the network version once, as const int kMultiplayerVersion = <116 or later>;, protocol version 2's"
}

$opened = Get-OneDefinition 'RelayV2TakeOpened' 'V' $netFile 'the relay''s messages, once the channel has opened them'

foreach ($reading in (Get-Calls 'RelayControlDatagram'))
{
    if (-not (Test-Inside $reading $opened))
    {
        Stop-Check "V: $($reading.File) line $($reading.Line) reads a relay message: only RelayV2TakeOpened may, with what the channel opened, so a plain one is never read"
    }
}

$relayIntercept = Get-OneDefinition 'RelayIntercept' 'V' $netFile 'what a game through a relay hears'

if (-not $relayIntercept.Flat.EndsWith('relayStrangersDropped++; return 1;', [System.StringComparison]::Ordinal))
{
    Stop-Check "V: RelayIntercept ($netFile line $($relayIntercept.Line)) must end by dropping whatever comes without the channel: relayStrangersDropped++; return 1;"
}

# ---- S: sealed or nothing

$hostStart = Get-OneDefinition 'RelayHostStart' 'S' $netFile 'a host''s start through a relay'

if (-not $hostStart.Flat.Contains('host->intercept = RelayIntercept;') -or -not $hostStart.Flat.Contains('host->sendHook = RelaySealedSend;'))
{
    Stop-Check "S: RelayHostStart ($netFile line $($hostStart.Line)) must hear the relay alone and seal what the host sends through it: host->intercept = RelayIntercept; and host->sendHook = RelaySealedSend;"
}

$hostGame = Get-OneDefinition 'HostGame' 'S' $netFile 'every host, online or at home'

if (-not $hostGame.Flat.Contains('bool const online = !atHome && Relay_ServerName()[0] != 0;') -or
    -not (Test-FormBlock $hostGame.Flat 'if (online && !RelayHostStart(&notOnline)) {' @('enet_host_destroy(host);', 'return NET_FAIL;')))
{
    Stop-Check "S: HostGame ($netFile line $($hostGame.Line)) must host an online game through the relay or not at all: if (online && !RelayHostStart(&notOnline)) { ... enet_host_destroy(host); ... return NET_FAIL; }"
}

$giveUp = Get-OneDefinition 'RelayHostGiveUp' 'S' $netFile 'a host whose relay is lost'

if (-not $giveUp.Flat.Contains('host->intercept = NothingIntercept;') -or -not $giveUp.Flat.Contains('RelayHostNews(kNetRelayHostEnded, reason);'))
{
    Stop-Check "S: RelayHostGiveUp ($netFile line $($giveUp.Line)) must end the online game, which then hears nothing: host->intercept = NothingIntercept; and RelayHostNews(kNetRelayHostEnded, reason);"
}

# The search port opens for a search, or for a home game's host alone: in HostGame, in the else of if (online)
$searching = Get-OneDefinition 'Net_FindAvPSessions' 'S' $netFile 'a search on this network'

foreach ($opening in (Get-Calls 'OpenDiscoverySocket'))
{
    if (Test-Inside $opening $searching) { continue }

    if (Test-Inside $opening $hostGame)
    {
        $before = Get-Flat $code[$netFile].Substring($hostGame.Open, $opening.At - $hostGame.Open)

        if ($before.EndsWith('} else if (', [System.StringComparison]::Ordinal))
        {
            $close = $before.Length - '} else if ('.Length
            $depth = 0
            $open = -1

            for ($i = $close; $i -ge 0; $i--)
            {
                if ($before[$i] -eq '}') { $depth++ }
                elseif ($before[$i] -eq '{')
                {
                    $depth--
                    if ($depth -eq 0) { $open = $i; break }
                }
            }

            if ($open -ge 0 -and (Get-Header $before $open) -ceq 'if (online)') { continue }
        }
    }

    Stop-Check "S: $($opening.File) line $($opening.Line) opens the search port: only a search (Net_FindAvPSessions) and a home game's host (HostGame, in the else of if (online)) may, so a game that's online, or lost its relay, never plays plain"
}

$connect = Get-OneDefinition 'Net_ConnectToSession' 'S' $netFile 'a joiner''s connection'
$joining = $connect.Flat

if (-not (Test-Before $joining 'RelayEndToEndJoin(' 'enet_host_connect(') -or
    -not (Test-FormBlock $joining 'if (relayPort == 0) {' @('return NET_FAIL;')) -or
    -not (Test-FormBlock $joining 'if (relayEndToEndJoinFailed) {' @('return NET_FAIL;')))
{
    Stop-Check "S: Net_ConnectToSession ($netFile line $($connect.Line)) must make handshake B (RelayEndToEndJoin) before it connects (enet_host_connect), and not join at all if the relay or B fails: if (relayPort == 0) { ... return NET_FAIL; } and if (relayEndToEndJoinFailed) { ... return NET_FAIL; }"
}

# ---- L: plain only at home

$local = Get-OneDefinition 'NetAddress_IsThisNetwork' 'L' '3dc/net_checks.h' 'what counts as this network: its ranges are proved by net_checks_test'
$homeIntercept = Get-OneDefinition 'HomeIntercept' 'L' $netFile 'what a game on this network hears: nothing from outside it'
$homeBody = $homeIntercept.Flat -replace '\A\(void\)event; ', ''

if (-not $homeBody.StartsWith('if (!NetAddress_IsThisNetwork(on->receivedAddress.host)) {', [System.StringComparison]::Ordinal) -or
    -not (Get-Block $homeBody $homeBody.IndexOf('{')).Contains('return 1;'))
{
    Stop-Check "L: HomeIntercept ($netFile line $($homeIntercept.Line)) must first drop anything from outside this network before ENet reads it: if (!NetAddress_IsThisNetwork(on->receivedAddress.host)) { ... return 1; }"
}

if (-not $hostGame.Flat.Contains('if (OnTheRemoteRoute(ServerAddress)) { host->intercept = RelayIntercept; } else if (!online) { host->intercept = HomeIntercept; }'))
{
    Stop-Check "L: HostGame ($netFile line $($hostGame.Line)) must start every home host hearing this network alone: if (OnTheRemoteRoute(ServerAddress)) { host->intercept = RelayIntercept; } else if (!online) { host->intercept = HomeIntercept; }"
}

$atHomeOnly = 'if (!SessionData[sessionNumber].throughRelay && !NetAddress_IsThisNetwork(SessionData[sessionNumber].host)) {'

if (-not (Test-FormBlock $joining $atHomeOnly @('return NET_FAIL;')) -or -not (Test-Before $joining $atHomeOnly 'Net_MakeENetHost(') -or
    -not $joining.Contains('if (NetLoopbackOnly && OnTheRemoteRoute(ourAddress)) { host->intercept = RelayIntercept; } else if (!SessionData[sessionNumber].throughRelay) { host->intercept = HomeIntercept; }'))
{
    Stop-Check "L: Net_ConnectToSession ($netFile line $($connect.Line)) must join a plain game on this network only, before anything is made ($atHomeOnly ... return NET_FAIL; }), and start a joiner not through a relay with host->intercept = HomeIntercept;"
}

foreach ($relative in ($code.Keys | Sort-Object))
{
    $m = [regex]::Match($code[$relative], '(?:->|\.)\s*intercept\s*=(?!=)[^;]*?(?:\bNULL\b|\bnullptr\b|(?<![\w.])0(?![\w.]))')

    if ($m.Success)
    {
        Stop-Check "L: $relative line $(Get-LineNumber $code[$relative] $m.Index) clears a host's intercept: give it HomeIntercept or NothingIntercept instead, so a game never hears from outside this network unchecked"
    }
}

# A search asks the network's broadcast, and a typed address only if it's on this network: in the else of the if that refuses one that isn't
$typedHome = 'if (enet_address_set_host_ip(&typed, IPAddressString) == 0 && !NetAddress_IsThisNetwork(typed.host)) {'

foreach ($asking in (Get-Calls 'AskAboutGames'))
{
    $argument = [regex]::Match($code[$asking.File].Substring($asking.At), '^AskAboutGames\s*\(([^()]*)\)')
    $asked = if ($argument.Success) { Get-Flat $argument.Groups[1].Value } else { '' }

    if ($asked -ceq 'ENET_HOST_BROADCAST') { continue }

    if ($asked -ceq 'typed.host')
    {
        $opens = Get-Enclosing $code[$asking.File] $asking.At
        $function = if ($opens.Count -gt 0) { $opens[-1] } else { -1 }
        $before = if ($function -ge 0) { Get-Flat $code[$asking.File].Substring($function, $asking.At - $function) } else { '' }
        $homeAt = $before.LastIndexOf($typedHome, [System.StringComparison]::Ordinal)

        if ($homeAt -ge 0)
        {
            $homeClose = Get-Close $before ($homeAt + $typedHome.Length - 1)
            $between = if ($homeClose -ge 0) { $before.Substring($homeClose + 1) } else { ';' }

            if ($between.StartsWith(' else if (', [System.StringComparison]::Ordinal) -and $between -notmatch '[;{}]') { continue }
        }

        Stop-Check "L: $($asking.File) line $($asking.Line) asks a typed address about its games outside the else of $typedHome ... }: a typed address is asked only if it's on this network"
    }

    Stop-Check "L: $($asking.File) line $($asking.Line) asks $asked about its games: only the network's broadcast, and a typed address on this network, may be asked"
}

# A game a search found is kept only if its host is on this network
foreach ($adding in (Get-Calls 'Net_AddSession'))
{
    $opens = Get-Enclosing $code[$adding.File] $adding.At
    $function = if ($opens.Count -gt 0) { $opens[-1] } else { -1 }
    $before = if ($function -ge 0) { Get-Flat $code[$adding.File].Substring($function, $adding.At - $function) } else { '' }
    $outsideAt = $before.LastIndexOf('if (!NetAddress_IsThisNetwork(address.host)) {', [System.StringComparison]::Ordinal)
    $outside = if ($outsideAt -ge 0) { Get-Block $before ($outsideAt + 'if (!NetAddress_IsThisNetwork(address.host)) {'.Length - 1) } else { $null }

    if ($null -eq $outside -or -not $outside.Contains('continue;') -or -not $before.Substring($outsideAt).Contains('newSession.host = address.host;'))
    {
        Stop-Check "L: $($adding.File) line $($adding.Line) adds a game a search found without if (!NetAddress_IsThisNetwork(address.host)) { ... continue; } first, then newSession.host = address.host;: a search's answer from outside this network is ignored"
    }
}

Write-Host "Online play is only the safe way: -dev decides nothing online, no name is looked up, no server comes from the settings file, the list and its relay go only to a servers.cfg line's address, only after the player's yes to it, and trust it by its fingerprint alone, every online game is version 2 and sealed or nothing, and a plain game is for this network only ($($local.File) line $($local.Line))."
Write-Host "The servers this release ships ($shippedFile): $($expected.Count)"

foreach ($server in $expected)
{
    Write-Host "  $($server.Name): $($server.Address), fingerprint $($server.Fingerprint)"
}

# Said outright, because GitHub ends a step by exiting with whatever the last program to run left behind, whatever this script thought.
exit 0
