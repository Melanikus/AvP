<#
Checks that a package has every library it needs, or that Windows or Microsoft's runtime will supply it. Goes online for nothing.

A program can't start if a library it links to is missing, and the same goes for each library it brings along. A machine that has built the
game before tends to have copies of everything, so a missing file wouldn't show there, and a tester would be the first to find out. So this
reads the list of libraries inside each program and library in the package, and doesn't look at what's installed on this machine at all.

Each name has to be in the package, or on one of the two lists below. A new one fails until someone has decided where it comes from, and added
it to the package or to a list, on purpose. That's the point: a file from an old DirectX runtime, which a clean Windows doesn't have, can't
slip through because a developer's machine happens to have it.

WHAT IT CAN'T SEE: a library loaded by name while the program is running, such as D3DCompiler_43.dll. tests/package_test.cpp checks that one.
#>
param(
    [Parameter(Mandatory = $true)][string]$Package
)

$ErrorActionPreference = 'Stop'

# Part of Windows itself, from Windows 8 on, which is what PLAYTEST.md asks for
$partOfWindows = @(
    'advapi32.dll', 'avrt.dll', 'd3d9.dll', 'gdi32.dll', 'kernel32.dll', 'msvcrt.dll', 'ole32.dll', 'shell32.dll', 'user32.dll', 'winmm.dll',
    'ws2_32.dll', 'xinput1_4.dll'
)

# Microsoft's Visual C++ Redistributable, which PLAYTEST.md tells a tester to install from Microsoft
$visualCRuntime = @('msvcp140.dll', 'msvcp140_atomic_wait.dll', 'vcruntime140.dll')

function Test-SuppliedElsewhere([string]$name) {
    $lower = $name.ToLowerInvariant()

    # Windows' own names for its C runtime, which the same redistributable provides on the oldest Windows we allow
    return ($partOfWindows -contains $lower) -or ($visualCRuntime -contains $lower) -or ($lower -like 'api-ms-win-*')
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$dumpbin = Get-ChildItem (Join-Path $visualStudio 'VC\Tools\MSVC\*\bin\Hostx64\x86\dumpbin.exe') | Select-Object -Last 1
if (-not $dumpbin) { throw 'dumpbin was not found' }

$files = @(Get-ChildItem $Package -File | Where-Object { $_.Extension -in '.exe', '.dll' })
$inPackage = $files | ForEach-Object { $_.Name.ToLowerInvariant() }
$missing = @()
$namesRead = 0

foreach ($file in $files) {
    $output = & $dumpbin.FullName /nologo /dependents $file.FullName
    if ($LASTEXITCODE -ne 0) { throw "dumpbin could not read $($file.Name)" }

    # The names are the only indented lines ending in .dll. The line naming the file itself starts "Dump of file".
    $names = @($output | Where-Object { $_ -match '^\s+\S+\.dll\s*$' } | ForEach-Object { $_.Trim() })
    $namesRead += $names.Count

    foreach ($name in $names) {
        if (($inPackage -notcontains $name.ToLowerInvariant()) -and -not (Test-SuppliedElsewhere $name)) {
            $missing += "$($file.Name) needs $name"
        }
    }
}

# With nothing read, everything would have passed, which would prove nothing
if ($files.Count -lt 2 -or $inPackage -notcontains 'avpx.exe' -or $namesRead -eq 0) {
    throw "This doesn't look like a package: $($files.Count) programs and libraries, $namesRead names read in $Package"
}

if ($missing.Count -gt 0) {
    $missing | ForEach-Object { "MISSING: $_" }
    throw "$($missing.Count) libraries aren't in the package, and Windows and Microsoft's runtime don't supply them. Add each to the package, or to a list in this script if it really is part of Windows."
}

"Every library the package links to is accounted for: $($files.Count) files, $namesRead names read"
