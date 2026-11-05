# Builds rimpng. Written 2026-11-05 by Melanikus for the AvP port. README.md, beside this file, says what it's for.
#
# Run it from anywhere, in PowerShell: tools/rimpng/build.ps1
# The program lands in a folder called AvP-test-out\rimpng next to the repository. Not the temp folder: antivirus programs distrust new programs that appear there.
# If anything fails, this stops with a non-zero exit code.

$ErrorActionPreference = 'Stop'
trap { Write-Host "Could not build it: $_"; exit 1 }

$repository = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$out = Join-Path (Split-Path -Parent $repository) 'AvP-test-out\rimpng'
if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force $out | Out-Null

# The compiler isn't on the path until Visual Studio's own setup script has run, so find that script
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio with the C++ tools was not found' }
$setupScript = Join-Path $visualStudio 'Common7\Tools\VsDevCmd.bat'

# 32-bit, with the settings the game's own code expects: /J makes a plain char unsigned, as it is in the game
$batchFile = Join-Path $out 'build.cmd'
@(
    '@echo off',
    "set `"PATH=$(Split-Path -Parent $vswhere);%PATH%`"",
    "call `"$setupScript`" -arch=x86 -host_arch=amd64 -no_logo",
    'if errorlevel 1 exit /b 1',
    "cd /d `"$repository`"",
    "cl /nologo /std:c++17 /EHsc /MD /J /W3 /O2 /DNDEBUG /D_CRT_SECURE_NO_DEPRECATE /I3dc /Itools/rimpng tools/rimpng/rimpng.cpp 3dc/RimLoader.cpp 3dc/FileStream.cpp /Fe:`"$out\rimpng.exe`" /Fo:`"$out\\`"",
    'exit /b %errorlevel%'
) | Set-Content -Encoding Ascii -Path $batchFile

& cmd.exe /c $batchFile
if ($LASTEXITCODE -ne 0) { throw 'The compiler said no' }

Write-Host "Built $out\rimpng.exe"
