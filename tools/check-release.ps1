<#
Says whether this commit may be released. release.yml runs this before it builds anything, and a refusal stops the whole release.

It refuses unless all of these are true:
- The version is three plain numbers, such as 1.0.0, or a beta of a version: its three numbers, then -beta. and the beta's number, such as
  1.0.0-beta.0. It comes from a person typing it into a box, so nothing about it is believed, and each wrong way of writing it is refused
  with its own reason.
- RELEASE-BLOCKERS.md says nothing is in the way.
- tests/assert_limits_allowed.txt marks no assert as not yet safe.
- PLAYTEST.md, the notes testers read, has no line still marked [PENDING].
- Shadows & Shaders is really here, so that an old commit can't be released by mistake.
- It's running from avpx_main, on the very tip of it.
- Every required check passed on this exact commit. A check that was skipped, or never ran, counts as a failure.
- No code scanning alert is open.
- The version hasn't been used.
- A beta's number is the next one: one more than the highest beta of that version, or 0 for its first. A number is never used twice,
  gone back to or skipped (the user's ruling, 2026-11-08). The tags and releases on GitHub say which there are.
- A beta's version hasn't been released yet: a beta comes before its version, never after.

-WithoutGitHub leaves out everything that has to ask GitHub, for tests/release-test.ps1, which runs with no network. release.yml
never uses it, and that test fails if it ever does. With it, -ExistingTags can stand in for the tags and releases GitHub would list,
names with spaces between, so that test can try a beta's number. -ExistingTags is refused without -WithoutGitHub: a real release only
ever counts what GitHub says.

THE TAG: a version's tag is v and the version, such as v1.0.0, and a beta's too, such as v1.0.0-beta.0. A beta is a pre-release. Once the
version is checked, this writes both into the file GitHub gives a step for its outputs, when there is one, so that release.yml's last
job, which runs no code from the repository, uses the same tag without working it out again.

Before 2026-11-08 a beta was named betarelease and its day, such as betarelease20261107, which allowed one a day. That beta is still on
the releases page under its name, and nothing here lets that name through again.
#>
param(
    [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Version,
    [string]$Repository = '',
    [switch]$WithoutGitHub,
    [string]$ExistingTags
)

$ErrorActionPreference = 'Stop'

# Worked out here and not in param(), because older PowerShell doesn't know where the script is until it's finished reading that.
if (-not $Repository)
{
    $Repository = Split-Path -Parent $PSScriptRoot
}

# The checks this commit has to have passed, by the names branch protection and build.yml use. Add a new required check here as well.
# Take-outs is the one required check that isn't here: it runs only on pull requests, never on a push to avpx_main, so this commit has
# no Take-outs run of its own to look at. Its result counted when each pull request merged.
$requiredChecks = 'Snyk Code (SAST)', 'Snyk Open Source (SCA)', 'Snyk on the vcpkg libraries', 'Unit tests', 'Build the game', 'Release checks on Ubuntu', 'Tests under the address sanitizer'

# What Shadows & Shaders is made of: the shadows, the per-pixel lighting, the relief mapping and the rendering menu. A file being here
# isn't enough for code, because a file can be left behind and not built, so each .cpp has to be in the game's project too.
$shadowsAndShaders = @(
    '3dc/shadow_volumes.cpp',
    '3dc/surface_normals.cpp',
    '3dc/surface_pictures.cpp',
    '3dc/texture_relief.cpp',
    '3dc/view_settings.cpp',
    '3dc/view_switches.h',
    'shaders/volume.vsh',
    'shaders/surface.vsh',
    'shaders/surface.psh',
    'shaders/lightOne.psh',
    'shaders/reliefMain.vsh',
    'shaders/reliefMain.psh',
    'shaders/reliefMarch.fxh'
)

function Stop-Release([string]$why)
{
    Write-Host "REFUSED: $why"
    exit 1
}

if ($ExistingTags -and -not $WithoutGitHub)
{
    Stop-Release '-ExistingTags is only for tests/release-test.ps1, with -WithoutGitHub. A release counts the tags and releases GitHub has'
}

# --- The version. \z and not $, because $ would let a line break through at the end. [0-9] and not \d, because \d takes digits from
# every alphabet there is. -cmatch, so a capital letter is something else. It becomes a tag and part of a file name, so anything else
# in it could mean something to whatever reads those.
#
# Each number is 0, or digits that don't start with 0, so that one version can't be written two ways (1.0.0 and 01.0.0) and have its
# betas counted twice. The three numbers have at most 6 digits each, and a beta's number at most 4. These two forms are written the same,
# letter for letter, in release.yml and tools/build-and-package.ps1, and tests/release-test.ps1 checks they still are. 3dc/beta_build.h
# reads a beta the same way in C++, for the game.
$isBeta = $Version -cmatch '^(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})-beta\.(0|[1-9][0-9]{0,3})\z'

if (-not $isBeta -and $Version -cnotmatch '^(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})\z')
{
    # Refused whatever it is. These only choose which reason to give, so that whoever typed it knows what to change.
    if ($Version -cmatch '^[vV]')
    {
        Stop-Release 'the version is written without a v in front, because the tag adds one: 1.0.0-beta.0 is tagged v1.0.0-beta.0'
    }

    if ($Version -match '^betarelease')
    {
        Stop-Release 'betarelease and a day is the old way of naming a beta, which allowed only one a day. A beta is now its version, -beta. and the next number, such as 1.0.0-beta.0'
    }

    if ($Version -cmatch '^[0-9]+\.[0-9]+\.[0-9]+(-beta\.[0-9]+)?\z')
    {
        if ($Version -cmatch '(^|\.)0[0-9]')
        {
            Stop-Release 'a number in the version starts with 0, such as 01 or beta.01. Each is written with no 0 in front, so that one version or beta cannot be written two ways and counted twice'
        }

        Stop-Release "a number in the version is too long: each of the three has at most 6 digits, and a beta's number at most 4, so beta.9999 is a version's last"
    }

    if ($Version -cmatch '^[0-9]+\.[0-9]+\.[0-9]+-beta\.\z')
    {
        Stop-Release "the beta's number is missing after -beta.: a version's first beta is -beta.0"
    }

    Stop-Release 'the version has to be three plain numbers, such as 1.0.0, or a beta of a version: its numbers, -beta. and the beta''s number, such as 1.0.0-beta.0'
}

# The version the beta leads up to, and the beta's number. The form above has already made sure of what's on each side of the first -.
$release = if ($isBeta) { $Version.Substring(0, $Version.IndexOf('-')) } else { $Version }
$betaNumber = if ($isBeta) { [int]$Version.Substring($Version.LastIndexOf('.') + 1) } else { -1 }

# Its tag, and whether it's a pre-release: see THE TAG at the top. They're only handed on at the end, once everything has passed.
$tag = "v$Version"

# --- Has this tag been used, and is a beta's number the next one? $names is every tag and release there is, with or without refs/tags/ in
# front. Only this version's betas count towards a beta's number, and they're counted as numbers, so beta.10 comes after beta.9.
function Stop-IfTheNumberIsTaken([string[]]$names)
{
    $names = @($names | ForEach-Object { $_ -creplace '^refs/tags/', '' })

    if ($names -ccontains $tag)
    {
        Stop-Release "$tag has already been used"
    }

    if (-not $isBeta)
    {
        return
    }

    # A beta comes before the version it leads up to, never after it (PO, for the user, 2026-11-08)
    if ($names -ccontains "v$release")
    {
        $parts = $release.Split('.')
        Stop-Release "v$release is already released, so a beta of it would come after it. The next betas are of a newer version, such as $($parts[0]).$($parts[1]).$([int]$parts[2] + 1)-beta.0"
    }

    $start = "v$release-beta."
    $highest = -1

    foreach ($name in $names)
    {
        if (-not $name.StartsWith($start, [System.StringComparison]::Ordinal))
        {
            continue
        }

        # One that starts like a beta of this version but isn't written the way this script writes them can't be counted. It's refused,
        # rather than guessed at, until someone finds out where it came from.
        $number = $name.Substring($start.Length)

        if ($number -cnotmatch '^(0|[1-9][0-9]{0,3})\z')
        {
            Stop-Release "$name is already there, and it isn't written the way a beta is, so the next beta of $release can't be worked out. Find out where it came from first"
        }

        if ([int]$number -gt $highest)
        {
            $highest = [int]$number
        }
    }

    if ($highest -eq 9999)
    {
        Stop-Release "$release-beta.9999 was the last beta $release can have, so the next beta has to be of a new version"
    }

    if ($betaNumber -lt $highest)
    {
        Stop-Release "a beta's number never goes back: $release-beta.$highest is already there, so the next is $release-beta.$($highest + 1)"
    }

    if ($betaNumber -gt $highest + 1)
    {
        if ($highest -lt 0)
        {
            Stop-Release "a beta's number is never skipped: $release has no beta yet, so its first is $release-beta.0"
        }

        Stop-Release "a beta's number is never skipped: the last beta of $release is $release-beta.$highest, so the next is $release-beta.$($highest + 1)"
    }
}

# --- What's still in the way. Anything at all under the heading counts, however it's written, except the one word that says there's
# nothing. An empty list isn't taken to mean that, because a file cut short by a mistake would look the same.
$blockersFile = Join-Path $Repository 'RELEASE-BLOCKERS.md'

if (-not (Test-Path -LiteralPath $blockersFile))
{
    Stop-Release 'RELEASE-BLOCKERS.md is missing, so nothing says the way is clear'
}

$lines = @(Get-Content -LiteralPath $blockersFile)
$heading = [array]::IndexOf($lines, "## What's in the way")

if ($heading -lt 0)
{
    Stop-Release "RELEASE-BLOCKERS.md has no heading called What's in the way, so it can't be read"
}

$underIt = @($lines | Select-Object -Skip ($heading + 1) | Where-Object { $_.Trim() -ne '' })

if ($underIt.Count -eq 0)
{
    Stop-Release 'RELEASE-BLOCKERS.md has nothing under its heading. If the way really is clear, the line to write is: Nothing.'
}

if (-not ($underIt.Count -eq 1 -and $underIt[0].Trim() -ceq 'Nothing.'))
{
    foreach ($blocker in $underIt)
    {
        Write-Host "In the way: $blocker"
    }

    Stop-Release 'RELEASE-BLOCKERS.md still lists something in the way'
}

# --- Asserts still marked not yet safe. tests/assert_limits_allowed.txt lists every assert on a limit and why it's safe, and a row
# that says NOT SAFE YET is a known hole waiting for its fix. Its header talks about such rows too, so only rows count: lines that
# don't start with #. The match is exact, capitals and all. A missing list is a refusal, because nothing would then say they're safe.
$assertList = Join-Path $Repository 'tests/assert_limits_allowed.txt'

if (-not (Test-Path -LiteralPath $assertList))
{
    Stop-Release 'tests/assert_limits_allowed.txt is missing, so nothing says every assert on a limit is safe'
}

$notYetSafe = @(Get-Content -LiteralPath $assertList | Where-Object { -not $_.TrimStart().StartsWith('#') -and $_.Contains('| NOT SAFE YET') })

if ($notYetSafe.Count -gt 0)
{
    foreach ($row in $notYetSafe)
    {
        Write-Host "Not yet safe: $row"
    }

    Stop-Release 'tests/assert_limits_allowed.txt still marks an assert as not yet safe'
}

# --- The tester notes say what's true. PLAYTEST.md marks a line [PENDING] until what it describes is in, and a release can't ship
# notes that say something will be true later.
$notes = Join-Path $Repository 'PLAYTEST.md'

if (-not (Test-Path -LiteralPath $notes))
{
    Stop-Release 'PLAYTEST.md is missing, and it is what testers read'
}

$pending = @(Get-Content -LiteralPath $notes | Where-Object { $_.Contains('[PENDING') })

if ($pending.Count -gt 0)
{
    foreach ($line in $pending)
    {
        Write-Host "Still pending: $($line.Trim())"
    }

    Stop-Release 'PLAYTEST.md still has a line marked [PENDING]'
}

# --- Shadows & Shaders
$projectFile = Join-Path $Repository 'win32/VS2026/AvP.vcxproj'
$project = if (Test-Path -LiteralPath $projectFile) { Get-Content -LiteralPath $projectFile -Raw } else { '' }

foreach ($file in $shadowsAndShaders)
{
    if (-not (Test-Path -LiteralPath (Join-Path $Repository $file)))
    {
        Stop-Release "Shadows & Shaders isn't in this commit: $file is missing"
    }

    if ($file.EndsWith('.cpp') -and -not $project.Contains(($file -replace '/', '\')))
    {
        Stop-Release "Shadows & Shaders isn't really in this commit: $file is there, but the game's project doesn't build it"
    }
}

if ($WithoutGitHub)
{
    # Only tests/release-test.ps1 gives these: see the top
    if ($ExistingTags)
    {
        Stop-IfTheNumberIsTaken @($ExistingTags -split ' ' | Where-Object { $_ -ne '' })
    }

    Write-Host 'Everything that can be checked without GitHub is in order.'
    exit 0
}

# --- From here on it's the commit, as GitHub knows it. If GitHub can't be asked, that's a refusal, because not knowing isn't passing.
$commit = $env:GITHUB_SHA
$repositoryName = $env:GITHUB_REPOSITORY

if ($env:GITHUB_REF -cne 'refs/heads/avpx_main')
{
    Stop-Release 'a release can only be started from avpx_main'
}

$tip = gh api "repos/$repositoryName/commits/avpx_main" --jq .sha

if ($LASTEXITCODE -ne 0)
{
    Stop-Release 'GitHub would not say what the tip of avpx_main is'
}

if ($tip -cne $commit)
{
    Stop-Release "this commit is not the tip of avpx_main, which is $tip"
}

# filter=latest because a check run again has an old answer and a new one, and only the new one counts. Every answer with that name has
# to be a pass, and there has to be at least one.
$results = @(gh api --paginate "repos/$repositoryName/commits/$commit/check-runs?filter=latest&per_page=100" --jq '.check_runs[] | .name + "=" + .conclusion')

if ($LASTEXITCODE -ne 0)
{
    Stop-Release 'GitHub would not give the check results of this commit'
}

foreach ($check in $requiredChecks)
{
    $answers = @($results | Where-Object { $_.StartsWith("$check=", [System.StringComparison]::Ordinal) })

    if ($answers.Count -eq 0)
    {
        Stop-Release "'$check' never ran on this commit"
    }

    $notPassed = @($answers | Where-Object { $_ -cne "$check=success" })

    if ($notPassed.Count -gt 0)
    {
        Stop-Release "'$check' did not pass on this commit: $($notPassed[0])"
    }
}

# Passing the checks on this commit doesn't cover an alert from an earlier one that's still open. Nothing is released while one is.
$alerts = @(gh api --paginate "repos/$repositoryName/code-scanning/alerts?state=open&ref=refs/heads/avpx_main&per_page=100" --jq '.[].number')

if ($LASTEXITCODE -ne 0)
{
    Stop-Release 'GitHub would not say whether any code scanning alert is open'
}

if ($alerts.Count -gt 0)
{
    Stop-Release "$($alerts.Count) code scanning alerts are open"
}

# Every tag starting v and this version, which is the version's own and all its betas', and every release. This job can't see a release
# that's still a draft, and a draft has no tag until it's published: release.yml's last job, which can, refuses a tag a draft already has.
$tags = @(gh api --paginate "repos/$repositoryName/git/matching-refs/tags/v$release" --jq '.[].ref')

if ($LASTEXITCODE -ne 0)
{
    Stop-Release 'GitHub would not say which tags there are'
}

$releases = @(gh api --paginate "repos/$repositoryName/releases?per_page=100" --jq '.[].tag_name')

if ($LASTEXITCODE -ne 0)
{
    Stop-Release 'GitHub would not give the list of releases'
}

Stop-IfTheNumberIsTaken ($tags + $releases)

# For release.yml's last job: the tag, and whether it's a pre-release. Written without a byte-order mark, which GitHub would read as part of
# the first name, whichever PowerShell runs this.
if ($env:GITHUB_OUTPUT)
{
    $noMark = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::AppendAllText($env:GITHUB_OUTPUT, "tag=$tag`nprerelease=$(if ($isBeta) { 'true' } else { 'false' })`n", $noMark)
}

Write-Host "$commit may be released as $tag$(if ($isBeta) { ', a pre-release' })."

# Said outright, because GitHub ends a step by exiting with whatever the last program to run left behind, whatever this script thought.
exit 0
