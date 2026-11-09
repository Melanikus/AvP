#Requires -Version 4.0
<#
Downloads a test build of the game from GitHub, checks it, and installs it into a COPY of the game. PLAYTEST.md explains the
same steps by hand.

Needs the GitHub CLI (gh), signed in. From a copy of this repository:

  powershell -ExecutionPolicy Bypass -File tools\install-build.ps1 -GameFolder C:\Games\AvP-test

What the checks mean:
- It only takes a build that the project's owner started from avpx_main. A run from a fork's pull request can carry the same
  workflow name and the same branch name, but only someone with write access to this repository can start one of these.
- The hashes show that the download is complete and undamaged. On their own they don't show who made it: the list of hashes
  comes inside the same download, so anyone who could replace a file could replace the list too. So before the list is
  believed, GitHub must vouch for it: build.yml records which workflow made the list, from which commit, and this asks GitHub
  for that record (gh attestation verify). A build made before that record was added (2026-11-09) has none, and is refused.

Your previous files are copied to a new <GameFolder>-previous-build-<date and time> folder first, so you can go back. If the
install fails part of the way through, they are put back.
#>
param(
    [Parameter(Mandatory = $true)][string]$GameFolder
)
$ErrorActionPreference = 'Stop'
$repo = 'Melanikus/AvP'
$branch = 'avpx_main'

# The script replaces the game's files, so it refuses the real Steam folder and anything that isn't a game folder. This guards
# against a slip, not against someone determined: a mapped drive, for one, still looks like any other folder. It walks up
# every folder in the path, because a link anywhere along it could lead somewhere the path doesn't show.
$targetItem = Get-Item -LiteralPath $GameFolder
if (-not $targetItem.PSIsContainer) { throw "$GameFolder is not a folder." }
for ($folder = $targetItem; $folder; $folder = $folder.Parent) {
    if ($folder.Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw "$($folder.FullName) is a link to somewhere else, so this script can't tell where it would install. Use a plain folder."
    }
    if ($folder.Name.TrimEnd('.', ' ') -eq 'steamapps') {
        throw "Refusing to install into Steam's own folder. Copy the game first (see PLAYTEST.md)."
    }
}
$target = $targetItem.FullName.TrimEnd('\')
if (-not (Test-Path -LiteralPath (Join-Path $target 'avp_rifs'))) { throw "$target has no avp_rifs folder, so it is not a copy of the game." }

# The newest build the owner started from avpx_main.
$found = gh run list -R $repo --workflow build.yml --branch $branch --event workflow_dispatch --status success --limit 1 --json 'databaseId,headSha,createdAt' --jq '.[0]'
if ($LASTEXITCODE -ne 0) { throw 'Could not ask GitHub for the builds. Is gh installed and signed in?' }
if (-not $found -or "$found" -eq 'null') { throw "No build found. Builds are made only when the project's owner starts one (see PLAYTEST.md)." }
$run = ($found -join "`n") | ConvertFrom-Json

# Added 2026-11-09 by Melanikus for the AvP port: what GitHub answered is checked before it's used in a command or an address (AvP
# Tao #3): a run's number is digits, and a commit is 40 small hex digits.
if ("$($run.databaseId)" -cnotmatch '\A[1-9][0-9]{0,19}\z') { throw "GitHub's answer has no run number. Nothing was installed." }
if ("$($run.headSha)" -cnotmatch '\A[0-9a-f]{40}\z') { throw "GitHub's answer has no commit. Nothing was installed." }

# The commit must be part of avpx_main. "ahead" means avpx_main has moved on since: it is still on the branch, but newer code exists.
$compare = gh api "repos/$repo/compare/$($run.headSha)...$branch" --jq '.status + " " + (.ahead_by | tostring)'
if ($LASTEXITCODE -ne 0) { throw 'Could not ask GitHub about that commit.' }
$status, $newer = "$compare" -split ' '
if ($status -ne 'identical' -and $status -ne 'ahead') { throw "That build's commit is not on $branch. Nothing was installed." }
"Build run $($run.databaseId), commit $($run.headSha), made $($run.createdAt)"
if ($newer -ne '0') { "Note: $branch has $newer newer commit(s) with no build yet, so this is not the latest code." }

# Download into a new, empty folder. Whatever happens next, the download doesn't stay behind.
$download = (New-Item -ItemType Directory (Join-Path $env:TEMP ("avp-build-" + [guid]::NewGuid()))).FullName
try {
    gh run download $run.databaseId -R $repo -n "avp-$branch" -D $download
    if ($LASTEXITCODE -ne 0) { throw 'The download failed.' }

    # Check every file before anything is copied. Every file must be on the list and every entry on the list must be a file,
    # so a file can't be slipped in unlisted, and an empty list proves nothing.
    $sums = Join-Path $download 'SHA256SUMS.txt'
    if (-not (Test-Path -LiteralPath $sums)) { throw 'The build has no SHA256SUMS.txt. Nothing was installed.' }

    # Added 2026-11-09 by Melanikus for the AvP port: who made the list. GitHub must vouch that this very file came from this
    # repository's Build workflow (build.yml) run on avpx_main, at the commit GitHub named above, on GitHub's own machines. build.yml
    # records that only once a job that builds nothing has checked every file against the list. --cert-identity is the workflow
    # and branch exactly, where --signer-workflow would also let through a workflow whose name only starts the same.
    gh attestation verify $sums -R $repo --cert-identity "https://github.com/$repo/.github/workflows/build.yml@refs/heads/$branch" --source-ref "refs/heads/$branch" --source-digest $run.headSha --deny-self-hosted-runners
    if ($LASTEXITCODE -ne 0) { throw "GitHub can't vouch that this build's list came from $repo's Build workflow at commit $($run.headSha). Nothing was installed." }
    "GitHub vouches for the list: $repo's Build workflow made it, from commit $($run.headSha)."

    $expected = @{}
    foreach ($line in Get-Content -LiteralPath $sums) {
        $hash, $file = $line -split '  ', 2
        $expected[$file] = $hash
    }
    if ($expected.Count -eq 0) { throw 'SHA256SUMS.txt lists no files. Nothing was installed.' }
    $names = @()
    # -Force, so a hidden file is looked at too: one left out here would be a file nobody listed
    foreach ($item in Get-ChildItem -LiteralPath $download -Recurse -File -Force) {
        $name = $item.FullName.Substring($download.Length + 1).Replace('\', '/')
        if ($name -eq 'SHA256SUMS.txt') { continue }
        if (-not $expected.ContainsKey($name)) { throw "The build holds a file its list doesn't mention: $name. Nothing was installed." }
        if ((Get-FileHash -Algorithm SHA256 -LiteralPath $item.FullName).Hash.ToLower() -ne $expected[$name]) { throw "Hash mismatch: $name. Nothing was installed." }
        $expected.Remove($name)
        $names += $name
    }
    if ($expected.Count -ne 0) { throw "The build is missing a file its list mentions: $(@($expected.Keys)[0]). Nothing was installed." }
    "Every file matches the list, so the download is complete and undamaged."

    # The build says which commit it is. It must be the one GitHub just named.
    $label = Join-Path $download 'BUILD.txt'
    if (-not (Test-Path -LiteralPath $label) -or "$(Get-Content -LiteralPath $label)" -ne "$branch $($run.headSha)") {
        throw "The build does not say it is commit $($run.headSha). Nothing was installed."
    }

    # What goes into the game folder. A build is laid out as the game folder is: the program, bin/AvP_Classic.exe, and its libraries
    # in bin, the shaders and our game files at the top, and the note of which build this is. PLAYTEST.md and the rest are for reading.
    $install = @($names | Where-Object { $_ -match '^bin/[^/]+\.(exe|dll)$' -or $_ -eq 'BUILD.txt' -or $_ -like 'shaders/*' -or $_ -like 'avp_huds/*' -or $_ -like 'MelPyth/*' -or $_ -like 'graphics/*' })
    if ($install -notcontains 'bin/AvP_Classic.exe') { throw 'The build has no bin/AvP_Classic.exe. Nothing was installed.' }

    # Keep a copy of every file about to be replaced, and remember the ones that are new, so a failed install can be undone.
    $backup = '{0}-previous-build-{1}' -f $target, (Get-Date -Format 'yyyyMMdd-HHmmss')
    New-Item -ItemType Directory $backup | Out-Null
    $added = @()
    foreach ($name in $install) {
        $old = Join-Path $target $name.Replace('/', '\')
        if (Test-Path -LiteralPath $old) {
            $copy = Join-Path $backup $name.Replace('/', '\')
            New-Item -ItemType Directory -Force (Split-Path $copy -Parent) | Out-Null
            Copy-Item -LiteralPath $old -Destination $copy -Force
        }
        else {
            $added += $old
        }
    }

    try {
        foreach ($name in $install) {
            $to = Join-Path $target $name.Replace('/', '\')
            # The folder must exist first. If it doesn't, the copy makes a FILE with the folder's name, and a first install
            # into a copy of the Steam game (which has no shaders folder) ends with no shaders.
            New-Item -ItemType Directory -Force (Split-Path $to -Parent) | Out-Null
            Copy-Item -LiteralPath (Join-Path $download $name.Replace('/', '\')) -Destination $to -Force
        }
    }
    catch {
        $why = $_.Exception.Message
        foreach ($file in $added) { Remove-Item -LiteralPath $file -Force -ErrorAction SilentlyContinue }
        $stuck = @()
        foreach ($item in Get-ChildItem -LiteralPath $backup -Recurse -File) {
            $to = $target + $item.FullName.Substring($backup.Length)
            try {
                # A file the install never reached is already right, and it may be the very file that made the install
                # fail by being in use, so it is left alone.
                $same = (Test-Path -LiteralPath $to) -and
                    (Get-FileHash -Algorithm SHA256 -LiteralPath $to).Hash -eq (Get-FileHash -Algorithm SHA256 -LiteralPath $item.FullName).Hash
                if (-not $same) { Copy-Item -LiteralPath $item.FullName -Destination $to -Force }
            }
            catch {
                $stuck += $to
            }
        }
        if ($stuck.Count -ne 0) { throw "The install failed, and $($stuck.Count) file(s) could not be put back, such as $($stuck[0]). Your previous files are in $backup. ($why)" }
        throw "The install failed, so your previous files were put back. Is the game still running? ($why)"
    }
}
finally {
    Remove-Item -LiteralPath $download -Recurse -Force -ErrorAction SilentlyContinue
}

"Installed: $(Get-Content -LiteralPath (Join-Path $target 'BUILD.txt'))"
"Previous files: $backup"
