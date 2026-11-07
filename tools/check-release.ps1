<#
Says whether this commit may be released. release.yml runs this before it builds anything, and a refusal stops the whole release.

It refuses unless all of these are true:
- The version is three plain numbers. It comes from a person typing it into a box, so nothing about it is believed.
- RELEASE-BLOCKERS.md says nothing is in the way.
- Shadows & Shaders is really here, so that an old commit can't be released by mistake.
- It's running from avpx_main, on the very tip of it.
- Every required check passed on this exact commit. A check that was skipped, or never ran, counts as a failure.
- No code scanning alert is open.
- The version hasn't been used.

-WithoutGitHub leaves out everything that has to ask GitHub, for tests/release-test.ps1, which runs with no network. release.yml
never uses it, and that test fails if it ever does.
#>
param(
    [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Version,
    [string]$Repository = '',
    [switch]$WithoutGitHub
)

$ErrorActionPreference = 'Stop'

# Worked out here and not in param(), because older PowerShell doesn't know where the script is until it's finished reading that.
if (-not $Repository)
{
    $Repository = Split-Path -Parent $PSScriptRoot
}

# The checks a merge has to pass, the same names as in branch protection and in build.yml. Add a new required check here as well.
$requiredChecks = 'Snyk Code (SAST)', 'Snyk Open Source (SCA)', 'Snyk on the vcpkg libraries', 'Unit tests', 'Build the game', 'Release checks on Ubuntu'

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

# --- The version. \z and not $, because $ would let a line break through at the end. [0-9] and not \d, because \d takes digits from
# every alphabet there is. It becomes a tag and part of a file name, so anything else in it could mean something to whatever reads those.
if ($Version -cnotmatch '^[0-9]{1,6}\.[0-9]{1,6}\.[0-9]{1,6}\z')
{
    Stop-Release 'the version has to be three plain numbers, such as 1.0.0'
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

$tags = @(gh api "repos/$repositoryName/git/matching-refs/tags/v$Version" --jq '.[].ref')

if ($LASTEXITCODE -ne 0)
{
    Stop-Release 'GitHub would not say which tags there are'
}

if ($tags -ccontains "refs/tags/v$Version")
{
    Stop-Release "v$Version has already been used"
}

Write-Host "$commit may be released as v$Version."

# Said outright, because GitHub ends a step by exiting with whatever the last program to run left behind, whatever this script thought.
exit 0
