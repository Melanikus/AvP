# Builds every test in this folder. Written 2026-11-03 by Melanikus for the AvP port.
#
# This script only builds. Each test becomes a small program in the output folder; running them is a separate step.
# Two automatic checks use it: the one that runs the tests, and CodeQL, which reads code by watching it being compiled.
# That's why it compiles from scratch every time and leaves nothing behind in the repository.
#
# ADDING A TEST: a test is a pair of files with the same name. name.cpp is the test, and name.args is a list of what the
# compiler needs, one or more to a line: settings, folders to search and every source file, the game's real ones
# included. Paths start from the top of the repository. Copy tests/rebeccas_test.args, whose settings are the game's own.
# Nothing else needs changing. A test that needs stand-ins for parts of the game keeps them in tests/name_stubs/, and
# lists that folder in its .args with /Itests/name_stubs, ahead of the game's folders.
#
# Run it from anywhere, in PowerShell:  tests/build.ps1
# To build only one test:  tests/build.ps1 -Only name
# The programs land in $env:RUNNER_TEMP\avp-tests. Outside the automatic checks, they land in a folder called
# AvP-test-out next to the repository. Not the temp folder: antivirus programs distrust new programs that appear there.
#
# If anything fails, this script stops with a non-zero exit code, and the automatic check fails with it.

param([string]$Only = '')

$ErrorActionPreference = 'Stop'

# Any error at all that stops this script ends up here, so the exit code always tells the truth
trap { Write-Host "Could not build the tests: $_"; exit 1 }

$repository = Split-Path -Parent $PSScriptRoot
$outputDir  = if ($env:RUNNER_TEMP) { Join-Path $env:RUNNER_TEMP 'avp-tests' } else { Join-Path (Split-Path -Parent $repository) 'AvP-test-out' }

# Start from an empty folder, so nothing old can be mistaken for a new build
if (Test-Path $outputDir) { Remove-Item -Recurse -Force $outputDir }
New-Item -ItemType Directory -Force $outputDir | Out-Null

# The compiler isn't on the path until Visual Studio's own setup script has run, so find that script
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { Write-Error 'Visual Studio with the C++ tools was not found' }
$setupScript = Join-Path $visualStudio 'Common7\Tools\VsDevCmd.bat'

$tests = @(Get-ChildItem -Path (Join-Path $PSScriptRoot '*.args') | Where-Object { -not $Only -or $_.BaseName -ceq $Only })
if ($tests.Count -eq 0) { Write-Error 'No tests found to build' }

foreach ($test in $tests)
{
    $objectDir = Join-Path $outputDir ($test.BaseName + '_objects')
    New-Item -ItemType Directory -Force $objectDir | Out-Null

    # The setup script's settings only last while its own command window is open, so one small batch file does all the
    # steps. -arch=x86 builds 32-bit programs, the same as the game. The setup script looks for vswhere on the path
    # itself, and complains if it isn't there, so it goes on the path.
    $batchFile = Join-Path $outputDir ($test.BaseName + '.cmd')
    @(
        '@echo off',
        "set `"PATH=$(Split-Path -Parent $vswhere);%PATH%`"",
        "call `"$setupScript`" -arch=x86 -host_arch=amd64 -no_logo",
        'if errorlevel 1 exit /b 1',
        "cd /d `"$repository`"",
        "cl /nologo @`"$($test.FullName)`" /Fe:`"$outputDir\$($test.BaseName).exe`" /Fo:`"$objectDir\\`"",
        'exit /b %errorlevel%'
    ) | Set-Content -Encoding Ascii -Path $batchFile

    & cmd.exe /c $batchFile

    if ($LASTEXITCODE -ne 0)
    {
        Write-Host "Could not build $($test.Name)"
        exit 1
    }
}

Write-Host "Built $($tests.Count) test(s) into $outputDir"
exit 0
