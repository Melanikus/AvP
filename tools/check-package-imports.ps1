<#
Checks that a package has every library it needs, or that Windows itself will supply it; that the parts of Microsoft's Visual C++ runtime it
carries are exactly the ones it needs, each the very file Visual Studio hands on and signed by Microsoft; and that the two D3DX files we can't
build from source are really Microsoft's. Goes online for nothing.

A program can't start if a library it links to is missing, and the same goes for each library it brings along. A machine that has built the
game before tends to have copies of everything, so a missing file wouldn't show there, and a tester would be the first to find out. So this
reads the list of libraries inside each program and library in the package, and doesn't look at what's installed on this machine at all.

Each name has to be in the package, or on the list of Windows' own libraries below. A new one fails until someone has decided where it comes
from, and added it to the package or to the list, on purpose. That's the point: a file from an old DirectX runtime, which a clean Windows
doesn't have, can't slip through because a developer's machine happens to have it.

It also refuses a program or library that names a folder on the machine that built it, such as C:\Users\<name>\..., because that would go out
with the game for anyone to read. See the end of this script.

WHAT IT CAN'T SEE: a library loaded by name while the program is running, such as D3DCompiler_43.dll. tests/package_test.cpp checks that one
works, and this only checks that it's there and signed. tests/try-package.ps1 shows that each of these checks refuses when it should.
#>
# -Package is the folder the program and its libraries are in: the package's bin, where the game is AvP_Classic.exe
param(
    [Parameter(Mandatory = $true)][string]$Package
)

$ErrorActionPreference = 'Stop'

# Part of Windows itself, on Windows 10 and 11, which is what PLAYTEST.md asks for. bcrypt.dll is Windows' own cryptography library, there
# since Windows Vista: the game links it because ENet's host_secret.c (vcpkg-overlay-ports/enet, for patch 006) asks it for random
# numbers with BCryptGenRandom.
$partOfWindows = @(
    'advapi32.dll', 'avrt.dll', 'bcrypt.dll', 'd3d9.dll', 'gdi32.dll', 'kernel32.dll', 'msvcrt.dll', 'ole32.dll', 'shell32.dll', 'user32.dll',
    'winmm.dll', 'ws2_32.dll', 'xinput1_4.dll'
)

# Microsoft's Visual C++ runtime. The game and every library built for it take it from DLLs (the /MD switch). It was built into them (/MT)
# for a day, until Bitdefender flagged the game built that way (#111) and the user ruled it out (card STATIC-UNDO-1, #125). It has two parts:
# - The C part, the Universal C runtime: ucrtbase.dll, and the api-ms-win-crt-* names Windows gives it. It's part of Windows 10 and 11, the
#   Windows PLAYTEST.md asks for, and Microsoft's page "Universal CRT deployment" says that there the copy in Windows' own folder is used even
#   when a program brings one. So the package doesn't carry it, and refuses a copy.
# - The C++ part: vcruntime140.dll, and the C++ standard library, msvcp140.dll and its smaller pieces. Windows doesn't have that, so the package
#   carries it, beside the game, and a tester installs nothing. These are the only ones it may carry, and only those something in it needs.
#   tools/build-and-package.ps1 copies exactly those, from the place below.
# Any other name from the runtime is refused wherever it turns up, as a file or as something a file needs: the debug ones, such as
# vcruntime140d.dll and ucrtbased.dll, which no player has; concrt140.dll and vccorlib140.dll, which nothing of ours uses; and any newer
# piece, until someone has looked at it and added it here on purpose.
$runtimeThePackageMayCarry = @('vcruntime140.dll', 'msvcp140.dll', 'msvcp140_1.dll', 'msvcp140_2.dll', 'msvcp140_atomic_wait.dll', 'msvcp140_codecvt_ids.dll')

# Whether a name is any part of Microsoft's Visual C++ runtime: vcruntime*, msvcp*, concrt* and vccorlib* are its C++ parts, and ucrtbase* and
# api-ms-win-crt-* its C part. msvcrt.dll is a different, much older library that is part of Windows, and Microsoft's own D3DX files use it,
# so it isn't one of these.
function Test-MicrosoftRuntime([string]$name) {
    $lower = $name.ToLowerInvariant()

    return ($lower -like 'vcruntime*.dll') -or ($lower -like 'msvcp*.dll') -or ($lower -like 'concrt*.dll') -or ($lower -like 'vccorlib*.dll') -or
        ($lower -like 'ucrtbase*.dll') -or ($lower -like 'api-ms-win-crt-*')
}

# The Universal C runtime as a program asks for it, which Windows 10 and 11 supply. Not ucrtbased.dll, the debug one.
function Test-UniversalCRuntime([string]$name) {
    $lower = $name.ToLowerInvariant()

    return ($lower -ceq 'ucrtbase.dll') -or ($lower -like 'api-ms-win-crt-*')
}

function Test-SuppliedElsewhere([string]$name) {
    $lower = $name.ToLowerInvariant()

    # The other api-ms-win-* names (api-ms-win-core-* and the like) are Windows' own names for parts of itself, so Windows supplies them
    return ($partOfWindows -contains $lower) -or ($lower -like 'api-ms-win-*')
}

# Whether a file carries a signature Windows accepts, made with a certificate held by Microsoft and given out by Microsoft. Windows checks that
# the signature covers every byte of the file and that its certificate leads to a root it trusts. Both names are read from the certificate the
# file carries, so nothing goes online.
function Assert-MicrosoftSignature([string]$path, [string]$name) {
    $signature = Get-AuthenticodeSignature -LiteralPath $path
    $signedBy = if ($signature.SignerCertificate) { $signature.SignerCertificate.Subject } else { 'nobody' }
    $givenBy = if ($signature.SignerCertificate) { $signature.SignerCertificate.Issuer } else { 'nobody' }
    if ($signature.Status -ne 'Valid' -or $signedBy -notmatch '(^|, )O=Microsoft Corporation(,|$)' -or $givenBy -notmatch '(^|, )O=Microsoft Corporation(,|$)') {
        throw "$name doesn't carry Microsoft's signature: its signature is $($signature.Status), by $signedBy, given out by $givenBy"
    }
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$dumpbin = Get-ChildItem (Join-Path $visualStudio 'VC\Tools\MSVC\*\bin\Hostx64\x86\dumpbin.exe') | Select-Object -Last 1
if (-not $dumpbin) { throw 'dumpbin was not found' }

# Where Visual Studio keeps the runtime DLLs it lets a program carry: VC\Redist\MSVC\<version>\x86\Microsoft.VC<toolset>.CRT, at the version its
# own file Microsoft.VCRedistVersion.default.txt names, which goes with the compiler it builds with. tools/build-and-package.ps1 finds it the
# same way, in the same lines.
$redistVersion = "$(Get-Content -LiteralPath (Join-Path $visualStudio 'VC\Auxiliary\Build\Microsoft.VCRedistVersion.default.txt') -TotalCount 1)".Trim()
if ($redistVersion -cnotmatch '^[0-9]+\.[0-9]+\.[0-9]+$') { throw "Visual Studio's Microsoft.VCRedistVersion.default.txt doesn't name a version: $redistVersion" }
$runtimeFolders = @(Get-ChildItem -Directory -LiteralPath (Join-Path $visualStudio "VC\Redist\MSVC\$redistVersion\x86") -Filter 'Microsoft.VC*.CRT')
if ($runtimeFolders.Count -ne 1) { throw "Visual Studio's redistributable folder for $redistVersion doesn't have exactly one Microsoft.VC*.CRT folder for x86" }
$runtimeFolder = $runtimeFolders[0].FullName

$files = @(Get-ChildItem $Package -File | Where-Object { $_.Extension -in '.exe', '.dll' })
$inPackage = $files | ForEach-Object { $_.Name.ToLowerInvariant() }
$missing = @()
$runtimeRefused = @()
$runtimeNeeded = @{}
$namesRead = 0

foreach ($file in $files) {
    if ((Test-MicrosoftRuntime $file.Name) -and ($runtimeThePackageMayCarry -notcontains $file.Name.ToLowerInvariant())) {
        $runtimeRefused += "the package carries $($file.Name.ToLowerInvariant()), which isn't one of the runtime DLLs it may carry"
    }

    $output = & $dumpbin.FullName /nologo /dependents $file.FullName
    if ($LASTEXITCODE -ne 0) { throw "dumpbin could not read $($file.Name)" }

    # The names are the only indented lines ending in .dll. The line naming the file itself starts "Dump of file".
    $names = @($output | Where-Object { $_ -match '^\s+\S+\.dll\s*$' } | ForEach-Object { $_.Trim() })
    $namesRead += $names.Count

    foreach ($name in $names) {
        # Written in small letters, so what's said is the same whichever way a file spells it
        $lower = $name.ToLowerInvariant()

        if (Test-UniversalCRuntime $name) {
            continue
        }
        elseif (Test-MicrosoftRuntime $name) {
            if ($runtimeThePackageMayCarry -notcontains $lower) {
                $runtimeRefused += "$($file.Name) needs $lower, which isn't one of the runtime DLLs the package may carry"
            }
            else {
                $runtimeNeeded[$lower] = $true
                if ($inPackage -notcontains $lower) { $missing += "$($file.Name) needs $lower" }
            }
        }
        elseif (($inPackage -notcontains $lower) -and -not (Test-SuppliedElsewhere $name)) {
            $missing += "$($file.Name) needs $name"
        }
    }
}

# With nothing read, everything would have passed, which would prove nothing
if ($files.Count -lt 2 -or $inPackage -notcontains 'avp_classic.exe' -or $namesRead -eq 0) {
    throw "This doesn't look like a package: $($files.Count) programs and libraries, $namesRead names read in $Package"
}

# A runtime DLL that nothing in the package needs is one more file that could be anything, so it's refused too
foreach ($name in $inPackage) {
    if (($runtimeThePackageMayCarry -contains $name) -and -not $runtimeNeeded.ContainsKey($name)) {
        $runtimeRefused += "the package carries $name, and nothing in it needs it"
    }
}

if ($runtimeRefused.Count -gt 0) {
    $runtimeRefused | ForEach-Object { "MICROSOFT'S RUNTIME: $_" }
    throw "$($runtimeRefused.Count) times the package carries or needs a part of Microsoft's Visual C++ runtime it mustn't. It may carry only $($runtimeThePackageMayCarry -join ', '), each only if something in it needs it."
}

if ($missing.Count -gt 0) {
    $missing | ForEach-Object { "MISSING: $_" }
    throw "$($missing.Count) libraries aren't in the package, and Windows doesn't supply them. Add each to the package, or to the list in this script if it really is part of Windows."
}

# D3DX and its compiler only exist as finished files from Microsoft, so nothing here built them or can scan them. vcpkg checks the hash of the download they
# came in, but a copy already saved on a machine isn't downloaded. Microsoft's signature covers every byte of the file itself, whichever way it got here.
foreach ($name in 'D3DCompiler_43.dll', 'd3dx9_43.dll') {
    $path = Join-Path $Package $name
    if (-not (Test-Path -LiteralPath $path)) { throw "$name isn't in the package" }

    Assert-MicrosoftSignature $path $name
}

# The runtime DLLs the package carries are Microsoft's too, and carry Microsoft's signature. But the certificate Microsoft signs them with, its
# "Windows Software Compatibility Publisher", comes from a part of Microsoft that signs other companies' files as well. So each one also has to
# be, byte for byte, the copy in the redistributable folder of the Visual Studio here, the one that built the game: a file signed by Microsoft
# for anyone else, under a runtime DLL's name, is refused.
foreach ($name in @($inPackage | Where-Object { $runtimeThePackageMayCarry -contains $_ })) {
    $path = Join-Path $Package $name
    Assert-MicrosoftSignature $path $name

    $visualStudiosCopy = Join-Path $runtimeFolder $name
    if (-not (Test-Path -LiteralPath $visualStudiosCopy -PathType Leaf)) { throw "$name isn't in Visual Studio's redistributable folder, $runtimeFolder" }
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash -cne (Get-FileHash -Algorithm SHA256 -LiteralPath $visualStudiosCopy).Hash) {
        throw "$name isn't the copy in Visual Studio's redistributable folder, $runtimeFolder"
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

"Every library the package links to is accounted for: $($files.Count) files, $namesRead names read, Microsoft's runtime is only the $($runtimeNeeded.Count) DLLs it needs, each Visual Studio's own $redistVersion copy with Microsoft's signature, and none names a folder on the machine that built it"
