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
# The .args file comes last on the compiler's line, so it can end with /link and settings for the linker.
# Run it from anywhere, in PowerShell:  tests/build.ps1
# To build only one test:  tests/build.ps1 -Only name
# The programs land in $env:RUNNER_TEMP\avp-tests. Outside the automatic checks, they land in a folder called
# AvP-test-out next to the repository. Not the temp folder: antivirus programs distrust new programs that appear there.
#
# If anything fails, this script stops with a non-zero exit code, and the automatic check fails with it.
#
# Modified 2026-11-08 by Melanikus for the AvP port:
#   every test asks for the C runtime as DLLs (/MD), the same as the game, and one whose settings say anything else is refused, with the
#   reason (STATIC-UNDO-1). For a day the tests were built with the runtime inside them (/MT, STATIC-1). That's undone: Bitdefender flagged
#   the game built that way (#111), and the user ruled static linking out (#125)

param([string]$Only = '')

$ErrorActionPreference = 'Stop'

# Any error at all that stops this script ends up here, so the exit code always tells the truth
trap { Write-Host "Could not build the tests: $_"; exit 1 }

$repository = Split-Path -Parent $PSScriptRoot
$outputDir  = if ($env:RUNNER_TEMP) { Join-Path $env:RUNNER_TEMP 'avp-tests' } else { Join-Path (Split-Path -Parent $repository) 'AvP-test-out' }

# Start from an empty folder, so nothing old can be mistaken for a new build
if (Test-Path $outputDir) { Remove-Item -Recurse -Force $outputDir }
New-Item -ItemType Directory -Force $outputDir | Out-Null

# The compiler isn't on the path until Visual Studio has been set up, so find Visual Studio
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { Write-Error 'Visual Studio with the C++ tools was not found' }

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
    $started = @{ FilePath = $found; WorkingDirectory = $folder; NoNewWindow = $true; PassThru = $true; RedirectStandardOutput = $saysTo; RedirectStandardError = "$saysTo.errors" }

    # Start-Process refuses an empty list of arguments, and a program run on its own has none
    if ($arguments) { $started.ArgumentList = $arguments }

    $process = Start-Process @started
    $handle = $process.Handle    # without asking for this, Windows PowerShell can't read the exit code afterwards
    $process.WaitForExit()
    return $process.ExitCode
}

$tests = @(Get-ChildItem -Path (Join-Path $PSScriptRoot '*.args') | Where-Object { -not $Only -or $_.BaseName -ceq $Only })
if ($tests.Count -eq 0) { Write-Error 'No tests found to build' }

foreach ($test in $tests)
{
    # STATIC-UNDO-1: a test is built the way the game is, with the C runtime as DLLs (/MD), so it runs the runtime the game runs. Not with the
    # runtime inside it (/MT), and not with the debug runtime (/MDd, /MTd), which no player has. The game's own settings are in
    # win32/VS2026/AvP.vcxproj. It's checked before anything is built, so the reason is plain, and a test copied from an older branch, still
    # on /MT, is caught here instead of passing on a runtime the game doesn't use.
    $settings = [System.IO.File]::ReadAllText($test.FullName)

    if ($settings -cmatch '(^|\s)[/-](MT|MTd|MDd)(?=\s|$)')
    {
        Write-Host "$($test.Name) asks for the C runtime as /$($Matches[2]). Every test is built with /MD, the C runtime as DLLs, the same as the game (STATIC-UNDO-1)"
        exit 1
    }

    if ($settings -cnotmatch '(^|\s)[/-]MD(?=\s|$)')
    {
        Write-Host "$($test.Name) doesn't ask for /MD. Every test is built with /MD, the C runtime as DLLs, the same as the game (STATIC-UNDO-1)"
        exit 1
    }

    $objectDir = Join-Path $outputDir ($test.BaseName + '_objects')
    New-Item -ItemType Directory -Force $objectDir | Out-Null

    $saysTo = Join-Path $outputDir ($test.BaseName + '_build.txt')
    $exitCode = Invoke-Tool 'cl.exe' "/nologo `"/Fe:$outputDir\$($test.BaseName).exe`" `"/Fo:$objectDir/`" `"@$($test.FullName)`"" $repository $saysTo

    # What the compiler said goes on the screen too, so that the automatic check's log has it
    Get-Content -LiteralPath $saysTo, "$saysTo.errors" -ErrorAction SilentlyContinue

    if ($exitCode -ne 0)
    {
        Write-Host "Could not build $($test.Name)"
        exit 1
    }
}

Write-Host "Built $($tests.Count) test(s) into $outputDir"
exit 0
