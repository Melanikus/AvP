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
Expect-Refused (Run-Imports) 'AvPx.exe needs d3dx9_43.dll' "with D3DX taken out, it's named, whatever this machine has"

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

if (Test-Path -LiteralPath $work) { Remove-Item -Recurse -Force -LiteralPath $work }

Write-Host "$($script:passes) passed, $($script:failures) failed"

# Said outright, because GitHub ends a step by exiting with whatever the last program to run left behind, and the last one here was meant to refuse.
if ($script:failures -gt 0) { exit 1 }
exit 0
