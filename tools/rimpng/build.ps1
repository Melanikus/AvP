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

# The compiler isn't on the path until Visual Studio has been set up, so find Visual Studio
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

# 32-bit, with the settings the game's own code expects: /J makes a plain char unsigned, as it is in the game
$saysTo = Join-Path $out 'build.txt'
$exitCode = Invoke-Tool 'cl.exe' "/nologo /std:c++17 /EHsc /MD /J /W3 /O2 /DNDEBUG /D_CRT_SECURE_NO_DEPRECATE /I3dc /Itools/rimpng tools/rimpng/rimpng.cpp 3dc/RimLoader.cpp 3dc/FileStream.cpp /Fe:`"$out\rimpng.exe`" /Fo:`"$out\\`"" $repository $saysTo
Get-Content -LiteralPath $saysTo, "$saysTo.errors" -ErrorAction SilentlyContinue
if ($exitCode -ne 0) { throw 'The compiler said no' }

Write-Host "Built $out\rimpng.exe"
