<#
Builds the game from a clean copy of the repository and makes the package a tester gets. Nothing else builds it this way.

There is one script so that there's one way of doing it: every pull request runs this (game-build.yml), and the owner's test build
runs this too (build.yml). So a pull request proves the very steps a test build will take, before anything is merged. Without this,
the first time those steps ran would be after the merge.

It fetches vcpkg at the commit vcpkg.json names, so the libraries can't drift, and builds them from their source if there's no copy
of them already saved on the machine. The first build on a clean machine takes a while.

What it leaves in -OutFolder: the program, its libraries, the shaders, the notes, each library's licence, and a list of every file's hash.

WHAT GOES ON THE NETWORK, so that nobody is surprised when security software asks: git fetches vcpkg from github.com. vcpkg's bootstrap
then runs its own scripts\tls12-download.exe to fetch vcpkg.exe from github.com. And vcpkg.exe fetches each library's source and the
tools to build it. The programs are all Microsoft's own, and vcpkg's list is at the commit vcpkg.json names, but on your
own machine they'll look like unknown programs going online.

-UseVcpkgAt is for trying this on your own machine with a vcpkg you already have, such as the one inside Visual Studio. It skips the
first two of those, not the third. The checks don't use it: they always fetch the pinned one.
#>
param(
    [Parameter(Mandatory = $true)][string]$OutFolder,
    [string]$UseVcpkgAt = ''
)

$ErrorActionPreference = 'Stop'

$repository = Split-Path -Parent $PSScriptRoot
Set-Location $repository

# --- vcpkg, at the manifest's baseline. bootstrap-vcpkg checks the signature of the vcpkg program it downloads.
$baseline = (Get-Content vcpkg.json -Raw | ConvertFrom-Json).'builtin-baseline'
# It goes to git, so it has to be a commit's full hash first: a value starting with a dash would be read by git as an option (BASELINE-1, added 2026-11-07)
if ("$baseline" -cnotmatch '^[0-9a-f]{40}$') { throw "vcpkg.json's builtin-baseline isn't a commit's full hash: $baseline" }
$temporary = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [System.IO.Path]::GetTempPath() }
$vcpkgRoot = if ($UseVcpkgAt) { $UseVcpkgAt } else { Join-Path $temporary 'vcpkg' }

if (-not $UseVcpkgAt) {
    git init -q $vcpkgRoot
    git -C $vcpkgRoot fetch -q --depth 1 https://github.com/microsoft/vcpkg.git $baseline
    git -C $vcpkgRoot checkout -q FETCH_HEAD
    if ((git -C $vcpkgRoot rev-parse HEAD) -cne $baseline) { throw 'vcpkg is not at the manifest baseline' }

    # The likeliest step to fail on a machine that guards its network, so it says what happened in plain words. Older PowerShell treats
    # the error text bootstrap prints as a failure of its own, which is why this asks both ways.
    $bootstrapFailed = $false
    try {
        & (Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat') -disableMetrics
        $bootstrapFailed = $LASTEXITCODE -ne 0
    }
    catch {
        $bootstrapFailed = $true
    }

    if ($bootstrapFailed -or -not (Test-Path (Join-Path $vcpkgRoot 'vcpkg.exe'))) {
        throw "vcpkg could not be set up, so nothing was built. Its bootstrap uses scripts\tls12-download.exe to fetch vcpkg.exe from github.com, and that download failed or was blocked. Don't retry this blindly: see what stopped it. On your own machine, -UseVcpkgAt can use a vcpkg you already have."
    }
}

# The game's project file looks here for vcpkg
$env:VCPKG_ROOT = $vcpkgRoot

# --- Which commit this is, for the log a tester sends us. The code carries on if this file isn't there, and it's never committed.
$commit = (git rev-parse --short HEAD)
Set-Content -Encoding Ascii -Path '3dc\build_commit.h' -Value "#define AVP_BUILD_COMMIT `"$commit`""

# --- The libraries, with vcpkg run here and not from inside the build. MSBuild reads everything vcpkg prints, and fails the build if a line
# looks like an error, even when vcpkg recovers. On the first run on a runner, one download mirror failed, vcpkg got the file from another and
# finished with success, and the build still stopped. vcpkg's own answer is the one that counts.
& (Join-Path $vcpkgRoot 'vcpkg.exe') install --triplet x86-windows --x-install-root="$(Join-Path $repository 'vcpkg_installed')" --x-wait-for-lock --disable-metrics
if ($LASTEXITCODE -ne 0) { throw 'The libraries did not build' }

# --- The game. The libraries are already there, in the folder the project looks in, so the project doesn't need to install them again.
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) { throw 'MSBuild was not found' }

# --- Which build this is, for the top of the game's log (3dc/build_identity.cpp). The release workflow gives a version in VERSION; every
# other build is a test build. Only what tools/check-release.ps1 lets through is written, in the same two forms, letter for letter: three
# numbers with dots between, such as 1.0.0, or a beta, those and -beta. and its number, such as 1.0.0-beta.0. So nothing in it can break
# the C++ it goes into. Never four numbers: the log leaves out anything that looks like a network address (log_files.h).
$commit = (git rev-parse HEAD).Trim()
if ($commit -cnotmatch '^[0-9a-f]{40}$') { throw "The commit doesn't look like one: $commit" }
$version = if ($env:VERSION -and ($env:VERSION -cmatch '^(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})\z' -or $env:VERSION -cmatch '^(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})-beta\.(0|[1-9][0-9]{0,3})\z')) { $env:VERSION } else { 'a test build, not a release' }
@(
    '// Written by tools/build-and-package.ps1 for this build. Not in the repository.'
    "#define AVP_RELEASE_VERSION `"$version`""
    "#define AVP_RELEASE_COMMIT `"$commit`""
) | Out-File -Encoding ascii '3dc\build_version.h'

& $msbuild win32\VS2026\AvP.sln /nologo /v:minimal /m /p:Configuration=Release /p:Platform=Win32 /p:VcpkgManifestInstall=false
if ($LASTEXITCODE -ne 0) { throw 'The game did not build' }

# --- The package: only what the game needs, plus a hash of every file so anyone can check their copy
New-Item -ItemType Directory -Force $OutFolder | Out-Null
$release = 'win32\VS2026\Win32\Release'

# The package is laid out as the game folder is, so a tester unzips it over theirs and lets it replace what's there. Steam starts
# bin\AvP_Classic.exe, so the game goes there under that name, with its libraries beside it: Windows looks for a library in the
# program's own folder first, and D3DX looks there for D3DCompiler_43.dll. The shaders go at the top: the game reads them from the
# folder Steam starts it in, which is the game folder. Steam's "Verify integrity of game files" puts its own back. (2026-11-07.)
$programFolder = Join-Path $OutFolder 'bin'
New-Item -ItemType Directory -Force $programFolder | Out-Null
Copy-Item (Join-Path $release 'AvPx.exe') (Join-Path $programFolder 'AvP_Classic.exe')
Copy-Item (Join-Path $release '*.dll') $programFolder

# --- Microsoft's C++ runtime, beside the game, so a tester installs nothing (card STATIC-UNDO-1, #125). The game and its libraries are built
# with /MD, so they take Microsoft's C and C++ runtime from DLLs. Windows 10 and 11 have the C part, the Universal C runtime, as part of
# themselves. They don't have the C++ part, vcruntime140.dll and the C++ standard library's msvcp140*.dll, so those are copied here, unchanged,
# from the redistributable folder of the Visual Studio that has just built the game: Microsoft lets a program carry them (licences/README.txt).
# Only what something in bin asks for is copied, and then what those ask for in turn, until nothing new is asked for.
# tools/check-package-imports.ps1 then refuses any that isn't on its list, isn't needed, isn't that very file, or isn't signed by Microsoft.
$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$dumpbin = Get-ChildItem (Join-Path $visualStudio 'VC\Tools\MSVC\*\bin\Hostx64\x86\dumpbin.exe') | Select-Object -Last 1
if (-not $dumpbin) { throw 'dumpbin was not found' }

# Where Visual Studio keeps them: VC\Redist\MSVC\<version>\x86\Microsoft.VC<toolset>.CRT, at the version its own file
# Microsoft.VCRedistVersion.default.txt names, which goes with the compiler it builds with. tools/check-package-imports.ps1 finds it the same
# way, in the same lines.
$redistVersion = "$(Get-Content -LiteralPath (Join-Path $visualStudio 'VC\Auxiliary\Build\Microsoft.VCRedistVersion.default.txt') -TotalCount 1)".Trim()
if ($redistVersion -cnotmatch '^[0-9]+\.[0-9]+\.[0-9]+$') { throw "Visual Studio's Microsoft.VCRedistVersion.default.txt doesn't name a version: $redistVersion" }
$runtimeFolders = @(Get-ChildItem -Directory -LiteralPath (Join-Path $visualStudio "VC\Redist\MSVC\$redistVersion\x86") -Filter 'Microsoft.VC*.CRT')
if ($runtimeFolders.Count -ne 1) { throw "Visual Studio's redistributable folder for $redistVersion doesn't have exactly one Microsoft.VC*.CRT folder for x86" }
$runtimeFolder = $runtimeFolders[0].FullName

do {
    $copiedThisTime = 0

    foreach ($binary in @(Get-ChildItem -LiteralPath $programFolder -File | Where-Object { $_.Extension -in '.exe', '.dll' })) {
        $output = & $dumpbin.FullName /nologo /dependents $binary.FullName
        if ($LASTEXITCODE -ne 0) { throw "dumpbin could not read $($binary.Name)" }

        # The names are the only indented lines ending in .dll, as tools/check-package-imports.ps1 reads them. Only the runtime's C++ parts.
        foreach ($name in @($output | Where-Object { $_ -match '^\s+\S+\.dll\s*$' } | ForEach-Object { $_.Trim().ToLowerInvariant() })) {
            if ($name -notmatch '^(vcruntime|msvcp|concrt|vccorlib)[0-9a-z_]*\.dll$') { continue }
            if (Test-Path -LiteralPath (Join-Path $programFolder $name)) { continue }

            $from = Join-Path $runtimeFolder $name
            if (-not (Test-Path -LiteralPath $from -PathType Leaf)) { throw "$($binary.Name) needs $name, and it isn't in Visual Studio's redistributable folder, $runtimeFolder" }
            Copy-Item -LiteralPath $from -Destination (Join-Path $programFolder $name)
            "Microsoft's runtime: $name, because $($binary.Name) needs it, from $runtimeFolder"
            $copiedThisTime++
        }
    }
} while ($copiedThisTime -gt 0)

Copy-Item -Recurse shaders (Join-Path $OutFolder 'shaders')
Copy-Item PLAYTEST.md, License.txt $OutFolder

# The servers the game plays online through (#175), beside bin, where the game looks for them (3dc/servers_cfg_file.h). The one at the
# repository's root is the release's (with #175's last part, tools/check-online-play.ps1 holds it to its rules; until then it names no
# server). Added 2026-11-10 by Melanikus for the AvP port.
if (-not (Test-Path -LiteralPath 'servers.cfg' -PathType Leaf)) { throw 'servers.cfg is missing from the repository, so the package would have no servers file' }
Copy-Item -LiteralPath 'servers.cfg' -Destination (Join-Path $OutFolder 'servers.cfg')

# --- Each library's own licence, in the folder licences, copied from the very files the build used. licences/README.txt names each
# program and library in bin, says what it is and which licences go with it, and, for the ones under the GNU licences, where their
# source is. The libraries ask for this: a copy of a program has to come with their notices. The build stops if something in bin
# isn't known here, or if a licence copied here isn't named for anything in bin, so neither a new library nor a moved one can go out
# without its licence. (LICENCE-NOTICES-1, the user's ruling, 2026-11-08: fixed before the first release.)
$installed = 'vcpkg_installed\x86-windows\share'
$licences = [ordered]@{
    'ENet.txt'          = "$installed\enet\copyright"
    'SipHash.txt'       = 'vcpkg-overlay-ports\enet\siphash\LICENSE_CC0'
    'OpenAL-Soft.txt'   = "$installed\openal-soft\copyright"
    'fmt.txt'           = "$installed\fmt\copyright"
    'libogg.txt'        = "$installed\libogg\copyright"
    'libvorbis.txt'     = "$installed\libvorbis\copyright"
    'PThreads4W.txt'    = "$installed\pthreads\copyright"
    'PThreads4W-NOTICE.txt'       = "$installed\pthreads\NOTICE"         # installed by our port, for the Apache License's 4(d)
    'PThreads4W-CONTRIBUTORS.txt' = "$installed\pthreads\CONTRIBUTORS"   # which its NOTICE names
    'libbinkdec.txt'    = 'dependencies\libbinkdec\COPYING'
    'LGPL-2.1.txt'      = 'dependencies\libbinkdec\COPYING'   # the GNU Lesser GPL 2.1, the version this project takes OpenAL Soft under
    'libsmackerdec.txt' = 'dependencies\libsmackerdec\COPYING'
    'D3DX.txt'          = "$installed\dxsdk-d3dx\copyright"
    'Microsoft-VC-Runtime.txt' = 'notices\Microsoft-VC-Runtime.txt'   # ours: what the files are, and where Microsoft's terms are
}

$licenceFolder = Join-Path $OutFolder 'licences'
New-Item -ItemType Directory -Force $licenceFolder | Out-Null

foreach ($name in $licences.Keys) {
    $from = $licences[$name]
    if (-not (Test-Path -LiteralPath $from -PathType Leaf) -or (Get-Item -LiteralPath $from).Length -eq 0) { throw "$from, the licence for licences/$name, isn't there" }
    Copy-Item -LiteralPath $from -Destination (Join-Path $licenceFolder $name)
}

# Where OpenAL Soft's source is. vcpkg's record of what it built (vcpkg.spdx.json) names the place its source came from, and at which
# version, checked for its shape before it's written down. It's built from this project's own port, vcpkg-overlay-ports/openal-soft,
# which changes it, so that folder, at this commit, is the rest of its source.
$record = Get-Content -LiteralPath "$installed\openal-soft\vcpkg.spdx.json" -Raw | ConvertFrom-Json
$places = @($record.packages | ForEach-Object { "$($_.downloadLocation)" })
$openalSource = @($places | Where-Object { $_ -cmatch '^git\+https://github\.com/kcat/openal-soft@[0-9A-Za-z._-]+$' })
if ($openalSource.Count -ne 1) { throw "vcpkg's record of OpenAL Soft doesn't say where its source came from: $($places -join ', ')" }
$openalVersion = $openalSource[0].Split('@')[1]

# OpenAL Soft's source goes with it, in licences/sources/openal-soft, because it isn't all in this repository: upstream's archive, the very
# file vcpkg downloaded and checked against the SHA-512 our port names, and our port and triplet, which change it and say how it's built.
# vcpkg keeps its downloads in its own folder, or where VCPKG_DOWNLOADS says, or, for the copy inside Visual Studio, in the user's own.
$openalPortFolder = 'vcpkg-overlay-ports\openal-soft'
$portText = [System.IO.File]::ReadAllText((Join-Path $openalPortFolder 'portfile.cmake'))
$portSums = @([regex]::Matches($portText, '(?m)^\s*SHA512\s+([0-9a-f]{128})\s*$') | ForEach-Object { $_.Groups[1].Value })
if ($portSums.Count -ne 1) { throw "vcpkg-overlay-ports/openal-soft/portfile.cmake doesn't name one SHA-512 for OpenAL Soft's source" }
$downloadFolders = @($env:VCPKG_DOWNLOADS, (Join-Path $vcpkgRoot 'downloads'), $(if ($env:LOCALAPPDATA) { Join-Path $env:LOCALAPPDATA 'vcpkg\downloads' })) | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Container) }
$openalArchives = @($downloadFolders | ForEach-Object { Get-ChildItem -LiteralPath $_ -File -Filter 'kcat-openal-soft-*.tar.gz' } | Where-Object { (Get-FileHash -Algorithm SHA512 -LiteralPath $_.FullName).Hash.ToLower() -ceq $portSums[0] } | Select-Object -First 1)
if ($openalArchives.Count -ne 1) { throw "OpenAL Soft's source archive, with the SHA-512 our port names, isn't in vcpkg's downloads ($($downloadFolders -join ', ')). vcpkg only downloads it when it builds OpenAL Soft, not when it reuses a build it kept, so build once with nothing kept" }
$openalArchive = $openalArchives[0].Name
$openalSourceFolder = Join-Path $licenceFolder 'sources\openal-soft'
New-Item -ItemType Directory -Force (Join-Path $openalSourceFolder 'port') | Out-Null
Copy-Item -LiteralPath $openalArchives[0].FullName -Destination (Join-Path $openalSourceFolder $openalArchive)
Copy-Item -Path (Join-Path $openalPortFolder '*') -Destination (Join-Path $openalSourceFolder 'port') -Recurse
Copy-Item -LiteralPath 'vcpkg-overlay-triplets\x86-windows.cmake' -Destination (Join-Path $openalSourceFolder 'port\triplet-x86-windows.cmake')

# Where this repository is, so our own changes to OpenAL Soft, and our two film libraries, can be found at the very commit this package
# was built from
$here = if ($env:GITHUB_SERVER_URL -and $env:GITHUB_REPOSITORY) { "$env:GITHUB_SERVER_URL/$env:GITHUB_REPOSITORY" } else { 'https://github.com/Melanikus/AvP' }

$whatEachIs = @{
    'avp_classic.exe'    = "the game. Rebellion's and Barry Duncan's terms for it are in License.txt, at the top. Built into it: ENet (licences/ENet.txt, MIT), changed by this project's own patches, which are in its source in vcpkg-overlay-ports/enet, and the reference SipHash that one of them adds (licences/SipHash.txt, CC0)."
    'openal32.dll'       = "OpenAL Soft $openalVersion (licences/OpenAL-Soft.txt: the GNU Library General Public License, version 2, and then the licences of some of its parts). It may be used under version 2 or any later version, and this project uses it under the GNU Lesser General Public License, version 2.1 (licences/LGPL-2.1.txt). Its source is here with it: upstream's, as vcpkg downloaded and checked it, in licences/sources/openal-soft/$openalArchive, and this project's own changes to it and the settings it's built with in licences/sources/openal-soft/port, which are also at $here/tree/$commit/vcpkg-overlay-ports/openal-soft. You may put your own build of OpenAL Soft in its place."
    'fmt.dll'            = 'fmt, which OpenAL Soft uses (licences/fmt.txt, MIT).'
    'ogg.dll'            = 'libogg (licences/libogg.txt, BSD).'
    'vorbis.dll'         = 'libvorbis, changed by this project''s own patches in vcpkg-overlay-ports/libvorbis (licences/libvorbis.txt, BSD).'
    'vorbisfile.dll'     = 'libvorbis, changed by this project''s own patches in vcpkg-overlay-ports/libvorbis (licences/libvorbis.txt, BSD).'
    'vorbisenc.dll'      = 'libvorbis, changed by this project''s own patches in vcpkg-overlay-ports/libvorbis (licences/libvorbis.txt, BSD).'
    'pthreadvc3.dll'     = 'PThreads4W, built by this project''s own port in vcpkg-overlay-ports/pthreads (licences/PThreads4W.txt, the Apache License 2.0, with its notice in licences/PThreads4W-NOTICE.txt and the contributors that names in licences/PThreads4W-CONTRIBUTORS.txt).'
    'libbinkdec.dll'     = "libbinkdec, Barry Duncan's player for Bink films (licences/libbinkdec.txt, the GNU Lesser General Public License). Its source: $here/tree/$commit/dependencies/libbinkdec. You may put your own build of it in its place."
    'libsmackerdec.dll'  = "libsmackerdec, Barry Duncan's player for Smacker films (licences/libsmackerdec.txt, the GNU Lesser General Public License). Its source: $here/tree/$commit/dependencies/libsmackerdec. You may put your own build of it in its place."
    'd3dx9_43.dll'       = "Microsoft's D3DX (licences/D3DX.txt, Microsoft's licence terms for it). Using this file means accepting those terms."
    'd3dcompiler_43.dll' = "Microsoft's D3DX shader compiler (licences/D3DX.txt, Microsoft's licence terms for it). Using this file means accepting those terms."
}

# Microsoft's Visual C++ runtime, copied above: the same words for each piece, with the version it came from
foreach ($name in 'vcruntime140.dll', 'msvcp140.dll', 'msvcp140_1.dll', 'msvcp140_2.dll', 'msvcp140_atomic_wait.dll', 'msvcp140_codecvt_ids.dll') {
    $whatEachIs[$name] = "part of Microsoft's Visual C++ runtime, $redistVersion, copied unchanged and with Microsoft's signature from the redistributable folder of the Visual Studio that built the game (licences/Microsoft-VC-Runtime.txt, which says where Microsoft's terms for it are)."
}

$aboutLines = @(foreach ($binary in @(Get-ChildItem -LiteralPath $programFolder -File | Where-Object { $_.Extension -in '.exe', '.dll' } | Sort-Object Name)) {
    $about = $whatEachIs[$binary.Name.ToLowerInvariant()]
    if (-not $about) { throw "bin\$($binary.Name) is in the package, and its licence isn't known here. Add it to the licences above, from the file the build used" }
    "bin/$($binary.Name): $about"
})

foreach ($name in $licences.Keys) {
    if (@($aboutLines | Where-Object { $_.Contains("licences/$name") }).Count -eq 0) { throw "licences/$name is copied, and nothing in bin names it. If its library is now inside another one, name it on that one's line" }
}

$readme = @(
    "The libraries this game is built with, and each one's licence. Each file in this folder is that library's own licence, copied from the"
    'files the build used. Each line below names a program or library in bin, says what it is and which licences go with it, and, for the'
    'ones under the GNU licences, where their source is.'
    ''
) + $aboutLines
[System.IO.File]::WriteAllLines((Join-Path $licenceFolder 'README.txt'), [string[]]$readme)

# Our game files, laid out as the game folder wants them (package-files/README.md). package-files/SHA256SUMS.txt lists each one with its
# hash, and only those go in. Each is checked against its line before it's copied, so a file changed without its line stops the build,
# and a file in the folder that isn't on the list stops it too, rather than being left out without a word. (Added 2026-11-07.)
$gameFiles = @(Get-Content -LiteralPath 'package-files\SHA256SUMS.txt' | Where-Object { $_ })
if ($gameFiles.Count -eq 0) { throw 'package-files/SHA256SUMS.txt lists no game files' }
$listedNames = @{}

foreach ($line in $gameFiles) {
    $hash, $name = $line -split '  ', 2

    # A line names a plain file inside the folder: parts of letters, digits, _ . - and spaces, between /, each starting with a letter,
    # a digit or _. So no drive, no root, no way out with .., and nothing PowerShell would read as a wildcard. And it's one of the game
    # files' own kinds (.rif, .rim, .ffl) in one of their own folders, so a line can't name the notes, a shader or the program and have
    # it written over, since the game files are copied in last.
    if ($hash -cnotmatch '^[0-9a-f]{64}$' -or "$name" -cnotmatch '^[A-Za-z0-9_][A-Za-z0-9_ .-]*(/[A-Za-z0-9_][A-Za-z0-9_ .-]*)*$' -or "$name" -notmatch '(?i)^(avp_huds|graphics|MelPyth)/.+\.(rif|rim|ffl)$') {
        throw "package-files/SHA256SUMS.txt has a line that isn't a hash and a file in the folder: $line"
    }

    $from = Join-Path 'package-files' $name

    if (-not (Test-Path -LiteralPath $from -PathType Leaf)) { throw "package-files/SHA256SUMS.txt lists $name, and it isn't in package-files" }
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $from).Hash.ToLower() -cne $hash) { throw "package-files/$name isn't the file package-files/SHA256SUMS.txt lists" }

    $to = Join-Path $OutFolder $name
    New-Item -ItemType Directory -Force (Split-Path -Parent $to) | Out-Null
    Copy-Item -LiteralPath $from -Destination $to
    $listedNames[($name -replace '\\', '/').ToLower()] = $true
}

# git keeps every one of them as a plain file. A link (mode 120000) would be followed, to whatever it points at.
# If git can't answer, or answers with less than the folder has to hold (every listed file, and its 3 notes), that's a stop too, not a
# pass: an empty answer would otherwise look like a folder with nothing wrong in it.
$entries = @(git ls-files -s -- package-files)
if ($LASTEXITCODE -ne 0) { throw "git couldn't list package-files (exit $LASTEXITCODE)" }
if ($entries.Count -lt $gameFiles.Count + 3) { throw "git lists $($entries.Count) entries in package-files, and it has to hold $($gameFiles.Count) game files and 3 notes" }

foreach ($entry in $entries) {
    if ($entry -cnotmatch '^100644 ') { throw "package-files has something git doesn't keep as a plain file: $entry" }
}

$folder = (Resolve-Path -LiteralPath 'package-files').Path

foreach ($file in @(Get-ChildItem -LiteralPath 'package-files' -Recurse -File)) {
    $name = $file.FullName.Substring($folder.Length + 1) -replace '\\', '/'

    if ($name -cin 'README.md', 'SHA256SUMS.txt', '.gitattributes') { continue }
    if (-not $listedNames.ContainsKey($name.ToLower())) { throw "package-files/$name isn't on package-files/SHA256SUMS.txt, so it wouldn't go in the package" }
}

if (-not (Test-Path (Join-Path $programFolder 'AvP_Classic.exe'))) { throw 'the game was not built, so there is no bin\AvP_Classic.exe' }

# Nothing links to this one: D3DX loads it by name while the game is running, so losing it would give no error here. Without it the game
# can't compile its shaders on a Windows that doesn't already have it.
if (-not (Test-Path (Join-Path $programFolder 'D3DCompiler_43.dll'))) { throw 'D3DCompiler_43.dll is not in the package''s bin' }

# The program and its libraries are all in bin, so that's the folder whose links are read
& (Join-Path $PSScriptRoot 'check-package-imports.ps1') -Package $programFolder

"$env:GITHUB_REF_NAME $(git rev-parse HEAD)" | Out-File -Encoding ascii (Join-Path $OutFolder 'BUILD.txt')

Push-Location $OutFolder
# The hashes are worked out first and written after. Writing the list while the folder is being read would make it hash its own
# half-written file, and that fails with a sharing violation.
$hashes = Get-ChildItem -Recurse -File | Resolve-Path -Relative | ForEach-Object {
    '{0}  {1}' -f (Get-FileHash -Algorithm SHA256 -LiteralPath $_).Hash.ToLower(), ($_ -replace '^\.\\', '' -replace '\\', '/')
}
$hashes | Out-File -Encoding ascii SHA256SUMS.txt
Pop-Location

Get-Content (Join-Path $OutFolder 'SHA256SUMS.txt')
