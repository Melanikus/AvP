# Checks that a test really notices when the code it guards is broken. Written 2026-11-05 by Melanikus for the AvP port.
#
# README.md, beside this file, says what this is for. Each row of tests/takeouts/<test>.json is one small change to a copy of the code. For each, this builds the test against the
# changed copy and runs it. The test should fail. A row it doesn't fail on means that check isn't really being tested. The repository itself is never touched.
#
# Run it from anywhere, in PowerShell:
#   tests/takeouts/run.ps1 <test>                  for example: tests/takeouts/run.ps1 rim_test
#   tests/takeouts/run.ps1 <test> -From 3 -To 5    only those rows
#   tests/takeouts/run.ps1 -CheckRefusals          checks that this refuses the names in a list it should, each for its own reason (added 2026-11-08)
#
# The copy and the programs land in a folder called AvP-test-out next to the repository, or under $env:RUNNER_TEMP in the automatic checks. Not the temp folder: antivirus programs distrust
# new programs that appear there. It gives back a non-zero result if any row wasn't noticed, or couldn't be run.
#
# A row that can't run says why and what to do. When its text has moved, node tests/takeouts/find-moved-row.js <test> <row> finds where it went (added 2026-11-07).

param(
    [string]$Test,
    [int]$From = 0,
    [int]$To = -1,
    [int]$SecondsForEachRun = 120,
    [switch]$CheckRefusals
)

$ErrorActionPreference = 'Stop'
trap { Write-Host "Could not run the take-outs: $_"; exit 2 }

# ---- What a list may name, checked before anything is copied (added 2026-11-08)
#
# A list can come from anyone's pull request, and so can a test's settings. Before a name in either is used to copy anything, these say
# whether it's refused and why, or give back nothing when it's fine. -CheckRefusals, at the end of this part, shows each refusal happens,
# for its own reason.

# A name shown in a refusal is kept to one line, so a name with a line end in it can't start a line of its own in the log.
function Show-Name([object]$named) { return ("$named" -replace "[`r`n]+", ' / ') }

# Whether every part of a path is one Windows reads the way it looks. Windows drops dots and spaces from the end of a part, and a part made
# of dots alone means "this folder" or a folder above it. So those are refused, and so is an empty part, or any character but letters,
# digits and - _ . +
function Test-PlainParts([string]$path)
{
    foreach ($part in ($path -split '[\\/]')) {
        if ($part -cnotmatch '^[A-Za-z0-9_.+-]+$' -or $part -match '^\.+$' -or $part.EndsWith('.')) { return $false }
    }
    return $true
}

# A library a test's settings or its list names, to copy from this repository's vcpkg_installed. It has to be inside vcpkg_installed, not
# climb out of it with .., have no : (which names a drive, or a hidden second part of a file), be made of plain parts, and be there. A list's
# libraries have to be files. A test's settings can also name a folder, of headers for instance.
function Get-LibraryRefusal([object]$named, [string]$root, [string]$namedBy, [bool]$filesOnly)
{
    $shown = Show-Name $named
    if ($named -isnot [string] -or $named -eq '') { return "$namedBy has a library that isn't a name: '$shown'" }
    if ($named -cnotmatch '^vcpkg_installed[\\/]') { return "$namedBy names $shown, which isn't inside vcpkg_installed" }
    if (($named -split '[\\/]') -contains '..') { return "$namedBy names $shown, which climbs out of vcpkg_installed with .." }
    if ($named.Contains(':')) { return "$namedBy names $shown, which has a : in it, so it could name a drive, or a hidden part of a file" }
    if (-not (Test-PlainParts $named)) { return "$namedBy names $shown, which has a part Windows wouldn't read the way it looks" }

    $full = Join-Path $root $named
    if (-not (Test-Path -LiteralPath $full)) { return "$namedBy names $shown, and it isn't there. Build the game once, so vcpkg puts its libraries in vcpkg_installed." }
    if ($filesOnly -and -not (Test-Path -LiteralPath $full -PathType Leaf)) { return "$namedBy names $shown, which is a folder, and a list's libraries are files" }
    return ''
}

# A folder a list copies: a path inside the repository, such as 3dc or dependencies/libbinkdec/src, with no .. and no :, not starting from a
# root, made of plain parts, and there.
function Get-CopyRefusal([object]$folder, [string]$root)
{
    $shown = Show-Name $folder
    if ($folder -isnot [string] -or $folder -eq '') { return "The list copies something that isn't a folder's name: '$shown'" }
    if (($folder -split '[\\/]') -contains '..') { return "The list copies $shown, which climbs out of the repository with .." }
    if ($folder.Contains(':')) { return "The list copies $shown, which has a : in it, so it could name a drive" }
    if ($folder -match '^[\\/]') { return "The list copies $shown, which starts from a root, instead of being a folder inside the repository" }
    if (-not (Test-PlainParts $folder)) { return "The list copies $shown, which has a part Windows wouldn't read the way it looks" }
    if (-not (Test-Path -LiteralPath (Join-Path $root $folder) -PathType Container)) { return "The list copies $shown, and the repository has no such folder" }
    return ''
}

# Copies the folders a list names into the copy of the code, each one checked first. Only the sources, not what earlier builds left behind.
function Copy-ListFolders([object]$list, [string]$root, [string]$copyRoot)
{
    if ($null -eq $list.copy) { throw 'The list has no "copy": the folders its test needs' }
    foreach ($folder in @($list.copy)) {
        $why = Get-CopyRefusal $folder $root
        if ($why) { throw $why }
        & robocopy (Join-Path $root $folder) (Join-Path $copyRoot $folder) /E /NFL /NDL /NJH /NJS /NP /XF *.obj *.pdb *.exe *.ilk *.iobj *.ipdb *.dll *.lib *.exp *.idb *.tlog *.recipe *.log | Out-Null
        if ($LASTEXITCODE -ge 8) { throw "Couldn't copy $folder" }
    }
    $global:LASTEXITCODE = 0
}

# Copies the libraries a list names into the copy of the code, each one checked first. A test that loads a library while it runs, instead
# of linking it, can't name it in its settings, because those go to the compiler: shader_test loads Microsoft's D3DX that way. So its list
# names the files, under "libraries". The copy of the folders leaves every .dll out on purpose, so these are the only ones in it.
function Copy-ListLibraries([object]$list, [string]$root, [string]$copyRoot, [string]$namedBy)
{
    if ($null -eq $list.libraries) { return }
    foreach ($named in @($list.libraries)) {
        $why = Get-LibraryRefusal $named $root $namedBy $true
        if ($why) { throw $why }
        $libraryTo = Join-Path $copyRoot $named
        New-Item -ItemType Directory -Force (Split-Path -Parent $libraryTo) | Out-Null
        Copy-Item -LiteralPath (Join-Path $root $named) -Destination $libraryTo
    }
}

# -CheckRefusals: shows that the checks above refuse each thing they should, for its own reason, and still take a good name. It makes a
# made-up repository in a folder of its own, with one made-up library in it (a text file, since nothing here is run), and asks about names
# made to go wrong. takeouts.yml runs this before it chooses the lists. Gives back 1 if any answer is wrong, and 0 if all are right.
function Test-Refusals
{
    $outside = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
    $root = if ($env:RUNNER_TEMP) { Join-Path $env:RUNNER_TEMP 'avp-takeouts-refusals' } else { Join-Path $outside 'AvP-test-out\takeouts-refusals' }

    # The folder is emptied each time, so only one this made is: it leaves a note in it saying so
    $note = 'made by the take-outs refusal check.txt'
    if (Test-Path -LiteralPath $root) {
        if (-not (Test-Path -LiteralPath (Join-Path $root $note) -PathType Leaf)) { throw "$root is there already, and this didn't make it, so it's left alone" }
        Remove-Item -LiteralPath $root -Recurse -Force
    }
    $made = Join-Path $root 'repository'
    $copyRoot = Join-Path $root 'copy'
    foreach ($folder in @('vcpkg_installed\x86-windows\bin', 'tests', 'dependencies\made-up\src', '.github')) {
        New-Item -ItemType Directory -Force (Join-Path $made $folder) | Out-Null
    }
    New-Item -ItemType Directory -Force $copyRoot | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $root $note), 'tests/takeouts/run.ps1 -CheckRefusals makes this folder, and empties it the next time it runs.')
    $library = 'vcpkg_installed/x86-windows/bin/made-up.txt'
    [System.IO.File]::WriteAllText((Join-Path $made $library), 'A made-up library: only its name is checked.')

    $script:wrongAnswers = 0
    function Answer([bool]$right, [string]$what, [string]$said)
    {
        if ($right) { Write-Host "ok      $what" }
        else { Write-Host "FAILED  $what. It said: '$said'"; $script:wrongAnswers++ }
    }

    Write-Host 'Library names, in a list or in a test''s settings:'
    $libraryNames = @(
        @{ Name = $library; Files = $true; Reason = ''; What = 'a library inside vcpkg_installed is taken' },
        @{ Name = 'vcpkg_installed\x86-windows\bin\made-up.txt'; Files = $true; Reason = ''; What = 'and the same written with \' },
        @{ Name = 'vcpkg_installed/x86-windows/bin'; Files = $false; Reason = ''; What = 'a test''s settings can name a folder in it' },
        @{ Name = 'tests/made-up.txt'; Files = $true; Reason = "which isn't inside vcpkg_installed"; What = 'a file outside vcpkg_installed is refused' },
        @{ Name = 'vcpkg_installed/../tests/made-up.txt'; Files = $true; Reason = 'which climbs out of vcpkg_installed with ..'; What = 'a name that climbs out with .. is refused' },
        @{ Name = 'vcpkg_installed\x86-windows\..\..\..\Windows\notepad.exe'; Files = $true; Reason = 'which climbs out of vcpkg_installed with ..'; What = 'and the same with \' },
        @{ Name = 'vcpkg_installed/x86-windows/bin/made-up.txt:hidden'; Files = $true; Reason = 'which has a : in it'; What = 'a name with a : is refused' },
        @{ Name = 'vcpkg_installed/x86-windows/.../made-up.txt'; Files = $true; Reason = "which has a part Windows wouldn't read the way it looks"; What = 'a part made of dots alone is refused' },
        @{ Name = 'vcpkg_installed/x86-windows/bin/made-up.txt.'; Files = $true; Reason = "which has a part Windows wouldn't read the way it looks"; What = 'a part ending in a dot, which Windows drops, is refused' },
        @{ Name = 'vcpkg_installed/x86-windows/bin/made up.txt'; Files = $true; Reason = "which has a part Windows wouldn't read the way it looks"; What = 'a part with a space is refused' },
        @{ Name = 'vcpkg_installed/x86-windows/bin/missing.txt'; Files = $true; Reason = "and it isn't there"; What = 'a library that isn''t there is refused' },
        @{ Name = 'vcpkg_installed/x86-windows/bin'; Files = $true; Reason = 'which is a folder'; What = 'a library in a list that is a folder is refused' },
        @{ Name = ''; Files = $true; Reason = "a library that isn't a name"; What = 'an empty name is refused' },
        @{ Name = 5; Files = $true; Reason = "a library that isn't a name"; What = 'a number is refused' },
        @{ Name = $null; Files = $true; Reason = "a library that isn't a name"; What = 'nothing at all is refused' }
    )
    foreach ($case in $libraryNames) {
        $said = Get-LibraryRefusal $case.Name $made 'The list' $case.Files
        Answer $(if ($case.Reason) { $said.Contains($case.Reason) } else { $said -eq '' }) $case.What $said
    }

    Write-Host 'Folders a list copies:'
    $folders = @(
        @{ Name = 'tests'; Reason = ''; What = 'a top folder of the repository is taken' },
        @{ Name = 'dependencies/made-up/src'; Reason = ''; What = 'and a folder inside one' },
        @{ Name = '.github'; Reason = ''; What = 'and one whose name starts with a dot' },
        @{ Name = '..'; Reason = 'which climbs out of the repository with ..'; What = 'copying .. is refused' },
        @{ Name = 'tests\..\..'; Reason = 'which climbs out of the repository with ..'; What = 'and climbing out from inside a folder' },
        @{ Name = 'C:\Windows'; Reason = 'which has a : in it'; What = 'copying a folder on a drive is refused' },
        @{ Name = '\Windows'; Reason = 'which starts from a root'; What = 'copying from the root of a drive is refused' },
        @{ Name = '\\server\share'; Reason = 'which starts from a root'; What = 'and from another machine' },
        @{ Name = 'tests/...'; Reason = "which has a part Windows wouldn't read the way it looks"; What = 'a part made of dots alone is refused' },
        @{ Name = 'missing'; Reason = 'and the repository has no such folder'; What = 'a folder that isn''t there is refused' },
        @{ Name = 7; Reason = "something that isn't a folder's name"; What = 'a number is refused' }
    )
    foreach ($case in $folders) {
        $said = Get-CopyRefusal $case.Name $made
        Answer $(if ($case.Reason) { $said.Contains($case.Reason) } else { $said -eq '' }) $case.What $said
    }

    Write-Host 'What a list asks for goes through those checks:'
    $lists = @(
        @{ Json = '{"copy": [".."]}'; Copy = $true; Reason = 'which climbs out of the repository with ..'; What = 'a list copying .. stops the run before anything is copied' },
        @{ Json = '{"rows": []}'; Copy = $true; Reason = 'The list has no "copy"'; What = 'and so does a list that copies nothing' },
        @{ Json = '{"libraries": ["vcpkg_installed/../tests/made-up.txt"]}'; Copy = $false; Reason = 'which climbs out of vcpkg_installed with ..'; What = 'a list naming a library outside vcpkg_installed stops the run' },
        @{ Json = '{"libraries": "vcpkg_installed/x86-windows/bin/made-up.txt:x"}'; Copy = $false; Reason = 'which has a : in it'; What = 'and so does one name on its own, not in a list' },
        @{ Json = '{"libraries": [{"name": "vcpkg_installed/x86-windows/bin/made-up.txt"}]}'; Copy = $false; Reason = "a library that isn't a name"; What = 'and a library written as something other than a name' },
        @{ Json = '{"copy": ["tests"]}'; Copy = $false; Reason = ''; What = 'a list with no libraries copies none, and that is fine' }
    )
    foreach ($case in $lists) {
        $list = $case.Json | ConvertFrom-Json
        try {
            if ($case.Copy) { Copy-ListFolders $list $made $copyRoot } else { Copy-ListLibraries $list $made $copyRoot 'The list' }
            $said = ''
        }
        catch { $said = "$_" }
        Answer $(if ($case.Reason) { $said.Contains($case.Reason) } else { $said -eq '' }) $case.What $said
    }
    Copy-ListLibraries ('{"libraries": ["vcpkg_installed/x86-windows/bin/made-up.txt"]}' | ConvertFrom-Json) $made $copyRoot 'The list'
    Answer (Test-Path -LiteralPath (Join-Path $copyRoot $library) -PathType Leaf) 'a good library is copied, to the same place in the copy' ''

    Write-Host "$(if ($script:wrongAnswers) { "$($script:wrongAnswers) answers were WRONG" } else { 'Every answer was right' })"
    return $(if ($script:wrongAnswers) { 1 } else { 0 })
}

if ($CheckRefusals) { exit (Test-Refusals) }

# ---- The run

# A list's name is a test's name, which is only letters, digits and underscores, so it can't name a file anywhere else
if (-not $Test) { throw 'Name a test, such as rim_test, or ask for -CheckRefusals' }
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

Copy-ListFolders $list $repository $tree

# A test that links a library vcpkg built names it in its settings, under vcpkg_installed (net_close_test names ENet's
# headers and enet.lib). The copy above leaves .lib files out on purpose, so exactly the ones the settings name are
# copied here, from this repository's own vcpkg_installed, which a build of the game fills. Each name gets the checks in
# Get-LibraryRefusal above. (Added 2026-11-07, for net_close_test.)
$settings = Join-Path $repository "tests\$Test.args"
if (Test-Path -LiteralPath $settings) {
    foreach ($word in @(Get-Content -LiteralPath $settings | ForEach-Object { $_ -split '\s+' } | Where-Object { $_ })) {
        $named = $word -replace '^/I', ''
        if ($named -cnotmatch '^vcpkg_installed[\\/]') { continue }
        $why = Get-LibraryRefusal $named $repository "tests\$Test.args" $false
        if ($why) { throw $why }

        # Not $from and $to: PowerShell reads a name the same in capitals or small letters, so those would be -From and -To above,
        # which hold whole numbers, and a path can't go in one.
        $libraryFrom = Join-Path $repository $named
        $libraryTo = Join-Path $tree $named
        if (Test-Path -LiteralPath $libraryFrom -PathType Container) {
            & robocopy $libraryFrom $libraryTo /E /NFL /NDL /NJH /NJS /NP | Out-Null
            if ($LASTEXITCODE -ge 8) { throw "Couldn't copy $named" }
        }
        else {
            New-Item -ItemType Directory -Force (Split-Path -Parent $libraryTo) | Out-Null
            Copy-Item -LiteralPath $libraryFrom -Destination $libraryTo
        }
    }
    $global:LASTEXITCODE = 0
}

# A test that loads a library while it runs names it in its list instead (Copy-ListLibraries above says why)
Copy-ListLibraries $list $repository $tree "tests/takeouts/$Test.json"

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
