# msvc-pin-version.ps1: THE ONE COMPILER's version, for the scripts that set Visual Studio up themselves (win32/msvc-pin.props says why there is
# one, and every project imports that file). Written 2026-11-21 by Melanikus (Frost) for the AvP port, Bishop for the owner.
#
# Dot-source it, then call Get-MsvcPinVersion <the repository's root>: it gives the pinned version (as 14.51.36231), read from
# win32/msvc-pin.props, so the version is written in one place only. Get-MsvcPinBuild <the repository's root> gives the compiler's pinned build
# (as 36263, AvpxMsvcBuild there: Microsoft re-services a toolset under the same folder name, so the folder alone doesn't pin the compiler).
# Get-MsvcPinFolder <Visual Studio's folder> <version> gives that MSVC's folder (VC\Tools\MSVC\<version>) and stops the script if it isn't
# installed, or if its compiler or linker is another build than the pinned one: nothing ever falls back to another compiler.

function Get-MsvcPinVersion([string]$repository)
{
    $props = Join-Path $repository 'win32/msvc-pin.props'
    $match = [regex]::Match([System.IO.File]::ReadAllText($props), '<AvpxMsvcVersion>([0-9]+\.[0-9]+\.[0-9]+)</AvpxMsvcVersion>')
    if (-not $match.Success) { throw "no AvpxMsvcVersion of the form 14.51.36231 in $props" }
    return $match.Groups[1].Value
}

function Get-MsvcPinBuild([string]$repository)
{
    $props = Join-Path $repository 'win32/msvc-pin.props'
    $match = [regex]::Match([System.IO.File]::ReadAllText($props), '<AvpxMsvcBuild>([0-9]+)</AvpxMsvcBuild>')
    if (-not $match.Success) { throw "no AvpxMsvcBuild of the form 36263 in $props" }
    return $match.Groups[1].Value
}

# The build of one program of a toolset: the third number of its file version (cl.exe 19.51.36263.0, link.exe 14.51.36263.0), as Windows reads it
function Get-MsvcToolBuild([string]$program)
{
    return [string](Get-Item -LiteralPath $program).VersionInfo.FileBuildPart
}

function Get-MsvcPinFolder([string]$visualStudio, [string]$version)
{
    $folder = Join-Path $visualStudio "VC\Tools\MSVC\$version"
    $compiler = Join-Path $folder 'bin\Hostx64\x86\cl.exe'
    $linker = Join-Path $folder 'bin\Hostx64\x86\link.exe'
    if (-not (Test-Path -LiteralPath $compiler -PathType Leaf) -or -not (Test-Path -LiteralPath $linker -PathType Leaf))
    {
        throw "MSVC $version, the one this project builds with (win32/msvc-pin.props), is not installed in $visualStudio. tools/install-msvc-pin.ps1 installs it."
    }
    # The pinned build is read from the msvc-pin.props of the repository this file is in (it is that repository's tools folder), so no caller
    # passes it and none can pass another
    $build = Get-MsvcPinBuild (Split-Path -Parent $PSScriptRoot)
    $have = "compiler build $(Get-MsvcToolBuild $compiler), linker build $(Get-MsvcToolBuild $linker)"
    if ((Get-MsvcToolBuild $compiler) -ne $build -or (Get-MsvcToolBuild $linker) -ne $build)
    {
        throw "MSVC $version in $visualStudio has $have, and this project builds with build $build (win32/msvc-pin.props): Microsoft re-serviced the toolset under the same folder. Update Visual Studio's MSVC $version build tools to build $build, or move the pin on purpose, with a scan."
    }
    return $folder
}
