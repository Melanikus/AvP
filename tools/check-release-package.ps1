<#
Says whether a finished package is fit to be a release. release.yml runs this after tools/build-and-package.ps1, on what that made, and
every pull request does too (game-build.yml), so a change to the package is tried before a release depends on it.

The build has already passed if we get here, so this looks for what a build wouldn't notice:
- A file the game only asks for while it's running. The game compiles its shaders from text files every time it starts, using
  D3DCompiler_43.dll, and nothing links to that file, so the build can't tell if it's gone. Without it, on a Windows that doesn't
  already have one, no shader would compile and the game wouldn't start.
- Shaders that aren't the ones in this commit, or that aren't there at all.
- A package from another commit, or with a list of hashes that doesn't match it.
- A program in it that isn't the game.
- One of our game files missing, or not the one this commit lists (package-files/SHA256SUMS.txt).
- A program or library without its licence beside it (licences/README.txt says which is whose).
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

# The package is laid out as the game folder is: the game in bin as AvP_Classic.exe, the name Steam starts, with the libraries it needs
# there too, and the notes and servers.cfg at the top (servers.cfg added 2026-11-10 by Melanikus for the AvP port, #175: the game looks
# for it beside bin)
foreach ($needed in 'bin/AvP_Classic.exe', 'bin/D3DCompiler_43.dll', 'bin/d3dx9_43.dll', 'License.txt', 'PLAYTEST.md', 'servers.cfg', 'BUILD.txt', 'SHA256SUMS.txt')
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

# The only program is the game, at bin/AvP_Classic.exe. One with that name anywhere else, or any other, is refused.
$packageRoot = (Resolve-Path -LiteralPath $Package).Path.TrimEnd('\', '/')
$programs = @(Get-ChildItem -LiteralPath $Package -Recurse -File -Filter '*.exe' | Where-Object { ($_.FullName.Substring($packageRoot.Length + 1) -replace '\\', '/') -cne 'bin/AvP_Classic.exe' })

if ($programs.Count -gt 0)
{
    Stop-Release "$($programs[0].FullName.Substring($packageRoot.Length + 1) -replace '\\', '/') is in the package, and the only program in it should be the game, bin/AvP_Classic.exe"
}

# Nothing else Windows runs may be anywhere in the package but the game's own libraries, directly in bin beside it. The package's top
# is the game folder, the folder the game starts in, and Windows looks for a library there too.
$runnable = '.exe', '.dll', '.com', '.scr', '.bat', '.cmd', '.ps1', '.vbs', '.js', '.msi', '.lnk', '.cpl', '.ocx', '.sys'
$runnableOutsideBin = @(Get-ChildItem -LiteralPath $Package -Recurse -File | Where-Object {
    $relative = $_.FullName.Substring($packageRoot.Length + 1) -replace '\\', '/'
    ($runnable -contains $_.Extension.ToLowerInvariant()) -and $relative -cne 'bin/AvP_Classic.exe' -and $relative -notmatch '^bin/[^/]+\.dll$'
})

if ($runnableOutsideBin.Count -gt 0)
{
    Stop-Release "$($runnableOutsideBin[0].FullName.Substring($packageRoot.Length + 1) -replace '\\', '/') is something Windows runs, and only the game and its own libraries, in bin, may be in the package"
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

$listedGameFiles = @{}

foreach ($line in $gameFiles)
{
    $hash, $name = $line -split '  ', 2

    # A line names a plain file inside the game folder: parts of letters, digits, _ . - and spaces, between /, each starting with a
    # letter, a digit or _. So no drive, no root, no way out with .., and nothing PowerShell would read as a wildcard. And it's one of the
    # game files' own kinds (.rif, .rim, .ffl) in one of their own folders (avp_huds, graphics, MelPyth), so it can't stand for the notes,
    # a shader or the program.
    if ($hash -cnotmatch '^[0-9a-f]{64}$' -or "$name" -cnotmatch '^[A-Za-z0-9_][A-Za-z0-9_ .-]*(/[A-Za-z0-9_][A-Za-z0-9_ .-]*)*$' -or "$name" -notmatch '(?i)^(avp_huds|graphics|MelPyth)/.+\.(rif|rim|ffl)$')
    {
        Stop-Release "package-files/SHA256SUMS.txt has a line that isn't a hash and a file in the game folder: $line"
    }

    # The commit's own copy has to be the file the list names, so the list and the folder can't drift apart
    $inCommit = Join-Path (Join-Path $Repository 'package-files') $name

    if (-not (Test-Path -LiteralPath $inCommit -PathType Leaf) -or (Get-FileHash -Algorithm SHA256 -LiteralPath $inCommit).Hash.ToLower() -cne $hash)
    {
        Stop-Release "package-files/$name in this commit isn't the file its list names"
    }

    $listedGameFiles[$name.ToLower()] = $true
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

# Nothing else is in the commit's folder, apart from its notes, and nothing in it is a link, which would be followed to wherever it
# points
$gameFolder = Join-Path $Repository 'package-files'
$gameFolderPath = (Resolve-Path -LiteralPath $gameFolder).Path.TrimEnd('\', '/')

foreach ($item in @(Get-ChildItem -LiteralPath $gameFolder -Recurse -Force))
{
    $name = $item.FullName.Substring($gameFolderPath.Length + 1) -replace '\\', '/'

    if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint)
    {
        Stop-Release "package-files/$name is a link, and every game file has to be a plain file"
    }

    if (-not $item.PSIsContainer -and $name -cnotin 'README.md', 'SHA256SUMS.txt', '.gitattributes' -and -not $listedGameFiles.ContainsKey($name.ToLower()))
    {
        Stop-Release "package-files/$name isn't on package-files/SHA256SUMS.txt"
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

# Each program and library in bin comes with its licence. licences/README.txt names each one once, at the start of a line, with the
# licences in licences that go with it, and each of those has to be there and not empty. So no library can go out without its licence.
# (LICENCE-NOTICES-1, added 2026-11-08.)
$licenceList = Join-Path $Package 'licences/README.txt'

if (-not (Test-Path -LiteralPath $licenceList -PathType Leaf))
{
    Stop-Release 'licences/README.txt is not in the package'
}

$licenceLines = @(Get-Content -LiteralPath $licenceList)

foreach ($binary in @(Get-ChildItem -LiteralPath (Join-Path $Package 'bin') -File | Where-Object { $_.Extension -in '.exe', '.dll' } | Sort-Object Name))
{
    $line = @($licenceLines | Where-Object { $_.StartsWith("bin/$($binary.Name): ", [System.StringComparison]::OrdinalIgnoreCase) })

    if ($line.Count -eq 0)
    {
        Stop-Release "bin/$($binary.Name) isn't named in licences/README.txt, so its licence wouldn't go with it"
    }

    if ($line.Count -gt 1)
    {
        Stop-Release "bin/$($binary.Name) is named more than once in licences/README.txt"
    }

    $licenceFiles = @([regex]::Matches($line[0], 'licences/[A-Za-z0-9_-][A-Za-z0-9_.-]*\.txt') | ForEach-Object { $_.Value })

    if ($licenceFiles.Count -eq 0)
    {
        Stop-Release "licences/README.txt names no licence for bin/$($binary.Name)"
    }

    foreach ($licence in $licenceFiles)
    {
        $licencePath = Join-Path $Package $licence

        if (-not (Test-Path -LiteralPath $licencePath -PathType Leaf) -or (Get-Item -LiteralPath $licencePath).Length -eq 0)
        {
            Stop-Release "$licence, the licence for bin/$($binary.Name), is missing or empty"
        }
    }

    # A source the line says is in the package has to be there. OpenAL Soft's isn't all in this repository, so its line has to name one.
    $sources = @([regex]::Matches($line[0], 'licences/sources/[A-Za-z0-9_-][A-Za-z0-9_./-]*[A-Za-z0-9_-]') | ForEach-Object { $_.Value })

    if ($binary.Name -ieq 'OpenAL32.dll' -and $sources.Count -eq 0)
    {
        Stop-Release "licences/README.txt names no source in licences/sources for bin/OpenAL32.dll, and its source isn't all in this repository"
    }

    foreach ($source in $sources)
    {
        $sourcePath = Join-Path $Package $source
        $there = -not $source.Contains('..') -and (((Test-Path -LiteralPath $sourcePath -PathType Leaf) -and (Get-Item -LiteralPath $sourcePath).Length -gt 0) -or
            ((Test-Path -LiteralPath $sourcePath -PathType Container) -and @(Get-ChildItem -LiteralPath $sourcePath -Recurse -File).Count -gt 0))

        if (-not $there)
        {
            Stop-Release "$source, the source licences/README.txt names for bin/$($binary.Name), is missing or empty"
        }
    }
}

Write-Host 'The package has everything it should, and it matches this commit.'

# Said outright, because GitHub ends a step by exiting with whatever the last program to run left behind, whatever this script thought.
exit 0
