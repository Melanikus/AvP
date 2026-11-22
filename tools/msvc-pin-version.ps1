# msvc-pin-version.ps1: THE ONE COMPILER's version, for the scripts that set Visual Studio up themselves (win32/msvc-pin.props says why there is
# one, and every project imports that file). Written 2026-11-21 by Melanikus (Frost) for the AvP port, Bishop for the owner.
#
# Dot-source it, then call Get-MsvcPinVersion <the repository's root>: it gives the pinned version (as 14.51.36231), read from
# win32/msvc-pin.props, so the version is written in one place only. Get-MsvcPinFolder <Visual Studio's folder> <version> gives that MSVC's
# folder (VC\Tools\MSVC\<version>) and stops the script if it isn't installed: nothing ever falls back to another compiler.

function Get-MsvcPinVersion([string]$repository)
{
    $props = Join-Path $repository 'win32/msvc-pin.props'
    $match = [regex]::Match([System.IO.File]::ReadAllText($props), '<AvpxMsvcVersion>([0-9]+\.[0-9]+\.[0-9]+)</AvpxMsvcVersion>')
    if (-not $match.Success) { throw "no AvpxMsvcVersion of the form 14.51.36231 in $props" }
    return $match.Groups[1].Value
}

function Get-MsvcPinFolder([string]$visualStudio, [string]$version)
{
    $folder = Join-Path $visualStudio "VC\Tools\MSVC\$version"
    if (-not (Test-Path -LiteralPath (Join-Path $folder 'bin\Hostx64\x86\cl.exe') -PathType Leaf))
    {
        throw "MSVC $version, the one this project builds with (win32/msvc-pin.props), is not installed in $visualStudio. tools/install-msvc-pin.ps1 installs it."
    }
    return $folder
}
