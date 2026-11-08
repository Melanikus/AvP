<#
Tries a finished package the way a tester's machine would use it, then shows that the checks would catch something going wrong. game-build.yml
runs this on every pull request. It goes online for nothing, and it never changes the package it's given: it works on copies.

A check that has only ever passed doesn't show much, and the file that matters most here is one nothing links to, so losing it gives no error
anywhere else. So each check runs on the whole package, and then on a copy with one thing wrong, where it has to refuse for the reason it's
meant to. A later check could otherwise hide a missing one.

Build the package and tests/package_test.cpp first, then:
    tests/try-package.ps1 -Package <folder> -TestProgram <folder>\package_test.exe

WHAT IT CAN'T SHOW: that the game itself starts on a clean Windows. It compiles one small shader, and needs no graphics card. RELEASE-BLOCKERS.md
keeps that as a job for a person.
#>
param(
    [Parameter(Mandatory = $true)][string]$Package,
    [Parameter(Mandatory = $true)][string]$TestProgram
)

$ErrorActionPreference = 'Stop'

$repository = Split-Path -Parent $PSScriptRoot
$checkImports = Join-Path $repository 'tools/check-package-imports.ps1'
$powershell = (Get-Process -Id $PID).Path

# Not the temp folder on a person's machine: antivirus programs distrust new programs that appear there.
$workParent = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { Split-Path -Parent $repository }
$work = Join-Path $workParent 'AvP-package-try'

$script:failures = 0
$script:passes = 0

function Check([bool]$passed, [string]$what, [string]$said)
{
    if ($passed)
    {
        $script:passes++
        Write-Host "  ok  $what"
    }
    else
    {
        $script:failures++
        Write-Host "  FAILED  $what. It said: $said"
    }
}

# A new copy of the package with the test program beside it, with nothing else left from the last try
function New-Copy
{
    if (Test-Path -LiteralPath $work) { Remove-Item -Recurse -Force -LiteralPath $work }
    Copy-Item -Recurse -LiteralPath $Package $work
    Copy-Item -LiteralPath $TestProgram $work
}

# Older PowerShell turns anything a program writes to its error stream into a stopping error while this is set to Stop, and what's said is what's being tested.
function Run-Test([string]$word = '--here')
{
    $ErrorActionPreference = 'Continue'
    $output = & (Join-Path $work 'package_test.exe') $word 2>&1 | ForEach-Object { "$_" }
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    return [pscustomobject]@{ Code = $code; Text = ($output -join "`n") }
}

# The test program is taken away first, because it's not part of a package and the check would ask where its libraries come from.
function Run-Imports
{
    Remove-Item -LiteralPath (Join-Path $work 'package_test.exe')
    $ErrorActionPreference = 'Continue'
    $output = & $powershell -NoProfile -ExecutionPolicy Bypass -File $checkImports -Package $work 2>&1 | ForEach-Object { "$_" }
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    return [pscustomobject]@{ Code = $code; Text = ($output -join "`n") }
}

function Expect-Passes($result, [string]$said, [string]$what)
{
    Check ($result.Code -eq 0 -and $result.Text.Contains($said)) $what $result.Text
}

function Expect-Refused($result, [string]$reason, [string]$what)
{
    Check ($result.Code -ne 0 -and $result.Text.Contains($reason)) $what $result.Text
}

Write-Host 'Which of these this machine has makes no difference to what follows, because the test tells Windows not to look there:'
foreach ($name in 'D3DCompiler_43.dll', 'd3dx9_43.dll')
{
    Write-Host ('  {0} in {1}: {2}' -f $name, [Environment]::SystemDirectory, (Test-Path (Join-Path ([Environment]::SystemDirectory) $name)))
    Write-Host ('  {0} in {1}: {2}' -f $name, "$env:windir\SysWOW64", (Test-Path (Join-Path "$env:windir\SysWOW64" $name)))
}

# --- The shader compiler, in the package and out of it
New-Copy
$result = Run-Test
Expect-Passes $result "The package's own compiler compiled a shader" 'the whole package compiles a shader with its own compiler'
Check ($result.Text.Contains("D3DCompiler_43.dll came from $((Resolve-Path -LiteralPath $work).Path)\")) 'and the compiler it used is the one in the package' $result.Text
Write-Host $result.Text

New-Copy
Remove-Item -LiteralPath (Join-Path $work 'D3DCompiler_43.dll')
$result = Run-Test
Expect-Refused $result "D3DX couldn't compile a small shader" 'with no compiler in the package, no shader compiles'
Write-Host $result.Text

New-Copy
Remove-Item -LiteralPath (Join-Path $work 'd3dx9_43.dll')
Expect-Refused (Run-Test) "d3dx9_43.dll isn't in the package" 'with no D3DX in the package, that is said by name'

# The word that asks for all this has to be the right one. A test that ran its easy part on a mistyped word would pass while checking nothing.
New-Copy
Expect-Refused (Run-Test '--her') 'the only thing this takes is --here' 'a word it does not know is refused'

# --- Every library the package links to
New-Copy
Expect-Passes (Run-Imports) 'Every library the package links to is accounted for' 'the whole package accounts for every library'

New-Copy
Remove-Item -LiteralPath (Join-Path $work 'ogg.dll')
Expect-Refused (Run-Imports) 'needs ogg.dll' 'with a library taken out, that library is named'

New-Copy
Remove-Item -LiteralPath (Join-Path $work 'd3dx9_43.dll')
Expect-Refused (Run-Imports) 'AvP_Classic.exe needs d3dx9_43.dll' "with D3DX taken out, it's named, whatever this machine has"

# --- Microsoft's two files, which we can't build from source, so all we can check is Microsoft's signature on them
New-Copy
$file = Join-Path $work 'D3DCompiler_43.dll'
$bytes = [System.IO.File]::ReadAllBytes($file)
$bytes[$bytes.Length / 2] = $bytes[$bytes.Length / 2] -bxor 1
[System.IO.File]::WriteAllBytes($file, $bytes)
Expect-Refused (Run-Imports) "D3DCompiler_43.dll doesn't carry Microsoft's signature" 'one changed bit in the compiler is refused'

New-Copy
Copy-Item -Force -LiteralPath (Join-Path $work 'ogg.dll') (Join-Path $work 'd3dx9_43.dll')
Expect-Refused (Run-Imports) "d3dx9_43.dll doesn't carry Microsoft's signature" "another library under D3DX's name is refused"

New-Copy
Remove-Item -LiteralPath (Join-Path $work 'D3DCompiler_43.dll')
Expect-Refused (Run-Imports) "D3DCompiler_43.dll isn't in the package" 'the check that reads what links to what notices the compiler too, though nothing links to it'

# --- Microsoft's Visual C++ runtime, which the package carries beside the game (card STATIC-UNDO-1, #125): only the DLLs it needs, each the very
# copy in Visual Studio's redistributable folder, signed by Microsoft. Each way of getting that wrong is tried by itself. The extra files tried
# here are Microsoft's own, from Visual Studio's copy of the runtime and from Windows, as a stray one would be.
New-Copy
Expect-Passes (Run-Imports) "Microsoft's runtime is only the" 'the whole package carries only the runtime it needs, each one checked'

New-Copy
Remove-Item -LiteralPath (Join-Path $work 'vcruntime140.dll')
Expect-Refused (Run-Imports) 'needs vcruntime140.dll' "with the runtime's vcruntime140.dll taken out, it's named, whatever this machine has"

New-Copy
$file = Join-Path $work 'vcruntime140.dll'
$bytes = [System.IO.File]::ReadAllBytes($file)
$bytes[$bytes.Length / 2] = $bytes[$bytes.Length / 2] -bxor 1
[System.IO.File]::WriteAllBytes($file, $bytes)
Expect-Refused (Run-Imports) "vcruntime140.dll doesn't carry Microsoft's signature" 'one changed bit in the runtime is refused'

New-Copy
Copy-Item -Force -LiteralPath (Join-Path $work 'ogg.dll') (Join-Path $work 'vcruntime140.dll')
Expect-Refused (Run-Imports) "vcruntime140.dll doesn't carry Microsoft's signature" "another library under the runtime's name is refused"

# Signed by Microsoft, and not Visual Studio's copy: the check doesn't stop at the signature
New-Copy
Copy-Item -Force -LiteralPath (Join-Path $work 'D3DCompiler_43.dll') (Join-Path $work 'msvcp140.dll')
Expect-Refused (Run-Imports) "msvcp140.dll isn't the copy in Visual Studio's redistributable folder" "another file Microsoft signed, under the runtime's name, is refused"

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$runtimeFolder = Get-ChildItem -Directory (Join-Path $visualStudio 'VC\Redist\MSVC\*\x86\Microsoft.VC*.CRT') | Select-Object -Last 1
if (-not $runtimeFolder) { throw "Visual Studio's copy of Microsoft's runtime (VC\Redist\MSVC\<version>\x86) wasn't found, so the runtime's rows can't be tried" }

# A piece the package may carry, but which nothing in it needs
$unneeded = @('msvcp140_codecvt_ids.dll', 'msvcp140_2.dll', 'msvcp140_1.dll' | Where-Object { -not (Test-Path -LiteralPath (Join-Path $Package $_)) }) | Select-Object -First 1
if (-not $unneeded) { throw "every piece of the runtime the package may carry is in it, so none is left to try as one nothing needs" }
New-Copy
Copy-Item -LiteralPath (Join-Path $runtimeFolder.FullName $unneeded) $work
Expect-Refused (Run-Imports) "the package carries $unneeded, and nothing in it needs it" 'a piece of the runtime that nothing needs is refused'

New-Copy
Copy-Item -LiteralPath (Join-Path $runtimeFolder.FullName 'concrt140.dll') $work
Expect-Refused (Run-Imports) "the package carries concrt140.dll, which isn't one of the runtime DLLs it may carry" "a piece of the runtime that isn't on the list is refused, though it's Microsoft's"

New-Copy
Copy-Item -LiteralPath (Join-Path "$env:windir\SysWOW64" 'ucrtbase.dll') $work
Expect-Refused (Run-Imports) "the package carries ucrtbase.dll, which isn't one of the runtime DLLs it may carry" "Windows' own C runtime in the package is refused: Windows 10 and 11 use theirs"

New-Copy
Copy-Item -LiteralPath (Join-Path $work 'vcruntime140.dll') (Join-Path $work 'vcruntime140d.dll')
Expect-Refused (Run-Imports) "the package carries vcruntime140d.dll, which isn't one of the runtime DLLs it may carry" 'a debug runtime, which no player has, is refused by its name'

# --- No folder of the machine that built it. A copy of a library with one put after its end, which nothing else here notices: the end of the file isn't
# part of what Windows loads, and this library isn't signed. First as plain text, then as UTF-16, the way a program keeps a wide string, then UTF-16 starting
# on an odd byte, and last a /home/ folder from another system, which doesn't count on its own, so it has to pass.
function Add-ToTheEnd([string]$name, [byte[]]$more)
{
    $file = Join-Path $work $name
    $bytes = [System.IO.File]::ReadAllBytes($file)
    [System.IO.File]::WriteAllBytes($file, [byte[]]($bytes + $more))
}

$folder = 'C:\Users\someone\source\AvP\3dc\utilities.cpp'

New-Copy
Add-ToTheEnd 'libsmackerdec.dll' ([System.Text.Encoding]::ASCII.GetBytes("$folder`0"))
Expect-Refused (Run-Imports) 'libsmackerdec.dll, as plain text, names a folder: C:\Users\someone' 'a library naming the folder it was built in, as plain text, is refused'

New-Copy
Add-ToTheEnd 'libsmackerdec.dll' ([System.Text.Encoding]::Unicode.GetBytes("$folder`0"))
Expect-Refused (Run-Imports) 'libsmackerdec.dll, as UTF-16' 'and as UTF-16'

New-Copy
Add-ToTheEnd 'libsmackerdec.dll' ([byte[]](@(0x41) + [System.Text.Encoding]::Unicode.GetBytes("$folder`0")))
Expect-Refused (Run-Imports) 'names a folder: C:\Users\someone' 'and as UTF-16 starting on an odd byte'

New-Copy
Add-ToTheEnd 'AvP_Classic.exe' ([System.Text.Encoding]::ASCII.GetBytes("D:\a\AvP\AvP\vcpkg_installed\x86-windows\bin\ogg.pdb`0"))
Expect-Refused (Run-Imports) 'AvP_Classic.exe, as plain text, names a folder: D:\a\AvP' "the game naming a .pdb in a build runner's folder is refused"

New-Copy
Add-ToTheEnd 'libsmackerdec.dll' ([System.Text.Encoding]::ASCII.GetBytes("see https://github.com/Melanikus/AvP and ogg.pdb`0"))
Expect-Passes (Run-Imports) 'none names a folder on the machine that built it' "a web address, and a .pdb's name with no folder, aren't refused"

if (Test-Path -LiteralPath $work) { Remove-Item -Recurse -Force -LiteralPath $work }

Write-Host "$($script:passes) passed, $($script:failures) failed"

# Said outright, because GitHub ends a step by exiting with whatever the last program to run left behind, and the last one here was meant to refuse.
if ($script:failures -gt 0) { exit 1 }
exit 0
