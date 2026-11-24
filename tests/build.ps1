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
# To build only part 0 of 4 (counted from 0), as the Unit tests check does:  tests/build.ps1 -Part 0 -Parts 4
# The programs land in $env:RUNNER_TEMP\avp-tests. Outside the automatic checks, they land in AvP-test-out\tests\<this copy's folder name>,
# next to the repository: a folder of this copy's own, so a build here never empties another copy's (modified 2026-11-21, below). Not the
# temp folder: antivirus programs distrust new programs that appear there.
#
# If anything fails, this script stops with a non-zero exit code, and the automatic check fails with it.
#
# Modified 2026-11-08 by Melanikus for the AvP port:
#   every test asks for the C runtime as DLLs (/MD), the same as the game, and one whose settings say anything else is refused, with the
#   reason (STATIC-UNDO-1). For a day the tests were built with the runtime inside them (/MT, STATIC-1). That's undone: Bitdefender flagged
#   the game built that way (#111), and the user ruled static linking out (#125)
#   every test is built with the standard library's own bounds checks, and the guard that refuses to compile one without them, and a test
#   whose settings leave either out, or turn the checks off again, is refused with the reason (STL-HARDEN-1, #140)
#
# Modified 2026-11-21 by Melanikus for the AvP port:
#   outside the automatic checks, each copy of the repository (each worktree) builds into a folder of its own, AvP-test-out\tests\<its folder's
#   name>. This empties its output folder first, and copies side by side shared the AvP-test-out next to them, so a build in one emptied
#   another's tests, take-out copies and sanitizer builds while they ran (2026-11-04, 2026-11-21). The tests\ part keeps the name apart from
#   the folders the take-outs and the sanitizer use there. It is emptied by -LiteralPath, so a [ in a copy's name can never make it empty
#   another copy's folder that the name would match as a pattern.
#
# Modified 2026-11-23 by Melanikus (Drake) for the AvP port (the owner: "You obviously need to paralelize the tests"):
#   -Part and -Parts build one part of the tests, so the Unit tests check runs its parts side by side (.github/workflows/tests.yml). They are
#   shared out as the sanitizer job shares them: in name order, compared letter by letter, the first test to part 0, the second to part 1, and so
#   on, round again, so every test is in exactly one part and a new one joins a part with no change anywhere. A part's number that makes no sense is
#   refused, never read as "every test" or "none". The names of the tests built are written to built.txt beside them, one to a line, so the step
#   that runs them runs exactly those.

param([string]$Only = '', [int]$Part = 0, [int]$Parts = 0)

$ErrorActionPreference = 'Stop'

# Any error at all that stops this script ends up here, so the exit code always tells the truth
trap { Write-Host "Could not build the tests: $_"; exit 1 }

# A part's number that makes no sense is refused first, before any folder is emptied or anything is set up
if ($Parts -lt 0 -or ($Parts -eq 0 -and $Part -ne 0) -or ($Parts -gt 0 -and ($Part -lt 0 -or $Part -ge $Parts -or $Only)))
{
    Write-Host "A part is a number from 0 to one less than -Parts, -Parts is at least 1, and -Only goes without them (asked for part $Part of $Parts, -Only '$Only')"
    exit 1
}

$repository = Split-Path -Parent $PSScriptRoot
$outputDir  = if ($env:RUNNER_TEMP) { Join-Path $env:RUNNER_TEMP 'avp-tests' } else { Join-Path (Split-Path -Parent $repository) "AvP-test-out\tests\$(Split-Path -Leaf $repository)" }

# Start from an empty folder, so nothing old can be mistaken for a new build
if (Test-Path -LiteralPath $outputDir) { Remove-Item -LiteralPath $outputDir -Recurse -Force }
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
# THE ONE COMPILER (win32/msvc-pin.props): the tests are built with the same MSVC as the game (-vcvars_ver). On GitHub's runner it is installed
# first, from Microsoft's own installer (tools/install-msvc-pin.ps1, as tools/build-and-package.ps1 does); anywhere else, the tests stop if it isn't there.
. (Join-Path $repository 'tools/msvc-pin-version.ps1')
$msvcPin = Get-MsvcPinVersion $repository
if ($env:GITHUB_ACTIONS -eq 'true')
{
    & (Join-Path $repository 'tools/install-msvc-pin.ps1')
    if ($LASTEXITCODE -ne 0) { Write-Error "MSVC $msvcPin could not be installed (tools/install-msvc-pin.ps1 said why, above)" }
}
Get-MsvcPinFolder $visualStudio $msvcPin | Out-Null
Enter-VsDevShell -VsInstallPath $visualStudio -SkipAutomaticLocation -DevCmdArguments "-arch=x86 -host_arch=amd64 -no_logo -vcvars_ver=$msvcPin" | Out-Null

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

if ($Parts -gt 0)
{
    $names = [string[]]@($tests | ForEach-Object { $_.BaseName })
    [Array]::Sort($names, [StringComparer]::Ordinal)
    $mine = @(for ($i = $Part; $i -lt $names.Count; $i += $Parts) { $names[$i] })
    Write-Host "Part $($Part + 1) of ${Parts}: $($mine.Count) of the $($names.Count) tests: $($mine -join ', ')"
    $tests = @($tests | Where-Object { $mine -ccontains $_.BaseName })
}

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

    # STL-HARDEN-1 (#140): every test is built with the standard library's own bounds checks (/D_MSVC_STL_HARDENING=1). With them, looking
    # past the end of a std::vector or another of its containers stops the test at once. Without them, it reads whatever is in memory there,
    # and a check can pass by luck. tests/every_test/every_test.h, which every C++ file of a test reads first (/Itests/every_test
    # /FIevery_test.h), refuses to compile if they're off. A setting that turns them off again, or a part of them, is refused here too.
    if ($settings -cnotmatch '(^|\s)[/-]D_MSVC_STL_HARDENING=1(?=\s|$)')
    {
        Write-Host "$($test.Name) doesn't ask for /D_MSVC_STL_HARDENING=1. Every test is built with the standard library's bounds checks (STL-HARDEN-1, #140)"
        exit 1
    }

    $offAgain = @([regex]::Matches($settings, '(^|\s)([/-][DU]_MSVC_STL_HARDENING\S*)') | Where-Object { $_.Groups[2].Value.Substring(1) -cne 'D_MSVC_STL_HARDENING=1' })

    if ($offAgain.Count -gt 0)
    {
        Write-Host "$($test.Name) asks for $($offAgain[0].Groups[2].Value), which could turn the standard library's bounds checks, or a part of them, off again. Every test is built with just /D_MSVC_STL_HARDENING=1 (STL-HARDEN-1, #140)"
        exit 1
    }

    if ($settings -cnotmatch '(^|\s)[/-]Itests/every_test(?=\s|$)' -or $settings -cnotmatch '(^|\s)[/-]FIevery_test\.h(?=\s|$)')
    {
        Write-Host "$($test.Name) doesn't ask for /Itests/every_test /FIevery_test.h, the guard that refuses to compile a test without the standard library's bounds checks (STL-HARDEN-1, #140)"
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

# The names of the tests built, one to a line, for the step that runs them (it runs exactly these, and fails on one with no program)
[System.IO.File]::WriteAllLines((Join-Path $outputDir 'built.txt'), [string[]]@($tests | ForEach-Object { $_.BaseName }))

Write-Host "Built $($tests.Count) test(s) into $outputDir"
exit 0
