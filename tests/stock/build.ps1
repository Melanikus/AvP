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

# The address sanitizer's own library has to be beside the program, and it's on the path once Visual Studio is set up
function Copy-SanitizerLibrary([string]$to)
{
    $library = @($env:PATH -split ';' | Where-Object { $_ } | ForEach-Object { Join-Path $_ 'clang_rt.asan_dynamic-i386.dll' } | Where-Object { Test-Path -LiteralPath $_ }) | Select-Object -First 1

    if (-not $library) { throw "The address sanitizer's library wasn't found on the path" }

    Copy-Item -LiteralPath $library -Destination $to -Force
}

$flags = '/nologo /std:c++17 /EHsc /MD /W1 /Zi /fsanitize=address /DNDEBUG /D_CRT_SECURE_NO_DEPRECATE /D_CRT_SECURE_NO_WARNINGS'
$bink = 'dependencies\libbinkdec'
$smacker = 'dependencies\libsmackerdec'

# Each step stops the build if it fails, as the batch file's checks did
$steps = @(
    @('cl.exe', "/c $flags /I$bink\include /I$bink\src `"/Fo$out\bink/`" `"/Fd$out\bink\vc.pdb`" $bink\src\*.cpp"),
    @('cl.exe', "/nologo /c /MD /W1 /Zi /fsanitize=address /D_CRT_SECURE_NO_WARNINGS /DNDEBUG /I$bink\include /I$bink\src `"/Fo$out\bink/`" `"/Fd$out\bink\vc.pdb`" $bink\src\*.c"),
    @('cl.exe', "/c $flags /I$smacker\include /I$smacker\src `"/Fo$out\smacker/`" `"/Fd$out\smacker\vc.pdb`" $smacker\src\*.cpp"),
    @('cl.exe', "/c $flags /I$bink\include /I$smacker\include `"/Fo$out/`" `"/Fd$out\main.pdb`" tests\stock\film_close_check.cpp"),
    @('link.exe', "/nologo /DEBUG /INCREMENTAL:NO `"/OUT:$out\film_close_check.exe`" `"$out\film_close_check.obj`" `"$out\bink\*.obj`" `"$out\smacker\*.obj`""),
    @('cl.exe', "/nologo /std:c++17 /EHsc /MD /J /W3 /DNDEBUG /D_CRT_SECURE_NO_DEPRECATE /I3dc tests\stock\rim_fingerprints.cpp 3dc\RimLoader.cpp 3dc\FileStream.cpp /Fe:`"$out\rim_fingerprints.exe`" /Fo:`"$out\pictures\\`"")
)
$number = 0

foreach ($step in $steps)
{
    $number++
    $saysTo = Join-Path $out "step-$number.txt"

    if ((Invoke-Tool $step[0] $step[1] $repository $saysTo) -ne 0)
    {
        Get-Content -LiteralPath $saysTo, "$saysTo.errors" -ErrorAction SilentlyContinue
        Write-Host 'Could not build them'
        exit 1
    }

    # The film check uses the address sanitizer, so its library goes beside it once it's linked
    if ($step[0] -eq 'link.exe') { Copy-SanitizerLibrary $out }
}
Write-Host "Built into $out"
exit 0
