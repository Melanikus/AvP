<#
Downloads a build of the game from GitHub, checks every file against its published hash, and installs it into a COPY of
the game. PLAYTEST.md explains the same steps by hand.

Needs the GitHub CLI (gh), signed in.

  .\tools\install-build.ps1 -Branch avpx_main -GameFolder C:\Games\AvP-test     newest test build
  .\tools\install-build.ps1 -Tag v1.2.3 -GameFolder C:\Games\AvP-test           a stable build

Your previous files are moved to <GameFolder>-previous-build first, so you can go back.
#>
param(
    [string]$Branch,
    [string]$Tag,
    [Parameter(Mandatory = $true)][string]$GameFolder
)
$ErrorActionPreference = 'Stop'
$repo = 'Melanikus/AvP'

# The script replaces the game's program, so it refuses the real Steam folder and anything that isn't a game folder.
$target = (Resolve-Path $GameFolder).Path
if ($target -match '\\steamapps\\') { throw "Refusing to install into Steam's own folder. Copy the game first (see PLAYTEST.md)." }
if (-not (Test-Path (Join-Path $target 'avp_rifs'))) { throw "$target has no avp_rifs folder, so it is not a copy of the game." }

if ([bool]$Branch -eq [bool]$Tag) { throw 'Give either -Branch or -Tag.' }
$ref = if ($Tag) { $Tag } else { $Branch }

# The newest successful Build run for that branch or tag.
$runId = gh run list -R $repo --workflow build.yml --branch $ref --status success --limit 1 --json databaseId --jq '.[0].databaseId'
if (-not $runId) { throw "No successful build found for $ref." }
"Build run: $runId"

# Download into a new, empty folder, and check every file before anything is copied.
$download = Join-Path $env:TEMP ("avp-build-" + [guid]::NewGuid())
New-Item -ItemType Directory $download | Out-Null
gh run download $runId -R $repo -n "avp-$ref" -D $download
if ($LASTEXITCODE -ne 0) { throw 'The download failed.' }

$sums = Join-Path $download 'SHA256SUMS.txt'
if (-not (Test-Path $sums)) { throw 'The build has no SHA256SUMS.txt.' }
foreach ($line in Get-Content $sums) {
    $hash, $file = $line -split '  ', 2
    $path = Join-Path $download $file
    if ((Get-FileHash -Algorithm SHA256 $path).Hash.ToLower() -ne $hash) { throw "Hash mismatch: $file. Nothing was installed." }
}
"All hashes match."

# Keep what we are about to replace.
$backup = "$target-previous-build"
New-Item -ItemType Directory -Force $backup | Out-Null
$items = @(Get-ChildItem $download -File | Where-Object { $_.Extension -in '.exe', '.dll' })
foreach ($item in $items) {
    $old = Join-Path $target $item.Name
    if (Test-Path $old) { Copy-Item $old $backup -Force }
    Copy-Item $item.FullName $target -Force
}
Copy-Item (Join-Path $download 'shaders\*') (Join-Path $target 'shaders') -Recurse -Force
Copy-Item (Join-Path $download 'BUILD.txt') $target -Force
Remove-Item $download -Recurse -Force

"Installed: $(Get-Content (Join-Path $target 'BUILD.txt'))"
"Previous files: $backup"
