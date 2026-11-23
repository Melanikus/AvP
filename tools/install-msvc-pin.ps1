<#
install-msvc-pin.ps1: makes sure THE ONE COMPILER is on this machine (win32/msvc-pin.props says why: alpha.3 shipped what a newer runner
compiler made, never what had been scanned). Written 2026-11-21 by Melanikus (Frost) for the AvP port, Bishop for the owner.

It reads the pinned version (AvpxMsvcVersion, as 14.51.36231) from win32/msvc-pin.props. If that MSVC is installed in this machine's
Visual Studio (VC\Tools\MSVC\<version>\bin\Hostx64\x86\cl.exe), it says so and does nothing. Else it installs it the way Microsoft
documents, with the Visual Studio Installer this machine already has, from Microsoft's own servers, and nothing else:
  1. The installer is checked first: its Authenticode signature must be valid and Microsoft Corporation's, or nothing is run.
  2. The component is found in this Visual Studio's own catalog: the "MSVC build tools for x64/x86" of that toolset
     (Microsoft.VisualStudio.Component.VC.<major>.<minor>[.<VS version>].x86.x64). Every one the catalog offers for that toolset is
     printed, with the tools package's own version, so a log says what was there.
  3. "setup.exe modify --add <component> --quiet --norestart", timed.
  4. The pinned folder must then be there, with its compiler. The component installs the newest patch Microsoft serves for that
     toolset: if that isn't the pinned one, this stops and says which it got, so the pin is moved on purpose, never by accident.
  5. The compiler's own build is pinned too (AvpxMsvcBuild, as 36263): Microsoft re-services a toolset under the same folder name. Before
     installing, the catalog's tools package must be that build (<toolset>.<build>), and after, cl.exe and link.exe must be it; a folder
     already here with another build stops this too, saying to update Visual Studio's build tools (2026-11-22, editor PR #98). Each of
     those STOPS only on GitHub (GITHUB_ACTIONS "true"), where what ships is built; on a machine of ours it is a warning and this goes on.
Its runtime DLLs come with it (the component's Microsoft.VC.<toolset>.CRT.Redist.X86), in VC\Redist\MSVC: they're listed too.

Run on a build machine before building, from the repository's root: ./tools/install-msvc-pin.ps1. It answers 0 when the pinned MSVC is
there (installed now or before), 1 otherwise. -ListOnly says what is installed and what the catalog offers, and installs nothing.
#>
param([switch]$ListOnly)
$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot

function Stop-Here([string]$why)
{
    Write-Host "install-msvc-pin: STOPPED: $why"
    exit 1
}

# The pinned version, from the one file every project imports
. (Join-Path $PSScriptRoot 'msvc-pin-version.ps1')
$version = Get-MsvcPinVersion $repository
$build = Get-MsvcPinBuild $repository
$toolset = ($version -split '\.')[0..1] -join '.'
Write-Host "install-msvc-pin: the pinned MSVC is $version, its compiler and linker build $build (toolset $toolset)"

# vcpkg builds the libraries with the same compiler: its triplets name the same version (VCPKG_PLATFORM_TOOLSET_VERSION), or nothing is built.
# Both of them: x86-windows, the libraries the game ships, and x64-windows, the HOST triplet vcpkg builds its own build-time tools with
# (vcpkg-cmake and the like: nothing of it ships, but no tool of the build drifts either; 2026-11-22, the alpha.4 release log's
# "Detecting compiler hash for triplet x64-windows" found the image's 14.52.37025)
foreach ($name in 'x86-windows', 'x64-windows')
{
    $triplet = Join-Path $repository "vcpkg-overlay-triplets/$name.cmake"
    if (-not (Test-Path -LiteralPath $triplet -PathType Leaf)) { Stop-Here "there is no ${triplet}: vcpkg would take its own $name triplet, and the newest MSVC with it" }
    if (-not [System.IO.File]::ReadAllText($triplet).Contains("set(VCPKG_PLATFORM_TOOLSET_VERSION `"$version`")"))
    {
        Stop-Here "$triplet doesn't say set(VCPKG_PLATFORM_TOOLSET_VERSION `"$version`"): vcpkg would build with another compiler"
    }
}

# This machine's Visual Studio, as vswhere (Microsoft's, beside the installer) gives it
$installerFolder = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer'
$vswhere = Join-Path $installerFolder 'vswhere.exe'
$setup = Join-Path $installerFolder 'setup.exe'
foreach ($tool in $vswhere, $setup)
{
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) { Stop-Here "$tool is not on this machine" }
    $signature = Get-AuthenticodeSignature -LiteralPath $tool
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch '(^|, )O=Microsoft Corporation(,|$)')
    {
        Stop-Here "$tool is not signed by Microsoft Corporation (signature $($signature.Status), signer $($signature.SignerCertificate.Subject)): nothing is run"
    }
}
$instance = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | ConvertFrom-Json | Select-Object -First 1
if (-not $instance) { Stop-Here 'vswhere finds no Visual Studio with the C++ build tools' }
$visualStudio = $instance.installationPath
Write-Host "install-msvc-pin: Visual Studio $($instance.catalog.productDisplayVersion) at $visualStudio"

$compiler = Join-Path $visualStudio "VC\Tools\MSVC\$version\bin\Hostx64\x86\cl.exe"
# vcpkg's host triplet (vcpkg-overlay-triplets/x64-windows.cmake) builds its build-time tools with the pinned MSVC's x64 compiler: it must be there too,
# or vcpkg stops where it can't find it; this says so first, naming it
$hostCompiler = Join-Path $visualStudio "VC\Tools\MSVC\$version\bin\Hostx64\x64\cl.exe"
function Stop-WithoutHostCompiler
{
    if (-not (Test-Path -LiteralPath $hostCompiler -PathType Leaf))
    {
        Stop-Here "MSVC $version has no x64 compiler ($hostCompiler), which vcpkg's host triplet x64-windows builds its tools with"
    }
}
function Say-Installed
{
    $tools = @(Get-ChildItem -LiteralPath (Join-Path $visualStudio 'VC\Tools\MSVC') -Directory -ErrorAction SilentlyContinue | ForEach-Object { $_.Name })
    $redist = @(Get-ChildItem -LiteralPath (Join-Path $visualStudio 'VC\Redist\MSVC') -Directory -ErrorAction SilentlyContinue | ForEach-Object { $_.Name })
    Write-Host "install-msvc-pin: MSVC here: $($tools -join ', '); runtime DLLs (VC\Redist\MSVC): $($redist -join ', ')"
}
Say-Installed

# The component, from this Visual Studio's own catalog (said every time, so a log shows what this machine's catalog offers)
$catalogPath = Join-Path $env:ProgramData "Microsoft\VisualStudio\Packages\_Instances\$($instance.instanceId)\catalog.json"
if (-not (Test-Path -LiteralPath $catalogPath -PathType Leaf)) { Stop-Here "no catalog at $catalogPath" }
$catalog = [System.IO.File]::ReadAllText($catalogPath) | ConvertFrom-Json
$escaped = [regex]::Escape($toolset)
$components = @($catalog.packages | Where-Object { $_.id -match "^Microsoft\.VisualStudio\.Component\.VC\.$escaped(\.\d+\.\d+)?\.x86\.x64$" })
$toolPackages = @($catalog.packages | Where-Object { $_.id -match "^Microsoft\.VC\.$escaped(\.\d+\.\d+)?\.Tools\.HostX64\.TargetX86\.base$" })
foreach ($c in $components) { Write-Host "install-msvc-pin: the catalog offers $($c.id) ($($c.version))" }
foreach ($t in $toolPackages) { Write-Host "install-msvc-pin: its tools package $($t.id) is $($t.version)" }

if (Test-Path -LiteralPath $compiler -PathType Leaf)
{
    # The folder is there: its compiler and linker must be the pinned build too (Get-MsvcPinFolder stops here on GitHub, saying which build it found,
    # and warns on a machine of ours), and its x64 compiler must be there for vcpkg's host triplet
    try { Get-MsvcPinFolder $visualStudio $version | Out-Null } catch { Stop-Here $_.Exception.Message }
    Stop-WithoutHostCompiler
    Write-Host "install-msvc-pin: MSVC $version is installed ($compiler, and the x64 one vcpkg's host triplet uses): nothing to do"
    exit 0
}
if ($ListOnly) { Stop-Here "MSVC $version is not installed (-ListOnly: nothing installed)" }
if ($components.Count -ne 1) { Stop-Here "the catalog offers $($components.Count) components for MSVC $toolset (x86/x64); one is needed" }
# The component installs the build its tools package is (the catalog says it before anything is installed): any other build would put another
# compiler in the pinned folder (2026-11-22: VS 18.12's catalog served 14.51.36263 into the folder 14.51.36231). On GitHub, where what ships is
# built, nothing is installed then; on a machine of ours it is installed with a warning (win32/msvc-pin.props: "enforce on CI, warn locally")
if ($toolPackages.Count -ne 1 -or $toolPackages[0].version -cne "$toolset.$build")
{
    $other = "the catalog's tools package for MSVC $toolset is $(@($toolPackages | ForEach-Object { $_.version }) -join ', '), not $toolset.${build}"
    if ($env:GITHUB_ACTIONS -ceq 'true') { Stop-Here "${other}: Microsoft serves another build of it now. Move the pin on purpose (AvpxMsvcBuild, with a scan), never by accident" }
    Write-Warning "install-msvc-pin: ${other}: it is installed, but builds with it are not the shipped bytes. Update this machine's Visual Studio to build $build when you can"
}

$timer = [System.Diagnostics.Stopwatch]::StartNew()
# One command line, the folder in quotes: Start-Process joins a list of arguments with spaces and quotes none of them, so the first run gave the
# installer "--installPath C:\Program" (Visual Studio is in C:\Program Files\...) and it found no product there (#282, run 1). The folder comes from
# vswhere and the component from the catalog, and neither can hold a quote: both are checked first.
if ($visualStudio.Contains('"') -or $components[0].id -notmatch '^[A-Za-z0-9.]+$') { Stop-Here 'the folder or the component name has a character that cannot go on a command line' }
$arguments = "modify --installPath `"$visualStudio`" --add $($components[0].id) --quiet --norestart --nocache"
Write-Host "install-msvc-pin: setup.exe $arguments"
$process = Start-Process -FilePath $setup -ArgumentList $arguments -Wait -PassThru -NoNewWindow
$timer.Stop()
Write-Host ("install-msvc-pin: the installer answered {0} after {1:N0} s" -f $process.ExitCode, $timer.Elapsed.TotalSeconds)
Say-Installed
if ($process.ExitCode -ne 0 -and $process.ExitCode -ne 3010) { Stop-Here "the Visual Studio Installer answered $($process.ExitCode)" }
if (-not (Test-Path -LiteralPath $compiler -PathType Leaf))
{
    Stop-Here "after the install there is still no ${compiler}: the component installs another patch of MSVC $toolset (above). Move the pin on purpose, with a scan, or ask for this one"
}
try { Get-MsvcPinFolder $visualStudio $version | Out-Null } catch { Stop-Here $_.Exception.Message }
Stop-WithoutHostCompiler
Write-Host "install-msvc-pin: MSVC $version build $build is installed ($compiler, and the x64 one vcpkg's host triplet uses)"
exit 0
