<#
Known vulnerabilities for the libraries Snyk can't recognise, from each project's own GitHub security advisories, checked by machine.
Written 2026-11-09 by Melanikus for the AvP port (#174).

WHY. Snyk Open Source recognises a library by matching its files with its index of releases. mlkem-native and mldsa-native v2.0.0 aren't
in that index (#174, run 42671277153: not one file matched), so Snyk can't say whether any known vulnerability affects them. The owner
decided, 2026-11-09: for these two, the answer comes from the projects themselves. Both take security reports through GitHub's private
vulnerability reporting (their SECURITY.md), and publish what they fix as security advisories on their own repositories, which GitHub's
API lists. Snyk's library scan (snyk.yml) takes "not recognised" for a library only when this check passed for it in the same run.
Snyk has been asked to add them to its index too; once it has, this check can go.

WHAT IT CHECKS, for each library in .github/advisories-read.json:
1. The version written there is the one the library's port builds (vcpkg-overlay-ports/<port>/vcpkg.json). So moving to a new release
   means reading its advisories again.
2. GitHub's list of the repository's published advisories comes back whole, as a list. No answer, an answer that isn't a list of
   advisories, or a full first page (this reads one page of 100, and won't guess at the rest) fails.
3. Every advisory on it has been read for this version, as below, and none says this version is affected. What each one affects is
   read by machine (its vulnerable version ranges, such as ">= 1.0.0, < 2.0.1"). One whose ranges this check can't read fails, and so
   does one with none at all: a person has to look, and this check has to learn to read it.
4. GitHub's answer is written by another project's people, so it's treated as hostile: an id must be exactly GitHub's form, and
   whatever of it is printed reaches the log as plain printable letters only (Write-Safe), so it can't start a line of its own.

WHAT "READ" MEANS, AND WHERE IT'S WRITTEN DOWN. An advisory is read when a person has opened it on the project's page, compared what it
affects and what fixed it with the version the port builds, and found that version not affected. They write it in
.github/advisories-read.json, under its library's "read", as
  { "id": "GHSA-....-....-....", "version": "2.0.0", "why": "...", "readBy": "...", "readOn": "YYYY-MM-DD" }
the advisory's own id, the version they read it for, why that version isn't affected, who read it, and the day. An entry for another
version doesn't count. An advisory that does affect the version is never written down as read: the fix is to build a fixed release, or to
patch the port, and then the new version's advisories are read again. Nothing here can make an affected version pass: rule 3's machine
reading stops it whatever "read" says.

It runs in snyk.yml's library job on every pull request and every night, and tests/advisories-test.ps1 tries every way it can refuse.
It needs the network (GitHub's API), so it doesn't run on the team's own machines. -Answers is for that test alone: a folder of saved
answers, named <owner>_<repository>.json, in place of GitHub's.
#>
param(
    [string]$Repository = '',
    [string]$Answers = '',
    [string]$Passed = ''
)

$ErrorActionPreference = 'Stop'

# PowerShell 7 (pwsh), as GitHub's runners have: Windows PowerShell 5.1 reads a JSON list as one object, which would read every
# advisory wrongly, so it's refused rather than trusted
if ($PSVersionTable.PSVersion.Major -lt 7)
{
    Write-Host 'REFUSED: this check needs PowerShell 7 (pwsh)'
    exit 1
}

if (-not $Repository)
{
    $Repository = Split-Path -Parent $PSScriptRoot
}

$Repository = (Resolve-Path -LiteralPath $Repository).Path
$script:refusals = 0

# Some of what this prints comes from outside: GitHub's answer, written by another project's people, and an error's own words. Input is
# treated as hostile, so only plain printable letters reach the log, and anything else becomes '?'. That matters most for a line break:
# the text after one would start a line of its own, and a line that starts with '::' is an order to GitHub's runner. A long text is cut
# at 300 letters too, so an answer can't fill the log.
function Write-Safe([string]$text)
{
    $plain = $text -replace '[^\x20-\x7E]', '?'

    if ($plain.Length -gt 300)
    {
        $plain = $plain.Substring(0, 300) + '...'
    }

    Write-Host $plain
}

function Write-Refusal([string]$why)
{
    Write-Safe "REFUSED: $why"
    $script:refusals++
}

# Every pattern here ends with \z, the very end of the text. '$' would do almost the same, but it also lets one line break through at
# the end, and a line break must never pass as part of an id or a version.

# A version as three whole numbers, such as 2.0.0, or $null for anything else. "v2.0.0" and "2.0" are taken as 2.0.0.
function Read-Version([string]$text)
{
    $match = [regex]::Match($text, '^v?(\d{1,9})(?:\.(\d{1,9}))?(?:\.(\d{1,9}))?\z')

    if (-not $match.Success)
    {
        return $null
    }

    $parts = @(1, 2, 3 | ForEach-Object { if ($match.Groups[$_].Success) { [int]$match.Groups[$_].Value } else { 0 } })
    return [version]::new($parts[0], $parts[1], $parts[2])
}

# Whether version is inside one advisory range, such as ">= 1.0.0, < 2.0.1" or "= 2.0.0" (every part must hold). $null if any part of
# it isn't one this can read, so that the advisory goes to a person.
function Test-InRange([version]$version, [string]$range)
{
    $inside = $true

    foreach ($part in ($range -split ','))
    {
        $match = [regex]::Match($part.Trim(), '^(>=|<=|>|<|==|=)?\s*(\S+)\z')

        if (-not $match.Success)
        {
            return $null
        }

        $limit = Read-Version $match.Groups[2].Value

        if ($null -eq $limit)
        {
            return $null
        }

        $holds = switch ($match.Groups[1].Value)
        {
            '>=' { $version -ge $limit }
            '<=' { $version -le $limit }
            '>' { $version -gt $limit }
            '<' { $version -lt $limit }
            default { $version -eq $limit }
        }

        $inside = $inside -and $holds
    }

    return $inside
}

# GitHub's published advisories for a repository, as the raw text of its answer, or $null with a refusal
function Get-AdvisoriesText([string]$repositoryName)
{
    if ($Answers)
    {
        $saved = Join-Path $Answers (($repositoryName -replace '/', '_') + '.json')

        if (-not (Test-Path -LiteralPath $saved))
        {
            Write-Refusal "${repositoryName}: no answer came from GitHub"
            return $null
        }

        return [System.IO.File]::ReadAllText($saved)
    }

    $headers = @{ 'Accept' = 'application/vnd.github+json'; 'X-GitHub-Api-Version' = '2022-11-28' }

    if ($env:GITHUB_TOKEN)
    {
        $headers['Authorization'] = "Bearer $env:GITHUB_TOKEN"
    }

    try
    {
        $answer = Invoke-WebRequest -UseBasicParsing -Headers $headers -TimeoutSec 60 `
            -Uri "https://api.github.com/repos/$repositoryName/security-advisories?state=published&per_page=100"
    }
    catch
    {
        Write-Refusal "${repositoryName}: GitHub didn't give its advisories ($($_.Exception.Message))"
        return $null
    }

    if ([int]$answer.StatusCode -ne 200)
    {
        Write-Refusal "${repositoryName}: GitHub answered $($answer.StatusCode), not 200"
        return $null
    }

    return [string]$answer.Content
}

# ---- What has been read, and for what

$readFile = Join-Path $Repository '.github/advisories-read.json'

if (-not (Test-Path -LiteralPath $readFile))
{
    Write-Refusal '.github/advisories-read.json is missing'
    exit 1
}

try
{
    $read = [System.IO.File]::ReadAllText($readFile) | ConvertFrom-Json
}
catch
{
    Write-Refusal ".github/advisories-read.json isn't JSON ($($_.Exception.Message))"
    exit 1
}

$libraries = @($read.libraries)

if ($libraries.Count -eq 0)
{
    Write-Refusal '.github/advisories-read.json names no library, so this check would check nothing'
    exit 1
}

$passedPorts = @()

foreach ($library in $libraries)
{
    $port = [string]$library.port
    $repositoryName = [string]$library.repository
    $refusalsBefore = $script:refusals

    if ($port -cnotmatch '^[a-z0-9-]{1,64}\z' -or $repositoryName -notmatch '^[A-Za-z0-9-]{1,39}/[A-Za-z0-9._-]{1,100}\z')
    {
        Write-Refusal "a library in .github/advisories-read.json has a port or repository that isn't one ('$port', '$repositoryName')"
        continue
    }

    # 1. The version read for is the one built
    $version = Read-Version ([string]$library.version)
    $portFile = Join-Path $Repository "vcpkg-overlay-ports/$port/vcpkg.json"

    if ($null -eq $version -or -not (Test-Path -LiteralPath $portFile))
    {
        Write-Refusal "${port}: its version '$($library.version)', or its port, isn't there"
        continue
    }

    $built = Read-Version ([string](([System.IO.File]::ReadAllText($portFile) | ConvertFrom-Json).version))

    if ($null -eq $built -or $built -ne $version)
    {
        Write-Refusal "${port}: its port builds $built, but its advisories were read for ${version}, so read them again for the new version"
        continue
    }

    # What was read for this version, by id
    $readIds = @{}

    foreach ($entry in @($library.read))
    {
        if ($null -eq $entry)
        {
            continue
        }

        $entryVersion = Read-Version ([string]$entry.version)

        if ([string]$entry.id -cmatch '^GHSA(-[23456789cfghjmpqrvwx]{4}){3}\z' -and $null -ne $entryVersion -and $entryVersion -eq $version -and
            ([string]$entry.why).Trim() -and ([string]$entry.readBy).Trim() -and [string]$entry.readOn -match '^\d{4}-\d{2}-\d{2}\z')
        {
            $readIds[[string]$entry.id] = $true
        }
    }

    # 2. GitHub's list, whole
    $text = Get-AdvisoriesText $repositoryName

    if ($null -eq $text)
    {
        continue
    }

    if (-not $text.TrimStart().StartsWith('['))
    {
        Write-Refusal "${repositoryName}: GitHub's answer isn't a list"
        continue
    }

    try
    {
        $advisories = @($text | ConvertFrom-Json)
    }
    catch
    {
        Write-Refusal "${repositoryName}: GitHub's answer isn't JSON"
        continue
    }

    if ($advisories.Count -ge 100)
    {
        Write-Refusal "${repositoryName}: $($advisories.Count) advisories came, a whole page, so there may be more: this check must page through them"
        continue
    }

    # 3. Each one read, and none affecting this version
    foreach ($advisory in $advisories)
    {
        $id = [string]$advisory.ghsa_id

        if ($id -cnotmatch '^GHSA(-[23456789cfghjmpqrvwx]{4}){3}\z')
        {
            Write-Refusal "${repositoryName}: an advisory has an id that isn't a GitHub advisory's"
            continue
        }

        $ranges = @($advisory.vulnerabilities | Where-Object { $null -ne $_ } | ForEach-Object { [string]$_.vulnerable_version_range })

        if ($ranges.Count -eq 0)
        {
            Write-Refusal "${repositoryName}: $id names no affected versions, so a person must read it"
        }

        foreach ($range in $ranges)
        {
            $inside = Test-InRange $version $range

            if ($null -eq $inside)
            {
                Write-Refusal "${repositoryName}: $id's affected versions ('$range') aren't ones this check can read: a person must read it"
            }
            elseif ($inside)
            {
                Write-Refusal "${repositoryName}: $id affects $version (its range is '$range'): build a fixed release or patch the port"
            }
        }

        if (-not $readIds.ContainsKey($id))
        {
            Write-Refusal "${repositoryName}: $id is new, and hasn't been read for $version (.github/advisories-read.json says how)"
        }
    }

    if ($script:refusals -eq $refusalsBefore)
    {
        Write-Safe "${port} ${version}: $($advisories.Count) published advisories on $repositoryName, every one read for this version, none affecting it"
        $passedPorts += $port
    }
}

if ($Passed)
{
    [System.IO.File]::WriteAllLines($Passed, [string[]]$passedPorts)
}

if ($script:refusals -gt 0)
{
    exit 1
}

exit 0
