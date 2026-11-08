# Builds a test with the address sanitizer and runs it. Written 2026-11-05 by Melanikus for the AvP port.
#
# The sanitizer watches every read and write, and stops the program at the first one outside what it was given. A test can only see what it looks for, and this sees the rest. It gives back 0
# when the test passes under it, 1 when the sanitizer stopped the test or the test failed, and 2 when it couldn't be run at all. Its last line says which.
#
# Run it from anywhere, in PowerShell:
#   tests/takeouts/sanitize.ps1 <test>              for example: tests/takeouts/sanitize.ps1 relief_map_file_test
#
# The program lands in a folder called AvP-test-out next to the repository, or under $env:RUNNER_TEMP in the automatic checks.
#
# Modified 2026-11-08 by Melanikus for the AvP port:
#   the test is built with the C runtime inside it (/MT), the same as every test, and refused if it comes out asking Windows for it as a DLL (STATIC-1)
#   Visual Studio's setup is done once in a window, not again on every run, and this stops, saying why, if the setup didn't give the compiler (SANITIZE-RUN-1)
#   the test's name is checked before its folder is emptied, and anything that isn't one of the repository's tests is refused (AvP Tao 3)
#   a test that doesn't pass shows everything it and the sanitizer said, because in the automatic checks the folder that keeps it is gone once the check ends

param([Parameter(Mandatory = $true)][string]$Test)

$ErrorActionPreference = 'Stop'
trap { Write-Host "Could not run the sanitizer: $_"; exit 2 }

$repository = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

# The test's name picks the folder that's emptied below, so it's checked before anything is touched (AvP Tao 3: input is treated as hostile).
# Without this, a name like ..\..\AvP points outside the test's own folder, for example at a repository beside it, and everything in it would be deleted.
# A name is only letters, digits and _, and it has to be one of the repository's tests, with its tests\<name>.args.
if ($Test -cnotmatch '\A[A-Za-z0-9_]+\z') { throw "'$Test' isn't a test's name. A test's name is only letters, digits and _, so nothing outside its own folder is ever emptied" }
if (-not (Test-Path -LiteralPath (Join-Path $repository "tests\$Test.args") -PathType Leaf)) { throw "there's no test called $Test`: tests\$Test.args isn't there" }

$base = if ($env:RUNNER_TEMP) { Join-Path $env:RUNNER_TEMP 'avp-sanitize' } else { Join-Path (Split-Path -Parent $repository) 'AvP-test-out\sanitize' }
$out = Join-Path $base $Test
if (Test-Path -LiteralPath $out) { Remove-Item -LiteralPath $out -Recurse -Force }
New-Item -ItemType Directory -Force $out | Out-Null

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio with the C++ tools was not found' }

# Visual Studio's own PowerShell module sets the compiler up in this window, the way its setup batch file does in a command window, so nothing has to be written to disk to run it.
# The antivirus distrusts a script that writes another to run, and took one of ours. The setup looks for vswhere on the path, so that goes on first. -arch=x86 builds 32-bit programs,
# the same as the game.
#
# Once in a window is enough, and more does harm. Each time, the setup puts Visual Studio's folders at the front of the path again. It's run
# by Windows' command interpreter, which can't read a line longer than 8,191 characters, and the setup adds to the path one line at a time.
# Once the path is long enough, each of those lines fails, saying only "The input line is too long", and the window carries on with whatever
# it had before. A run of four tests from one window got there on the third test (SANITIZE-RUN-1's probe, 2026-11-08). That time the
# window still had the second test's setup, so nothing went wrong, but nothing would have said so if it had. So:
# - if this window already has this Visual Studio's 32-bit compiler set up, it isn't set up again;
# - if the path is already too long for the setup, this stops and says so;
# - after the setup, this checks that it really gave the compiler, and stops, saying what's missing, if it didn't.
$commandLineLimit = 8191

# What a finished setup leaves in the window: VCToolsInstallDir naming a compiler inside this Visual Studio, that compiler's 32-bit cl.exe
# and dumpbin.exe found first on the path (the ones made to run on a 64-bit Windows, as -host_arch=amd64 asks), and its own 32-bit
# libraries and headers in LIB and INCLUDE. Gives back what's missing, or nothing when it's all there.
function Get-SetUpProblem([string]$visualStudio)
{
    $tools = "$env:VCToolsInstallDir".TrimEnd('\')

    if (-not $tools -or -not $tools.StartsWith($visualStudio.TrimEnd('\') + '\', [System.StringComparison]::OrdinalIgnoreCase))
    {
        return "VCToolsInstallDir doesn't name a compiler inside $visualStudio"
    }

    $programFolder = Join-Path $tools 'bin\Hostx64\x86'

    foreach ($program in 'cl.exe', 'dumpbin.exe')
    {
        $found = Get-Command $program -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1

        if (-not $found -or (Split-Path -Parent $found.Source) -ine $programFolder)
        {
            return "the $program found first on the path isn't the 32-bit one in $programFolder"
        }
    }

    foreach ($wanted in @(@{ name = 'LIB'; folder = (Join-Path $tools 'lib\x86') }, @{ name = 'INCLUDE'; folder = (Join-Path $tools 'include') }))
    {
        $folders = @("$([Environment]::GetEnvironmentVariable($wanted.name))".Split(';') | ForEach-Object { $_.TrimEnd('\') })

        if ($folders -inotcontains $wanted.folder)
        {
            return "$($wanted.name) doesn't have $($wanted.folder)"
        }
    }

    return ''
}

if (Get-SetUpProblem $visualStudio)
{
    if ($env:PATH.Length -gt $commandLineLimit)
    {
        throw "the path is $($env:PATH.Length) characters, and Visual Studio's setup can't add to a path longer than $commandLineLimit, because Windows' command interpreter, which runs it, can't read the line. Start this from a window with a shorter path"
    }

    $vswhereFolder = Split-Path -Parent $vswhere

    if (@($env:PATH.Split(';') | Where-Object { $_.TrimEnd('\') -ieq $vswhereFolder }).Count -eq 0)
    {
        $env:PATH = "$vswhereFolder;$env:PATH"
    }

    try
    {
        Import-Module (Join-Path $visualStudio 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
        Enter-VsDevShell -VsInstallPath $visualStudio -SkipAutomaticLocation -DevCmdArguments '-arch=x86 -host_arch=amd64 -no_logo' | Out-Null
        $problem = Get-SetUpProblem $visualStudio
    }
    catch
    {
        $problem = "it said: $_"
    }

    if ($problem)
    {
        throw "Visual Studio's setup didn't finish: $problem. The path is now $($env:PATH.Length) characters. The setup adds to it a line at a time, and Windows' command interpreter can't read a line longer than $commandLineLimit, so a long path stops the setup part way"
    }
}

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

# Prints all that a program said, from the files Invoke-Tool kept it in, when something has gone wrong. In the automatic checks those
# files are gone once the check ends, so the check's own log is the only place anyone can read why.
function Show-WhatWasSaid([string]$what, [string[]]$files)
{
    Write-Host "----- $what"
    Get-Content -LiteralPath $files -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_ }
    Write-Host "----- the end of $what"
}

# The address sanitizer's own library has to be beside the program, and it's on the path once Visual Studio is set up
function Copy-SanitizerLibrary([string]$to)
{
    $library = @($env:PATH -split ';' | Where-Object { $_ } | ForEach-Object { Join-Path $_ 'clang_rt.asan_dynamic-i386.dll' } | Where-Object { Test-Path -LiteralPath $_ }) | Select-Object -First 1

    if (-not $library) { throw "The address sanitizer's library wasn't found on the path" }

    Copy-Item -LiteralPath $library -Destination $to -Force
}

# STATIC-1: the tests are built with the C runtime inside them (/MT), the same as the game, so a test needs no Visual C++ install to run,
# and can't pass only because the machine it runs on happens to have one. This asks Visual Studio's dumpbin which DLLs a program asks
# Windows for, and gives back those that are the C runtime's: vcruntime, msvcp, ucrtbase, and the api-ms-win-crt- names Windows uses for
# ucrtbase. An empty answer means the program carries its own. The same function is in tests/build.ps1 and tests/takeouts/run.ps1.
function Get-CRuntimeLibraries([string]$program, [string]$folder, [string]$saysTo)
{
    if ((Invoke-Tool 'dumpbin.exe' "/nologo /dependents `"$program`"" $folder $saysTo) -ne 0)
    {
        # What dumpbin said goes into the message, because the folder can be emptied before anyone looks, by the next build or by an
        # antivirus program holding the new program
        $said = @(Get-Content -LiteralPath $saysTo, "$saysTo.errors" -ErrorAction SilentlyContinue | ForEach-Object { $_.Trim() } | Where-Object { $_ }) -join ' / '
        throw "dumpbin couldn't read $program`: $said"
    }

    # dumpbin writes each DLL's name on a line of its own, under "Image has the following dependencies", and the same for any it loads later
    return @(Get-Content -LiteralPath $saysTo | ForEach-Object { $_.Trim() } |
        Where-Object { $_ -match '^(vcruntime|msvcp|msvcr|ucrtbase|concrt|vccorlib)[0-9a-z_]*\.dll$' -or $_ -match '^api-ms-win-crt-[0-9a-z-]*\.dll$' })
}

# The test's settings come last, because they can end with /link, and anything after /link goes to the linker
$built = Invoke-Tool 'cl.exe' "/nologo /Zi /fsanitize=address `"/Fe:$out\t.exe`" `"/Fo:$out/`" `"/Fd:$out/`" @tests\$Test.args" $repository (Join-Path $out 'build.txt')

if ($built -ne 0) {
    $buildSaid = Join-Path $out 'build.txt'
    Show-WhatWasSaid 'what the compiler said' @($buildSaid, "$buildSaid.errors")
    Write-Host "$Test didn't build. What the compiler said is above"
    exit 1
}

# STATIC-1: built the same as every test, with the C runtime inside it. The sanitizer's own library is a DLL, and that one is allowed:
# it's Microsoft's, it's only for this check, and it never goes out with the game.
#
# That library itself asks Windows for the C runtime's DLLs: VCRUNTIME140.dll and six api-ms-win-crt- names, read with dumpbin from Visual
# Studio 18's compiler 14.51.36231 (SANITIZE-RUN-1's probe, 2026-11-08). So while a test built for the sanitizer runs, the runtime IS
# loaded as DLLs, through the sanitizer's library. What this check shows is only that the test's own code has the runtime built in. A
# pass here never means a program runs without the Visual C++ runtime installed. tests/build.ps1's same check, on the ordinary build of
# each test, is the one that shows that.
$runtime = @(Get-CRuntimeLibraries (Join-Path $out 't.exe') $repository (Join-Path $out 'imports.txt'))
if ($runtime.Count -gt 0) {
    Write-Host "$Test, built for the sanitizer, asks Windows for the C runtime's DLLs: $($runtime -join ', '). A test carries its own (/MT), the same as the game (STATIC-1)"
    exit 1
}

Copy-SanitizerLibrary $out

# The sanitizer writes what it found to the program's error stream, so both streams are read
$result = Invoke-Tool (Join-Path $out 't.exe') '' $repository (Join-Path $out 'said.txt')
Get-Content -LiteralPath (Join-Path $out 'said.txt.errors') | Add-Content -LiteralPath (Join-Path $out 'said.txt')

$said = Join-Path $out 'said.txt'
$stopped = @(Select-String -LiteralPath $said -Pattern 'ERROR: AddressSanitizer' -SimpleMatch)
if ($stopped.Count) {
    Show-WhatWasSaid "what $Test and the sanitizer said" @($said)
    Write-Host "The sanitizer stopped $Test`: $($stopped[0].Line)"
    exit 1
}

if ($result -ne 0) {
    Show-WhatWasSaid "what $Test said" @($said)
    Write-Host "$Test failed under the sanitizer, with exit code $result"
    exit 1
}

Write-Host "$Test passes under the address sanitizer"
exit 0
