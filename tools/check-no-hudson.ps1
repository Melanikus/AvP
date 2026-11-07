<#
Says whether any of Hudson is in this commit. Hudson is the team's private test bot: it plays the game for our tests, and goes on top of a
release's code only to test it. Its code never ships.

Every pull request runs this, in tests.yml, so a hook left behind fails the pull request that brings it. A release runs it again, in release.yml.

What its code looks like, anywhere in the repository:
- a file with "hudson" in its name or its folder's;
- a line that includes one of its files;
- a name from its code: Hudson_, or Hudson and a word and then _, such as HudsonPlay_ or HudsonNav_;
- a line in the game's project that names one of its files.

The word alone isn't enough. "Hudson" is also a name a test gives a player, and a comment can say a profile was called HUDSON.

Left out: git's own folder, and the two files that have to name what's looked for, this one and the test that tries it.
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

function Stop-Check([string]$why)
{
    Write-Host "REFUSED: $why"
    exit 1
}

$namesWhatItLooksFor = @('tools/check-no-hudson.ps1', 'tests/release-test.ps1')
$everyFile = @(Get-ChildItem -LiteralPath $Repository -Recurse -File -Force | Where-Object {
    $path = $_.FullName.Substring($Repository.Length).TrimStart('\', '/') -replace '\\', '/'
    $path -notmatch '^\.git(/|$)' -and $namesWhatItLooksFor -notcontains $path
})

foreach ($file in $everyFile)
{
    $relative = $file.FullName.Substring($Repository.Length).TrimStart('\', '/') -replace '\\', '/'

    # The whole path, so that a file in a folder of Hudson's own is found too
    if ($relative -match 'hudson')
    {
        Stop-Check "Hudson is in this commit: a file named $relative"
    }
}

$codeFiles = @($everyFile | Where-Object { $_.Extension -match '^\.(c|cc|cpp|cxx|h|hpp|inl|vcxproj|filters|args|json|ps1|yml|txt)$' })

foreach ($file in $codeFiles)
{
    $relative = $file.FullName.Substring($Repository.Length).TrimStart('\', '/') -replace '\\', '/'
    $lineNumber = 0

    foreach ($line in [System.IO.File]::ReadLines($file.FullName))
    {
        $lineNumber++

        if ($line -match '#\s*include\s*["<][^">]*hudson')
        {
            Stop-Check "Hudson is in this commit: $relative line $lineNumber includes one of its files"
        }

        if ($line -cmatch '\bHudson([A-Z][A-Za-z]*)?_[A-Za-z]')
        {
            Stop-Check "Hudson is in this commit: $relative line $lineNumber uses its code"
        }

        if ($file.Extension -match '^\.(vcxproj|filters)$' -and $line -match 'hudson')
        {
            Stop-Check "Hudson is in this commit: $relative line $lineNumber, in a project, names one of its files"
        }
    }
}

Write-Host 'None of Hudson is in this commit.'

# Said outright, because GitHub ends a step by exiting with whatever the last program to run left behind, whatever this script thought.
exit 0
