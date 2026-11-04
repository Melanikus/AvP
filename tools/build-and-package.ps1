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
$temporary = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [System.IO.Path]::GetTempPath() }
$vcpkgRoot = if ($UseVcpkgAt) { $UseVcpkgAt } else { Join-Path $temporary 'vcpkg' }

if (-not $UseVcpkgAt) {
    git init -q $vcpkgRoot
    git -C $vcpkgRoot fetch -q --depth 1 https://github.com/microsoft/vcpkg.git $baseline
    git -C $vcpkgRoot checkout -q FETCH_HEAD
    if ((git -C $vcpkgRoot rev-parse HEAD) -ne $baseline) { throw 'vcpkg is not at the manifest baseline' }

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

& $msbuild win32\VS2026\AvP.sln /nologo /v:minimal /m /p:Configuration=Release /p:Platform=Win32 /p:VcpkgManifestInstall=false
if ($LASTEXITCODE -ne 0) { throw 'The game did not build' }

# --- The package: only what the game needs, plus a hash of every file so anyone can check their copy
New-Item -ItemType Directory -Force $OutFolder | Out-Null
$release = 'win32\VS2026\Win32\Release'

Copy-Item (Join-Path $release 'AvPx.exe') $OutFolder
Copy-Item (Join-Path $release '*.dll') $OutFolder
Copy-Item -Recurse shaders (Join-Path $OutFolder 'shaders')
Copy-Item PLAYTEST.md, License.txt $OutFolder

if (-not (Test-Path (Join-Path $OutFolder 'AvPx.exe'))) { throw 'AvPx.exe was not built' }

"$env:GITHUB_REF_NAME $(git rev-parse HEAD)" | Out-File -Encoding ascii (Join-Path $OutFolder 'BUILD.txt')

Push-Location $OutFolder
# The hashes are worked out first and written after. Writing the list while the folder is being read would make it hash its own
# half-written file, and that fails with a sharing violation.
$hashes = Get-ChildItem -Recurse -File | Resolve-Path -Relative | ForEach-Object {
    '{0}  {1}' -f (Get-FileHash -Algorithm SHA256 $_).Hash.ToLower(), ($_ -replace '^\.\\', '' -replace '\\', '/')
}
$hashes | Out-File -Encoding ascii SHA256SUMS.txt
Pop-Location

Get-Content (Join-Path $OutFolder 'SHA256SUMS.txt')
