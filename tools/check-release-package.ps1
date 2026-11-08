<#
Says whether a finished package is fit to be a release. release.yml runs this after tools/build-and-package.ps1, on what that made.

The build has already passed if we get here, so this looks for what a build wouldn't notice:
- A file the game only asks for while it's running. The game compiles its shaders from text files every time it starts, using
  D3DCompiler_43.dll, and nothing links to that file, so the build can't tell if it's gone. Without it, on a Windows that doesn't
  already have one, no shader would compile and the game wouldn't start.
- Shaders that aren't the ones in this commit, or that aren't there at all.
- A package from another commit, or with a list of hashes that doesn't match it.
- A program in it that isn't the game.
- One of our game files missing, or not the one this commit lists (package-files/SHA256SUMS.txt).
#>
param(
    [Parameter(Mandatory = $true)][string]$Package,
    [Parameter(Mandatory = $true)][string]$Commit,
    [string]$Repository = ''
)

$ErrorActionPreference = 'Stop'

# Worked out here and not in param(), because older PowerShell doesn't know where the script is until it's finished reading that.
if (-not $Repository)
{
    $Repository = Split-Path -Parent $PSScriptRoot
}

function Stop-Release([string]$why)
{
    Write-Host "REFUSED: $why"
    exit 1
}

foreach ($needed in 'AvPx.exe', 'D3DCompiler_43.dll', 'd3dx9_43.dll', 'License.txt', 'PLAYTEST.md', 'BUILD.txt', 'SHA256SUMS.txt')
{
    if (-not (Test-Path -LiteralPath (Join-Path $Package $needed)))
    {
        Stop-Release "$needed is not in the package"
    }
}

# A build's symbols and map (.pdb, .map) name the place of a crash. They stay with the build, and a player never needs them.
$symbols = @(Get-ChildItem -LiteralPath $Package -Recurse -File | Where-Object { $_.Extension -eq '.pdb' -or $_.Extension -eq '.map' })

if ($symbols.Count -gt 0)
{
    Stop-Release "$($symbols[0].Name) is in the package, and a build's symbols stay with the build"
}

$programs = @(Get-ChildItem -LiteralPath $Package -Recurse -File -Filter '*.exe' | Where-Object { $_.Name -ne 'AvPx.exe' })

if ($programs.Count -gt 0)
{
    Stop-Release "$($programs[0].Name) is in the package, and the only program in it should be the game"
}

if (-not (Get-Content -LiteralPath (Join-Path $Package 'BUILD.txt') -Raw).Contains($Commit))
{
    Stop-Release "BUILD.txt doesn't say this package came from $Commit"
}

# Every shader in this commit, byte for byte. A count of none is a refusal, or a wrong folder would pass.
$shaderFolder = Join-Path $Repository 'shaders'
$shaders = @(if (Test-Path -LiteralPath $shaderFolder) { Get-ChildItem -LiteralPath $shaderFolder -File })

if ($shaders.Count -eq 0)
{
    Stop-Release 'this commit has no shaders'
}

foreach ($shader in $shaders)
{
    $inPackage = Join-Path (Join-Path $Package 'shaders') $shader.Name

    if (-not (Test-Path -LiteralPath $inPackage))
    {
        Stop-Release "the shader $($shader.Name) is not in the package"
    }

    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $inPackage).Hash -cne (Get-FileHash -Algorithm SHA256 -LiteralPath $shader.FullName).Hash)
    {
        Stop-Release "the shader $($shader.Name) in the package isn't the one in this commit"
    }
}

# Our game files: the pictures, models and sounds the game needs that aren't the player's own copy of the game (package-files/README.md).
# package-files/SHA256SUMS.txt in this commit lists each one with its hash, and every one has to be in the package, byte for byte. A
# list with nothing on it is a refusal, or a wrong folder would pass. (Added 2026-11-07.)
$gameFilesList = Join-Path $Repository 'package-files/SHA256SUMS.txt'
$gameFiles = @(if (Test-Path -LiteralPath $gameFilesList) { Get-Content -LiteralPath $gameFilesList | Where-Object { $_ } })

if ($gameFiles.Count -eq 0)
{
    Stop-Release 'this commit lists none of our game files (package-files/SHA256SUMS.txt)'
}

foreach ($line in $gameFiles)
{
    $hash, $name = $line -split '  ', 2

    # A line names a file inside the game folder: no drive, no root, and no way out of it with ..
    if ($hash -cnotmatch '^[0-9a-f]{64}$' -or -not $name -or $name.Contains(':') -or $name.StartsWith('/') -or $name.StartsWith('\') -or ($name -split '[\\/]') -contains '..')
    {
        Stop-Release "package-files/SHA256SUMS.txt has a line that isn't a hash and a file in the game folder: $line"
    }

    $inPackage = Join-Path $Package $name

    if (-not (Test-Path -LiteralPath $inPackage -PathType Leaf))
    {
        Stop-Release "the game file $name is not in the package"
    }

    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $inPackage).Hash.ToLower() -cne $hash)
    {
        Stop-Release "the game file $name in the package isn't the one this commit lists"
    }
}

# The list of hashes is what someone checks their copy against, so every file has to be on it, and every line has to be right.
$packageFolder = (Resolve-Path -LiteralPath $Package).Path.TrimEnd('\', '/')
$listed = @{}

foreach ($line in @(Get-Content -LiteralPath (Join-Path $Package 'SHA256SUMS.txt')))
{
    $hash, $name = $line -split '  ', 2
    $listed[$name] = $hash
}

foreach ($file in @(Get-ChildItem -LiteralPath $Package -Recurse -File))
{
    $name = $file.FullName.Substring($packageFolder.Length + 1) -replace '\\', '/'

    if ($name -ceq 'SHA256SUMS.txt')
    {
        continue
    }

    if (-not $listed.ContainsKey($name))
    {
        Stop-Release "$name is in the package, but not on its list of hashes"
    }

    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash.ToLower() -cne $listed[$name])
    {
        Stop-Release "$name doesn't match its hash on the list"
    }

    $listed.Remove($name)
}

if ($listed.Count -gt 0)
{
    Stop-Release "the list of hashes names a file that isn't in the package: $(@($listed.Keys)[0])"
}

Write-Host 'The package has everything it should, and it matches this commit.'

# Said outright, because GitHub ends a step by exiting with whatever the last program to run left behind, whatever this script thought.
exit 0
