# Checks that a test really notices when the code it guards is broken. Written 2026-11-05 by Melanikus for the AvP port.
#
# README.md, beside this file, says what this is for. Each row of tests/takeouts/<test>.json is one small change to a copy of the code. For each, this builds the test against the
# changed copy and runs it. The test should fail. A row it doesn't fail on means that check isn't really being tested. The repository itself is never touched.
#
# Run it from anywhere, in PowerShell:
#   tests/takeouts/run.ps1 <test>                  for example: tests/takeouts/run.ps1 rim_test
#   tests/takeouts/run.ps1 <test> -From 3 -To 5    only those rows
#
# The copy and the programs land in a folder called AvP-test-out next to the repository, or under $env:RUNNER_TEMP in the automatic checks. Not the temp folder: antivirus programs distrust
# new programs that appear there. It gives back a non-zero result if any row wasn't noticed, or couldn't be run.
#
# A row that can't run says why and what to do. When its text has moved, node tests/takeouts/find-moved-row.js <test> <row> finds where it went (added 2026-11-07).

param(
    [Parameter(Mandatory = $true)][string]$Test,
    [int]$From = 0,
    [int]$To = -1,
    [int]$SecondsForEachRun = 120
)

$ErrorActionPreference = 'Stop'
trap { Write-Host "Could not run the take-outs: $_"; exit 2 }

# A list's name is a test's name, which is only letters, digits and underscores, so it can't name a file anywhere else
if ($Test -cnotmatch '^[A-Za-z0-9_]+$') { throw "A list's name is letters, digits and underscores only, and this isn't: $Test" }

$repository = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$rowsFile = Join-Path $PSScriptRoot "$Test.json"
if (-not (Test-Path $rowsFile)) { throw "There's no list of take-outs at $rowsFile" }

$base = if ($env:RUNNER_TEMP) { Join-Path $env:RUNNER_TEMP 'avp-takeouts' } else { Join-Path (Split-Path -Parent $repository) 'AvP-test-out\takeouts' }
$tree = Join-Path $base "$Test\tree"
$out = Join-Path $base "$Test\out"
if (Test-Path (Join-Path $base $Test)) { Remove-Item -Recurse -Force (Join-Path $base $Test) }
New-Item -ItemType Directory -Force $tree | Out-Null

# These bytes go in and out exactly as they are, whatever they are, and line endings are put back as they were found.
$bytes = [System.Text.Encoding]::GetEncoding(28591)
$list = [System.IO.File]::ReadAllText($rowsFile, [System.Text.Encoding]::UTF8) | ConvertFrom-Json

# Only the sources, not what earlier builds left behind. Modified 2026-11-11 by Melanikus for the AvP port: a list may also name one file at
# the top of the repository, such as servers.cfg (the servers the game ships with, which relay_list_test holds to its own list of them). Such a
# file is copied as it is, and only by its plain name, so nothing from outside the repository, or deeper in it, comes in that way.
foreach ($folder in $list.copy) {
    $from = Join-Path $repository $folder
    if (Test-Path -LiteralPath $from -PathType Leaf) {
        if ("$folder" -cnotmatch '\A[A-Za-z0-9_][A-Za-z0-9_.-]*\z') { throw "A list copies a file only from the top of the repository, by its plain name: $folder" }
        Copy-Item -LiteralPath $from -Destination (Join-Path $tree $folder)
        continue
    }
    & robocopy $from (Join-Path $tree $folder) /E /NFL /NDL /NJH /NJS /NP /XF *.obj *.pdb *.exe *.ilk *.iobj *.ipdb *.dll *.lib *.exp *.idb *.tlog *.recipe *.log | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "Couldn't copy $folder" }
}
$global:LASTEXITCODE = 0

# A test that links a library vcpkg built names it in its settings, under vcpkg_installed (net_close_test names ENet's
# headers and enet.lib). The copy above leaves .lib files out on purpose, so exactly the ones the settings name are
# copied here, from this repository's own vcpkg_installed, which a build of the game fills. A name has to stay inside
# vcpkg_installed. (Added 2026-11-07, for net_close_test.)
$settings = Join-Path $repository "tests\$Test.args"
if (Test-Path -LiteralPath $settings) {
    foreach ($word in @(Get-Content -LiteralPath $settings | ForEach-Object { $_ -split '\s+' } | Where-Object { $_ })) {
        $named = $word -replace '^/I', ''
        if ($named -cnotmatch '^vcpkg_installed[\\/]') { continue }
        if (($named -split '[\\/]') -contains '..' -or $named.Contains(':')) { throw "tests\$Test.args names $named, which isn't inside vcpkg_installed" }

        # Not $from and $to: PowerShell reads a name the same in capitals or small letters, so those would be -From and -To above,
        # which hold whole numbers, and a path can't go in one.
        $libraryFrom = Join-Path $repository $named
        $libraryTo = Join-Path $tree $named
        if (Test-Path -LiteralPath $libraryFrom -PathType Container) {
            & robocopy $libraryFrom $libraryTo /E /NFL /NDL /NJH /NJS /NP | Out-Null
            if ($LASTEXITCODE -ge 8) { throw "Couldn't copy $named" }
        }
        elseif (Test-Path -LiteralPath $libraryFrom -PathType Leaf) {
            New-Item -ItemType Directory -Force (Split-Path -Parent $libraryTo) | Out-Null
            Copy-Item -LiteralPath $libraryFrom -Destination $libraryTo
        }
        else {
            throw "tests\$Test.args names $named, and it isn't there. Build the game once, so vcpkg puts its libraries in vcpkg_installed."
        }
    }
    $global:LASTEXITCODE = 0
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio with the C++ tools was not found' }

# Visual Studio's own PowerShell module sets the compiler up in this window, the way its setup batch file does in a command window, so nothing has to be written to disk to run it.
# The antivirus distrusts a script that writes another to run, and took one of ours. The setup looks for vswhere on the path, so that goes on first. -arch=x86 builds 32-bit programs,
# the same as the game.
$env:PATH = "$(Split-Path -Parent $vswhere);$env:PATH"
Import-Module (Join-Path $visualStudio 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $visualStudio -SkipAutomaticLocation -DevCmdArguments '-arch=x86 -host_arch=amd64 -no_logo' | Out-Null

# Runs one of the compiler's programs in a folder and gives back its exit code. What it says goes in a file, and anything it says as an error in that file's .errors. It's started this way,
# and not by its name on its own, because Windows PowerShell takes anything a program writes as an error as the script failing.
function Invoke-Tool([string]$program, [string]$arguments, [string]$folder, [string]$saysTo)
{
    $found = (Get-Command $program -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
    $started = @{ FilePath = $found; WorkingDirectory = $folder; NoNewWindow = $true; PassThru = $true; RedirectStandardOutput = $saysTo; RedirectStandardError = "$saysTo.errors" }

    # Start-Process refuses an empty list of arguments, and a program run on its own has none
    if ($arguments) { $started.ArgumentList = $arguments }

    $process = Start-Process @started
    $handle = $process.Handle    # without asking for this, Windows PowerShell can't read the exit code afterwards
    $process.WaitForExit()
    return $process.ExitCode
}

function Build-And-Run {
    if (Test-Path $out) { Remove-Item -Recurse -Force $out }
    New-Item -ItemType Directory -Force $out | Out-Null
    # The test's settings come last, because they can end with /link, and anything after /link goes to the linker
    Invoke-Tool 'cl.exe' "/nologo `"/Fe:$out\t.exe`" `"/Fo:$out/`" @tests\$Test.args" $tree (Join-Path $out 'build.txt') | Out-Null
    $built = Test-Path (Join-Path $out 't.exe')
    if (-not $built) { return @{ Built = $false; Exit = -1; Words = (Get-Content (Join-Path $out 'build.txt') -TotalCount 3) -join ' ' } }

    $said = Join-Path $out 'said.txt'
    $process = Start-Process -FilePath (Join-Path $out 't.exe') -WorkingDirectory $tree -NoNewWindow -PassThru -RedirectStandardOutput $said -RedirectStandardError (Join-Path $out 'errors.txt')
    $handle = $process.Handle    # without asking for this, Windows PowerShell can't read the exit code afterwards
    if (-not $process.WaitForExit($SecondsForEachRun * 1000)) {
        $process.Kill()
        $process.WaitForExit()
        return @{ Built = $true; Exit = 'ran on until it was stopped'; Words = '' }
    }

    # The tests write FAILED in capitals for a check that failed, and Select-String ignores case unless told, which counted
    # passing lines that only say "failed" in passing, such as "the failed join's ENet host"
    $failed = @(Select-String -Path $said -Pattern 'FAILED' -SimpleMatch -CaseSensitive | Where-Object { $_.Line -cnotmatch 'checks FAILED$' })
    $first = if ($failed.Count) { $failed[0].Line.Trim() } else { '' }
    return @{ Built = $true; Exit = $process.ExitCode; Words = "$($failed.Count) failed. $first"; Ok = (@(Select-String -Path $said -Pattern '^\s*ok').Count) }
}

$first = Build-And-Run
if (-not $first.Built -or $first.Exit -ne 0) { throw "With nothing changed the test should pass, and it doesn't: $($first.Exit) $($first.Words)" }
Write-Host "With nothing changed, $Test passes $($first.Ok) checks"

$rows = @($list.rows)
$last = if ($To -ge 0) { [Math]::Min($To, $rows.Count - 1) } else { $rows.Count - 1 }
$noticed = 0; $missed = 0; $notRun = 0

# A list can come from anyone's pull request, and what it says is shown in the log. On GitHub, a log line that starts
# with :: is read as an order to GitHub, so a row's name is kept to one line, and its text is shown behind a margin.
function Show-OneLine([string]$text) { return ($text -replace "[`r`n]+", ' / ') }
function Show-Lines([string]$text) { return @(($text -replace "`r", '') -split "`n" | ForEach-Object { "      | $_" }) }

# On GitHub, a row that fails also becomes a note on the pull request's page. GitHub reads %, line ends and, in the
# note's title, : and , as part of the note itself, so those are written the way GitHub asks.
function Write-GitHubNote([string]$title, [string]$words) {
    if (-not $env:GITHUB_ACTIONS) { return }
    $safeTitle = $title.Replace('%', '%25').Replace("`r", '%0D').Replace("`n", '%0A').Replace(':', '%3A').Replace(',', '%2C')
    $safeWords = $words.Replace('%', '%25').Replace("`r", '%0D').Replace("`n", '%0A')
    Write-Host "::error file=tests/takeouts/$Test.json,title=$($safeTitle)::$safeWords"
}

$whatToDo = 'tests/takeouts/README.md, under "When the Take-outs check fails", says what to do.'

for ($r = $From; $r -le $last; $r++) {
    $row = $rows[$r]
    $name = Show-OneLine "$($row.name)"
    $kept = @{}
    $matched = $true

    foreach ($change in $row.changes) {
        # The file a row changes has to be in the copy: not a path from a drive or a root, not one that climbs out
        # with .., and not one the copy doesn't have
        $file = "$($change.file)"
        $parts = $file -split '[\\/]'
        if ($file -eq '' -or $file.Contains(':') -or [System.IO.Path]::IsPathRooted($file) -or ($parts -contains '..') -or
            -not (Test-Path -LiteralPath (Join-Path $tree $file) -PathType Leaf)) {
            Write-Host "#$r $($name): CAN'T RUN, it names a file that isn't in the copy: $(Show-OneLine $file)"
            Write-Host "    A row names a file inside the repository, such as 3dc/module.cpp, in a folder the list copies. $whatToDo"
            Write-GitHubNote "Take-out row $r of $Test names a file that isn't there" "Row $r ($name) names $(Show-OneLine $file). $whatToDo"
            $matched = $false
            continue
        }

        $path = Join-Path $tree $file
        if (-not $kept.ContainsKey($path)) { $kept[$path] = [System.IO.File]::ReadAllText($path, $bytes) }

        $text = [System.IO.File]::ReadAllText($path, $bytes)
        $windows = $text.Contains("`r`n")
        $plain = $text.Replace("`r`n", "`n")
        $at = $plain.IndexOf($change.from, [StringComparison]::Ordinal)
        $again = if ($at -ge 0) { $plain.IndexOf($change.from, $at + 1, [StringComparison]::Ordinal) } else { -1 }

        # A row that can't find its text fails, so a check nobody is testing any more can't hide. What to do about it
        # is said here, where the person who has to do it will look.
        if ($at -lt 0 -or $again -ge 0) {
            $why = if ($at -lt 0) { "CAN'T FIND ITS TEXT in $file" } else { "FINDS ITS TEXT MORE THAN ONCE in $file, so it can't tell which to change" }
            Write-Host "#$r $($name): $why (tests/takeouts/$Test.json, row $r, counted from 0). It looks for:"
            foreach ($line in (Show-Lines $change.from)) { Write-Host $line }
            Write-Host "    To fix it, in the same pull request as the change that moved the code:"
            Write-Host "    - If the code it breaks is still there with different words, change the row's ""from"" to the code as it is now, and check its ""to"" still breaks it."
            Write-Host "    - If what it checks has moved somewhere else, or is gone, remove the row, and add one where the check is now."
            Write-Host "    - If the text is there more than once, make ""from"" longer, with lines around it, until it's there only once."
            Write-Host "    node tests/takeouts/find-moved-row.js $Test $r   shows the places most like the old text, and with --write puts the best one in the row for you to check."
            Write-GitHubNote "Take-out row $r of $Test $(if ($at -lt 0) { "can't find its text" } else { 'finds its text more than once' })" "Row $r ($name) looks in $(Show-OneLine $file). The job's log shows the text and how to fix it, and node tests/takeouts/find-moved-row.js $Test $r finds where it went. $whatToDo"
            $matched = $false
            continue
        }

        $changed = $plain.Substring(0, $at) + $change.to + $plain.Substring($at + $change.from.Length)
        if ($windows) { $changed = $changed.Replace("`n", "`r`n") }
        [System.IO.File]::WriteAllText($path, $changed, $bytes)
    }

    if (-not $matched) {
        $notRun++
    }
    else {
        $result = Build-And-Run
        if (-not $result.Built) {
            Write-Host "#$r $($name): CAN'T RUN, it didn't build: $($result.Words)"
            Write-Host "    The row's ""to"" has to leave code that still builds, so only what it breaks is broken. $whatToDo"
            Write-GitHubNote "Take-out row $r of $Test didn't build" "Row $r ($name): the copy with this row's change didn't build. $whatToDo"
            $notRun++
        }
        elseif ($result.Exit -eq 0) {
            Write-Host "#$r NOT NOTICED $name"
            Write-Host "    The test still passes with this broken, so nothing is checking it. Make the test notice it, or, if it no longer needs checking, remove the row. $whatToDo"
            Write-GitHubNote "Take-out row $r of $Test wasn't noticed" "Row $r ($name): the test still passes with this broken, so nothing is checking it. $whatToDo"
            $missed++
        }
        else {
            Write-Host "#$r noticed     $($name): exit $($result.Exit), $($result.Words)"
            $noticed++
        }
    }

    foreach ($path in $kept.Keys) { [System.IO.File]::WriteAllText($path, $kept[$path], $bytes) }
}

Write-Host "$noticed noticed, $missed not noticed, $notRun not run"
if ($missed -or $notRun) { Write-Host "A row that isn't noticed, or can't run, fails this. $whatToDo" }
exit $(if ($missed -or $notRun) { 1 } else { 0 })
