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

# Visual Studio's own PowerShell module sets the compiler up in this window, the way its setup batch file does in a command window, so nothing has to be written to disk to run it.
# The antivirus distrusts a script that writes another to run, and took one of ours. The setup looks for vswhere on the path, so that goes on first. -arch=x86 builds 32-bit programs,
# the same as the game.
$env:PATH = "$(Split-Path -Parent $vswhere);$env:PATH"
Import-Module (Join-Path $visualStudio 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $visualStudio -SkipAutomaticLocation -DevCmdArguments '-arch=x86 -host_arch=amd64 -no_logo' | Out-Null

# Runs one of the compiler's programs in a folder and gives back its exit code. What it says goes in a file, and anything it says as an error in that file's .errors. It's started this way,
# and not by its name on its own, because Windows PowerShell takes anything a program writes as an error as the script failing.
function Invoke-Tool([string]$program, [string]$arguments, [string]$folder, [string]$saysTo)
{
    $found = (Get-Command $program -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
    $process = Start-Process -FilePath $found -ArgumentList $arguments -WorkingDirectory $folder -NoNewWindow -PassThru -RedirectStandardOutput $saysTo -RedirectStandardError "$saysTo.errors"
    $handle = $process.Handle    # without asking for this, Windows PowerShell can't read the exit code afterwards
    $process.WaitForExit()
    return $process.ExitCode
}

# The address sanitizer's own library has to be beside the program, and it's on the path once Visual Studio is set up
function Copy-SanitizerLibrary([string]$to)
{
    $library = @($env:PATH -split ';' | Where-Object { $_ } | ForEach-Object { Join-Path $_ 'clang_rt.asan_dynamic-i386.dll' } | Where-Object { Test-Path -LiteralPath $_ }) | Select-Object -First 1

    if (-not $library) { throw "The address sanitizer's library wasn't found on the path" }

    Copy-Item -LiteralPath $library -Destination $to -Force
}

$built = Invoke-Tool 'cl.exe' "/nologo @tests\$Test.args /Zi /fsanitize=address `"/Fe:$out\t.exe`" `"/Fo:$out/`" `"/Fd:$out/`"" $repository (Join-Path $out 'build.txt')

if ($built -ne 0) {
    Write-Host "$Test didn't build:"
    Get-Content (Join-Path $out 'build.txt') -TotalCount 10
    exit 1
}

Copy-SanitizerLibrary $out

# The sanitizer writes what it found to the program's error stream, so both streams are read
$result = Invoke-Tool (Join-Path $out 't.exe') '' $repository (Join-Path $out 'said.txt')
Get-Content -LiteralPath (Join-Path $out 'said.txt.errors') | Add-Content -LiteralPath (Join-Path $out 'said.txt')

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
