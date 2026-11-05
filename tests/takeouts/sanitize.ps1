# Builds a test with the address sanitizer and runs it. Written 2026-11-05 by Melanikus for the AvP port.
#
# The sanitizer watches every read and write, and stops the program at the first one outside what it was given. A test can only see what it looks for, and this sees the rest. It gives back a
# non-zero result if the sanitizer stopped the test, or the test failed.
#
# Run it from anywhere, in PowerShell:
#   tests/takeouts/sanitize.ps1 <test>              for example: tests/takeouts/sanitize.ps1 relief_map_file_test
#
# The program lands in a folder called AvP-test-out next to the repository, or under $env:RUNNER_TEMP in the automatic checks.

param([Parameter(Mandatory = $true)][string]$Test)

$ErrorActionPreference = 'Stop'
trap { Write-Host "Could not run the sanitizer: $_"; exit 2 }

$repository = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$base = if ($env:RUNNER_TEMP) { Join-Path $env:RUNNER_TEMP 'avp-sanitize' } else { Join-Path (Split-Path -Parent $repository) 'AvP-test-out\sanitize' }
$out = Join-Path $base $Test
if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force $out | Out-Null

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio with the C++ tools was not found' }
$setupScript = Join-Path $visualStudio 'Common7\Tools\VsDevCmd.bat'

$batchFile = Join-Path $out 'run.cmd'
@(
    '@echo off',
    "set `"PATH=$(Split-Path -Parent $vswhere);%PATH%`"",
    "call `"$setupScript`" -arch=x86 -host_arch=amd64 -no_logo",
    'if errorlevel 1 exit /b 1',
    "cd /d `"$repository`"",
    "cl /nologo @tests\$Test.args /Zi /fsanitize=address /Fe:`"$out\t.exe`" /Fo:`"$out\\`" /Fd:`"$out\\`" > `"$out\build.txt`" 2>&1",
    'if errorlevel 1 exit /b 3',
    # the sanitizer's own library has to be beside the program, or on the path
    "for /f `"delims=`" %%D in ('where clang_rt.asan_dynamic-i386.dll') do copy /y `"%%D`" `"$out`" >nul",
    "`"$out\t.exe`" > `"$out\said.txt`" 2>&1",
    'exit /b %errorlevel%'
) | Set-Content -Encoding Ascii -Path $batchFile

& cmd.exe /c $batchFile
$result = $LASTEXITCODE

if ($result -eq 3) {
    Write-Host "$Test didn't build:"
    Get-Content (Join-Path $out 'build.txt') -TotalCount 10
    exit 1
}

$stopped = @(Select-String -Path (Join-Path $out 'said.txt') -Pattern 'ERROR: AddressSanitizer' -SimpleMatch)
if ($stopped.Count) {
    Write-Host "The sanitizer stopped $Test`: $($stopped[0].Line)"
    Write-Host "All of it is in $out\said.txt"
    exit 1
}

if ($result -ne 0) {
    Write-Host "$Test failed under the sanitizer, with exit code $result. All of it is in $out\said.txt"
    exit 1
}

Write-Host "$Test passes under the address sanitizer"
exit 0
