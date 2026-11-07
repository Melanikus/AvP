<#
Checks that a package has every library it needs, or that Windows or Microsoft's runtime will supply it, and that the two files we can't build
from source are really Microsoft's. Goes online for nothing.

A program can't start if a library it links to is missing, and the same goes for each library it brings along. A machine that has built the
game before tends to have copies of everything, so a missing file wouldn't show there, and a tester would be the first to find out. So this
reads the list of libraries inside each program and library in the package, and doesn't look at what's installed on this machine at all.

Each name has to be in the package, or on one of the two lists below. A new one fails until someone has decided where it comes from, and added
it to the package or to a list, on purpose. That's the point: a file from an old DirectX runtime, which a clean Windows doesn't have, can't
slip through because a developer's machine happens to have it.

It also refuses a program or library that names a folder on the machine that built it, such as C:\Users\<name>\..., because that would go out
with the game for anyone to read. See the end of this script.

WHAT IT CAN'T SEE: a library loaded by name while the program is running, such as D3DCompiler_43.dll. tests/package_test.cpp checks that one
works, and this only checks that it's there and signed. tests/try-package.ps1 shows that each of these checks refuses when it should.
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

# D3DX and its compiler only exist as finished files from Microsoft, so nothing here built them or can scan them. vcpkg checks the hash of the download they
# came in, but a copy already saved on a machine isn't downloaded. Microsoft's signature covers every byte of the file itself, whichever way it got here.
foreach ($name in 'D3DCompiler_43.dll', 'd3dx9_43.dll') {
    $path = Join-Path $Package $name
    if (-not (Test-Path -LiteralPath $path)) { throw "$name isn't in the package" }

    $signature = Get-AuthenticodeSignature -LiteralPath $path
    $signedBy = if ($signature.SignerCertificate) { $signature.SignerCertificate.Subject } else { 'nobody' }
    if ($signature.Status -ne 'Valid' -or $signedBy -notmatch '(^|, )O=Microsoft Corporation(,|$)') {
        throw "$name doesn't carry Microsoft's signature: its signature is $($signature.Status), by $signedBy"
    }
}

# --- No folder of the machine that built it. A program can keep the folders it was built in: an error message given __FILE__ does, and so does a library
# that records where its symbols (.pdb) were. Built on a developer's machine, that's C:\Users\<their name>\..., which would go out with the game for
# anyone to read. So every program and library in the package is read as plain text, and as UTF-16 starting from an even byte and from an odd one,
# for a drive letter, a folder name and a separator, or a Users or source folder between separators. A web address doesn't match, because in "https://"
# the colon is followed by a second slash, not a name. The look-behind, rather than \b, is because .NET's \b counts as letters the accented characters
# that stray bytes read as, and would miss a drive letter after one.
$buildFolder = [regex]::new('(?<![A-Za-z0-9])[A-Za-z]:[\\/][A-Za-z0-9_ .$()~-]+[\\/]|[\\/](?:Users|source)[\\/]', 'IgnoreCase')
$plainText = [System.Text.Encoding]::GetEncoding(28591)
$utf16 = [System.Text.Encoding]::Unicode
$folderNamed = @()

foreach ($file in $files) {
    $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
    $evenLength = $bytes.Length - ($bytes.Length % 2)
    $oddLength = if ($bytes.Length -gt 1) { ($bytes.Length - 1) - (($bytes.Length - 1) % 2) } else { 0 }
    $readings = @(
        @{ How = 'as plain text'; Text = $plainText.GetString($bytes) },
        @{ How = 'as UTF-16'; Text = $utf16.GetString($bytes, 0, $evenLength) },
        @{ How = 'as UTF-16 from an odd byte'; Text = $utf16.GetString($bytes, [Math]::Min(1, $bytes.Length), $oddLength) }
    )

    foreach ($reading in $readings) {
        $lastAt = -1000

        foreach ($match in $buildFolder.Matches($reading.Text)) {
            # One folder can match more than once, as C:\Users\ and again as \source\ further on, so a match within 100 characters of the last is the same one
            if ($match.Index -lt $lastAt + 100) { continue }
            $lastAt = $match.Index

            # What follows it, so that a person can see what it is. Anything that isn't printable is shown as a dot.
            $around = $reading.Text.Substring($match.Index, [Math]::Min(100, $reading.Text.Length - $match.Index)) -replace '[^\x20-\x7e]', '.'
            $folderNamed += "$($file.Name), $($reading.How), names a folder: $around"
        }
    }
}

if ($folderNamed.Count -gt 0) {
    $folderNamed | Select-Object -First 40 | ForEach-Object { "BUILD FOLDER: $_" }
    throw "$($folderNamed.Count) places in the package name a folder on the machine that built it. Name the file by hand rather than with __FILE__, and link with /PDBALTPATH:%_PDB% so that only a .pdb's name is kept."
}

"Every library the package links to is accounted for: $($files.Count) files, $namesRead names read, and none names a folder on the machine that built it"
