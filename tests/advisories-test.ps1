<#
Tries tools/check-advisories.ps1 against made-up advisories, every way it can pass and refuse, with no network: each case writes a small
repository (the read list and the ports' versions) and GitHub's answers as files, and runs the check with -Answers. Written 2026-11-09 by
Melanikus for the AvP port (#174).

snyk.yml runs this on GitHub, just before the real check. Like every new .ps1, it doesn't run on the team's own machines.
#>
$ErrorActionPreference = 'Stop'

$repository = Split-Path -Parent $PSScriptRoot
$powershell = (Get-Process -Id $PID).Path
$temporary = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [System.IO.Path]::GetTempPath() }
$work = Join-Path $temporary 'avp-advisories-test'
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

function Write-File([string]$path, [string]$text)
{
    New-Item -ItemType Directory -Force (Split-Path -Parent $path) | Out-Null
    [System.IO.File]::WriteAllText($path, $text)
}

# One made-up advisory, as GitHub's API gives it, with the given ranges (none: no "vulnerabilities" at all)
function New-Advisory([string]$id, [string[]]$ranges)
{
    $vulnerabilities = @($ranges | ForEach-Object { @{ package = @{ ecosystem = 'other'; name = 'x' }; vulnerable_version_range = $_ } })
    return @{ ghsa_id = $id; summary = 'made up'; vulnerabilities = $vulnerabilities }
}

# A read entry, for version
function New-Read([string]$id, [string]$version)
{
    return @{ id = $id; version = $version; why = 'it was fixed before this release'; readBy = 'test'; readOn = '2026-11-09' }
}

# Writes the case and runs the check. kem and dsa are each { version (what was read), built (what the port builds), read, answer }; an
# answer of $null writes no answer file, and a string is written as it is.
function Run-Case([hashtable]$kem, [hashtable]$dsa)
{
    if (Test-Path -LiteralPath $work) { Remove-Item -Recurse -Force -LiteralPath $work }

    $libraries = @()

    foreach ($pair in @(@('mlkem-native', $kem), @('mldsa-native', $dsa)))
    {
        $port = $pair[0]
        $case = $pair[1]

        if ($null -eq $case)
        {
            continue
        }

        $libraries += @{ port = $port; repository = "pq-code-package/$port"; version = $case.version; read = @($case.read) }
        Write-File (Join-Path $work "repo/vcpkg-overlay-ports/$port/vcpkg.json") (@{ name = $port; version = $case.built } | ConvertTo-Json)

        if ($case.answer -is [string])
        {
            Write-File (Join-Path $work "answers/pq-code-package_$port.json") $case.answer
        }
        elseif ($null -ne $case.answer)
        {
            Write-File (Join-Path $work "answers/pq-code-package_$port.json") (ConvertTo-Json -Depth 8 -InputObject @($case.answer))
        }
    }

    Write-File (Join-Path $work 'repo/.github/advisories-read.json') (ConvertTo-Json -Depth 8 -InputObject @{ about = 'test'; libraries = $libraries })
    New-Item -ItemType Directory -Force (Join-Path $work 'answers') | Out-Null
    $passedFile = Join-Path $work 'passed.txt'

    $ErrorActionPreference = 'Continue'
    $output = & $powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repository 'tools/check-advisories.ps1') `
        -Repository (Join-Path $work 'repo') -Answers (Join-Path $work 'answers') -Passed $passedFile 2>&1 | ForEach-Object { "$_" }
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    $passedPorts = if (Test-Path -LiteralPath $passedFile) { @([System.IO.File]::ReadAllLines($passedFile) | Where-Object { $_ }) } else { @() }
    return [pscustomobject]@{ Code = $code; Text = ($output -join "`n"); Passed = ($passedPorts -join ',') }
}

function Clean([object[]]$answer) { return @{ version = '2.0.0'; built = '2.0.0'; read = @(); answer = $answer } }

$id1 = 'GHSA-2345-6789-cfgh'
$id2 = 'GHSA-jmpq-rvwx-2345'

# --- Passing
$r = Run-Case (Clean @()) (Clean @())
Check ($r.Code -eq 0 -and $r.Passed -eq 'mlkem-native,mldsa-native') "no advisories at all passes both, and lists both as passed. It said: $($r.Text)"

$kem = @{ version = '2.0.0'; built = '2.0.0'; read = @(New-Read $id1 '2.0.0'); answer = @(New-Advisory $id1 @('< 1.5.0')) }
$r = Run-Case $kem (Clean @())
Check ($r.Code -eq 0 -and $r.Passed -eq 'mlkem-native,mldsa-native') "an advisory read for this version, about versions before it, passes. It said: $($r.Text)"

$kem = @{ version = '2.0.0'; built = '2.0.0'; read = @(New-Read $id1 'v2.0.0'); answer = @(New-Advisory $id1 @('>= 2.0.1', '> 2.0.0, < 3')) }
$r = Run-Case $kem (Clean @())
Check ($r.Code -eq 0) "ranges above this version, and 'v2.0.0' read as 2.0.0, pass. It said: $($r.Text)"

# --- Refusing, each for its own reason, and the other library still passing and listed alone
function Expect-Refusal([hashtable]$kem, [string]$reason, [string]$what)
{
    $r = Run-Case $kem (Clean @())
    Check ($r.Code -ne 0 -and $r.Text.Contains($reason) -and $r.Passed -eq 'mldsa-native') "$what. It said: $($r.Text)"
}

Expect-Refusal @{ version = '2.0.0'; built = '2.0.0'; read = @(); answer = @(New-Advisory $id1 @('< 1.5.0')) } 'is new, and hasn''t been read' `
    'an advisory nobody has read is refused'
Expect-Refusal @{ version = '2.0.0'; built = '2.0.0'; read = @(New-Read $id1 '1.9.0'); answer = @(New-Advisory $id1 @('< 1.5.0')) } 'hasn''t been read for 2.0.0' `
    'an advisory read only for another version is refused'
Expect-Refusal @{ version = '2.0.0'; built = '2.0.0'; read = @(@{ id = $id1; version = '2.0.0'; why = ' '; readBy = 'test'; readOn = '2026-11-09' }); answer = @(New-Advisory $id1 @('< 1.5.0')) } `
    'hasn''t been read' 'a read entry with no reason written is no read'
Expect-Refusal @{ version = '2.0.0'; built = '2.0.0'; read = @(New-Read $id1 '2.0.0'); answer = @(New-Advisory $id1 @('>= 1.0.0, < 2.0.1')) } 'affects 2.0.0' `
    'an advisory whose range holds this version is refused, even though it was read'
foreach ($range in @('= 2.0.0', '2.0.0', '<= 2.0.0', '>= 2.0.0', '== 2.0', '< 3'))
{
    Expect-Refusal @{ version = '2.0.0'; built = '2.0.0'; read = @(New-Read $id1 '2.0.0'); answer = @(New-Advisory $id1 @($range)) } 'affects 2.0.0' `
        "the range '$range' holds 2.0.0, and is refused"
}
foreach ($range in @('~> 2.0', '2.x', '>= one', ''))
{
    Expect-Refusal @{ version = '2.0.0'; built = '2.0.0'; read = @(New-Read $id1 '2.0.0'); answer = @(New-Advisory $id1 @($range)) } 'aren''t ones this check can read' `
        "a range it can't read ('$range') goes to a person"
}
Expect-Refusal @{ version = '2.0.0'; built = '2.0.0'; read = @(New-Read $id1 '2.0.0'); answer = @(New-Advisory $id1 @()) } 'names no affected versions' `
    'an advisory with no affected versions goes to a person'
Expect-Refusal @{ version = '2.0.0'; built = '2.0.1'; read = @(); answer = @() } 'read them again for the new version' `
    'a port that builds another version than the one read for is refused'
Expect-Refusal @{ version = '2.0.0'; built = '2.0.0'; read = @(); answer = $null } 'no answer came from GitHub' 'no answer is refused'
Expect-Refusal @{ version = '2.0.0'; built = '2.0.0'; read = @(); answer = '{"message": "Not Found"}' } 'isn''t a list' 'an answer that isn''t a list is refused'
Expect-Refusal @{ version = '2.0.0'; built = '2.0.0'; read = @(); answer = '[{"ghsa_id": ' } 'isn''t JSON' 'an answer that isn''t JSON is refused'
Expect-Refusal @{ version = '2.0.0'; built = '2.0.0'; read = @(); answer = @(1..100 | ForEach-Object { New-Advisory $id1 @('< 1') }) } 'a whole page' `
    'a full page of 100 is refused, as there may be more'
# GitHub's ids are 'GHSA-' and three groups of four, from the small letters and digits 23456789cfghjmpqrvwx only. Each of these isn't
# one, and is refused even though a read entry names it. ('x' is one of GitHub's letters, so 'GHSA-xxxx-xxxx-xxxx' is a real id's form:
# see the passing case below.)
foreach ($bad in @(@('GHSA-aaaa-bbbb-1111', 'letters GitHub never uses'), @('GHSA-2345-6789-CFGH', 'capitals'),
                   @('GHSA-2345-6789-cfg', 'too few letters'), @("GHSA-2345-6789-cfgh`n", 'a line break at its end')))
{
    Expect-Refusal @{ version = '2.0.0'; built = '2.0.0'; read = @(New-Read $bad[0] '2.0.0'); answer = @(New-Advisory $bad[0] @('< 1')) } `
        'isn''t a GitHub advisory''s' "an advisory id with $($bad[1]) is refused"
}

$kem = @{ version = '2.0.0'; built = '2.0.0'; read = @(New-Read 'GHSA-xxxx-wwww-2222' '2.0.0'); answer = @(New-Advisory 'GHSA-xxxx-wwww-2222' @('< 1.5.0')) }
$r = Run-Case $kem (Clean @())
Check ($r.Code -eq 0 -and $r.Passed -eq 'mlkem-native,mldsa-native') "an id in GitHub's own letters, x and w among them, passes. It said: $($r.Text)"

# A range is written by the other project's people, so it's hostile: a line break in it, with an order to GitHub's runner after it,
# must reach the log as '?', never as a line of its own
$kem = @{ version = '2.0.0'; built = '2.0.0'; read = @(New-Read $id1 '2.0.0'); answer = @(New-Advisory $id1 @("< 1`n::error::planted")) }
$r = Run-Case $kem (Clean @())
$ownLine = @($r.Text -split "`n" | Where-Object { $_.TrimStart().StartsWith('::') })
Check ($r.Code -ne 0 -and $r.Text.Contains('< 1?::error::planted') -and $ownLine.Count -eq 0) `
    "a line break in a range is printed as '?', so nothing after it starts a line. It said: $($r.Text -replace '[^\x20-\x7E]', '?')"

# Two advisories, one read and one not: the unread one alone is named
$kem = @{ version = '2.0.0'; built = '2.0.0'; read = @(New-Read $id1 '2.0.0'); answer = @((New-Advisory $id1 @('< 1')), (New-Advisory $id2 @('< 1'))) }
$r = Run-Case $kem (Clean @())
Check ($r.Code -ne 0 -and $r.Text.Contains("$id2 is new") -and -not $r.Text.Contains("$id1 is new")) "of two advisories, only the unread one is named. It said: $($r.Text)"

# A read list with no libraries checks nothing, so it's refused
$r = Run-Case $null $null
Check ($r.Code -ne 0 -and $r.Text.Contains('names no library')) "a read list with no library is refused. It said: $($r.Text)"

Write-Host "advisories-test: $script:passes passed, $script:failures failed"

if ($script:failures -gt 0 -or $script:passes -eq 0)
{
    exit 1
}

exit 0
