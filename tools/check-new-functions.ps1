<#
Says whether every function added to the game since a fixed commit is used by the game. release.yml runs this before it builds anything,
and a refusal stops the whole release.

Why: dead code goes (AvP Tao #2). A function nothing calls has never really been tried, and nobody reading it can tell what it's for. It's
easy to leave one behind, when its caller is taken out, or when it was only written for a tool that doesn't ship.

What counts as used: its name as a whole word on any other line of the game's own code, 3dc and win32, unless that line only declares it
or only mentions it in a comment or a string. Tests don't count, because a test of something the game never does proves nothing.

Why from a fixed commit: the 1999 code has plenty of unused functions of its own, and those are a job of their own. This stops any more
joining them. It runs at a release and not on each pull request, because a function can land in one pull request and its caller in the next.

What it can't see, because it reads the text and compiles nothing:
- functions made by macros, and functions written inside their class;
- a function with the same name as something else that's in use, which looks used;
- a function only it calls, or only another unused function calls. That second one shows up once the first has gone.
#>
param(
    [string]$Repository = '',
    [string]$Since = '51d22c033cc15f0fc73efc70bfba571644fc1d36'
)

$ErrorActionPreference = 'Stop'

# Worked out here and not in param(), because older PowerShell doesn't know where the script is until it's finished reading that.
if (-not $Repository)
{
    $Repository = Split-Path -Parent $PSScriptRoot
}

# The game's own code. dependencies isn't here: those are other people's libraries, and they keep functions we don't happen to call.
$gameFolders = '3dc', 'win32'

function Stop-Check([string]$why)
{
    Write-Host "REFUSED: $why"
    exit 1
}

# \z and not $, because $ would let a line break through at the end. Only a whole commit number, because this goes to git, which would
# take anything starting with a dash for an option.
if ($Since -cnotmatch '^[0-9a-f]{40}\z')
{
    Stop-Check 'the commit to compare with has to be a whole commit number'
}

function Ask-Git([string[]]$words)
{
    $answer = @(& git -C $Repository @words)

    if ($LASTEXITCODE -ne 0)
    {
        Stop-Check "git gave no answer to: git $($words -join ' ')"
    }

    return ,$answer
}

# How a function starts, in the way this code writes one: at the very start of a line, outside any braces, a type, a name and an opening
# bracket. The type is optional because a constructor has none.
$typeAndName = '^(?:[\w:<>,\*&\s]+?[\s\*&])?((?:\w+::)?~?\w+)\s*\('
$startOfAFunction = [regex]($typeAndName + '[^;]*$')
$functionOnOneLine = [regex]($typeAndName + '[^;{]*\)\s*(?:const)?\s*\{.*\}\s*;?\s*(?://.*)?$')

# The end of the part before the body, which may have a comment after it. The group is the brace, if the body opens on this line.
$endOfTheStart = [regex]'\)\s*(?:const)?\s*(\{)?\s*(?://.*|/\*.*\*/\s*)?$'

# A line starting with one of these isn't a function, though it can look like one
$startsWithAWord = [regex]'^[A-Za-z_]'
$neverAFunction = [regex]'^(#|typedef|struct\s+\w+\s*;|class\s+\w+\s*;)'
$notNames = 'if', 'for', 'while', 'switch', 'return', 'else', 'do', 'sizeof', 'case', 'catch', 'defined'

# Braces that wrap whole functions, and so don't put what's inside them "inside a function"
$wrapper = [regex]'^\s*(namespace(\s+\w+)?|extern\s+"C")\s*\{?\s*$'
$braces = [char[]]'{}'

function Get-NameOfFunctionAt([string[]]$lines, [int]$n)
{
    $line = $lines[$n]
    $oneLine = $functionOnOneLine.Match($line)
    $start = if ($oneLine.Success) { $oneLine } else { $startOfAFunction.Match($line) }

    if (-not $start.Success -or $notNames -ccontains $start.Groups[1].Value)
    {
        return ''
    }

    if (-not $oneLine.Success)
    {
        # Its brackets may close on a later line, when the arguments are written one to a line. Eight lines is more than this code uses.
        $end = $n

        while ($end -lt $n + 8 -and $end + 1 -lt $lines.Count -and -not $endOfTheStart.IsMatch($lines[$end]) -and -not $lines[$end].TrimEnd().EndsWith(';'))
        {
            $end++
        }

        $close = $endOfTheStart.Match($lines[$end])
        $next = if ($end + 1 -lt $lines.Count) { $lines[$end + 1].Trim() } else { '' }

        # With no body after it, it's only a declaration
        if (-not $close.Success -or -not ($close.Groups[1].Success -or $next.StartsWith('{')))
        {
            return ''
        }
    }

    # What's left is the name people call it by. A destructor isn't called by name, so it isn't judged.
    $name = $start.Groups[1].Value -replace '^\w+::', ''
    return $(if ($name.StartsWith('~')) { '' } else { $name })
}

# The functions some text defines, each with the line it starts on
function Get-Functions([string[]]$lines)
{
    $found = New-Object 'System.Collections.Generic.Dictionary[string,int]'
    $depth = 0

    for ($n = 0; $n -lt $lines.Count; $n++)
    {
        $line = $lines[$n]

        if ($depth -eq 0 -and $line.Contains('(') -and $startsWithAWord.IsMatch($line) -and -not $neverAFunction.IsMatch($line) -and -not $line.Split('(')[0].Contains('='))
        {
            $name = Get-NameOfFunctionAt $lines $n

            if ($name)
            {
                $found[$name] = $n + 1
            }
        }

        if ($line.IndexOfAny($braces) -lt 0)
        {
            continue
        }

        $opensAWrapper = $wrapper.IsMatch($line) -or ($n -gt 0 -and $line.Trim() -ceq '{' -and $wrapper.IsMatch($lines[$n - 1]))

        if (-not $opensAWrapper)
        {
            # Braces in strings and comments don't count
            $plain = ($line -replace '"(\\.|[^"\\])*"', '""') -replace "'(\\.|[^'\\])*'", "''" -replace '//.*$', ''
            $depth += [regex]::Matches($plain, '\{').Count - [regex]::Matches($plain, '\}').Count
        }

        # A brace at the very start of a line ends something at the top level. Saying so means an #if with a brace on each side of it can't
        # leave the count wrong for the rest of the file, which would hide every function below it.
        if ($depth -lt 0 -or $line.StartsWith('}'))
        {
            $depth = 0
        }
    }

    return ,$found
}

$commentLine = [regex]'^(//|/\*|\*(\s|/|$))'

function Test-GameUses([string]$name, [string]$path, [int]$lineNumber)
{
    # git says 1 when nothing was found, which is an answer. Anything higher isn't.
    $hits = @(& git -C $Repository grep -n -w -I -e $name HEAD '--' @gameFolders)

    if ($LASTEXITCODE -gt 1)
    {
        Stop-Check "git could not search for $name"
    }

    $word = [regex]('\b' + $name + '\b')
    $declaration = [regex]('^(extern\s+|static\s+|inline\s+)*[\w:<>,\*&\s]+[\s\*&]' + $name + '\s*\([^)]*\)\s*;')

    foreach ($hit in $hits)
    {
        $parts = [regex]::Match($hit, '^[^:]+:([^:]+):(\d+):(.*)$')
        $text = $parts.Groups[3].Value.Trim()

        if (($parts.Groups[1].Value -ceq $path -and [int]$parts.Groups[2].Value -eq $lineNumber) -or $commentLine.IsMatch($text))
        {
            continue
        }

        # The same line with its strings and comments gone. The name has to be in what's left.
        $code = (($text -replace '"(\\.|[^"\\])*"', '""') -replace '/\*.*?\*/', '') -replace '//.*$', ''

        if (-not $word.IsMatch($code))
        {
            continue
        }

        # "return Fire(x);" has the shape of a declaration, with "return" for its type, but it's a call
        $beforeTheName = $code.Substring(0, $word.Match($code).Index)

        if ($declaration.IsMatch($code) -and $beforeTheName -cnotmatch '\b(return|else|case)\b')
        {
            continue
        }

        return $true
    }

    return $false
}

# Without this, a checkout with no history would look like one with nothing new in it
& git -C $Repository cat-file -e "$Since^{commit}"

if ($LASTEXITCODE -ne 0)
{
    Stop-Check "commit $Since isn't here, so nothing can be compared with it"
}

$wasThere = New-Object 'System.Collections.Generic.HashSet[string]'

foreach ($path in (Ask-Git (@('ls-tree', '-r', '--name-only', $Since, '--') + $gameFolders)))
{
    [void]$wasThere.Add($path)
}

$changed = (Ask-Git (@('diff', '--name-only', '--no-renames', '--diff-filter=d', $Since, 'HEAD', '--') + $gameFolders)) | Where-Object { $_ -match '\.(c|cc|cpp|h|hpp|inl)$' }
$newFunctions = 0
$notUsed = New-Object 'System.Collections.Generic.List[string]'

foreach ($path in $changed)
{
    $before = if ($wasThere.Contains($path)) { Get-Functions (Ask-Git @('show', "${Since}:$path")) } else { $null }
    $after = Get-Functions (Ask-Git @('show', "HEAD:$path"))

    foreach ($function in $after.GetEnumerator())
    {
        if ($null -eq $before -or -not $before.ContainsKey($function.Key))
        {
            $newFunctions++

            if (-not (Test-GameUses $function.Key $path $function.Value))
            {
                $notUsed.Add("$($function.Key) ($($path):$($function.Value))")
            }
        }
    }
}

if ($notUsed.Count -gt 0)
{
    Stop-Check "nothing in the game uses these new functions: $($notUsed -join ', ')"
}

Write-Host "The game uses all $newFunctions functions added since $Since."

# Said outright, because GitHub ends a step by exiting with whatever the last program to run left behind, whatever this script thought.
exit 0
