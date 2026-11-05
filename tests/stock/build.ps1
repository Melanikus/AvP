# Builds the two programs that read the game's own files. Written 2026-11-05 by Melanikus for the AvP port. README.md, beside this file, says what they're for.
#
# Run it from anywhere, in PowerShell: tests/stock/build.ps1
# The programs land in a folder called AvP-test-out\stock next to the repository. Not the temp folder: antivirus programs distrust new programs that appear there.

$ErrorActionPreference = 'Stop'
trap { Write-Host "Could not build them: $_"; exit 1 }

$repository = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$out = Join-Path (Split-Path -Parent $repository) 'AvP-test-out\stock'
if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force "$out\bink", "$out\smacker", "$out\pictures" | Out-Null

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio with the C++ tools was not found' }

$flags = '/nologo /std:c++17 /EHsc /MD /W1 /Zi /fsanitize=address /DNDEBUG /D_CRT_SECURE_NO_DEPRECATE /D_CRT_SECURE_NO_WARNINGS'
$bink = 'dependencies\libbinkdec'
$smacker = 'dependencies\libsmackerdec'

$batchFile = Join-Path $out 'build.cmd'
@(
    '@echo off',
    "set `"PATH=$(Split-Path -Parent $vswhere);%PATH%`"",
    "call `"$(Join-Path $visualStudio 'Common7\Tools\VsDevCmd.bat')`" -arch=x86 -host_arch=amd64 -no_logo",
    'if errorlevel 1 exit /b 1',
    "cd /d `"$repository`"",
    "cl /c $flags /I$bink\include /I$bink\src /Fo`"$out\bink\\`" /Fd`"$out\bink\vc.pdb`" $bink\src\*.cpp",
    'if errorlevel 1 exit /b 1',
    "cl /nologo /c /MD /W1 /Zi /fsanitize=address /D_CRT_SECURE_NO_WARNINGS /DNDEBUG /I$bink\include /I$bink\src /Fo`"$out\bink\\`" /Fd`"$out\bink\vc.pdb`" $bink\src\*.c",
    'if errorlevel 1 exit /b 1',
    "cl /c $flags /I$smacker\include /I$smacker\src /Fo`"$out\smacker\\`" /Fd`"$out\smacker\vc.pdb`" $smacker\src\*.cpp",
    'if errorlevel 1 exit /b 1',
    "cl /c $flags /I$bink\include /I$smacker\include /Fo`"$out\\`" /Fd`"$out\main.pdb`" tests\stock\film_close_check.cpp",
    'if errorlevel 1 exit /b 1',
    "link /nologo /DEBUG /INCREMENTAL:NO /OUT:`"$out\film_close_check.exe`" `"$out\film_close_check.obj`" `"$out\bink\*.obj`" `"$out\smacker\*.obj`"",
    'if errorlevel 1 exit /b 1',
    "for /f `"delims=`" %%D in ('where clang_rt.asan_dynamic-i386.dll') do copy /y `"%%D`" `"$out`" >nul",
    "cl /nologo /std:c++17 /EHsc /MD /J /W3 /DNDEBUG /D_CRT_SECURE_NO_DEPRECATE /I3dc tests\stock\rim_fingerprints.cpp 3dc\RimLoader.cpp 3dc\FileStream.cpp /Fe:`"$out\rim_fingerprints.exe`" /Fo:`"$out\pictures\\`"",
    'exit /b %errorlevel%'
) | Set-Content -Encoding Ascii -Path $batchFile

& cmd.exe /c $batchFile
if ($LASTEXITCODE -ne 0) { Write-Host 'Could not build them'; exit 1 }
Write-Host "Built into $out"
exit 0
