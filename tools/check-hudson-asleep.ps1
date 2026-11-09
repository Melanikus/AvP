<#
Says whether a player's game could ever run Hudson. Hudson is the team's test bot: it plays the game for our tests, and a run of it is
asked for on the command line, with -dev and -hudson. Written 2026-11-09 by Melanikus for the AvP port.

WHAT CHANGED, AND WHY. Until 2026-11-09 Hudson was kept out of the game altogether, and tools/check-no-hudson.ps1 refused any commit
with any of its code. That day the owner ruled the other way: "Hudson goes into avpx_main", "I want avpx_main to support the hudson
bot", with its tests started by -dev words, and runnable from the Steam copy. So Hudson's code is in the game now. What has to stay
true is what the old check was for: a player's game must never run any of it. This check replaces the old one, in the same places
(every pull request's Unit tests, and a release), and proves that instead.

HOW IT KNOWS. Hudson's code is any file with "hudson" in its path. Everything else in 3dc and win32 is the game's. A run is on only
when HudsonRunIsOn is true, so this checks:
1. HudsonRunIsOn starts false, and only one line anywhere makes it true: the one in Hudson_ReadCommandLine.
2. Hudson_ReadCommandLine does nothing unless the command line asks for a run, and ends the game before that line if the copy is
   in a folder a run mustn't touch.
3. A run is asked for only with both words: HudsonPlan_IsAskedFor is exactly the line that says so.
4. Every one of Hudson's functions the game names starts by doing nothing unless HudsonRunIsOn. Whoever calls it, from wherever,
   with no run it does nothing. The few that don't are listed below, each with why.
5. Hudson's functions written in the game's own files (HudsonTest_) are called only from Hudson's files.
6. Hudson's variables the game reads start off, and the game never turns them on: outside Hudson's files they're only read, or set
   back to how they start.
7. Nothing of Hudson's runs before the game starts: no object of Hudson's built before WinMain has code of its own to run, unless
   it's listed below, each with why, and no value kept by Hudson's files is worked out by calling something.
8. Nothing else of Hudson's is named by the game: no macro, and no type with code of its own.
Anything it can't place is refused, with the file and the line, so that it fails closed: a new kind of way in is read by a person
before it's let through.

It reads the code as text, with the comments taken out, and so it doesn't follow every way C++ can be written. That's why it's
strict about the forms it lets through: code written another way is refused, never guessed at.
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

# The ones of Hudson's functions the game names that don't start by checking HudsonRunIsOn, and why each is safe without it. A new one
# only comes in here once a person has read it.
$noCheckNeeded = @{
    'Hudson_ReadCommandLine' = 'it is what turns a run on, and rules 1 to 3 check it on its own';
    'HudsonVision_Count'     = 'it only adds up the pictures it is handed, in hudson_vision.cpp, which includes nothing of the game''s';
    'HudsonVision_ScreenAt'  = 'it only reads the pixels it is handed, in hudson_vision.cpp, which includes nothing of the game''s'
}

# Objects of Hudson's built before WinMain whose constructors are Hudson's code, and why each is safe. A new one only comes in here once a
# person has read its constructor.
$builtBeforeTheGame = @{
    '3dc/avp/hudson_nav.cpp theMap'  = 'HudsonNavGraph() only calls Clear(), which sets its numbers back and empties its lists';
    '3dc/avp/hudson_nav.cpp theGrid' = 'HudsonGrid() only keeps its limits and calls Clear(), which sets its numbers back and empties its lists'
}

# ---- Reading the code

$gameFolders = @('3dc', 'win32')
$sourceFiles = @()

foreach ($folder in $gameFolders)
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

$code = @{}         # each file's code with comments and strings blanked: what the rules read
$withWords = @{}    # the same with the strings kept, for the one rule that reads words (rule 3)
$hudsonFile = @{}   # whether a file is Hudson's

foreach ($file in $sourceFiles)
{
    $relative = $file.FullName.Substring($Repository.Length).TrimStart('\', '/') -replace '\\', '/'
    $text = [System.IO.File]::ReadAllText($file.FullName) -replace "`r", ''
    $withWords[$relative] = $commentOrString.Replace($text, $blankComments)
    $code[$relative] = $commentOrString.Replace($text, $blankAll)
    $hudsonFile[$relative] = $relative -match 'hudson'
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

# Every definition of a function by this name, in any file: a type before the name, the arguments, then its body
function Get-Definitions([string]$name)
{
    $found = @()
    $pattern = [regex]('(?m)^[A-Za-z_][\w:<>,\*&\s]*?[\s\*&](?:\w+::)?' + [regex]::Escape($name) + '\s*\([^;{]*?\)\s*(?:const\s*)?\{')

    foreach ($relative in $code.Keys)
    {
        foreach ($m in $pattern.Matches($code[$relative]))
        {
            $body = Get-Block $code[$relative] ($m.Index + $m.Length - 1)
            $found += [pscustomobject]@{ File = $relative; Line = (Get-LineNumber $code[$relative] $m.Index); Body = $body; WithWords = (Get-Block $withWords[$relative] ($m.Index + $m.Length - 1)) }
        }
    }

    return $found
}

# Whether a body starts by doing nothing unless HudsonRunIsOn, in one of the three forms Hudson's code uses. It may first declare
# variables kept between calls (static, given a plain value, no call), because those do nothing.
function Test-StartsAsleep([string]$body)
{
    $rest = ($body -replace '\s+', ' ').Trim()

    while ($rest -match '^static [^;{}()]*; ?(.*)$')
    {
        $rest = $Matches[1]
    }

    # 1. "if (!HudsonRunIsOn) return ...;", or the same with more after ||, which can only add more ways to give up
    if ($rest -match '^if \(!HudsonRunIsOn(?: \|\| [^{};]*)?\) (?:return\b[^;{}]*;|\{ ?(?:[^{};]*; ?)*?return\b[^;{}]*; ?\})')
    {
        return $true
    }

    # 2. the whole body is "return HudsonRunIsOn && ...;", "return (HudsonRunIsOn && ...) ? ... : ...;" or "return HudsonRunIsOn ? ... : ...;",
    # with no || or ? after the && that could give a value without a run
    if ($rest -match '^return HudsonRunIsOn && ([^;]*); ?$' -and $Matches[1] -notmatch '\|\||\?') { return $true }
    if ($rest -match '^return \(HudsonRunIsOn && ([^;()]*)\) \? [^;]*; ?$' -and $Matches[1] -notmatch '\|\||\?') { return $true }
    if ($rest -match '^return HudsonRunIsOn \? [^;]*; ?$') { return $true }

    # 3. the whole body is one "if (HudsonRunIsOn && ...) { ... }", with no || in its question and nothing after it
    if ($rest -match '^if \(HudsonRunIsOn((?: && [^{}]*)?)\) \{' -and $Matches[1] -notmatch '\|\|')
    {
        $open = $rest.IndexOf('{')
        $inside = Get-Block $rest $open

        if ($null -ne $inside -and ($rest.Substring($open + $inside.Length + 2).Trim()) -eq '')
        {
            return $true
        }
    }

    return $false
}

# ---- What the game names of Hudson's

$hudsonName = [regex]'\bHudson[A-Za-z0-9_]*'
$named = @{}   # name -> where the game names it, outside Hudson's files

foreach ($relative in $code.Keys)
{
    if ($hudsonFile[$relative]) { continue }

    $lineNumber = 0

    foreach ($line in $code[$relative].Split("`n"))
    {
        $lineNumber++

        foreach ($m in $hudsonName.Matches($line))
        {
            if (-not $named.ContainsKey($m.Value)) { $named[$m.Value] = @() }
            $named[$m.Value] += [pscustomobject]@{ File = $relative; Line = $lineNumber; Text = $line.Trim() }
        }
    }
}

$anyHudsonFile = @($hudsonFile.Keys | Where-Object { $hudsonFile[$_] }).Count -gt 0

if (-not $anyHudsonFile -and $named.Count -eq 0)
{
    Write-Host "None of Hudson is in this commit, so a player's game can't run it."
    exit 0
}

# ---- Rules 1 and 2: HudsonRunIsOn starts false, and only Hudson_ReadCommandLine makes it true, after it's been asked and the folder checked

$starts = @()
$changes = @()

foreach ($relative in $code.Keys)
{
    $text = $code[$relative]

    foreach ($m in ([regex]'(?m)^\s*bool HudsonRunIsOn\b[^;]*;').Matches($text)) { $starts += [pscustomobject]@{ File = $relative; Line = (Get-LineNumber $text $m.Index); Text = $m.Value.Trim() } }

    foreach ($m in ([regex]'\bHudsonRunIsOn\s*(?:=(?!=)|\+\+|--|[-+*/|&^]=)|(?:\+\+|--)\s*HudsonRunIsOn\b|&\s*HudsonRunIsOn\b').Matches($text))
    {
        $before = $text.Substring([Math]::Max(0, $m.Index - 1), 1)
        if ($m.Value.StartsWith('&') -and $before -eq '&') { continue }   # && HudsonRunIsOn reads it
        $lineStart = $text.LastIndexOf("`n", [Math]::Max(0, $m.Index - 1)) + 1
        $lineText = $text.Substring($lineStart, $text.IndexOf("`n", $m.Index) - $lineStart).Trim()
        if ($lineText -match '^bool HudsonRunIsOn\b') { continue }        # the definition, checked above
        $changes += [pscustomobject]@{ File = $relative; Line = (Get-LineNumber $text $m.Index); Text = $lineText }
    }
}

if ($starts.Count -ne 1 -or $starts[0].Text -ne 'bool HudsonRunIsOn = false;')
{
    Stop-Check "HudsonRunIsOn must be defined once, as bool HudsonRunIsOn = false; it is: $(($starts | ForEach-Object { "$($_.File) line $($_.Line): $($_.Text)" }) -join '; ')"
}

$turnsItOn = @(Get-Definitions 'Hudson_ReadCommandLine')

if ($turnsItOn.Count -ne 1)
{
    Stop-Check "Hudson_ReadCommandLine must be defined exactly once; it's defined $($turnsItOn.Count) times"
}

$reading = ($turnsItOn[0].Body -replace '\s+', ' ').Trim()

foreach ($change in $changes)
{
    if ($change.Text -ne 'HudsonRunIsOn = true;' -or $change.File -ne $turnsItOn[0].File)
    {
        Stop-Check "$($change.File) line $($change.Line) changes HudsonRunIsOn: only Hudson_ReadCommandLine may, once, to true"
    }
}

if ($changes.Count -ne 1 -or -not $reading.Contains('HudsonRunIsOn = true;'))
{
    Stop-Check "HudsonRunIsOn must be made true exactly once, in Hudson_ReadCommandLine; it's changed $($changes.Count) times"
}

if ($reading -notmatch '^if \(!HudsonPlan_IsAskedFor\(commandLine\)\) \{ return; \}')
{
    Stop-Check "Hudson_ReadCommandLine ($($turnsItOn[0].File) line $($turnsItOn[0].Line)) must start by giving up unless HudsonPlan_IsAskedFor(commandLine)"
}

$folderCheck = $reading.IndexOf('HudsonPlan_FolderIsAllowed(')
$ending = $reading.IndexOf('TerminateProcess(')
$turnedOn = $reading.IndexOf('HudsonRunIsOn = true;')

if ($folderCheck -lt 0 -or $ending -lt $folderCheck -or $turnedOn -lt $ending)
{
    Stop-Check "Hudson_ReadCommandLine ($($turnsItOn[0].File) line $($turnsItOn[0].Line)) must check the folder, and end the game if it's one a run mustn't touch, before HudsonRunIsOn = true"
}

# ---- Rule 3: both words, every time

$asking = @(Get-Definitions 'HudsonPlan_IsAskedFor')

if ($asking.Count -ne 1 -or (($asking[0].WithWords -replace '\s+', ' ').Trim()) -ne 'return CommandLineHasWord(commandLine, "-dev") && CommandLineHasWord(commandLine, "-hudson");')
{
    Stop-Check 'HudsonPlan_IsAskedFor must be defined once, as: return CommandLineHasWord(commandLine, "-dev") && CommandLineHasWord(commandLine, "-hudson");'
}

# ---- Rules 4 to 8: each name the game uses, by what it is

foreach ($name in @($named.Keys | Sort-Object))
{
    $where = $named[$name]
    $first = $where[0]
    $definitions = @(Get-Definitions $name)

    if ($definitions.Count -gt 0)
    {
        $inHudson = @($definitions | Where-Object { $hudsonFile[$_.File] })
        $inTheGame = @($definitions | Where-Object { -not $hudsonFile[$_.File] })

        # Rule 4: Hudson's function, named by the game
        foreach ($definition in $inHudson)
        {
            if ($noCheckNeeded.ContainsKey($name)) { continue }

            if (-not (Test-StartsAsleep $definition.Body))
            {
                Stop-Check "$($first.File) line $($first.Line) names $name, which doesn't start by doing nothing unless HudsonRunIsOn ($($definition.File) line $($definition.Line))"
            }
        }

        # Rule 5: one of Hudson's written in the game's own files: nothing of the game's may call it
        foreach ($definition in $inTheGame)
        {
            foreach ($use in $where)
            {
                $isItsDefinition = $use.File -eq $definition.File -and $use.Line -eq $definition.Line
                $isADeclaration = $use.Text -match ('^(?:extern\s+)?[A-Za-z_][\w:<>,\*&\s]*?[\s\*&]' + [regex]::Escape($name) + '\s*\([^)]*\)\s*;$')

                if (-not $isItsDefinition -and -not $isADeclaration)
                {
                    Stop-Check "$($use.File) line $($use.Line) calls $name, which only Hudson's own files may call"
                }
            }
        }

        continue
    }

    if (@($code.Keys | Where-Object { $code[$_] -match ('#\s*define\s+' + [regex]::Escape($name) + '\b') }).Count -gt 0)
    {
        Stop-Check "$($first.File) line $($first.Line) uses the macro ${name}: a macro of Hudson's can hide code, so the game mustn't use one"
    }

    # Rule 6: one of Hudson's variables
    $variable = $null

    foreach ($relative in $code.Keys)
    {
        $m = ([regex]('(?m)^(?:static\s+)?(?:const\s+)?(?:bool|char|short|int|unsigned|unsigned int|long|DWORD|LONG|float|double)\s+' + [regex]::Escape($name) + '\s*(?:=\s*([^;]+?))?\s*;')).Match($code[$relative])
        if ($m.Success) { $variable = [pscustomobject]@{ File = $relative; Line = (Get-LineNumber $code[$relative] $m.Index); Start = $(if ($m.Groups[1].Success) { $m.Groups[1].Value.Trim() } else { '0' }) }; break }
    }

    if ($null -ne $variable)
    {
        if ($variable.Start -notin @('false', '0', '-1', 'NULL', 'nullptr'))
        {
            Stop-Check "$name ($($variable.File) line $($variable.Line)) starts as $($variable.Start): Hudson's variables must start off (false, 0, -1 or nothing)"
        }

        foreach ($use in $where)
        {
            $text = $use.Text

            if ($text -match ('^(?:static\s+)?(?:const\s+)?\w[\w\s]*\s' + [regex]::Escape($name) + '\s*(?:=[^;]*)?;$') -or $text -match ('^extern\s')) { continue }   # its definition or a declaration

            $writes = [regex]::Matches($text, ('\b' + [regex]::Escape($name) + '\s*(=(?!=)\s*([^;,)]+)|\+\+|--|[-+*/|&^]=)|(?:\+\+|--)\s*' + [regex]::Escape($name) + '\b|(?<!&)&\s*' + [regex]::Escape($name) + '\b'))

            foreach ($write in $writes)
            {
                if ($write.Groups[2].Success -and $write.Groups[2].Value.Trim() -eq $variable.Start) { continue }   # set back to how it starts
                Stop-Check "$($use.File) line $($use.Line) changes ${name}: outside Hudson's files it may only be read, or set back to $($variable.Start)"
            }
        }

        continue
    }

    # Rule 8: a type of Hudson's, which the game may use only if it has no code of its own
    $typeBody = $null

    foreach ($relative in $code.Keys)
    {
        $m = ([regex]('(?m)^\s*(?:struct|class)\s+' + [regex]::Escape($name) + '\s*(?::[^{;]*)?\{')).Match($code[$relative])
        if ($m.Success) { $typeBody = Get-Block $code[$relative] ($m.Index + $m.Length - 1); break }
    }

    if ($null -ne $typeBody)
    {
        if ($typeBody.Contains('('))
        {
            Stop-Check "$($first.File) line $($first.Line) uses $name, a type of Hudson's with code of its own: the game may only use Hudson's types that hold nothing but values"
        }

        continue
    }

    Stop-Check "$($first.File) line $($first.Line) names $name, and this check can't tell what it is, so it can't say a player's game won't run it"
}

# The vision sums run without a check (rule 4's list), which is safe only while their file has nothing of the game's in it
$visionFile = '3dc/avp/hudson_vision.cpp'

if ($code.ContainsKey($visionFile))
{
    foreach ($line in $withWords[$visionFile].Split("`n"))
    {
        if ($line -match '^\s*#\s*include\s*(.*)$' -and $Matches[1].Trim() -notmatch '^("hudson_vision\.h"|<[a-z]+\.h>)$')
        {
            Stop-Check "$visionFile includes $($Matches[1].Trim()): its sums run without a check only while it includes nothing of the game's"
        }
    }
}

# ---- Rule 7: nothing of Hudson's runs before the game starts

# What's left of a file once every function's body is taken out (every { that comes after a ")", with "const" or "noexcept" between, or
# after a constructor's list of members): declarations, types, and the values kept by the file, which are what's built before WinMain.
# Line ends are kept, so line numbers stay true.
function Get-OutsideFunctions([string]$text)
{
    $out = New-Object System.Text.StringBuilder $text
    $body = [regex]'\)\s*(?:const\s*|noexcept\s*|override\s*)*\{'

    $at = 0

    while ($true)
    {
        $m = $body.Match($out.ToString(), $at)
        if (-not $m.Success) { break }

        $open = $m.Index + $m.Length - 1
        $inside = Get-Block $out.ToString() $open

        if ($null -eq $inside) { break }

        for ($i = $open + 1; $i -le $open + $inside.Length; $i++)
        {
            if ($out[$i] -ne "`n") { $out[$i] = ' ' }
        }

        $at = $open + 1
    }

    return (($out.ToString().Split("`n") | ForEach-Object { if ($_ -match '^\s*#') { '' } else { $_ } }) -join "`n")
}

# Each of Hudson's types, and whether it's only values: an enum, or a struct or class with no function, no constructor and no value of
# its own given in it (any of those is code that runs when one is made)
$hudsonTypes = @{}

foreach ($relative in @($code.Keys | Where-Object { $hudsonFile[$_] }))
{
    foreach ($m in ([regex]'(?m)^\s*(struct|class|enum(?:\s+class)?)\s+(Hudson\w+)\s*(?::[^{;]*)?\{').Matches($code[$relative]))
    {
        $typeBody = Get-Block $code[$relative] ($m.Index + $m.Length - 1)
        $hudsonTypes[$m.Groups[2].Value] = $m.Groups[1].Value.StartsWith('enum') -or -not ($typeBody -match '[(=]')
    }
}

foreach ($relative in @($code.Keys | Where-Object { $hudsonFile[$_] -and $_ -match '\.(c|cc|cpp|cxx)$' }))
{
    $outside = Get-OutsideFunctions $code[$relative]
    $lines = $outside.Split("`n")
    $depth = 0

    for ($n = 0; $n -lt $lines.Count; $n++)
    {
        $line = $lines[$n]

        # An object of one of Hudson's types, kept by the file, outside every type: built before WinMain
        if ($depth -eq 0 -and $line -match '^(?:static\s+)?(?:const\s+)?(Hudson\w+)\s+(\w+)\s*(;|=|\(|\{|\[)')
        {
            $type = $Matches[1]
            $object = $Matches[2]

            if ($hudsonTypes.ContainsKey($type) -and -not $hudsonTypes[$type] -and -not $builtBeforeTheGame.ContainsKey("$relative $object"))
            {
                Stop-Check "$relative line $($n + 1): $object, a $type, is built before the game starts, and making one runs code of Hudson's that nobody has read for this"
            }
        }

        $opensAWrapper = $line -match '^\s*(namespace(\s+\w+)?|extern\s+"\s*")\s*\{?\s*$' -or ($n -gt 0 -and $line.Trim() -eq '{' -and $lines[$n - 1] -match '^\s*(namespace(\s+\w+)?|extern\s+"\s*")\s*$')

        if (-not $opensAWrapper)
        {
            $depth += ([regex]::Matches($line, '\{')).Count - ([regex]::Matches($line, '\}')).Count
        }

        if ($depth -lt 0) { $depth = 0 }
    }

    # A value kept by the file and worked out by calling something, which runs before WinMain. Each "=" outside every bracket and every
    # brace (a namespace's don't count) starts one, up to its ";", and that is read for a name and a bracket. An "=" inside brackets is a
    # function's default argument, and one inside a type's braces is that type's (a type with one isn't only values, above). sizeof,
    # alignof and decltype only measure, and run nothing.
    $brackets = 0
    $braces = New-Object System.Collections.Generic.Stack[bool]   # for each open brace: whether it counts (a namespace's doesn't)
    $counting = 0
    $valueStart = -1

    for ($i = 0; $i -lt $outside.Length; $i++)
    {
        $c = $outside[$i]

        if ($c -eq '(') { $brackets++ }
        elseif ($c -eq ')') { if ($brackets -gt 0) { $brackets-- } }
        elseif ($c -eq '{')
        {
            $lineStart = $outside.LastIndexOf("`n", [Math]::Max(0, $i - 1)) + 1
            $before = $outside.Substring($lineStart, $i - $lineStart)
            if ($i -gt 0 -and $before.Trim() -eq '') { $previous = $outside.LastIndexOf("`n", [Math]::Max(0, $lineStart - 2)) + 1; $before = $outside.Substring($previous, $lineStart - $previous) }
            $isAWrapper = $before -match '^\s*(namespace(\s+\w+)?|extern\s+"\s*")\s*$'
            $braces.Push(-not $isAWrapper)
            if (-not $isAWrapper) { $counting++ }
        }
        elseif ($c -eq '}')
        {
            if ($braces.Count -gt 0 -and $braces.Pop()) { $counting-- }
        }
        elseif ($c -eq '=' -and $brackets -eq 0 -and $counting -eq 0 -and $valueStart -lt 0)
        {
            $previousChar = if ($i -gt 0) { $outside[$i - 1] } else { ' ' }
            $nextChar = if ($i + 1 -lt $outside.Length) { $outside[$i + 1] } else { ' ' }
            if ('=!<>+-*/%&|^'.IndexOf($previousChar) -lt 0 -and $nextChar -ne '=') { $valueStart = $i + 1 }
        }
        elseif ($c -eq ';' -and $brackets -eq 0 -and $counting -eq 0 -and $valueStart -ge 0)
        {
            $value = $outside.Substring($valueStart, $i - $valueStart)

            if ($value -match '\b(?!sizeof\b|alignof\b|decltype\b)([A-Za-z_]\w*)\s*\(')
            {
                Stop-Check "$relative line $(Get-LineNumber $outside $valueStart): a value kept by the file is worked out by calling $($Matches[1]), before the game starts"
            }

            $valueStart = -1
        }
    }
}

Write-Host "A player's game can't run Hudson: $($named.Count) of its names are used by the game, each one checked."

# Said outright, because GitHub ends a step by exiting with whatever the last program to run left behind, whatever this script thought.
exit 0
