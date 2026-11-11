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
#   every test is built with the standard library's own bounds checks, and the guard that refuses to compile one without them, and a test
#   whose settings leave either out, or turn the checks off again, is refused with the reason (STL-HARDEN-1, #140)

# Modified 2026-11-11 by Melanikus for the AvP port (the CI plan's step 4, tests/reuse/Reuse.psm1):
#   -ShowIncludes keeps, beside each program, the list of the repository's files the compiler read for it (<test>.includes.txt), which a
#   reused result's key is made from. The compiler is asked for them in English (VSLANG), so the lines can be read on any machine
#   -Skip names a file of tests not to build, one a line: those whose earlier result tests/reuse/lookup.ps1 found and checked
param([string]$Only = '', [switch]$ShowIncludes, [string]$Skip = '')

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

# The tests whose earlier result is reused, one name a line: only names a test has, or nothing is skipped at all
$skipped = @{}
if ($Skip)
{
    foreach ($name in @(Get-Content -LiteralPath $Skip | ForEach-Object { $_.Trim() } | Where-Object { $_ }))
    {
        if ($name -cnotmatch '^[A-Za-z0-9_]+$' -or -not (Test-Path -LiteralPath (Join-Path $PSScriptRoot "$name.args"))) { Write-Error "$Skip names $name, which isn't a test here" }
        $skipped[$name] = $true
    }
}

# What the compiler read, for -ShowIncludes: the repository's files by their names from its top, with / between folders. A file of
# Visual Studio's, of Windows' SDK, or under vcpkg_installed is left out, as a reused result's key covers those another way; any other
# file outside the repository is written as "outside:" and its path, and then that test's result is never reused.
$repositoryFull = [System.IO.Path]::GetFullPath($repository).TrimEnd('\') + '\'
$kits = [System.IO.Path]::GetFullPath((Join-Path ${env:ProgramFiles(x86)} 'Windows Kits')).TrimEnd('\') + '\'
$studioFull = [System.IO.Path]::GetFullPath($visualStudio).TrimEnd('\') + '\'
if ($ShowIncludes) { $env:VSLANG = '1033' }

function Write-Includes([string]$says, [string]$to)
{
    $files = [System.Collections.Generic.SortedSet[string]]::new([StringComparer]::Ordinal)
    foreach ($line in @(Get-Content -LiteralPath $says -ErrorAction SilentlyContinue))
    {
        if ($line -cnotmatch '^Note: including file:\s*(.+?)\s*$') { continue }
        # A name the compiler gives from the folder it ran in, the top of the repository, is made whole from there, not from this window's
        $full = [System.IO.Path]::GetFullPath([System.IO.Path]::Combine($repository, $Matches[1]))
        if ($full.StartsWith($repositoryFull, [StringComparison]::OrdinalIgnoreCase))
        {
            $relative = $full.Substring($repositoryFull.Length).Replace('\', '/')
            if (-not $relative.StartsWith('vcpkg_installed/', [StringComparison]::OrdinalIgnoreCase)) { [void]$files.Add($relative) }
        }
        elseif (-not $full.StartsWith($studioFull, [StringComparison]::OrdinalIgnoreCase) -and -not $full.StartsWith($kits, [StringComparison]::OrdinalIgnoreCase))
        {
            [void]$files.Add("outside:$full")
        }
    }
    # Every test reads tests/every_test/every_test.h first (/FIevery_test.h), so a list without it means the compiler's words weren't
    # read: in another language, or said another way. Then that test's result is never reused, rather than reused on a list that's short.
    if (-not $files.Contains('tests/every_test/every_test.h')) { [void]$files.Add("outside:the compiler's list of what it read for this test couldn't be read") }
    [System.IO.File]::WriteAllLines($to, [string[]]@($files))
}

foreach ($test in $tests)
{
    if ($skipped.ContainsKey($test.BaseName))
    {
        Write-Host "Not built: $($test.BaseName), whose earlier result is reused (tests/reuse/lookup.ps1 says where it ran)"
        continue
    }

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
    # /showIncludes before the settings, which can end with /link
    # (PowerShell reads $ShowIncludes and $showIncludes as one name, so this one has another)
    $askForIncludes = if ($ShowIncludes) { '/showIncludes ' } else { '' }
    $exitCode = Invoke-Tool 'cl.exe' "/nologo $askForIncludes`"/Fe:$outputDir\$($test.BaseName).exe`" `"/Fo:$objectDir/`" `"@$($test.FullName)`"" $repository $saysTo

    # What the compiler said goes on the screen too, so that the automatic check's log has it: all but its list of what it read, which
    # goes in the test's own file instead
    Get-Content -LiteralPath $saysTo, "$saysTo.errors" -ErrorAction SilentlyContinue | Where-Object { $_ -cnotmatch '^Note: including file:' }
    if ($ShowIncludes -and $exitCode -eq 0) { Write-Includes $saysTo (Join-Path $outputDir ($test.BaseName + '.includes.txt')) }

    if ($exitCode -ne 0)
    {
        Write-Host "Could not build $($test.Name)"
        exit 1
    }
}

Write-Host "Built $($tests.Count - $skipped.Count) test(s) into $outputDir$(if ($skipped.Count) { ', and left out ' + $skipped.Count + ' whose result is reused' })"
exit 0
