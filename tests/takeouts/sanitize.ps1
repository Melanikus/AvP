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
#   Visual Studio's setup is done once in a window, not again on every run, and this stops, saying why, if the setup didn't give the compiler (SANITIZE-RUN-1)
#   the test's name is checked before its folder is emptied, and anything that isn't one of the repository's tests is refused (AvP Tao 3)
#   a test that doesn't pass shows everything it and the sanitizer said, because in the automatic checks the folder that keeps it is gone once the check ends
#   every object and library a test's settings name has to have been compiled with the sanitizer, or its code would run unwatched, and the
#   libraries vcpkg builds are taken from their copy built with it, in vcpkg_installed/x86-windows-asan
#   for a day the test was built with the C runtime inside it (/MT) and refused if it asked Windows for the runtime's DLLs (STATIC-1). That's
#   undone, with the rest of STATIC-1: Bitdefender flagged the game built that way (#111), and the user ruled static linking out
#   (STATIC-UNDO-1, #125). A test is built with /MD again, as the game is

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

# The address sanitizer's own library: the part that watches every read and write while a test runs. Any program linked with the
# sanitizer loads it, even one with none of its code compiled with it (run 42493315111), so loading it proves nothing about what's
# watched. What does is below: the mark each object compiled with the sanitizer carries.
$sanitizerLibrary = 'clang_rt.asan_dynamic-i386.dll'

# The address sanitizer's own library has to be beside the program, and it's on the path once Visual Studio is set up
function Copy-SanitizerLibrary([string]$to)
{
    $library = @($env:PATH -split ';' | Where-Object { $_ } | ForEach-Object { Join-Path $_ $sanitizerLibrary } | Where-Object { Test-Path -LiteralPath $_ }) | Select-Object -First 1

    if (-not $library) { throw "The address sanitizer's library wasn't found on the path" }

    Copy-Item -LiteralPath $library -Destination $to -Force
}

# SANITIZE-RUN-1: is every part of the test watched? The sanitizer only watches code compiled with it. Code can come in unwatched two ways:
# - compiled somewhere else: an object (.obj) or a library (.lib) the settings name. Each of those is read before anything is built.
# - compiled here, with the sanitizer turned off. Every source is compiled by the line below with /fsanitize=address, but the test's
#   settings come after it, and the compiler has a switch, /fno-sanitize=address, that turns it off again (found in cl.exe's own list of
#   switches, Visual Studio 18's compiler 14.51). So after the build, every object it made is read too. That doesn't depend on knowing
#   every switch that could do it.
#
# THE MARK: the compiler writes three lines into each object it compiles with the sanitizer, among the object's orders to the linker,
# which dumpbin /directives shows. One of them, below, is the mark. It's there even in an object that reads and writes no memory at all,
# and in none compiled without the sanitizer (SANITIZE-RUN-1's second probe, run 42494419512, Visual Studio 18's compiler). The
# sanitizer's own names among an object's symbols, such as ___asan_report_load1, aren't used, because an object that touches no memory
# has none of them, and would be refused for nothing.
#
# WHAT ISN'T READ: Windows' own libraries, named without a folder (user32.lib and the like), are Microsoft's, and only lead to Windows'
# DLLs. So are those an object names for itself (#pragma comment(lib)), and Microsoft's C runtime, which the sanitizer watches through its
# own stand-ins for memcpy, malloc and the rest. Microsoft's D3DX, which the shader test loads while it runs, can't be built with it either.
$sanitizerMark = '/INCLUDE:___you_must_link_with_VCAsan_lib'

# Was this object compiled with the sanitizer? Reads its orders to the linker with dumpbin, into the file given.
function Test-CompiledWithSanitizer([string]$object, [string]$saysTo)
{
    if ((Invoke-Tool 'dumpbin.exe' "/nologo /directives `"$object`"" $repository $saysTo) -ne 0)
    {
        throw "dumpbin couldn't read $object"
    }

    return @(Get-Content -LiteralPath $saysTo | ForEach-Object { $_.Trim() }) -contains $sanitizerMark
}

# Of the objects inside a library, those compiled without the sanitizer. dumpbin reads a library's orders as one block for each object,
# without saying which object each block came from, and an object with none has no block at all. So each object is taken out of the
# library on its own, with lib.exe, and read on its own.
function Get-UnwatchedMembers([string]$library, [string]$folder)
{
    New-Item -ItemType Directory -Force $folder | Out-Null
    $listed = Join-Path $folder 'members.txt'

    if ((Invoke-Tool 'lib.exe' "/nologo /list `"$library`"" $repository $listed) -ne 0)
    {
        throw "lib.exe couldn't list what's inside $library"
    }

    $members = @(Get-Content -LiteralPath $listed | ForEach-Object { $_.Trim() } | Where-Object { $_ })
    if ($members.Count -eq 0) { throw "$library has no objects in it" }

    $unwatched = [System.Collections.Generic.List[string]]::new()

    for ($i = 0; $i -lt $members.Count; $i++)
    {
        $member = Join-Path $folder "member-$i.obj"

        if ((Invoke-Tool 'lib.exe' "/nologo `"/extract:$($members[$i])`" `"/out:$member`" `"$library`"" $repository (Join-Path $folder "member-$i.txt")) -ne 0)
        {
            throw "lib.exe couldn't take $($members[$i]) out of $library"
        }

        if (-not (Test-CompiledWithSanitizer $member (Join-Path $folder "member-$i-directives.txt")))
        {
            $unwatched.Add($members[$i])
        }
    }

    return $unwatched.ToArray()
}

# The libraries vcpkg builds, which a test's settings name in vcpkg_installed/x86-windows, are taken from their copy built with the
# sanitizer: vcpkg-overlay-triplets/x86-windows-asan.cmake says how it's made, and the automatic checks make it (fetch-test-libraries,
# sanitized-ports). So the settings are copied into the test's folder, with that one folder changed, and the copy is what's built from.
# Settings are written with / and no quotes (tests/build.ps1 says how), so the folder's name is found as it's written. The trailing / is
# what tells the two apart: vcpkg_installed/x86-windows/ isn't the start of vcpkg_installed/x86-windows-asan/.
$ordinaryLibraries = 'vcpkg_installed/x86-windows/'
$sanitizedLibraries = 'vcpkg_installed/x86-windows-asan/'
$settings = [System.IO.File]::ReadAllText((Join-Path $repository "tests\$Test.args"))

if ($settings.Contains($ordinaryLibraries))
{
    if (-not (Test-Path -LiteralPath (Join-Path $repository $sanitizedLibraries) -PathType Container))
    {
        throw "$Test uses libraries from $ordinaryLibraries, and under the sanitizer it needs their copy built with it, in $sanitizedLibraries, which isn't here. The automatic checks build it: see fetch-test-libraries' sanitized-ports"
    }

    $settings = $settings.Replace($ordinaryLibraries, $sanitizedLibraries)
}

$settingsCopy = Join-Path $out 'settings.args'
[System.IO.File]::WriteAllText($settingsCopy, $settings)

$linked = 0

foreach ($word in @($settings -split '\s+' | Where-Object { $_ }))
{
    if ($word -inotmatch '\.(obj|lib)\z')
    {
        continue
    }

    $linked++

    # An order to the linker that names a library, such as /DEFAULTLIB:x.lib, brings in code this doesn't read, so it isn't allowed
    if ($word.StartsWith('/') -or $word.StartsWith('-'))
    {
        Write-Host "$Test's settings give the linker $word, and a library or object named in an order isn't read for the sanitizer's mark. Name it on its own, so it's read"
        exit 1
    }

    if (-not $word.Contains('/'))
    {
        # Named without a folder: one of Windows' own, which the linker finds in the Windows SDK, on LIB. The first it finds is what it links.
        $found = @("$env:LIB".Split(';') | Where-Object { $_ } | ForEach-Object { Join-Path $_ $word } | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }) | Select-Object -First 1

        if (-not $found -or $found -inotmatch '\\Windows Kits\\')
        {
            Write-Host "$Test's settings name $word, which isn't one of Windows' own libraries in the Windows SDK, and a library named without a folder has to be. One of ours is named with its folder, so it's read for the sanitizer's mark"
            exit 1
        }

        continue
    }

    # Named with a folder: one of the repository's, so it has to be inside it, and compiled with the sanitizer
    $file = [System.IO.Path]::GetFullPath((Join-Path $repository $word))

    if (-not $file.StartsWith($repository.TrimEnd('\') + '\', [System.StringComparison]::OrdinalIgnoreCase))
    {
        throw "$Test's settings name $word, which is outside the repository"
    }

    if (-not (Test-Path -LiteralPath $file -PathType Leaf))
    {
        throw "$Test's settings name $word, which isn't there"
    }

    if ($word -imatch '\.obj\z')
    {
        if (-not (Test-CompiledWithSanitizer $file (Join-Path $out "linked-$linked-directives.txt")))
        {
            Write-Host "$Test links $word, which wasn't compiled with the sanitizer, so its code would run unwatched"
            exit 1
        }
    }
    else
    {
        $unwatched = @(Get-UnwatchedMembers $file (Join-Path $out "linked-$linked"))

        if ($unwatched.Count -gt 0)
        {
            Write-Host "$Test links $word, whose $($unwatched -join ', ') wasn't compiled with the sanitizer, so its code would run unwatched"
            exit 1
        }
    }
}

# The test's settings come last, because they can end with /link, and anything after /link goes to the linker
$built = Invoke-Tool 'cl.exe' "/nologo /Zi /fsanitize=address `"/Fe:$out\t.exe`" `"/Fo:$out/`" `"/Fd:$out/`" @`"$settingsCopy`"" $repository (Join-Path $out 'build.txt')

if ($built -ne 0) {
    $buildSaid = Join-Path $out 'build.txt'
    Show-WhatWasSaid 'what the compiler said' @($buildSaid, "$buildSaid.errors")
    Write-Host "$Test didn't build. What the compiler said is above"
    exit 1
}

# Every object the build just made has to carry the sanitizer's mark too, or something in the test's settings turned the sanitizer off
# for it (see SANITIZE-RUN-1, above). With none at all, the build made nothing this can read, which proves nothing either.
$compiled = @(Get-ChildItem -LiteralPath $out -Filter '*.obj' -File)
if ($compiled.Count -eq 0) { throw "the build of $Test made no objects in $out to read for the sanitizer's mark" }

$unwatchedHere = @($compiled | Where-Object { -not (Test-CompiledWithSanitizer $_.FullName "$($_.FullName).directives.txt") } | ForEach-Object { $_.Name })
if ($unwatchedHere.Count -gt 0) {
    Write-Host "$Test's own $($unwatchedHere -join ', ') wasn't compiled with the sanitizer, though this asked for it: something in its settings turned it off, so its code would run unwatched"
    exit 1
}

# The sanitizer's own library is a DLL the test needs beside it to run. It's Microsoft's, it's only for this check, and it never goes out
# with the game.
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
