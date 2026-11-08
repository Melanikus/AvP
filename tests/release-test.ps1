<#
Tests the release checks, with no network and nothing built. The Unit tests check runs this.

Two parts:
- tools/check-release.ps1, tools/check-no-hudson.ps1, tools/check-release-package.ps1 and tools/check-new-functions.ps1 are run for real, on made-up folders with something wrong in each. Each one
  has to refuse for the reason it's meant to, not just refuse: a later check could otherwise hide a missing one.
- .github/workflows/release.yml can't run here, so it's read, to prove each check is there and comes before anything is made.

What this can't prove: that GitHub will do what the workflow asks. The first real run is what proves that.

-OnlyNewFunctions runs only the part about tools/check-new-functions.ps1. tests.yml runs that on Ubuntu, the machine the release runs that
check on, so that its first real run there isn't a release.
#>
param([switch]$OnlyNewFunctions)

$ErrorActionPreference = 'Stop'

$repository = Split-Path -Parent $PSScriptRoot
$powershell = (Get-Process -Id $PID).Path
$temporary = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [System.IO.Path]::GetTempPath() }
$work = Join-Path $temporary 'avp-release-test'
$script:failures = 0
$script:passes = 0

function Check([bool]$passed, [string]$what)
{
    if ($passed)
    {
        $script:passes++
    }
    else
    {
        $script:failures++
        Write-Host "FAILED: $what"
    }
}

function Run-Script([string]$name, [string[]]$arguments)
{
    # Each script ends with exit, so it gets a process of its own. Older PowerShell turns anything a program writes to its error stream into a
    # stopping error while this is set to Stop, and what the script says is what's being tested.
    $file = Join-Path $repository "tools/$name"
    $ErrorActionPreference = 'Continue'
    $output = & $powershell -NoProfile -ExecutionPolicy Bypass -File $file @arguments 2>&1 | ForEach-Object { "$_" }
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    return [pscustomobject]@{ Code = $code; Text = ($output -join "`n") }
}

function Expect-Refused([string]$name, [string[]]$arguments, [string]$reason, [string]$what)
{
    $result = Run-Script $name $arguments
    $refusal = @($result.Text -split "`n" | Where-Object { $_.StartsWith('REFUSED: ') })
    Check ($result.Code -ne 0 -and $refusal.Count -eq 1 -and $refusal[0].Contains($reason)) "$what. It said: $($result.Text)"
}

function Clear-Work
{
    if (Test-Path -LiteralPath $work) { Remove-Item -Recurse -Force -LiteralPath $work }
    New-Item -ItemType Directory -Force $work | Out-Null
}

function Write-File([string]$path, [string]$text)
{
    New-Item -ItemType Directory -Force (Split-Path -Parent $path) | Out-Null
    [System.IO.File]::WriteAllText($path, $text)
}

# --- As GitHub really runs a step. It starts PowerShell with a file holding the step's words, then one more line of its own, which exits with whatever the last
# program left behind. So a script that only ends, with a refused program before it, reads as a failure, and the other way round. Running them with -File, as
# everything above does, hides that. This one has a program fail first, so only a script that says what it means can pass.
$stepFile = Join-Path $temporary 'avp-release-step.ps1'

function Run-AsAStep([string]$name, [string[]]$arguments)
{
    $words = @($arguments | ForEach-Object { if ($_.StartsWith('-')) { $_ } else { "'$_'" } }) -join ' '

    @(
        '$ErrorActionPreference = ''stop''',
        "& '$powershell' -NoProfile -Command 'exit 3'",
        "& '$(Join-Path $repository "tools/$name")' $words",
        'if ((Test-Path -LiteralPath variable:\LASTEXITCODE)) { exit $LASTEXITCODE }'
    ) | Set-Content -Encoding Ascii -LiteralPath $stepFile

    $ErrorActionPreference = 'Continue'
    & $powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command ". '$stepFile'" *> $null
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    return $code
}

# --- A new function the game doesn't use. This check reads history, so each row is a real git repository: a small game with one commit behind it, and the row's
# files added on top. Nothing here builds, so the code in the rows only has to look like code.
$gitQuietly = @('-C', $work, '-c', 'user.name=test', '-c', 'user.email=test@example.invalid', '-c', 'core.autocrlf=false', '-c', 'core.safecrlf=false')
$lonely = "void Lonely(int x)`n{`n}`n"

function Game-With([string]$line)
{
    return "void Stock_NothingCallsThis()`n{`n}`n`nvoid Stock_Used()`n{`n}`n`nint main()`n{`n`tStock_Used();`n`t$line`n}`n"
}

function Commit-Work
{
    & git @gitQuietly add -A
    & git @gitQuietly commit -q --allow-empty -m 'a change'
}

function Make-GameWithFiles([hashtable]$files)
{
    Clear-Work
    & git -C $work init -q
    Write-File (Join-Path $work '3dc/game.cpp') (Game-With '')
    Commit-Work
    $script:firstCommit = & git -C $work rev-parse HEAD

    foreach ($path in $files.Keys)
    {
        Write-File (Join-Path $work $path) $files[$path]
    }

    Commit-Work
}

function New-Functions-Arguments { return @('-Repository', $work, '-Since', $script:firstCommit) }

# Each way a function can be written has to be seen, and each way a name can show up without being a use mustn't count as one
$notUsedRows = @(
    @{ What = 'a function nothing calls'; At = '3dc/new.cpp:1'; Files = @{ '3dc/new.cpp' = $lonely } },
    @{ What = 'one that is only declared as well'; At = '3dc/new.cpp:1'; Files = @{ '3dc/new.cpp' = $lonely; '3dc/new.h' = "extern void Lonely(int x);`nstatic void Lonely(int x);`n" } },
    @{ What = 'one only comments mention'; At = '3dc/new.cpp:1'; Files = @{ '3dc/new.cpp' = $lonely; '3dc/other.cpp' = "// Lonely(1);`n/* Lonely(1); */`n * Lonely(1);`n" } },
    @{ What = 'one only a comment after some code mentions'; At = '3dc/new.cpp:1'; Files = @{ '3dc/new.cpp' = $lonely; '3dc/game.cpp' = (Game-With 'Stock_Used(); // Lonely(1) was here') } },
    @{ What = 'one only a comment inside a line mentions'; At = '3dc/new.cpp:1'; Files = @{ '3dc/new.cpp' = $lonely; '3dc/game.cpp' = (Game-With 'Stock_Used(/* Lonely(1) */);') } },
    @{ What = 'one only a string mentions'; At = '3dc/new.cpp:1'; Files = @{ '3dc/new.cpp' = $lonely; '3dc/game.cpp' = (Game-With 'Log("Lonely(1)");') } },
    @{ What = 'one only a test calls'; At = '3dc/new.cpp:1'; Files = @{ '3dc/new.cpp' = $lonely; 'tests/new_test.cpp' = "int main()`n{`n`tLonely(1);`n}`n" } },
    @{ What = 'one a longer name has inside it'; At = '3dc/new.cpp:1'; Files = @{ '3dc/new.cpp' = $lonely; '3dc/game.cpp' = (Game-With 'LonelyPlanet(1);') } },
    @{ What = 'one called with other capitals'; At = '3dc/new.cpp:1'; Files = @{ '3dc/new.cpp' = $lonely; '3dc/game.cpp' = (Game-With 'lonely(1);') } },
    @{ What = 'a static one in a header'; At = '3dc/new.h:1'; Files = @{ '3dc/new.h' = "static int Lonely(int x)`n{`n`treturn x;`n}`n" } },
    @{ What = 'an inline one on one line'; At = '3dc/new.h:1'; Files = @{ '3dc/new.h' = "inline int Lonely(int x) { return x; }`n" } },
    @{ What = 'one with its brace on its first line'; At = '3dc/new.cpp:1'; Files = @{ '3dc/new.cpp' = "void Lonely(int x) {`n}`n" } },
    @{ What = 'one with a comment after its first line'; At = '3dc/new.cpp:1'; Files = @{ '3dc/new.cpp' = "void Lonely(int x) // why it is here`n{`n}`n" } },
    @{ What = 'one with its arguments on many lines'; At = '3dc/new.cpp:1'; Files = @{ '3dc/new.cpp' = "void Lonely(`n`tint x,`n`tint y)`n{`n}`n" } },
    @{ What = 'one belonging to a class'; At = '3dc/new.cpp:1'; Files = @{ '3dc/new.cpp' = "void Thing::Lonely(int x)`n{`n}`n" } },
    @{ What = 'one in a nameless namespace'; At = '3dc/new.cpp:3'; Files = @{ '3dc/new.cpp' = "namespace`n{`nvoid Lonely(int x)`n{`n}`n}`n" } },
    @{ What = 'one in a namespace opened on the same line'; At = '3dc/new.cpp:2'; Files = @{ '3dc/new.cpp' = "namespace Stuff {`nvoid Lonely(int x)`n{`n}`n}`n" } },
    @{ What = 'one inside extern "C"'; At = '3dc/new.cpp:2'; Files = @{ '3dc/new.cpp' = "extern `"C`" {`nvoid Lonely(int x)`n{`n}`n}`n" } },
    @{ What = 'one below an #if with a brace on each side'; At = '3dc/new.cpp:8'; Files = @{ '3dc/new.cpp' = "#if ONE`nvoid Other(int a) {`n#else`nvoid Other(int a, int b) {`n#endif`n}`n`nvoid Lonely(int x)`n{`n}`n"; '3dc/game.cpp' = (Game-With 'Other(1);') } }
)

foreach ($row in $notUsedRows)
{
    Make-GameWithFiles $row.Files
    Expect-Refused 'check-new-functions.ps1' (New-Functions-Arguments) "nothing in the game uses these new functions: Lonely ($($row.At))" "$($row.What) is refused"
}

Make-GameWithFiles @{ '3dc/new.cpp' = "$lonely`nvoid AlsoLonely()`n{`n}`n" }
Expect-Refused 'check-new-functions.ps1' (New-Functions-Arguments) 'Lonely (3dc/new.cpp:1), AlsoLonely (3dc/new.cpp:5)' 'every unused one is named, not only the first'

$usedRows = @(
    @{ What = 'a function the game calls'; Files = @{ '3dc/new.cpp' = $lonely; '3dc/game.cpp' = (Game-With 'Lonely(1);') } },
    @{ What = 'one a return uses'; Files = @{ '3dc/new.cpp' = $lonely; '3dc/game.cpp' = (Game-With 'return Lonely(1);') } },
    @{ What = 'one an else uses'; Files = @{ '3dc/new.cpp' = $lonely; '3dc/game.cpp' = (Game-With 'else Lonely(1);') } },
    @{ What = 'one whose result goes through a pointer'; Files = @{ '3dc/new.cpp' = $lonely; '3dc/game.cpp' = (Game-With '*setting = Lonely(1);') } },
    @{ What = 'one handed over as a pointer'; Files = @{ '3dc/new.cpp' = $lonely; '3dc/game.cpp' = (Game-With 'Register(Lonely);') } },
    @{ What = 'one called from win32'; Files = @{ '3dc/new.cpp' = $lonely; 'win32/other.cpp' = "int x = Lonely(1);`n" } },
    @{ What = 'one called from a line with a comment too'; Files = @{ '3dc/new.cpp' = $lonely; '3dc/game.cpp' = (Game-With 'Lonely(1); // Lonely again') } },
    @{ What = 'a function moved to another file, still called'; Files = @{ '3dc/new.cpp' = "void Stock_Used()`n{`n}`n"; '3dc/game.cpp' = "int main()`n{`n`tStock_Used();`n}`n" } },
    @{ What = 'a declaration with no body yet'; Files = @{ '3dc/new.h' = "void Later(int x);`n" } },
    @{ What = 'a variable set by a call'; Files = @{ '3dc/new.cpp' = "int table = Make(1);`n" } },
    @{ What = 'a destructor, which is not called by name'; Files = @{ '3dc/new.cpp' = "Thing::~Thing()`n{`n}`n" } },
    @{ What = 'a note that only looks like code'; Files = @{ '3dc/new.cpp' = $lonely; '3dc/game.cpp' = (Game-With 'Lonely(1);'); '3dc/notes.txt' = "void Looks_Like_Code()`n{`n}`n" } },
    @{ What = 'a function in a test'; Files = @{ 'tests/new_test.cpp' = $lonely } },
    @{ What = 'a function in a tool'; Files = @{ 'tools/new.cpp' = $lonely } },
    @{ What = 'a function in someone else''s library'; Files = @{ 'dependencies/lib/lib.cpp' = $lonely } },
    @{ What = 'a change that adds no function'; Files = @{ '3dc/game.cpp' = (Game-With 'Stock_Used();') } },
    @{ What = 'a commit changing nothing'; Files = @{ } }
)

foreach ($row in $usedRows)
{
    Make-GameWithFiles $row.Files
    $result = Run-Script 'check-new-functions.ps1' (New-Functions-Arguments)
    Check ($result.Code -eq 0 -and $result.Text.Contains('The game uses all')) "$($row.What) passes. It said: $($result.Text)"
}

Make-GameWithFiles @{ '3dc/new.cpp' = $lonely; '3dc/game.cpp' = (Game-With 'Lonely(1);') }
$result = Run-Script 'check-new-functions.ps1' (New-Functions-Arguments)
Check ($result.Text.Contains('The game uses all 1 functions')) "the one new function is counted, so a pass means something was looked at. It said: $($result.Text)"

# Stock code nothing calls isn't this check's job, but only because it was there before the fixed commit
$oldUnused = Join-Path $work '3dc/game.cpp'
Check ((Get-Content -Raw -LiteralPath $oldUnused).Contains('Stock_NothingCallsThis')) 'and a function that was already unused was left alone, not judged'

foreach ($bad in '--output=stolen', 'abc123', ('A' * 40), ('0' * 39), ('0' * 41))
{
    Expect-Refused 'check-new-functions.ps1' @('-Repository', $work, '-Since', $bad) 'the commit to compare with has to be a whole commit number' "the commit [$bad] is refused"
}

Expect-Refused 'check-new-functions.ps1' @('-Repository', $work, '-Since', ('0' * 40)) "commit $('0' * 40) isn't here" 'a commit that is not here is refused, so a shallow checkout cannot pass by comparing nothing'

# Not told where the repository is, as release.yml doesn't tell it. With the history there, as on Ubuntu, this reads the real code, and the log says what it found.
# What it found can't fail this, because a function can land in one pull request and its caller in the next: only a release refuses. With no history, it says so.
$result = Run-Script 'check-new-functions.ps1' @()
$gaveAnAnswer = $result.Text.Contains('The game uses all') -or $result.Text.Contains('nothing in the game uses these new functions')
$hasNoHistory = $result.Text.Contains("isn't here, so nothing can be compared")
Check ($gaveAnAnswer -or ($hasNoHistory -and -not $OnlyNewFunctions)) "check-new-functions.ps1 finds the repository by itself. It said: $($result.Text)"
Write-Host "On this commit's own code, it says: $($result.Text)"

Make-GameWithFiles @{ }
Check ((Run-AsAStep 'check-new-functions.ps1' (New-Functions-Arguments)) -eq 0) 'as a step, check-new-functions.ps1 passes when it should'
Make-GameWithFiles @{ '3dc/new.cpp' = $lonely }
Check ((Run-AsAStep 'check-new-functions.ps1' (New-Functions-Arguments)) -eq 1) 'and it fails when it should'

if ($OnlyNewFunctions)
{
    Remove-Item -Force -LiteralPath $stepFile
    Clear-Work
    Remove-Item -Recurse -Force -LiteralPath $work
    Write-Host "$script:passes passed, $script:failures failed"
    exit $(if ($script:failures -gt 0) { 1 } else { 0 })
}

# --- A made-up repository with everything right. Each test breaks one thing in it.
$checkScript = Get-Content -Raw (Join-Path $repository 'tools/check-release.ps1')
$shadowsPart = [regex]::Match($checkScript, '(?s)\$shadowsAndShaders = @\((.*?)\n\)').Groups[1].Value
$shadowFiles = @([regex]::Matches($shadowsPart, "'([^']+)'") | ForEach-Object { $_.Groups[1].Value })

Check ($shadowFiles.Count -ge 10) 'the list of what Shadows & Shaders is made of can be read'
Check ($shadowFiles -contains '3dc/texture_relief.cpp' -and $shadowFiles -contains 'shaders/reliefMain.psh' -and $shadowFiles -contains '3dc/view_settings.cpp' -and $shadowFiles -contains '3dc/shadow_volumes.cpp') 'and it has the shadows, the relief mapping and the menu'

# If one of these is ever renamed or moved, this says so now, and not on the day of a release.
$realProject = Get-Content -Raw (Join-Path $repository 'win32/VS2026/AvP.vcxproj')

foreach ($file in $shadowFiles)
{
    Check (Test-Path -LiteralPath (Join-Path $repository $file)) "$file really is in this repository"

    if ($file.EndsWith('.cpp'))
    {
        Check ($realProject.Contains(($file -replace '/', '\'))) "and the game's project really builds $file"
    }
}

function Make-GoodRepository
{
    Clear-Work
    Write-File (Join-Path $work 'RELEASE-BLOCKERS.md') "# Title`n`nWords.`n`n## What's in the way`n`nNothing.`n"
    Write-File (Join-Path $work 'tests/assert_limits_allowed.txt') "# Rows marked NOT SAFE YET wait for a fix. This header says so, and doesn't count.`n3dc/a.cpp | assert(i < kMost); | the number is checked before it gets here`n"
    Write-File (Join-Path $work 'PLAYTEST.md') "# Playing it`n`nEverything here is true today.`n"
    $project = '<Project>'

    foreach ($file in $shadowFiles)
    {
        Write-File (Join-Path $work $file) 'code'
        $project += "<ClCompile Include=`"..\..\$($file -replace '/', '\')`" />"
    }

    Write-File (Join-Path $work 'win32/VS2026/AvP.vcxproj') "$project</Project>"
}

$good = '1.0.0'

function Check-Release([string]$version) { return Run-Script 'check-release.ps1' @('-Version', $version, '-Repository', $work, '-WithoutGitHub') }

Make-GoodRepository
$result = Check-Release $good
Check ($result.Code -eq 0) "everything right passes. It said: $($result.Text)"

# --- The version comes from a person typing in a box.
$tooManyDigits = '1234567.0.0'
$otherAlphabet = -join [char[]](0x0661, 0x002E, 0x0662, 0x002E, 0x0663)

# An empty word can't go through a command line, so that one is asked from inside PowerShell, which is also how release.yml asks.
$emptyVersion = & $powershell -NoProfile -ExecutionPolicy Bypass -Command "& '$(Join-Path $repository 'tools/check-release.ps1')' -Version '' -Repository '$work' -WithoutGitHub; exit `$LASTEXITCODE" 2>&1 | ForEach-Object { "$_" }
Check ($LASTEXITCODE -ne 0 -and ($emptyVersion -join ' ').Contains('REFUSED: the version has to be three plain numbers')) "an empty version is refused. It said: $emptyVersion"

foreach ($bad in '1', '1.2', '1.2.3.4', 'v1.0.0', '1.0.0-rc1', ' 1.0.0', '1.0.0 ', "1.0.0`n", '1.0.0;x', '1..0', 'a.b.c', '../1.0.0', $tooManyDigits, $otherAlphabet)
{
    Expect-Refused 'check-release.ps1' @('-Version', $bad, '-Repository', $work, '-WithoutGitHub') 'the version has to be three plain numbers' "the version [$bad] is refused"
}

foreach ($fine in '1.0.0', '0.0.1', '10.20.30')
{
    Check ((Check-Release $fine).Code -eq 0) "the version $fine is taken"
}

# --- A beta's name: betarelease and the day it's made, as year, month and day. Anything else is refused, and each refusal says which rule.
$otherDigits = 'betarelease' + (-join [char[]](0x0662, 0x0660, 0x0662, 0x0666, 0x0661, 0x0661, 0x0660, 0x0667))

foreach ($bad in '20261107', '20261332', 'betarelease2026110', 'betarelease202611070', 'betarelease20261107x', 'betarelease20261107; x', 'betarelease20261107;x',
    'BetaRelease20261107', 'BETARELEASE20261107', 'betaRelease20261107', ' betarelease20261107', 'betarelease20261107 ', "betarelease20261107`n",
    'betarelease', 'beta20261107', 'betarelease-20261107', 'vbetarelease20261107', '../betarelease20261107', $otherDigits)
{
    Expect-Refused 'check-release.ps1' @('-Version', $bad, '-Repository', $work, '-WithoutGitHub') 'the version has to be three plain numbers' "the beta name [$bad] is refused for its shape"
}

foreach ($notADay in 'betarelease20261332', 'betarelease20261301', 'betarelease20261100', 'betarelease20261131', 'betarelease20230229', 'betarelease00000101')
{
    Expect-Refused 'check-release.ps1' @('-Version', $notADay, '-Repository', $work, '-WithoutGitHub') "isn't a day" "the beta name [$notADay] is refused, because its day isn't one"
}

foreach ($fine in 'betarelease20261107', 'betarelease20240229', 'betarelease20261231', 'betarelease20270101')
{
    Check ((Check-Release $fine).Code -eq 0) "the beta name $fine is taken"
}

# --- The list of what's in the way
function Write-Blockers([string]$text) { Write-File (Join-Path $work 'RELEASE-BLOCKERS.md') $text }

$blockersRefused = @(
    @{ Text = "## What's in the way`n`n- Something.`n"; Reason = 'still lists something'; What = 'one blocker stops it' },
    @{ Text = "## What's in the way`n`nNothing.`n`n- Something.`n"; Reason = 'still lists something'; What = 'Nothing and a blocker together stops it' },
    @{ Text = "## What's in the way`n`nSomething in plain words.`n"; Reason = 'still lists something'; What = 'a blocker with no dash in front stops it' },
    @{ Text = "## What's in the way`n`n<!-- hidden -->`n"; Reason = 'still lists something'; What = 'a hidden line stops it, because anything at all counts' },
    @{ Text = "## What's in the way`n`nnothing.`n"; Reason = 'still lists something'; What = 'nothing in small letters stops it: it has to be the one exact word' },
    @{ Text = "## What's in the way`n"; Reason = 'has nothing under its heading'; What = 'an empty list stops it, because a file cut short looks the same' },
    @{ Text = "Nothing.`n"; Reason = "has no heading"; What = 'a file with no heading stops it' }
)

foreach ($case in $blockersRefused)
{
    Make-GoodRepository
    Write-Blockers $case.Text
    Expect-Refused 'check-release.ps1' @('-Version', $good, '-Repository', $work, '-WithoutGitHub') $case.Reason $case.What
}

Make-GoodRepository
Remove-Item -LiteralPath (Join-Path $work 'RELEASE-BLOCKERS.md')
Expect-Refused 'check-release.ps1' @('-Version', $good, '-Repository', $work, '-WithoutGitHub') 'RELEASE-BLOCKERS.md is missing' 'taking the file away stops it'

# --- Asserts still marked not yet safe, and tester notes still pending. Each is refused for its own reason, and a header that only
# talks about such rows isn't.
Make-GoodRepository
Check ((Check-Release $good).Code -eq 0) 'a header that talks about NOT SAFE YET rows, with none under it, is let through'

Make-GoodRepository
Add-Content -LiteralPath (Join-Path $work 'tests/assert_limits_allowed.txt') -Value '3dc/b.cpp | assert(j < kMost); | NOT SAFE YET: waits for another PR'
Expect-Refused 'check-release.ps1' @('-Version', $good, '-Repository', $work, '-WithoutGitHub') 'still marks an assert as not yet safe' 'an assert still marked not yet safe stops it'

Make-GoodRepository
Remove-Item -LiteralPath (Join-Path $work 'tests/assert_limits_allowed.txt')
Expect-Refused 'check-release.ps1' @('-Version', $good, '-Repository', $work, '-WithoutGitHub') 'tests/assert_limits_allowed.txt is missing' 'taking the assert list away stops it'

Make-GoodRepository
Add-Content -LiteralPath (Join-Path $work 'PLAYTEST.md') -Value 'The mark key works. [PENDING: once it is in.]'
Expect-Refused 'check-release.ps1' @('-Version', $good, '-Repository', $work, '-WithoutGitHub') 'PLAYTEST.md still has a line marked [PENDING]' 'tester notes with a pending line stop it'

Make-GoodRepository
Remove-Item -LiteralPath (Join-Path $work 'PLAYTEST.md')
Expect-Refused 'check-release.ps1' @('-Version', $good, '-Repository', $work, '-WithoutGitHub') 'PLAYTEST.md is missing' 'taking the tester notes away stops it'

# The real assert list: how many rows it marks NOT SAFE YET, said, not failed. Such a row is an honest record of an assert that's in the
# code whether we write it down or not. Failing every pull request that records one would push people to leave it out, which is the
# opposite of writing things down for whoever comes later, and it would gain no security: check-release.ps1 refuses any release while
# one is listed. So a pull request can record one with a card, and nothing ships with one.
$realUnsafe = @(Get-Content (Join-Path $repository 'tests/assert_limits_allowed.txt') | Where-Object { -not $_.StartsWith('#') -and $_.Contains('| NOT SAFE YET') })
Write-Host "  The real assert list marks $($realUnsafe.Count) row(s) NOT SAFE YET. A release is refused until that's 0."

$realBlockers = Get-Content (Join-Path $repository 'RELEASE-BLOCKERS.md')
Check ($realBlockers -contains "## What's in the way") "the real file has the heading the script looks for"

# --- Shadows & Shaders
foreach ($file in $shadowFiles)
{
    Make-GoodRepository
    Remove-Item -LiteralPath (Join-Path $work $file)
    Expect-Refused 'check-release.ps1' @('-Version', $good, '-Repository', $work, '-WithoutGitHub') "Shadows & Shaders isn't in this commit: $file is missing" "without $file it's refused"
}

Make-GoodRepository
Write-File (Join-Path $work 'win32/VS2026/AvP.vcxproj') '<Project></Project>'
Expect-Refused 'check-release.ps1' @('-Version', $good, '-Repository', $work, '-WithoutGitHub') "the game's project doesn't build it" 'code the project does not build is refused'

# --- Hudson, the team's private test bot, never ships. Each way its code can show is refused, for its own reason, and the mentions of the word that
# aren't its code are let through.
$hudsonCases = @(
    @{ File = '3dc/avp/hudson.cpp'; Text = "code`n"; Reason = 'Hudson is in this commit: a file named 3dc/avp/hudson.cpp'; What = 'a file named after Hudson is refused' },
    @{ File = 'tools/hudson/run.ps1'; Text = "code`n"; Reason = 'Hudson is in this commit: a file named tools/hudson/run.ps1'; What = 'and one in a folder of its own' },
    @{ File = '3dc/avp/game.cpp'; Text = "#include `"hudson.h`"`n"; Reason = 'Hudson is in this commit: 3dc/avp/game.cpp line 1 includes one of its files'; What = 'a line including one of its files is refused' },
    @{ File = '3dc/avp/game.cpp'; Text = "int a;`n`tHudson_PlayInput(status);`n"; Reason = 'Hudson is in this commit: 3dc/avp/game.cpp line 2 uses its code'; What = 'a call into its code is refused' },
    @{ File = '3dc/avp/pmove.cpp'; Text = "`tif (HudsonPlay_IsRunning()) { }`n"; Reason = 'Hudson is in this commit: 3dc/avp/pmove.cpp line 1 uses its code'; What = 'and one with a longer name, such as HudsonPlay_' },
    @{ File = 'tests/net_test.cpp'; Text = "`tHudsonTest_AddRawPart(0);`n"; Reason = 'Hudson is in this commit: tests/net_test.cpp line 1 uses its code'; What = 'and in a test' }
)

foreach ($case in $hudsonCases)
{
    Make-GoodRepository
    Write-File (Join-Path $work $case.File) $case.Text
    Expect-Refused 'check-no-hudson.ps1' @('-Repository', $work) $case.Reason $case.What
}

Make-GoodRepository
$projectWithHudson = (Get-Content -Raw (Join-Path $work 'win32/VS2026/AvP.vcxproj')) -replace '</Project>', "`n<ClCompile Include=`"..\..\3dc\avp\hudson.cpp`" />`n</Project>"
Write-File (Join-Path $work 'win32/VS2026/AvP.vcxproj') $projectWithHudson
Expect-Refused 'check-no-hudson.ps1' @('-Repository', $work) 'Hudson is in this commit: win32/VS2026/AvP.vcxproj line 2, in a project, names one of its files' 'a project line naming one of its files is refused, even with the file gone'

# The two mentions main has always had: a comment about a profile called HUDSON, and a test's player called Hudson. Neither is Hudson's code.
Make-GoodRepository
Write-File (Join-Path $work '3dc/d3d_render.cpp') "// switched on, so without the test for menus, a profile called HUDSON would press H and turn the shadows off.`n"
Write-File (Join-Path $work 'tests/net_start_test.cpp') "static char playerName[] = `"Hudson`";`n"
$result = Run-Script 'check-no-hudson.ps1' @('-Repository', $work)
Check ($result.Code -eq 0) "the word Hudson in a comment, or as a test's player, is let through. It said: $($result.Text)"

# Only the check and this test may name what it looks for: the same words anywhere else are refused
Make-GoodRepository
Write-File (Join-Path $work 'tools/check-other.ps1') "if (HudsonPlay_IsRunning) { }`n"
Expect-Refused 'check-no-hudson.ps1' @('-Repository', $work) 'Hudson is in this commit: tools/check-other.ps1 line 1 uses its code' 'leaving out the check itself leaves out nothing else'

# The real repository, as it is now: only what git tracks, so that nothing built beside it counts. If this is ever refused, Hudson has come
# into the repository, or the check has started to catch something that isn't Hudson.
Clear-Work

foreach ($tracked in @(& git -C $repository ls-files))
{
    $from = Join-Path $repository $tracked

    if (Test-Path -LiteralPath $from -PathType Leaf)
    {
        $to = Join-Path $work $tracked
        New-Item -ItemType Directory -Force (Split-Path -Parent $to) | Out-Null
        Copy-Item -LiteralPath $from -Destination $to -Force
    }
}

$result = Run-Script 'check-no-hudson.ps1' @('-Repository', $work)
Check ($result.Code -eq 0) "the real repository has none of Hudson in it. It said: $($result.Text)"

# --- The checks that ask GitHub. There's no network here, so all that can be tried is that it refuses to go on without being on avpx_main.
Make-GoodRepository
$savedRef = $env:GITHUB_REF
$env:GITHUB_REF = 'refs/heads/some-other-branch'
$result = Run-Script 'check-release.ps1' @('-Version', $good, '-Repository', $work)
$env:GITHUB_REF = $savedRef
Check ($result.Code -ne 0 -and $result.Text.Contains('REFUSED: a release can only be started from avpx_main')) "another branch is refused. It said: $($result.Text)"

# --- The package
$commit = 'abcdef0123456789abcdef0123456789abcdef01'
$package = Join-Path $work 'package'
$packageRepository = Join-Path $work 'repo'

function Write-Sums
{
    $folder = (Resolve-Path -LiteralPath $package).Path
    $sums = @(Get-ChildItem -LiteralPath $package -Recurse -File | Where-Object { $_.Name -ne 'SHA256SUMS.txt' } | ForEach-Object {
        '{0}  {1}' -f (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLower(), ($_.FullName.Substring($folder.Length + 1) -replace '\\', '/')
    })
    [System.IO.File]::WriteAllLines((Join-Path $package 'SHA256SUMS.txt'), $sums)
}

function Make-GoodPackage
{
    Clear-Work
    foreach ($name in 'bin/AvP_Classic.exe', 'bin/D3DCompiler_43.dll', 'bin/d3dx9_43.dll', 'License.txt', 'PLAYTEST.md') { Write-File (Join-Path $package $name) $name }
    Write-File (Join-Path $package 'BUILD.txt') "avpx_main $commit"
    Write-File (Join-Path $package 'shaders/reliefMain.psh') 'shader one'
    Write-File (Join-Path $package 'shaders/volume.vsh') 'shader two'
    Write-File (Join-Path $packageRepository 'shaders/reliefMain.psh') 'shader one'
    Write-File (Join-Path $packageRepository 'shaders/volume.vsh') 'shader two'
    # One of our game files, in a folder, as the game folder wants it, with the commit's list naming it
    Write-File (Join-Path $package 'avp_huds/Ours.rif') 'a model'
    $modelHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $package 'avp_huds/Ours.rif')).Hash.ToLower()
    Write-File (Join-Path $packageRepository 'package-files/SHA256SUMS.txt') "$modelHash  avp_huds/Ours.rif"
    Write-File (Join-Path $packageRepository 'package-files/avp_huds/Ours.rif') 'a model'
    Write-Sums
}

function Package-Arguments { return @('-Package', $package, '-Commit', $commit, '-Repository', $packageRepository) }

Make-GoodPackage
$result = Run-Script 'check-release-package.ps1' (Package-Arguments)
Check ($result.Code -eq 0) "a good package passes. It said: $($result.Text)"

foreach ($name in 'bin/AvP_Classic.exe', 'bin/D3DCompiler_43.dll', 'bin/d3dx9_43.dll', 'License.txt', 'PLAYTEST.md', 'BUILD.txt')
{
    Make-GoodPackage
    Remove-Item -LiteralPath (Join-Path $package $name)
    Write-Sums
    Expect-Refused 'check-release-package.ps1' (Package-Arguments) "$name is not in the package" "a package without $name is refused"
}

Make-GoodPackage
Remove-Item -LiteralPath (Join-Path $package 'SHA256SUMS.txt')
Expect-Refused 'check-release-package.ps1' (Package-Arguments) 'SHA256SUMS.txt is not in the package' 'a package with no list of hashes is refused'

Make-GoodPackage
Remove-Item -LiteralPath (Join-Path $package 'shaders/reliefMain.psh')
Write-Sums
Expect-Refused 'check-release-package.ps1' (Package-Arguments) 'the shader reliefMain.psh is not in the package' 'a missing shader is refused'

Make-GoodPackage
Write-File (Join-Path $package 'shaders/reliefMain.psh') 'some other shader'
Write-Sums
Expect-Refused 'check-release-package.ps1' (Package-Arguments) "the shader reliefMain.psh in the package isn't the one in this commit" 'a different shader is refused'

Make-GoodPackage
Remove-Item -Recurse -LiteralPath (Join-Path $packageRepository 'shaders')
Expect-Refused 'check-release-package.ps1' (Package-Arguments) 'this commit has no shaders' 'no shaders to compare with is refused'

Make-GoodPackage
Remove-Item -LiteralPath (Join-Path $package 'avp_huds/Ours.rif')
Write-Sums
Expect-Refused 'check-release-package.ps1' (Package-Arguments) 'the game file avp_huds/Ours.rif is not in the package' 'a missing game file is refused'

Make-GoodPackage
Write-File (Join-Path $package 'avp_huds/Ours.rif') 'some other model'
Write-Sums
Expect-Refused 'check-release-package.ps1' (Package-Arguments) "the game file avp_huds/Ours.rif in the package isn't the one this commit lists" 'a different game file is refused'

Make-GoodPackage
Remove-Item -LiteralPath (Join-Path $packageRepository 'package-files/SHA256SUMS.txt')
Expect-Refused 'check-release-package.ps1' (Package-Arguments) 'this commit lists none of our game files' 'a commit with no list of game files is refused'

Make-GoodPackage
Write-File (Join-Path $packageRepository 'package-files/avp_huds/Ours.rif') 'changed in the commit, not on the list'
Expect-Refused 'check-release-package.ps1' (Package-Arguments) "package-files/avp_huds/Ours.rif in this commit isn't the file its list names" 'a game file that has drifted from its list is refused'

Make-GoodPackage
Write-File (Join-Path $packageRepository 'package-files/avp_huds/Extra.rif') 'in the folder, not on the list'
Expect-Refused 'check-release-package.ps1' (Package-Arguments) "package-files/avp_huds/Extra.rif isn't on package-files/SHA256SUMS.txt" 'a game file left off the list is refused'

Make-GoodPackage
New-Item -ItemType SymbolicLink -Path (Join-Path $packageRepository 'package-files/avp_huds/Linked.rif') -Target (Join-Path $packageRepository 'package-files/avp_huds/Ours.rif') | Out-Null
Expect-Refused 'check-release-package.ps1' (Package-Arguments) 'package-files/avp_huds/Linked.rif is a link' 'a link among the game files is refused'

foreach ($badLine in @(('0' * 64) + '  ../outside.rif', ('0' * 64) + '  C:/elsewhere.rif', 'not-a-hash  avp_huds/Ours.rif', ('0' * 64) + '  avp_huds/a[1].rif', ('0' * 64) + '  /root.rif'))
{
    Make-GoodPackage
    Write-File (Join-Path $packageRepository 'package-files/SHA256SUMS.txt') $badLine
    Expect-Refused 'check-release-package.ps1' (Package-Arguments) "package-files/SHA256SUMS.txt has a line that isn't a hash and a file in the game folder" "a list line naming somewhere else is refused: $badLine"
}

Make-GoodPackage
Write-File (Join-Path $package 'BUILD.txt') 'avpx_main 1111111111111111111111111111111111111111'
Write-Sums
Expect-Refused 'check-release-package.ps1' (Package-Arguments) "BUILD.txt doesn't say this package came from" 'a package from another commit is refused'

Make-GoodPackage
Write-File (Join-Path $package 'other.exe') 'a program'
Write-Sums
Expect-Refused 'check-release-package.ps1' (Package-Arguments) 'other.exe is in the package' 'a second program is refused'

Make-GoodPackage
Write-File (Join-Path $package 'AvP_Classic.exe') 'the game, but not in bin'
Write-Sums
Expect-Refused 'check-release-package.ps1' (Package-Arguments) 'AvP_Classic.exe is in the package, and the only program in it should be the game, bin/AvP_Classic.exe' 'the game anywhere but bin is refused'

foreach ($name in 'AvPx.pdb', 'AvPx.map')
{
    Make-GoodPackage
    Write-File (Join-Path $package $name) 'the build''s symbols'
    Write-Sums
    Expect-Refused 'check-release-package.ps1' (Package-Arguments) "$name is in the package, and a build's symbols stay with the build" "a package with $name is refused"
}

Make-GoodPackage
Write-File (Join-Path $package 'extra.txt') 'not on the list'
Expect-Refused 'check-release-package.ps1' (Package-Arguments) 'extra.txt is in the package, but not on its list of hashes' 'a file not on the list is refused'

Make-GoodPackage
Write-File (Join-Path $package 'License.txt') 'changed after the list was made'
Expect-Refused 'check-release-package.ps1' (Package-Arguments) "License.txt doesn't match its hash" 'a file that does not match the list is refused'

Make-GoodPackage
Add-Content -LiteralPath (Join-Path $package 'SHA256SUMS.txt') -Value ('0' * 64 + '  gone.txt')
Expect-Refused 'check-release-package.ps1' (Package-Arguments) "names a file that isn't in the package: gone.txt" 'a line for a file that is not there is refused'

# --- Not told where the repository is, as release.yml doesn't tell them. Everything above says where, so this is the only thing that tries it. What it answers
# depends on what RELEASE-BLOCKERS.md says today, but it has to be one of the script's own answers, and not a failure to start.
$result = Run-Script 'check-release.ps1' @('-Version', $good, '-WithoutGitHub')
Check ($result.Text.Contains('RELEASE-BLOCKERS.md still lists') -or $result.Text.Contains('Everything that can be checked')) "check-release.ps1 finds the repository by itself. It said: $($result.Text)"

# What it answers depends on what's been built beside the code, but it has to be one of the script's own answers.
$result = Run-Script 'check-no-hudson.ps1' @()
Check ($result.Text.Contains('None of Hudson is in this commit') -or $result.Text.Contains('Hudson is in this commit:')) "check-no-hudson.ps1 finds the repository by itself. It said: $($result.Text)"

Make-GoodPackage
Remove-Item -LiteralPath (Join-Path $package 'License.txt')
$result = Run-Script 'check-release-package.ps1' @('-Package', $package, '-Commit', $commit)
Check ($result.Text.Contains('REFUSED: License.txt is not in the package')) "check-release-package.ps1 starts without being told either. It said: $($result.Text)"
Make-GoodRepository
Check ((Run-AsAStep 'check-release.ps1' @('-Version', $good, '-Repository', $work, '-WithoutGitHub')) -eq 0) 'as a step, check-release.ps1 passes when it should'
Write-Blockers "## What's in the way`n`n- Something.`n"
Check ((Run-AsAStep 'check-release.ps1' @('-Version', $good, '-Repository', $work, '-WithoutGitHub')) -eq 1) 'and it fails when it should'

Make-GoodPackage
Check ((Run-AsAStep 'check-release-package.ps1' @('-Package', $package, '-Commit', $commit, '-Repository', $packageRepository)) -eq 0) 'as a step, check-release-package.ps1 passes when it should'
Remove-Item -LiteralPath (Join-Path $package 'D3DCompiler_43.dll')
Write-Sums
Check ((Run-AsAStep 'check-release-package.ps1' @('-Package', $package, '-Commit', $commit, '-Repository', $packageRepository)) -eq 1) 'and it fails when it should'

Clear-Work
Check ((Run-AsAStep 'check-no-hudson.ps1' @('-Repository', $work)) -eq 0) 'as a step, check-no-hudson.ps1 passes when it should'
Write-File (Join-Path $work '3dc/hudson.cpp') 'code'
Check ((Run-AsAStep 'check-no-hudson.ps1' @('-Repository', $work)) -eq 1) 'and it fails when it should'

Remove-Item -Force -LiteralPath $stepFile

foreach ($name in 'tools/check-release.ps1', 'tools/check-no-hudson.ps1', 'tools/check-release-package.ps1', 'tools/check-new-functions.ps1', 'tests/release-test.ps1')
{
    $lastWords = @(Get-Content (Join-Path $repository $name) | Where-Object { $_.Trim() -ne '' }) | Select-Object -Last 1
    Check ($lastWords -ceq 'exit 0') "$name ends by saying exit 0, so that nothing it ran earlier can decide"
}

# --- The workflow, read as text
$workflow = (Get-Content -Raw (Join-Path $repository '.github/workflows/release.yml')) -replace "`r", ''

function Check-Order([string]$text, [string]$first, [string]$second, [string]$what)
{
    $a = $text.IndexOf($first, [System.StringComparison]::Ordinal)
    $b = $text.IndexOf($second, [System.StringComparison]::Ordinal)
    Check ($a -ge 0 -and $b -ge 0 -and $a -lt $b) $what
}

$withoutComments = (($workflow -split "`n") | Where-Object { $_ -notmatch '^\s*#' }) -join "`n"
$jobs = [regex]::Split($withoutComments, '(?m)^  (?=(?:check|build|publish):\n)')

Check ($jobs.Count -eq 4) 'the workflow has the three jobs, and no others'
$top = $jobs[0]; $checkJob = $jobs[1]; $buildJob = $jobs[2]; $publishJob = $jobs[3]

Check ($top -match '(?m)^on:\n  workflow_dispatch:\n') 'it starts by hand'
Check ($top -notmatch '(?m)^\s+(push|pull_request|pull_request_target|schedule|release|workflow_run|repository_dispatch):') 'and in no other way, because a tag could point at a commit with the checks taken out'
Check ($top -match '(?m)^permissions: \{\}$') 'by default no job can do anything'

Check ($checkJob -match 'run: \./tools/check-release\.ps1 -Version \$env:VERSION\n') 'the first job runs the checks'
Check ($checkJob -match 'run: \./tools/check-no-hudson\.ps1\n') 'and refuses any of Hudson'
Check ($checkJob -match 'run: \./tools/check-new-functions\.ps1\n') 'and refuses a new function the game does not use'
Check ($checkJob -match '(?m)^          fetch-depth: 0\b') 'with the history it needs to compare'
Check (-not $withoutComments.Contains('-WithoutGitHub')) 'and never leaves out the ones that ask GitHub'
Check ($buildJob -match '(?m)^    needs: check$') 'the build needs the checks to have passed'
Check ($publishJob -match '(?m)^    needs: \[check, build\](\s+#[^\n]*)?$') 'and the draft needs the build, so it comes after the checks too, and the checks for its tag'
Check-Order $buildJob 'tools/build-and-package.ps1' 'tools/check-release-package.ps1' 'the package is checked after it is made'
Check-Order $buildJob 'tools/check-release-package.ps1' 'actions/upload-artifact' 'and before it goes anywhere'
Check ($publishJob -match '(?m)^    environment: release\s') 'the draft waits for the owner'

Check-Order $publishJob 'Refuse if anything has changed' 'attest-build-provenance' 'what has changed since the checks is looked at first'
Check-Order $publishJob 'attest-build-provenance' 'gh release create' 'and how it was built is recorded before the draft is made'
Check ($publishJob.Contains('--jq .sha') -and $publishJob.Contains('$tip -cne $env:GITHUB_SHA')) 'it is still the tip when the owner approves'
Check ($publishJob.Contains('--draft') -and $publishJob.Contains('--target $env:GITHUB_SHA')) 'it makes a draft, on the commit that was checked'
Check (-not ($withoutComments -match '--draft=false|gh release edit|git push|git tag|/git/refs')) 'nothing here publishes it, or makes a tag'
Check ($publishJob.Contains('-not $made.isDraft')) 'and it checks that what it made is a draft'
Check (-not $publishJob.Contains('actions/checkout') -and -not $publishJob.Contains('./tools/')) 'the job that can write has no copy of the code'

# A beta: its tag comes from the checks, is looked at again before it's used, and makes a pre-release that's still a draft
Check ($checkJob -match '(?m)^        id: refuse$' -and $checkJob.Contains('tag: ${{ steps.refuse.outputs.tag }}') -and $checkJob.Contains('prerelease: ${{ steps.refuse.outputs.prerelease }}')) 'the checks hand on the tag and whether it is a pre-release'
Check ($checkScript.Contains('$tag = if ($isBeta) { $Version } else { "v$Version" }')) "a beta's tag is its name, and a version's is v and its numbers"
Check ($checkScript.Contains('[datetime]::TryParseExact(') -and $checkScript.Contains("'yyyyMMdd'")) "a beta's day has to be one the calendar has"
Check ($publishJob.Contains('TAG: ${{ needs.check.outputs.tag }}') -and -not $publishJob.Contains('$tag = "v$env:VERSION"')) 'the draft uses the tag the checks made, through the environment'
Check ($publishJob.Contains("if (`$env:TAG -cne `$(if (`$isBeta) { `$env:VERSION } else { `"v`$env:VERSION`" })) { throw")) 'and looks at it again before anything uses it'
Check ($publishJob.Contains("if (`$preRelease) { `$alsoSay += '--prerelease' }") -and $publishJob.Contains('@alsoSay')) 'only a beta is made a pre-release'
Check ($publishJob.Contains('if ($made.isPrerelease -ne $preRelease) { throw')) 'and the draft is checked for it'
Check ($publishJob.Contains('if ($releases -ccontains $env:TAG) { throw')) 'a tag already used, even by a draft, is refused'

Check ($publishJob -match '(?m)^      contents: write\s') 'the last job can write'
Check (-not $checkJob.Contains('write') -and -not $buildJob.Contains(': write')) 'and the other two can not'

foreach ($other in Get-ChildItem (Join-Path $repository '.github/workflows') -Filter '*.yml' | Where-Object { $_.Name -ne 'release.yml' })
{
    Check (-not (Get-Content -Raw $other.FullName).Contains('contents: write')) "$($other.Name) can't write to the repository"
}

foreach ($match in [regex]::Matches($withoutComments, '(?m)^\s*(?:- )?uses:\s*(\S+)'))
{
    Check ($match.Groups[1].Value -cmatch '^[a-z-]+/[a-z-]+@[0-9a-f]{40}$') "$($match.Groups[1].Value) is pinned to a full commit hash"
}

Check ($withoutComments.Split("`n").Where({ $_ -match 'actions/checkout@' }).Count -eq 2) 'two jobs check out the code'
Check ([regex]::Matches($withoutComments, 'persist-credentials: false').Count -eq 2) 'and neither leaves its token behind for the build to find'

$inputUses = @([regex]::Matches($withoutComments, '.*\$\{\{\s*inputs\.[^\n]*') | ForEach-Object { $_.Value.Trim() })
Check ($inputUses.Count -ge 1 -and @($inputUses | Where-Object { $_ -cne 'VERSION: ${{ inputs.version }}' }).Count -eq 0) 'what a person typed only ever reaches a command as a setting, so it can never be run as part of one'
Check (-not $withoutComments.Contains('secrets.')) 'it uses no secret but the job''s own token'

# --- The list of required checks is the same as the test build's
$buildWorkflow = Get-Content -Raw (Join-Path $repository '.github/workflows/build.yml')
$listInBuild = [regex]::Match($buildWorkflow, '\$required = (.*)').Groups[1].Value.Trim()
$listInCheck = [regex]::Match($checkScript, '\$requiredChecks = (.*)').Groups[1].Value.Trim()
Check ($listInBuild.Length -gt 0 -and $listInBuild -ceq $listInCheck) 'a release asks for the same checks as a test build'

# --- Every pull request refuses any of Hudson as well, in a check that's already required, so a hook left behind fails the pull request that brings it
$testsWorkflow = (Get-Content -Raw (Join-Path $repository '.github/workflows/tests.yml')) -replace "`r", ''
Check ($testsWorkflow -match '(?m)^  pull_request:\n') 'the tests run on every pull request'
Check-Order $testsWorkflow 'name: Unit tests' './tools/check-no-hudson.ps1' 'and the required Unit tests check refuses any of Hudson'
Check-Order $testsWorkflow './tools/check-no-hudson.ps1' './tests/build.ps1' 'before it builds anything'

# --- Pull requests try the release's own checks on the machine the release runs them on
$ubuntuJob = [regex]::Match($testsWorkflow, '(?ms)^  release-checks-on-ubuntu:\n.*\z').Value
$releaseMachine = [regex]::Match($checkJob, '(?m)^    runs-on: (\S+)').Groups[1].Value
$ubuntuMachine = [regex]::Match($ubuntuJob, '(?m)^    runs-on: (\S+)').Groups[1].Value
$ubuntuName = [regex]::Match($ubuntuJob, '(?m)^    name: (.+)$').Groups[1].Value.Trim()
Check ($releaseMachine.Length -gt 0 -and $ubuntuMachine -ceq $releaseMachine) 'pull requests try the checks on the machine the release uses'
Check ($ubuntuJob -match 'shell: pwsh\n\s+run: \./tools/check-no-hudson\.ps1\s') 'and refuse any of Hudson there, with the same PowerShell'
Check ($ubuntuJob -match 'shell: pwsh\n\s+run: \./tests/release-test\.ps1 -OnlyNewFunctions\s') 'and try the new-function rows there'
Check ($ubuntuJob -match '(?m)^          fetch-depth: 0\b') 'with the history, so the real code gets read as well'
Check ($ubuntuJob.Contains('persist-credentials: false')) 'leaving no token behind'
Check ($ubuntuName.Length -gt 0 -and $listInCheck.Contains("'$ubuntuName'")) 'and a release asks for that check like the others'

Clear-Work
Remove-Item -Recurse -Force -LiteralPath $work

Write-Host "$script:passes passed, $script:failures failed"

if ($script:failures -gt 0)
{
    exit 1
}

# Said outright: the last script this ran was meant to be refused, and GitHub ends a step by exiting with what the last program left behind.
exit 0
