<#
Builds the game from a clean copy of the repository and makes the package a tester gets. Nothing else builds it this way.

There is one script so that there's one way of doing it: every pull request runs this (game-build.yml), and the owner's test build
runs this too (build.yml). So a pull request proves the very steps a test build will take, before anything is merged. Without this,
the first time those steps ran would be after the merge.

It fetches vcpkg at the commit vcpkg.json names, so the libraries can't drift, and builds them from their source if there's no copy
of them already saved on the machine. The first build on a clean machine takes a while.

What it leaves in -OutFolder: the program, its libraries, the shaders, the notes, and a list of every file's hash.

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
# other build is a test build. Only what tools/check-release.ps1 lets through is written: three numbers with dots between, or a beta's
# name, betarelease and the day as eight digits, such as betarelease20261107. So nothing in it can break the C++ it goes into. Never four
# numbers: the log leaves out anything that looks like a network address (log_files.h).
$commit = (git rev-parse HEAD).Trim()
if ($commit -cnotmatch '^[0-9a-f]{40}$') { throw "The commit doesn't look like one: $commit" }
$version = if ($env:VERSION -and ($env:VERSION -cmatch '^[0-9]{1,6}\.[0-9]{1,6}\.[0-9]{1,6}\z' -or $env:VERSION -cmatch '^betarelease[0-9]{8}\z')) { $env:VERSION } else { 'a test build, not a release' }
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
Copy-Item -Recurse shaders (Join-Path $OutFolder 'shaders')
Copy-Item PLAYTEST.md, License.txt $OutFolder

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
