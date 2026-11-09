<#
Tests the release checks, with no network and nothing built. The Unit tests check runs this.

Two parts:
- tools/check-release.ps1, tools/check-hudson-asleep.ps1, tools/check-release-package.ps1 and tools/check-new-functions.ps1 are run for real, on made-up folders with something wrong in each,
  and tools/check-virustotal.ps1 on made-up answers from VirusTotal. Each one
  has to refuse for the reason it's meant to, not just refuse: a later check could otherwise hide a missing one.
- .github/workflows/release.yml can't run here, so it's read, to prove each check is there and comes before anything is made.

What this can't prove: that GitHub will do what the workflow asks, or that VirusTotal answers the way the made-up answers here are written
(from its API's documentation). The first real run is what proves those.

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
# A checkout without the history, as the Windows Unit tests job has, can only say the older commit isn't there. The job
# release-checks-on-ubuntu fetches the history and runs these rows with -OnlyNewFunctions, where that answer fails.
$why = if ($hasNoHistory) { ' (this checkout has no history to compare with: release-checks-on-ubuntu reads it with the history)' } else { '' }
Write-Host "On this commit's own code, it says: $($result.Text)$why"

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
    Write-File (Join-Path $work 'tests/assert_limits_allowed.txt') "# Rows marked NOT SAFE YET wait for a fix. This header says so, and doesn't count.`n   # | NOT SAFE YET, in a note with spaces in front, doesn't count either`n3dc/a.cpp | assert(i < kMost); | the number is checked before it gets here`n"
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

function Expect-VersionRefused([string]$version, [string]$reason, [string]$what)
{
    Expect-Refused 'check-release.ps1' @('-Version', $version, '-Repository', $work, '-WithoutGitHub') $reason "the version [$version] is refused $what"
}

foreach ($bad in '1', '1.2', '1.2.3.4', '1.0.0-rc1', ' 1.0.0', '1.0.0 ', "1.0.0`n", '1.0.0;x', '1..0', 'a.b.c', '../1.0.0', $otherAlphabet)
{
    Expect-VersionRefused $bad 'the version has to be three plain numbers' 'for its shape'
}

foreach ($withAV in 'v1.0.0', 'V1.0.0')
{
    Expect-VersionRefused $withAV 'written without a v in front' 'because the tag adds the v'
}

foreach ($zeroInFront in '01.0.0', '1.00.0', '1.0.01', '00.0.0')
{
    Expect-VersionRefused $zeroInFront 'a number in the version starts with 0' 'because 01 would be a second way of writing 1'
}

Expect-VersionRefused $tooManyDigits 'a number in the version is too long' 'for its length'

foreach ($fine in '1.0.0', '0.0.1', '0.0.0', '10.20.30', '123456.654321.100000')
{
    Check ((Check-Release $fine).Code -eq 0) "the version $fine is taken"
}

# --- A beta: the version it leads up to, then -beta. and its number. Each wrong way of writing one is refused, and each refusal says which
# rule it broke.
foreach ($bad in '1.0.0-Beta.0', '1.0.0-BETA.0', '1.0.0-beta0', '1.0.0.beta.0', '1.0.0-beta.0x', '1.0.0-beta.0 ', "1.0.0-beta.0`n", '1.0.0-beta.0;x',
    '1.0.0-beta.1.2', '1.0.0-beta.-1', '1.0.0-beta.+1', '1.0.0-beta', '1.0.0-', '-beta.0', '1.0-beta.0', '1.0.0.0-beta.0', ' 1.0.0-beta.0',
    '../1.0.0-beta.0', '1.0.0-alpha.0', ('1.0.0-beta.' + [char]0x0661))
{
    Expect-VersionRefused $bad 'the version has to be three plain numbers' 'for its shape'
}

Expect-VersionRefused 'v1.0.0-beta.0' 'written without a v in front' 'because the tag adds the v'
Expect-VersionRefused '1.0.0-beta.' "the beta's number is missing after -beta." 'for the missing number'

foreach ($zeroInFront in '1.0.0-beta.01', '1.0.0-beta.00', '01.0.0-beta.0')
{
    Expect-VersionRefused $zeroInFront 'a number in the version starts with 0' 'because it would be a second way of writing the same beta'
}

foreach ($tooLong in '1.0.0-beta.10000', '1234567.0.0-beta.0')
{
    Expect-VersionRefused $tooLong 'a number in the version is too long' 'for its length'
}

# The old way of naming a beta, betarelease and its day, isn't taken any more, however it's written
foreach ($oldName in 'betarelease20261107', 'BetaRelease20261107', 'betarelease20261332', 'betarelease')
{
    Expect-VersionRefused $oldName 'the old way of naming a beta' 'as the old way of naming a beta'
}

foreach ($fine in '1.0.0-beta.0', '1.0.0-beta.1', '1.0.0-beta.10', '1.0.0-beta.9999', '0.0.1-beta.0', '123456.654321.0-beta.7')
{
    Check ((Check-Release $fine).Code -eq 0) "the beta $fine is taken"
}

# --- A beta's number is the next one. -ExistingTags stands in for the tags and releases GitHub would list: see check-release.ps1's top.
function Check-Beta([string]$version, [string]$tags) { return Run-Script 'check-release.ps1' @('-Version', $version, '-Repository', $work, '-WithoutGitHub', '-ExistingTags', $tags) }

function Expect-BetaRefused([string]$version, [string]$tags, [string]$reason, [string]$what)
{
    Expect-Refused 'check-release.ps1' @('-Version', $version, '-Repository', $work, '-WithoutGitHub', '-ExistingTags', $tags) $reason "$version, with $tags there already, is refused $what"
}

$result = Check-Beta '1.0.0-beta.0' 'betarelease20261107 v0.9.0 v0.9.0-beta.4 v1.0.1-beta.2 v10.0.0-beta.7'
Check ($result.Code -eq 0) "a version's first beta is 0: the old beta name and other versions' betas don't count. It said: $($result.Text)"
$result = Check-Beta '1.0.0-beta.1' 'refs/tags/v1.0.0-beta.0'
Check ($result.Code -eq 0) "the next is one more, read from a tag as GitHub writes one. It said: $($result.Text)"
$result = Check-Beta '1.0.0-beta.11' 'v1.0.0-beta.9 v1.0.0-beta.10'
Check ($result.Code -eq 0) "betas are counted as numbers, so beta.10 comes after beta.9. It said: $($result.Text)"
$result = Check-Beta '1.0.0' 'v1.0.0-beta.0 v1.0.0-beta.1'
Check ($result.Code -eq 0) "a version can be released after its betas. It said: $($result.Text)"

Expect-BetaRefused '1.0.0-beta.1' 'v1.0.0-beta.0 v1.0.0-beta.1' 'v1.0.0-beta.1 has already been used' 'because it has been used'
Expect-BetaRefused '1.0.0-beta.0' 'refs/tags/v1.0.0-beta.0' 'v1.0.0-beta.0 has already been used' 'when the tag that used it is written as GitHub writes one'
Expect-BetaRefused '1.0.0-beta.1' 'betarelease20261107 v1.0.1-beta.0' 'has no beta yet, so its first is 1.0.0-beta.0' 'because a first beta that is not 0 skips one'
Expect-BetaRefused '1.0.0-beta.3' 'v1.0.0-beta.0 v1.0.0-beta.1' 'the next is 1.0.0-beta.2' 'because it skips a number'
Expect-BetaRefused '1.0.0-beta.10' 'v1.0.0-beta.10 v1.0.0-beta.9' 'v1.0.0-beta.10 has already been used' 'whichever order the list is in'
Expect-BetaRefused '1.0.0-beta.0' 'v1.0.0-beta.1' 'never goes back' 'because a number below the highest goes back'
Expect-BetaRefused '1.0.0-beta.9998' 'v1.0.0-beta.9999' 'was the last beta 1.0.0 can have' 'because a version has no more than 10,000 betas'
Expect-BetaRefused '1.0.0' 'v1.0.0-beta.0 v1.0.0' 'v1.0.0 has already been used' 'because the version has been released'
Expect-BetaRefused '1.0.0-beta.1' 'v1.0.0-beta.0 v1.0.0' 'v1.0.0 is already released, so a beta of it would come after it' 'because a beta never comes after its version'
Expect-BetaRefused '1.0.0-beta.0' 'refs/tags/v1.0.0' 'v1.0.0 is already released' 'even when the version had no betas'
$result = Check-Beta '1.0.1-beta.0' 'v1.0.0-beta.0 v1.0.0'
Check ($result.Code -eq 0) "and the next version's betas start again at 0. It said: $($result.Text)"

# A tag that starts like a beta of this version but isn't written as one can't be counted, so it isn't guessed at
foreach ($stray in 'v1.0.0-beta.01', 'v1.0.0-beta.x', 'v1.0.0-beta.1.1', 'v1.0.0-beta.', 'v1.0.0-beta.10000')
{
    Expect-BetaRefused '1.0.0-beta.1' "v1.0.0-beta.0 $stray" "$stray is already there, and it isn't written the way a beta is" 'because a tag it cannot count is there'
}

# Only tests may hand in a list, and only with -WithoutGitHub: a real release counts what GitHub has
Expect-Refused 'check-release.ps1' @('-Version', '1.0.0-beta.0', '-Repository', $work, '-ExistingTags', 'v0.9.0') '-ExistingTags is only for tests/release-test.ps1' 'a list of tags without -WithoutGitHub is refused'

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
$realUnsafe = @(Get-Content (Join-Path $repository 'tests/assert_limits_allowed.txt') | Where-Object { -not $_.TrimStart().StartsWith('#') -and $_.Contains('| NOT SAFE YET') })
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

# --- Hudson, the team's test bot, has been in the game since 2026-11-09 (the owner: "Hudson goes into avpx_main"), and a player's game must
# never run any of it. check-hudson-asleep.ps1 proves that. Each way it can be broken is refused, for its own reason, and each way Hudson's
# code is written to stay asleep is let through. The smallest Hudson that passes: a run turned on only by both words and the folder check,
# and one way in that starts asleep. The same cases were tried by hand with AvP-docs' hudson_asleep_cases.js on 2026-11-09: 28 right.
$hudsonGood = @{
    '3dc/avp/hudson.cpp' = (@(
        'bool HudsonRunIsOn = false;',
        '',
        'void Hudson_ReadCommandLine(const char *commandLine)',
        '{',
        "`tif (!HudsonPlan_IsAskedFor(commandLine))",
        "`t{",
        "`t`treturn;",
        "`t}",
        '',
        "`tif (!HudsonPlan_FolderIsAllowed(commandLine))",
        "`t{",
        "`t`tTerminateProcess(0, 1);",
        "`t}",
        '',
        "`tHudsonRunIsOn = true;",
        '}',
        '',
        'void Hudson_EndOfFrame()',
        '{',
        "`tif (!HudsonRunIsOn)",
        "`t{",
        "`t`treturn;",
        "`t}",
        '',
        "`tDoIt();",
        '}',
        '') -join "`n");
    '3dc/avp/hudson_plan.cpp' = (@(
        'bool HudsonPlan_IsAskedFor(const char *commandLine)',
        '{',
        "`treturn CommandLineHasWord(commandLine, `"-dev`") && CommandLineHasWord(commandLine, `"-hudson`");",
        '}',
        '') -join "`n");
    '3dc/avp/game.cpp' = (@(
        'void Start(const char *commandLine)',
        '{',
        "`tHudson_ReadCommandLine(commandLine);",
        '}',
        '',
        'void Frame()',
        '{',
        "`tHudson_EndOfFrame();",
        '}',
        '') -join "`n")
}

# A good repository for the release's checks, with the smallest Hudson in it, and then the case's own files over it
function Make-HudsonRepository([hashtable]$files)
{
    Make-GoodRepository
    $all = @{}
    foreach ($name in $hudsonGood.Keys) { $all[$name] = $hudsonGood[$name] }
    foreach ($name in $files.Keys) { $all[$name] = $files[$name] }
    foreach ($name in $all.Keys) { Write-File (Join-Path $work $name) $all[$name] }
}

# One more way in, defined in Hudson's file and named by the game
function With-WayIn([string]$definition, [string]$call)
{
    return @{
        '3dc/avp/hudson.cpp' = $hudsonGood['3dc/avp/hudson.cpp'] + "`n" + $definition + "`n";
        '3dc/avp/game.cpp' = $hudsonGood['3dc/avp/game.cpp'] + "`nvoid More()`n{`n`t$call`n}`n"
    }
}

function With-Changed([string]$name, [string]$from, [string]$to)
{
    return @{ $name = $hudsonGood[$name].Replace($from, $to) }
}

$gameMore = $hudsonGood['3dc/avp/game.cpp']
$hudsonMore = $hudsonGood['3dc/avp/hudson.cpp']
$letThrough = "A player's game can't run Hudson"
$notAsleep = "which doesn't start by doing nothing unless HudsonRunIsOn"

$hudsonCases = @(
    @{ What = 'the smallest Hudson that starts asleep is let through'; Files = @{}; Passes = "A player's game can't run Hudson: 2 of its names are used by the game, each one checked." },
    @{ What = 'a way in written "return HudsonRunIsOn && ...;" is let through'; Files = (With-WayIn "bool Hudson_IsOn()`n{`n`treturn HudsonRunIsOn && plan.on;`n}" 'if (Hudson_IsOn()) { }'); Passes = $letThrough },
    @{ What = 'and "return HudsonRunIsOn ? ... : ...;"'; Files = (With-WayIn "int Hudson_Place()`n{`n`treturn HudsonRunIsOn ? place : -1;`n}" 'x = Hudson_Place();'); Passes = $letThrough },
    @{ What = 'and "return (HudsonRunIsOn && ...) ? ... : ...;"'; Files = (With-WayIn "int Hudson_Row()`n{`n`treturn (HudsonRunIsOn && plan.asked) ? plan.row : -1;`n}" 'x = Hudson_Row();'); Passes = $letThrough },
    @{ What = 'and counters first, then "if (!HudsonRunIsOn || ...) return;"'; Files = (With-WayIn "void Hudson_SayIt(int a)`n{`n`tstatic int said = 0;`n`n`tif (!HudsonRunIsOn || said > 9)`n`t{`n`t`treturn;`n`t}`n`n`tsaid++;`n}" 'Hudson_SayIt(1);'); Passes = $letThrough },
    @{ What = 'and a whole body inside "if (HudsonRunIsOn && ...)"'; Files = (With-WayIn "void Hudson_Over()`n{`n`tif (HudsonRunIsOn && plan.net)`n`t{`n`t`tEndIt();`n`t}`n}" 'Hudson_Over();'); Passes = $letThrough },
    @{ What = 'a comment or a string naming Hudson in the game is let through'; Files = @{ '3dc/avp/game.cpp' = $gameMore + "`n// Hudson_Unknown is a name in a comment`nconst char *name = `"Hudson_Unknown`";`n" }; Passes = $letThrough },
    @{ What = 'setting one of its variables back to false is let through'; Files = @{ '3dc/avp/hudson.cpp' = $hudsonMore + "`nbool HudsonMarineIsOn = false;`n"; '3dc/avp/game.cpp' = $gameMore + "`nvoid Reset()`n{`n`tHudsonMarineIsOn = false;`n}`n" }; Passes = $letThrough },
    @{ What = 'a type of its that only holds values is let through'; Files = @{ '3dc/avp/hudson.h' = "struct HudsonCounts`n{`n`tint a;`n`tint b;`n};`n"; '3dc/avp/game.cpp' = $gameMore + "`nvoid Other()`n{`n`tHudsonCounts counts;`n}`n" }; Passes = $letThrough },
    @{ What = 'a default argument and values without a call are let through'; Files = @{ '3dc/avp/hudson_nav.cpp' = "static const int kFar = 3;`nstatic int places[2] = { 1, 2 };`n`nstatic void Steer(int x, int stopsShort = Far())`n{`n`tint y = Measure(x);`n}`n" }; Passes = $letThrough },
    @{ What = 'a way in that starts by doing something is refused'; Files = (With-WayIn "void Hudson_Go()`n{`n`tDoIt();`n}" 'Hudson_Go();'); Reason = "3dc/avp/game.cpp line 13 names Hudson_Go, $notAsleep (3dc/avp/hudson.cpp line 28)" },
    @{ What = 'and one whose check gives up only with something else too (&&)'; Files = (With-WayIn "void Hudson_Go()`n{`n`tif (!HudsonRunIsOn && plan.x)`n`t{`n`t`treturn;`n`t}`n`n`tDoIt();`n}" 'Hudson_Go();'); Reason = "3dc/avp/game.cpp line 13 names Hudson_Go, $notAsleep (3dc/avp/hudson.cpp line 28)" },
    @{ What = 'and "return HudsonRunIsOn && a || b;", which gives b without a run'; Files = (With-WayIn "bool Hudson_Is()`n{`n`treturn HudsonRunIsOn && a || b;`n}" 'if (Hudson_Is()) { }'); Reason = "3dc/avp/game.cpp line 13 names Hudson_Is, $notAsleep (3dc/avp/hudson.cpp line 28)" },
    @{ What = 'and a body inside "if (HudsonRunIsOn ...)" with more after it'; Files = (With-WayIn "void Hudson_Over()`n{`n`tif (HudsonRunIsOn)`n`t{`n`t`tEndIt();`n`t}`n`n`tDoIt();`n}" 'Hudson_Over();'); Reason = "3dc/avp/game.cpp line 13 names Hudson_Over, $notAsleep (3dc/avp/hudson.cpp line 28)" },
    @{ What = 'HudsonRunIsOn starting true is refused'; Files = (With-Changed '3dc/avp/hudson.cpp' 'bool HudsonRunIsOn = false;' 'bool HudsonRunIsOn = true;'); Reason = 'HudsonRunIsOn must be defined once, as bool HudsonRunIsOn = false; it is: 3dc/avp/hudson.cpp line 1: bool HudsonRunIsOn = true;' },
    @{ What = 'the game turning HudsonRunIsOn on is refused'; Files = @{ '3dc/avp/game.cpp' = $gameMore + "`nvoid Cheat()`n{`n`tHudsonRunIsOn = true;`n}`n" }; Reason = '3dc/avp/game.cpp line 13 changes HudsonRunIsOn: only Hudson_ReadCommandLine may, once, to true' },
    @{ What = 'a run asked for without the first check is refused'; Files = (With-Changed '3dc/avp/hudson.cpp' "`tif (!HudsonPlan_IsAskedFor(commandLine))`n`t{`n`t`treturn;`n`t}`n`n" ''); Reason = 'Hudson_ReadCommandLine (3dc/avp/hudson.cpp line 3) must start by giving up unless HudsonPlan_IsAskedFor(commandLine)' },
    @{ What = 'a run turned on before the folder is checked is refused'; Files = (With-Changed '3dc/avp/hudson.cpp' "`tif (!HudsonPlan_FolderIsAllowed(commandLine))`n`t{`n`t`tTerminateProcess(0, 1);`n`t}`n`n`tHudsonRunIsOn = true;" "`tHudsonRunIsOn = true;`n`n`tif (!HudsonPlan_FolderIsAllowed(commandLine))`n`t{`n`t`tTerminateProcess(0, 1);`n`t}"); Reason = "Hudson_ReadCommandLine (3dc/avp/hudson.cpp line 3) must check the folder, and end the game if it's one a run mustn't touch, before HudsonRunIsOn = true" },
    @{ What = 'a run asked for with -dev alone is refused'; Files = (With-Changed '3dc/avp/hudson_plan.cpp' ' && CommandLineHasWord(commandLine, "-hudson")' ''); Reason = 'HudsonPlan_IsAskedFor must be defined once, as: return CommandLineHasWord(commandLine, "-dev") && CommandLineHasWord(commandLine, "-hudson");' },
    @{ What = 'the game calling a HudsonTest_ function of its own is refused'; Files = @{ '3dc/avp/game.cpp' = $gameMore + "`nvoid HudsonTest_Send(int a)`n{`n`tSend(a);`n}`n`nvoid Other()`n{`n`tHudsonTest_Send(1);`n}`n" }; Reason = "3dc/avp/game.cpp line 18 calls HudsonTest_Send, which only Hudson's own files may call" },
    @{ What = 'the game turning on one of its variables is refused'; Files = @{ '3dc/avp/hudson.cpp' = $hudsonMore + "`nbool HudsonMarineIsOn = false;`n"; '3dc/avp/game.cpp' = $gameMore + "`nvoid Cheat()`n{`n`tHudsonMarineIsOn = true;`n}`n" }; Reason = "3dc/avp/game.cpp line 13 changes HudsonMarineIsOn: outside Hudson's files it may only be read, or set back to false" },
    @{ What = 'one of its variables starting on is refused'; Files = @{ '3dc/avp/hudson.cpp' = $hudsonMore + "`nbool HudsonMarineIsOn = true;`n"; '3dc/avp/game.cpp' = $gameMore + "`nvoid Look()`n{`n`tif (HudsonMarineIsOn) { }`n}`n" }; Reason = "HudsonMarineIsOn (3dc/avp/hudson.cpp line 28) starts as true: Hudson's variables must start off (false, 0, -1 or nothing)" },
    @{ What = 'a macro of its used by the game is refused'; Files = @{ '3dc/avp/hudson.h' = "#define HudsonDoIt() DoIt()`n"; '3dc/avp/game.cpp' = $gameMore + "`nvoid Other()`n{`n`tHudsonDoIt();`n}`n" }; Reason = "3dc/avp/game.cpp line 13 uses the macro HudsonDoIt: a macro of Hudson's can hide code, so the game mustn't use one" },
    @{ What = 'a type of its with code of its own used by the game is refused'; Files = @{ '3dc/avp/hudson.h' = "struct HudsonThing`n{`n`tint a;`n`tvoid Go();`n};`n"; '3dc/avp/game.cpp' = $gameMore + "`nvoid Other()`n{`n`tHudsonThing thing;`n}`n" }; Reason = "3dc/avp/game.cpp line 13 uses HudsonThing, a type of Hudson's with code of its own: the game may only use Hudson's types that hold nothing but values" },
    @{ What = 'a name the check can''t place is refused'; Files = @{ '3dc/avp/game.cpp' = $gameMore + "`nvoid Other()`n{`n`tHudsonSomething(1);`n}`n" }; Reason = "3dc/avp/game.cpp line 13 names HudsonSomething, and this check can't tell what it is, so it can't say a player's game won't run it" },
    @{ What = 'an object of its built before WinMain, whose making runs code, is refused'; Files = @{ '3dc/avp/hudson_nav.h' = "class HudsonMap`n{`npublic:`n`tHudsonMap();`n};`n"; '3dc/avp/hudson_nav.cpp' = "static HudsonMap theWholeMap;`n" }; Reason = "3dc/avp/hudson_nav.cpp line 1: theWholeMap, a HudsonMap, is built before the game starts, and making one runs code of Hudson's that nobody has read for this" },
    @{ What = 'a value kept by its file worked out by a call is refused'; Files = @{ '3dc/avp/hudson_nav.cpp' = "static double startedAt = SecondsNow();`n`nstatic void Steer(int x, int stopsShort = 0)`n{`n}`n" }; Reason = '3dc/avp/hudson_nav.cpp line 1: a value kept by the file is worked out by calling SecondsNow, before the game starts' }
)

foreach ($case in $hudsonCases)
{
    Make-HudsonRepository $case.Files

    if ($case.ContainsKey('Passes'))
    {
        $result = Run-Script 'check-hudson-asleep.ps1' @('-Repository', $work)
        Check ($result.Code -eq 0 -and $result.Text.Contains($case.Passes)) "$($case.What). It said: $($result.Text)"
    }
    else
    {
        Expect-Refused 'check-hudson-asleep.ps1' @('-Repository', $work) $case.Reason $case.What
    }
}

# No Hudson at all, as in a commit from before it came in, is let through
Make-GoodRepository
$result = Run-Script 'check-hudson-asleep.ps1' @('-Repository', $work)
Check ($result.Code -eq 0 -and $result.Text.Contains("None of Hudson is in this commit, so a player's game can't run it.")) "no Hudson at all is let through. It said: $($result.Text)"

# The real repository, as it is now: only what git tracks, so that nothing built beside it counts. If this is ever refused, a way into Hudson
# that a player's game could reach has come into the repository, or the check has started to refuse something that's asleep.
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

$result = Run-Script 'check-hudson-asleep.ps1' @('-Repository', $work)
Check ($result.Code -eq 0 -and $result.Text.Contains($letThrough)) "the real repository's Hudson stays asleep in a player's game. It said: $($result.Text)"

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
    # Its licences, with the list of which is whose
    Write-File (Join-Path $package 'licences/README.txt') "The libraries and their licences.`n`nbin/AvP_Classic.exe: the game, with ENet in it (licences/ENet.txt, MIT).`nbin/D3DCompiler_43.dll: Microsoft's shader compiler (licences/D3DX.txt).`nbin/d3dx9_43.dll: Microsoft's D3DX (licences/D3DX.txt).`n"
    Write-File (Join-Path $package 'licences/ENet.txt') 'the ENet licence'
    Write-File (Join-Path $package 'licences/D3DX.txt') 'the D3DX licence'
    Write-Sums
}

function Package-Arguments { return @('-Package', $package, '-Commit', $commit, '-Repository', $packageRepository) }

Make-GoodPackage
$result = Run-Script 'check-release-package.ps1' (Package-Arguments)
Check ($result.Code -eq 0) "a good package passes. It said: $($result.Text)"

# Each program and library in bin comes with its licence (LICENCE-NOTICES-1)
Make-GoodPackage
Remove-Item -LiteralPath (Join-Path $package 'licences/README.txt')
Write-Sums
Expect-Refused 'check-release-package.ps1' (Package-Arguments) 'licences/README.txt is not in the package' 'a package without its list of licences is refused'

Make-GoodPackage
Write-File (Join-Path $package 'bin/ogg.dll') 'a library'
Write-Sums
Expect-Refused 'check-release-package.ps1' (Package-Arguments) "bin/ogg.dll isn't named in licences/README.txt, so its licence wouldn't go with it" 'a library in bin with no licence named is refused'

Make-GoodPackage
Add-Content -LiteralPath (Join-Path $package 'licences/README.txt') -Value 'bin/AvP_Classic.exe: again (licences/ENet.txt).'
Write-Sums
Expect-Refused 'check-release-package.ps1' (Package-Arguments) 'bin/AvP_Classic.exe is named more than once in licences/README.txt' 'and one named twice is refused'

Make-GoodPackage
Write-File (Join-Path $package 'licences/README.txt') "bin/AvP_Classic.exe: the game, with no licence named.`nbin/D3DCompiler_43.dll: (licences/D3DX.txt)`nbin/d3dx9_43.dll: (licences/D3DX.txt)`n"
Write-Sums
Expect-Refused 'check-release-package.ps1' (Package-Arguments) 'licences/README.txt names no licence for bin/AvP_Classic.exe' 'a line that names no licence is refused'

Make-GoodPackage
Remove-Item -LiteralPath (Join-Path $package 'licences/D3DX.txt')
Write-Sums
Expect-Refused 'check-release-package.ps1' (Package-Arguments) 'licences/D3DX.txt, the licence for bin/' 'a licence that is named but not there is refused'

Make-GoodPackage
Write-File (Join-Path $package 'licences/ENet.txt') ''
Write-Sums
Expect-Refused 'check-release-package.ps1' (Package-Arguments) 'licences/ENet.txt, the licence for bin/AvP_Classic.exe, is missing or empty' 'and so is an empty one'

# OpenAL Soft's source goes with it
Make-GoodPackage
Write-File (Join-Path $package 'bin/OpenAL32.dll') 'a library'
Add-Content -LiteralPath (Join-Path $package 'licences/README.txt') -Value 'bin/OpenAL32.dll: OpenAL Soft (licences/D3DX.txt), with no source named.'
Write-Sums
Expect-Refused 'check-release-package.ps1' (Package-Arguments) 'licences/README.txt names no source in licences/sources for bin/OpenAL32.dll' 'OpenAL Soft with no source named is refused'

Make-GoodPackage
Write-File (Join-Path $package 'bin/OpenAL32.dll') 'a library'
Add-Content -LiteralPath (Join-Path $package 'licences/README.txt') -Value 'bin/OpenAL32.dll: OpenAL Soft (licences/D3DX.txt). Its source: licences/sources/openal-soft/gone.tar.gz, here.'
Write-Sums
Expect-Refused 'check-release-package.ps1' (Package-Arguments) 'licences/sources/openal-soft/gone.tar.gz, the source licences/README.txt names for bin/OpenAL32.dll, is missing or empty' 'and so is one whose source is not there'

Make-GoodPackage
Write-File (Join-Path $package 'bin/OpenAL32.dll') 'a library'
Write-File (Join-Path $package 'licences/sources/openal-soft/openal.tar.gz') 'its source'
Write-File (Join-Path $package 'licences/sources/openal-soft/port/portfile.cmake') 'how it is built'
Add-Content -LiteralPath (Join-Path $package 'licences/README.txt') -Value 'bin/OpenAL32.dll: OpenAL Soft (licences/D3DX.txt). Its source: licences/sources/openal-soft/openal.tar.gz, and licences/sources/openal-soft/port, here.'
Write-Sums
$result = Run-Script 'check-release-package.ps1' (Package-Arguments)
Check ($result.Code -eq 0) "and with its source there, it passes. It said: $($result.Text)"

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

foreach ($badLine in @(('0' * 64) + '  ../outside.rif', ('0' * 64) + '  C:/elsewhere.rif', 'not-a-hash  avp_huds/Ours.rif', ('0' * 64) + '  avp_huds/a[1].rif', ('0' * 64) + '  /root.rif', ('0' * 64) + '  PLAYTEST.md', ('0' * 64) + '  shaders/reliefMain.psh', ('0' * 64) + '  avp_huds/run.bat', ('0' * 64) + '  bin/AvP_Classic.exe'))
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

foreach ($stray in 'evil.dll', 'bin/run.bat', 'bin/more/extra.dll', 'shaders/start.cmd')
{
    Make-GoodPackage
    Write-File (Join-Path $package $stray) 'something Windows runs'
    Write-Sums
    Expect-Refused 'check-release-package.ps1' (Package-Arguments) "$stray is something Windows runs, and only the game and its own libraries, in bin, may be in the package" "something Windows runs is refused outside bin: $stray"
}

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
$result = Run-Script 'check-hudson-asleep.ps1' @()
Check ($result.Text.Contains("A player's game can't run Hudson") -or $result.Text.Contains('None of Hudson is in this commit') -or $result.Text.Contains('REFUSED: ')) "check-hudson-asleep.ps1 finds the repository by itself. It said: $($result.Text)"

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
Remove-Item -LiteralPath (Join-Path $package 'bin/D3DCompiler_43.dll')
Write-Sums
Check ((Run-AsAStep 'check-release-package.ps1' @('-Package', $package, '-Commit', $commit, '-Repository', $packageRepository)) -eq 1) 'and it fails when it should'

Clear-Work
Check ((Run-AsAStep 'check-hudson-asleep.ps1' @('-Repository', $work)) -eq 0) 'as a step, check-hudson-asleep.ps1 passes when it should'
Write-File (Join-Path $work '3dc/avp/hudson.cpp') 'bool HudsonRunIsOn = true;'
Check ((Run-AsAStep 'check-hudson-asleep.ps1' @('-Repository', $work)) -eq 1) 'and it fails when it should'

Remove-Item -Force -LiteralPath $stepFile

# --- VirusTotal's answers, made up. tools/check-virustotal.ps1 never goes on the network, so each answer it can get is tried here as a file.
# Each refusal has to give its own reason; a good answer has to give back exactly the ANSWER: line the scan job reads.
Clear-Work
$sha = 'ab' * 32
$answerFile = Join-Path $work 'answer.json'
$goodCounts = '"malicious": 0, "suspicious": 0, "undetected": 58, "harmless": 0, "timeout": 1, "confirmed-timeout": 0, "failure": 0, "type-unsupported": 12'
$analysisId = 'OTFiMDcwMjVmYTM1MmQ2ZjA5YTM4NmU5ZTNlNjk4YmE6MTc2MjYwMDAwMA=='

# Answers as VirusTotal writes them, with HASH, ID, COUNTS and STATUS put in by each test
$lookupTemplate = '{ "data": { "type": "file", "id": "ID", "attributes": { "sha256": "HASH", "last_analysis_date": 1762600000, "last_analysis_stats": { COUNTS }, "names": ["AvP.zip"] } } }'
$uploadTemplate = '{ "data": { "type": "analysis", "id": "ID", "links": { "self": "https://www.virustotal.com/api/v3/analyses/ID" } } }'
$analysisTemplate = '{ "data": { "type": "analysis", "id": "ID", "attributes": { "status": "STATUS", "stats": { COUNTS }, "date": 1762600000 } }, "meta": { "file_info": { "sha256": "HASH", "size": 1234 } } }'

function Lookup-Text([string]$counts = $goodCounts, [string]$id = $sha, [string]$hash = $sha) { return $lookupTemplate.Replace('COUNTS', $counts).Replace('ID', $id).Replace('HASH', $hash) }
function Upload-Text([string]$id = $analysisId) { return $uploadTemplate.Replace('ID', $id) }
function Analysis-Text([string]$status = 'completed', [string]$counts = $goodCounts, [string]$id = $analysisId, [string]$hash = $sha) { return $analysisTemplate.Replace('COUNTS', $counts).Replace('STATUS', $status).Replace('ID', $id).Replace('HASH', $hash) }

function Virustotal-Arguments([string]$kind) { return @('-Answer', $answerFile, '-Kind', $kind, '-Sha256', $sha) + $(if ($kind -ceq 'analysis') { @('-AnalysisId', $analysisId) } else { @() }) }

function Expect-Answer([string]$kind, [string]$text, [string]$answer, [string]$what)
{
    Write-File $answerFile $text
    $result = Run-Script 'check-virustotal.ps1' (Virustotal-Arguments $kind)
    $said = @($result.Text -split "`n" | Where-Object { $_.StartsWith('ANSWER: ') })
    Check ($result.Code -eq 0 -and $said.Count -eq 1 -and $said[0] -ceq "ANSWER: $answer") "$what. It said: $($result.Text)"
    return $result
}

function Expect-AnswerRefused([string]$kind, [string]$text, [string]$reason, [string]$what)
{
    Write-File $answerFile $text
    Expect-Refused 'check-virustotal.ps1' (Virustotal-Arguments $kind) $reason $what
}

# What VirusTotal already knows about the file
$result = Expect-Answer 'lookup' (Lookup-Text) 'counted 0 0 58' 'a finished analysis of a file VirusTotal knows is counted'
Check ($result.Text.Contains("0 of 58 scanners flagged it") -and $result.Text.Contains("https://www.virustotal.com/gui/file/$sha")) "and the log says what it found, with the report's link. It said: $($result.Text)"
$result = Expect-Answer 'lookup' (Lookup-Text '"malicious": 2, "suspicious": 1, "undetected": 55, "harmless": 0') 'counted 2 1 58' 'a flagged file is counted, not refused, so the draft can say so'
Check ($result.Text.Contains('3 of 58 scanners flagged it (2 malicious, 1 suspicious')) "and the log says how many flagged it, and how. It said: $($result.Text)"
Expect-Answer 'lookup' (Lookup-Text).Replace('"last_analysis_date": 1762600000, ', '') 'not-analysed' 'a file with no finished analysis is sent again' | Out-Null
Expect-Answer 'lookup' (Lookup-Text '"malicious": 0, "suspicious": 0, "undetected": 10, "harmless": 9') 'not-analysed' 'and so is one only 19 scanners answered about' | Out-Null
Expect-AnswerRefused 'lookup' (Lookup-Text -id ('cd' * 32)) 'about a different file' 'an answer about another file is refused'
Expect-AnswerRefused 'lookup' (Lookup-Text -hash ('cd' * 32)) 'about a different file' 'and so is one whose own hash is another file''s'
Expect-AnswerRefused 'lookup' (Lookup-Text).Replace('"type": "file"', '"type": "url"') "something that isn't a file" 'and one about something that is not a file'
Expect-AnswerRefused 'lookup' (Lookup-Text).Replace('1762600000', '"yesterday"') "last_analysis_date isn't a time" 'a time that is not a number is refused'
Expect-AnswerRefused 'lookup' (Lookup-Text).Replace('1762600000', '99999999999999') "last_analysis_date isn't a time" 'and so is one past the year 9999'

# Answers that aren't JSON, or not the shape VirusTotal's are
Expect-AnswerRefused 'lookup' 'not an answer at all' "isn't JSON" 'text that is not JSON is refused'
Expect-AnswerRefused 'lookup' ((Lookup-Text).TrimEnd('}') + '}, }') "isn't JSON" 'and so is JSON with a comma left at the end'
Expect-AnswerRefused 'lookup' ("// a note`n" + (Lookup-Text)) "isn't JSON" 'and JSON with a comment in it'
Expect-AnswerRefused 'lookup' ((Lookup-Text) + ' {}') "isn't JSON" 'and JSON with more after its end'
Expect-AnswerRefused 'lookup' '[1, 2]' "the whole answer isn't an object" 'a list where an object should be is refused'
Expect-AnswerRefused 'lookup' '{}' 'has no data' 'an answer with no data is refused'
Expect-AnswerRefused 'lookup' '{ "data": "file" }' "data isn't an object" 'and so is one whose data is not an object'
Expect-AnswerRefused 'lookup' ('{ "data": {}, ' + (Lookup-Text).Substring(1)) 'has data more than once' 'a part that is there twice is refused, because the two could say different things'
Expect-AnswerRefused 'lookup' '' "VirusTotal's answer is empty" 'an empty answer is refused'
Expect-AnswerRefused 'lookup' (' ' * 5000001) 'far more than an answer about one file' 'and one too big to be an answer is not even read'

foreach ($badCount in '"3"', '1.5', '-1', '1001', '1e2', 'true', 'null', '[0]')
{
    Expect-AnswerRefused 'lookup' (Lookup-Text $goodCounts.Replace('"malicious": 0', "`"malicious`": $badCount")) "count of malicious in data.attributes.last_analysis_stats isn't a whole number from 0 to 1000" "a count of $badCount is refused"
}

Expect-AnswerRefused 'lookup' (Lookup-Text ($goodCounts + ', "sneaky": 0')) "a kind this doesn't know" 'a kind of count this does not know is refused, as it could be a verdict missed'
Expect-AnswerRefused 'lookup' (Lookup-Text ($goodCounts + ', "Malicious": 0')) "a kind this doesn't know" 'and a known kind written with a capital is one it does not know'
Expect-AnswerRefused 'lookup' (Lookup-Text $goodCounts.Replace('"malicious": 0, ', '')) 'no count of malicious' 'a missing count of malicious is refused'
Expect-AnswerRefused 'lookup' (Lookup-Text ('"malicious": 3, ' + $goodCounts)) 'have malicious more than once' 'a count that is there twice is refused, so a 0 after it cannot hide it'

# The upload, and the analysis it starts
Expect-Answer 'upload' (Upload-Text) "analysis $analysisId" 'the upload gives the number of its analysis' | Out-Null
Expect-AnswerRefused 'upload' (Upload-Text '../files/x') "the analysis number in VirusTotal's answer isn't one" 'a number with anything else in it is refused, as it goes into an address'
Expect-AnswerRefused 'upload' (Upload-Text).Replace('"type": "analysis"', '"type": "file"') "isn't an analysis" 'and an answer to the upload that is not an analysis'

Expect-Answer 'analysis' (Analysis-Text 'queued') 'waiting' 'an analysis that is waiting to start is waited for' | Out-Null
Expect-Answer 'analysis' (Analysis-Text 'in-progress') 'waiting' 'and so is one that is going' | Out-Null
Expect-Answer 'analysis' (Analysis-Text) 'counted 0 0 58' 'a finished analysis is counted' | Out-Null
Expect-Answer 'analysis' (Analysis-Text -counts '"malicious": 1, "suspicious": 0, "undetected": 57, "harmless": 0') 'counted 1 0 58' 'and so is a flagged one' | Out-Null
Expect-AnswerRefused 'analysis' (Analysis-Text -hash ('cd' * 32)) 'of a different file' 'an analysis of another file is refused'
Expect-AnswerRefused 'analysis' (Analysis-Text -id 'QW5vdGhlcg==') 'a different analysis' 'and so is an answer about another analysis'
Expect-AnswerRefused 'analysis' (Analysis-Text -counts '"malicious": 0, "suspicious": 0, "undetected": 19, "harmless": 0') 'only 19 scanners gave an answer about the zip, fewer than the 20' 'a finished analysis with too few scanners is refused'
Expect-AnswerRefused 'analysis' (Analysis-Text 'done') "in a state this doesn't know" 'an analysis in a state this does not know is refused'

# Nothing VirusTotal wrote is printed: GitHub takes a line starting :: as an order
Write-File $answerFile (Analysis-Text '::warning::taken over')
$result = Run-Script 'check-virustotal.ps1' (Virustotal-Arguments 'analysis')
Check ($result.Code -ne 0 -and -not $result.Text.Contains('::')) "a state written as an order to GitHub is refused, and not printed. It said: $($result.Text)"
Write-File $answerFile (Lookup-Text ($goodCounts + ', "::error::x": 0'))
$result = Run-Script 'check-virustotal.ps1' (Virustotal-Arguments 'lookup')
Check ($result.Code -ne 0 -and -not $result.Text.Contains('::')) "and so is a kind of count written as one. It said: $($result.Text)"

# What it's told by the scan job
Expect-Refused 'check-virustotal.ps1' @('-Answer', $answerFile, '-Kind', 'lookup', '-Sha256', $sha.ToUpperInvariant()) 'has to be a SHA-256, 64 small letters and digits' 'a hash to check against that is not written as one is refused'
Expect-Refused 'check-virustotal.ps1' @('-Answer', $answerFile, '-Kind', 'other', '-Sha256', $sha) '-Kind has to be lookup, upload or analysis' 'and so is a kind of answer it does not know'
Expect-Refused 'check-virustotal.ps1' @('-Answer', $answerFile, '-Kind', 'analysis', '-Sha256', $sha) "the number the upload gave, and that's missing" 'an analysis is not checked without the number the upload gave'
Expect-Refused 'check-virustotal.ps1' @('-Answer', (Join-Path $work 'gone.json'), '-Kind', 'lookup', '-Sha256', $sha) 'there is no saved answer to read' 'and nor is an answer that is not there'

# The zip, before it's sent
$zip = Join-Path $work 'AvP-1.0.0-beta.0-windows-x86.zip'
Write-File $zip 'a zip'
$zipHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zip).Hash.ToLowerInvariant()
$result = Run-Script 'check-virustotal.ps1' @('-Package', $zip, '-Sha256', $zipHash)
Check ($result.Code -eq 0 -and $result.Text.Contains('ANSWER: ready')) "the zip the build made may be sent. It said: $($result.Text)"
Expect-Refused 'check-virustotal.ps1' @('-Package', $zip, '-Sha256', $sha) 'the zip is not the one the build made' 'a zip that is not the one the build made is refused'
Expect-Refused 'check-virustotal.ps1' @('-Package', (Join-Path $work 'gone.zip'), '-Sha256', $zipHash) 'the zip to send is not there' 'and so is one that is not there'
$big = [System.IO.File]::Create((Join-Path $work 'big.zip'))
$big.SetLength(32000001)
$big.Dispose()
Expect-Refused 'check-virustotal.ps1' @('-Package', (Join-Path $work 'big.zip'), '-Sha256', $zipHash) 'VirusTotal takes at most 32000000 in one upload' 'a zip too big for one upload is refused'

foreach ($name in 'tools/check-release.ps1', 'tools/check-hudson-asleep.ps1', 'tools/check-release-package.ps1', 'tools/check-new-functions.ps1', 'tools/check-virustotal.ps1', 'tests/release-test.ps1')
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
$jobs = [regex]::Split($withoutComments, '(?m)^  (?=(?:check|build|scan|publish):\n)')

Check ($jobs.Count -eq 5) 'the workflow has the four jobs, and no others'
$top = $jobs[0]; $checkJob = $jobs[1]; $buildJob = $jobs[2]; $scanJob = $jobs[3]; $publishJob = $jobs[4]

Check ($top -match '(?m)^on:\n  workflow_dispatch:\n') 'it starts by hand'
Check ($top -notmatch '(?m)^\s+(push|pull_request|pull_request_target|schedule|release|workflow_run|repository_dispatch):') 'and in no other way, because a tag could point at a commit with the checks taken out'
Check ($top -match '(?m)^permissions: \{\}$') 'by default no job can do anything'

Check ($checkJob -match 'run: \./tools/check-release\.ps1 -Version \$env:VERSION\n') 'the first job runs the checks'
Check ($checkJob -match 'run: \./tools/check-hudson-asleep\.ps1\n') 'and refuses any way a player''s game could run Hudson'
Check ($checkJob -match 'run: \./tools/check-new-functions\.ps1\n') 'and refuses a new function the game does not use'
Check ($checkJob -match '(?m)^          fetch-depth: 0\b') 'with the history it needs to compare'
Check (-not $withoutComments.Contains('-WithoutGitHub')) 'and never leaves out the ones that ask GitHub'
Check ($buildJob -match '(?m)^    needs: check$') 'the build needs the checks to have passed'
Check ($scanJob -match '(?m)^    needs: build$') 'the scan needs the package'
Check ($publishJob -match '(?m)^    needs: \[check, build, scan\](\s+#[^\n]*)?$') 'and the draft needs the build and the scan, so it comes after the checks too, and the checks for its tag'
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
Check ($checkScript.Contains('$tag = "v$Version"')) "a version's tag is v and the version, and a beta's too"
Check ($publishJob.Contains('TAG: ${{ needs.check.outputs.tag }}') -and -not $publishJob.Contains('$tag = "v$env:VERSION"')) 'the draft uses the tag the checks made, through the environment'
Check ($publishJob.Contains('if ($env:TAG -cne "v$env:VERSION") { throw')) 'and looks at it again before anything uses it'

# The two forms a version can take are one rule, so they're written the same, letter for letter, in each place that lets a version through
$releaseForm = '^(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})\z'
$betaForm = '^(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})-beta\.(0|[1-9][0-9]{0,3})\z'
$buildScript = Get-Content -Raw (Join-Path $repository 'tools/build-and-package.ps1')

foreach ($place in @(@{ Name = 'tools/check-release.ps1'; Text = $checkScript }, @{ Name = "release.yml's scan"; Text = $scanJob }, @{ Name = "release.yml's last job"; Text = $publishJob }, @{ Name = 'tools/build-and-package.ps1'; Text = $buildScript }))
{
    Check ($place.Text.Contains("-cmatch '$betaForm'") -and ($place.Text.Contains("-cmatch '$releaseForm'") -or $place.Text.Contains("-cnotmatch '$releaseForm'"))) "$($place.Name) lets a version through in the same two forms"
    Check (-not $place.Text.Contains('betarelease[0-9]')) "and no longer lets a beta through named the old way"
}

# A beta's number is counted from what GitHub has: every tag of the version, and every release
Check ($checkScript.Contains('git/matching-refs/tags/v$release"') -and $checkScript.Contains("releases?per_page=100`" --jq '.[].tag_name'") -and $checkScript.Contains('Stop-IfTheNumberIsTaken ($tags + $releases)')) "a beta's number is counted from the tags and releases GitHub has"
Check (-not $withoutComments.Contains('-ExistingTags')) 'and the workflow never hands it a list of its own'
Check ($publishJob.Contains("if (`$preRelease) { `$alsoSay += '--prerelease' }") -and $publishJob.Contains('@alsoSay')) 'only a beta is made a pre-release'
Check ($publishJob.Contains('if ($made.isPrerelease -ne $preRelease) { throw')) 'and the draft is checked for it'
Check ($publishJob.Contains('if ($releases -ccontains $env:TAG) { throw')) 'a tag already used, even by a draft, is refused'

Check ($publishJob -match '(?m)^      contents: write\s') 'the last job can write'
Check (-not $checkJob.Contains('write') -and -not $buildJob.Contains(': write') -and -not ($scanJob -cmatch ':\s*write\b')) 'and the other three can not'

# --- VirusTotal: the scan, read as text. tools/check-virustotal.ps1 is tried above; this is the step around it, which can only run for real.
$virustotalScript = Get-Content -Raw (Join-Path $repository 'tools/check-virustotal.ps1')
Check ($scanJob -match '(?m)^    environment: release\s') "the scan runs under the release environment, the only place the key can be read"
Check ($scanJob -match '(?m)^    permissions:\n      contents: read( +#[^\n]*)?\n    outputs:') 'and it can only read the code'
Check ($scanJob.Contains('actions/checkout@') -and $scanJob.Contains('persist-credentials: false')) 'it has the code, for the checker, and leaves no token behind'
$secretUses = @([regex]::Matches($withoutComments, 'secrets\.[A-Za-z_]+') | ForEach-Object { $_.Value })
Check ($secretUses.Count -eq 1 -and $scanJob.Contains('          VIRUSTOTAL_API_KEY: ${{ secrets.VIRUSTOTAL_API_KEY }}')) 'the VirusTotal key is the only secret, and only the scan step has it, as a setting'
Check-Order $scanJob 'Remove-Item Env:VIRUSTOTAL_API_KEY' '& pwsh -NoProfile -File ./tools/check-virustotal.ps1' 'the key is taken out of the environment before any program starts, so the programs it starts are not given it'

# On Linux any program of the same user can still read the step's own starting environment, the key included, so the step may start
# only the checker. Every way PowerShell can start a program is looked for in the step's code: the call operator once, for the checker,
# and nothing else. A program named bare, as a command, is the one way this can't see, so the names of the programs a runner has that
# could reach the network are looked for too.
$askStep = [regex]::Match($scanJob, '(?s)- name: Ask VirusTotal about the package\n.*\z').Value
Check ($askStep.Length -gt 0) 'the step that asks VirusTotal can be read'
Check ([regex]::Matches($askStep, '(?:^|[\s(=;{])&\s').Count -eq 1 -and $askStep.Contains('& pwsh -NoProfile -File ./tools/check-virustotal.ps1 ')) 'it starts one program, the checker, and calls nothing else with &'
foreach ($starts in 'Start-Process', 'Invoke-Expression', 'iex ', 'Invoke-Item', 'Invoke-Command', 'Start-Job', 'Start-ThreadJob', 'Diagnostics.Process', '.Invoke(', 'bash', 'sh -c', 'cmd', 'gh ', 'git ', 'curl', 'wget', 'python', 'node ', '.exe', '.sh ')
{
    Check (-not $askStep.Contains($starts)) "and nothing that starts another: no $starts"
}

# SHA256SUMS comes from the build, which runs other people's code, so the jobs after it read it as hostile: one line, a hash, and exactly
# the zip this version makes, before the name is used
$nameRule = 'if ($name -cne "AvP-$env:VERSION-windows-x86.zip") { throw'
Check ($scanJob.Contains($nameRule) -and $scanJob.Contains("if (`$sums.Count -ne 1) { throw") -and $scanJob.Contains("if (`$sha -cnotmatch '^[0-9a-f]{64}\z') { throw")) 'the scan sends only the zip this version makes'
Check-Order $scanJob $nameRule '$zip = "release/$name"' 'and checks its name before it uses it'
Check ($scanJob -match '(?m)^          VERSION: \$\{\{ inputs\.version \}\}$') 'with the version, as a setting'
Check (([regex]::Matches($publishJob, [regex]::Escape($nameRule))).Count -eq 2 -and ([regex]::Matches($publishJob, [regex]::Escape("if (`$sums.Count -ne 1) { throw"))).Count -eq 2) 'the draft checks it too, in both its steps that read it'
Check-Order $publishJob $nameRule 'Get-FileHash -Algorithm SHA256 "release/$name"' 'before it hashes it'
Check-Order $publishJob $nameRule 'attest-build-provenance' 'and before it records how it was built'

# Asked too often (429): every question waits a minute and asks again, inside one time limit for the whole step
Check ($askStep.Contains('if ($answeredWith -ne 429) { return $answeredWith }') -and $askStep.Contains("if ((Get-Date) -gt `$giveUp) { throw")) 'a question VirusTotal says came too often is asked again, until the time is up'
Check (-not $askStep.Contains('$status -eq 429')) 'and no question deals with that on its own'
Check ([regex]::Matches($scanJob, '\$key\b').Count -eq 3 -and $scanJob.Contains("`$headers = @{ 'x-apikey' = `$key }")) 'and it goes nowhere but the header'
Check ($scanJob.Contains('MaximumRedirection = 0')) 'a redirect is not followed, so the header cannot go anywhere else'
$addresses = @([regex]::Matches($scanJob, 'https?://[^\s"/]+') | ForEach-Object { $_.Value } | Sort-Object -Unique)
Check ($addresses.Count -eq 1 -and $addresses[0] -ceq 'https://www.virustotal.com') "it asks VirusTotal and nothing else. It names: $addresses"
Check (-not $scanJob.Contains('ConvertFrom-Json') -and -not $scanJob.Contains('Get-Content $answers') -and -not $scanJob.Contains('Get-Content "$answers')) 'it never reads an answer itself: only the checker does'
Check-Order $scanJob "Ask-Checker @('-Package', `$zip)" "Ask-VirusTotal 'Post' 'files'" 'the zip is checked before it is sent'
Check ($scanJob.Contains("if (`$status -eq 404) {") -and $scanJob.Contains("while (`$answer -ceq 'waiting')") -and $scanJob.Contains('$giveUp = (Get-Date).AddMinutes(25)')) 'it sends the zip only when VirusTotal does not know it, and waits for the analysis, for a while'

$fewest = [regex]::Match($virustotalScript, '(?m)^\$fewestScanners = ([0-9]+)\r?$').Groups[1].Value
Check ($fewest.Length -gt 0 -and $publishJob.Contains("[int]`$env:SCAN_ANSWERED -lt $fewest) { throw")) 'the draft asks for as many scanners as the checker does'
Check ($publishJob.Contains('if ($env:SCAN_SHA256 -cne $wanted) { throw')) 'and checks the scan was of this very zip'
Check-Order $publishJob 'gh release create' 'if ($flagged -gt 0) { throw' 'a flagged zip still gets its draft, and then the run ends red'
Check ($publishJob.Contains('$first = if ($flagged -gt 0) { @("**$warning**", '''') }')) "and the draft's text starts by saying so"

foreach ($other in Get-ChildItem (Join-Path $repository '.github/workflows') -Filter '*.yml' | Where-Object { $_.Name -ne 'release.yml' })
{
    Check (-not (Get-Content -Raw $other.FullName).Contains('contents: write')) "$($other.Name) can't write to the repository"
}

foreach ($match in [regex]::Matches($withoutComments, '(?m)^\s*(?:- )?uses:\s*(\S+)'))
{
    Check ($match.Groups[1].Value -cmatch '^[a-z-]+/[a-z-]+@[0-9a-f]{40}$') "$($match.Groups[1].Value) is pinned to a full commit hash"
}

Check ($withoutComments.Split("`n").Where({ $_ -match 'actions/checkout@' }).Count -eq 3) 'three jobs check out the code'
Check ([regex]::Matches($withoutComments, 'persist-credentials: false').Count -eq 3) 'and none leaves its token behind for what runs after to find'

$inputUses = @([regex]::Matches($withoutComments, '.*\$\{\{\s*inputs\.[^\n]*') | ForEach-Object { $_.Value.Trim() })
Check ($inputUses.Count -ge 1 -and @($inputUses | Where-Object { $_ -cne 'VERSION: ${{ inputs.version }}' }).Count -eq 0) 'what a person typed only ever reaches a command as a setting, so it can never be run as part of one'
Check (@($secretUses | Where-Object { $_ -cne 'secrets.VIRUSTOTAL_API_KEY' }).Count -eq 0) 'it uses no other secret but the job''s own token'

# --- The list of required checks is the same as the test build's
$buildWorkflow = Get-Content -Raw (Join-Path $repository '.github/workflows/build.yml')
$listInBuild = [regex]::Match($buildWorkflow, '\$required = (.*)').Groups[1].Value.Trim()
$listInCheck = [regex]::Match($checkScript, '\$requiredChecks = (.*)').Groups[1].Value.Trim()
Check ($listInBuild.Length -gt 0 -and $listInBuild -ceq $listInCheck) 'a release asks for the same checks as a test build'

# --- Every pull request refuses any way a player's game could run Hudson as well, in a check that's already required, so a way in left awake fails
# the pull request that brings it
$testsWorkflow = (Get-Content -Raw (Join-Path $repository '.github/workflows/tests.yml')) -replace "`r", ''
Check ($testsWorkflow -match '(?m)^  pull_request:\n') 'the tests run on every pull request'
Check-Order $testsWorkflow 'name: Unit tests' './tools/check-hudson-asleep.ps1' 'and the required Unit tests check refuses any way a player''s game could run Hudson'
Check-Order $testsWorkflow './tools/check-hudson-asleep.ps1' './tests/build.ps1' 'before it builds anything'

# --- Pull requests try the release's own checks on the machine the release runs them on
$ubuntuJob = [regex]::Match($testsWorkflow, '(?ms)^  release-checks-on-ubuntu:\n.*\z').Value
$releaseMachine = [regex]::Match($checkJob, '(?m)^    runs-on: (\S+)').Groups[1].Value
$ubuntuMachine = [regex]::Match($ubuntuJob, '(?m)^    runs-on: (\S+)').Groups[1].Value
$ubuntuName = [regex]::Match($ubuntuJob, '(?m)^    name: (.+)$').Groups[1].Value.Trim()
Check ($releaseMachine.Length -gt 0 -and $ubuntuMachine -ceq $releaseMachine) 'pull requests try the checks on the machine the release uses'
Check ($ubuntuJob -match 'shell: pwsh\n\s+run: \./tools/check-hudson-asleep\.ps1\s') 'and refuse any way a player''s game could run Hudson there, with the same PowerShell'
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
