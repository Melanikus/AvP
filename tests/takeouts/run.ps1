# Checks that a test really notices when the code it guards is broken. Written 2026-11-05 by Melanikus for the AvP port.
#
# README.md, beside this file, says what this is for. Each row of tests/takeouts/<test>.json is one small change to a copy of the code. For each, this builds the test against the
# changed copy and runs it. The test should fail. A row it doesn't fail on means that check isn't really being tested. The repository itself is never touched.
#
# Run it from anywhere, in PowerShell:
#   tests/takeouts/run.ps1 <test>                  for example: tests/takeouts/run.ps1 rim_test
#   tests/takeouts/run.ps1 <test> -From 3 -To 5    only those rows
#   tests/takeouts/run.ps1 -CheckRefusals          checks that this refuses the names it should, each for its own reason (added 2026-11-21)
#
# The copy and the programs land in AvP-test-out\takeouts\<this copy's folder name> next to the repository, a folder of this copy of the
# repository's own (modified 2026-11-21: copies side by side shared one, and emptied each other's), or under $env:RUNNER_TEMP in the automatic checks. Not the temp folder: antivirus programs distrust
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

# ---- The names a list, or a test's settings, give this to copy or change, each checked before it's used (added 2026-11-21, #163)
#
# A list comes from a pull request, and so do a test's settings. Each name in them that this copies, or changes a file by, goes through one
# of the functions below first. Each gives back why the name is refused, or nothing when it's fine. -CheckRefusals, at the end of this part,
# asks them about names made to be refused, each for its own reason, and about good names, which have to be taken.

# A name shown in a refusal is kept to one line
function Show-Name([object]$named) { return ("$named" -replace "[`r`n]+", ' / ') }

# A test's name, which is its list's name too: letters, digits and underscores only, so it can't name a file anywhere else. \A and \z are
# the very start and end of the name: ^ and $ would also take a name that ends in a line end.
function Test-TestName([string]$name) { return $name -cmatch '\A[A-Za-z0-9_]+\z' }

# The rule for every name inside a folder that a list, or a test's settings, give this, such as 3dc/avp or vcpkg_installed/x86-windows/lib/enet.lib.
# It is a name: text, and not empty. It has no : (which names a drive, or a second part of a file). It doesn't start at a root (\ or /). And
# each of its parts is letters, digits and - _ . + only, is not dots alone (. is the folder itself, and .. the one above it), and doesn't end
# in a dot (Windows drops a dot at the end of a part, so it would read the name as another), and isn't one Windows reads as a device
# instead of a file: CON, PRN, AUX, NUL, COM0 to COM9 or LPT0 to LPT9, in capitals or not, alone or before a dot. A space or a line end
# isn't one of those letters, so no part has one. Gives back why a name breaks the rule, or nothing when it keeps it.
function Get-NameRefusal([object]$named)
{
    if ($named -isnot [string] -or $named -eq '') { return "isn't a name: a name here is text, and not empty" }
    if ($named.Contains(':')) { return 'has a :, and a name here has none' }
    if ($named -cmatch '\A[\\/]') { return 'starts at a root, and a name here starts inside its folder' }
    foreach ($part in ($named -split '[\\/]')) {
        if ($part -cmatch '\A\.+\z') { return 'has a part made of dots alone, and a name here has no . or .. part' }
        if ($part -cnotmatch '\A[A-Za-z0-9_.+-]+\z' -or $part.EndsWith('.')) { return "has a part that isn't plain: each part is letters, digits and - _ . + only, and doesn't end in a dot" }
        if ($part -match '\A(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])(\..*)?\z') { return 'has a part Windows reads as a device, not a file: CON, PRN, AUX, NUL, COM0-9 and LPT0-9 are never a part here' }
    }
    return ''
}

# Whether a name, inside a folder, is a link (a junction, or a symbolic link), or is inside one. Windows follows a link to wherever it
# points, so what such a name gives may not be inside the folder at all. Each part is looked at in turn, from the folder down, as far as
# the parts are there.
function Test-HasLink([string]$folder, [string]$named)
{
    $path = $folder
    foreach ($part in ($named -split '[\\/]')) {
        $path = Join-Path $path $part
        $item = Get-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
        if ($null -eq $item) { return $false }
        if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) { return $true }
    }
    return $false
}

# Whether a path is inside a folder: below it, and not the folder itself, once the path is read the way Windows will read it, .. parts and all
function Test-Inside([string]$path, [string]$folder)
{
    $full = [System.IO.Path]::GetFullPath($path)
    $within = [System.IO.Path]::GetFullPath($folder).TrimEnd('\') + '\'
    return $full.StartsWith($within, [StringComparison]::OrdinalIgnoreCase)
}

# A name in a list's "copy": a folder of the repository, or a file at its top by its plain name, such as servers.cfg. It keeps the rule for
# names (Get-NameRefusal), is no link and in none (Test-HasLink), and is there.
function Get-CopyRefusal([object]$named, [string]$root)
{
    $why = Get-NameRefusal $named
    if ($why) { return "The list copies '$(Show-Name $named)', which $why" }
    if (Test-HasLink $root $named) { return "The list copies $named, which is a link, or inside one, and what a list copies is in the repository itself" }
    $copyFrom = Join-Path $root $named
    if (Test-Path -LiteralPath $copyFrom -PathType Container) { return '' }
    if (-not (Test-Path -LiteralPath $copyFrom -PathType Leaf)) { return "The list copies $named, and the repository has no such folder or file" }
    if ($named -cnotmatch '\A[A-Za-z0-9_][A-Za-z0-9_.-]*\z') { return "A list copies a file only from the top of the repository, by its plain name: $named" }
    return ''
}

# A name in a test's settings that starts with vcpkg_installed: a file or a folder vcpkg built, such as vcpkg_installed/x86-windows/lib/enet.lib.
# It keeps the rule for names (Get-NameRefusal), is no link and in none (Test-HasLink), and is there.
function Get-SettingsNameRefusal([string]$named, [string]$root, [string]$settingsName)
{
    $why = Get-NameRefusal $named
    if ($why) { return "$settingsName names $(Show-Name $named), which $why" }
    if (Test-HasLink $root $named) { return "$settingsName names $named, which is a link, or inside one, and what is copied is in the repository itself" }
    if (-not (Test-Path -LiteralPath (Join-Path $root $named))) { return "$settingsName names $named, and it isn't there. Build the game once, so vcpkg puts its libraries in vcpkg_installed." }
    return ''
}

# The file a row changes, in the copy of the code. It keeps the rule for names (Get-NameRefusal), is no link and in none (Test-HasLink),
# and is a file inside the copy.
function Get-RowFileRefusal([object]$file, [string]$copyRoot)
{
    $why = Get-NameRefusal $file
    if ($why) { return "it names the file '$(Show-Name $file)', which $why" }
    if (Test-HasLink $copyRoot $file) { return "it names the file $file, which is a link, or inside one, and a row changes a file in the copy itself" }
    $path = Join-Path $copyRoot $file
    if (-not (Test-Inside $path $copyRoot) -or -not (Test-Path -LiteralPath $path -PathType Leaf)) { return "it names a file that isn't in the copy: $file" }
    return ''
}

# Copies what a list names under "copy" into the copy of the code. Every name is checked first, and nothing is copied unless all are fine.
# What is copied comes from inside the repository, and goes inside the copy. Of a folder, only the sources, not what earlier builds left
# behind. A file is copied as it is.
function Copy-ListEntries([object]$list, [string]$root, [string]$copyRoot)
{
    foreach ($named in $list.copy) {
        $why = Get-CopyRefusal $named $root
        if ($why) { throw $why }
        if (-not (Test-Inside (Join-Path $root $named) $root) -or -not (Test-Inside (Join-Path $copyRoot $named) $copyRoot)) {
            throw "The list copies $named, which isn't inside the repository, or wouldn't be inside the copy"
        }
    }

    # /XJ leaves out the links inside a folder (junctions, and symbolic links), so all that comes in is in the repository itself
    foreach ($named in $list.copy) {
        # (Not $from: PowerShell reads $from and the -From parameter above as the same name, and an [int] can't hold a path)
        $copyFrom = Join-Path $root $named
        if (Test-Path -LiteralPath $copyFrom -PathType Leaf) {
            Copy-Item -LiteralPath $copyFrom -Destination (Join-Path $copyRoot $named)
            continue
        }
        & robocopy $copyFrom (Join-Path $copyRoot $named) /E /XJ /NFL /NDL /NJH /NJS /NP /XF *.obj *.pdb *.exe *.ilk *.iobj *.ipdb *.dll *.lib *.exp *.idb *.tlog *.recipe *.log | Out-Null
        if ($LASTEXITCODE -ge 8) { throw "Couldn't copy $(Show-Name $named)" }
    }
    $global:LASTEXITCODE = 0
}

# A test that links a library vcpkg built names it in its settings, under vcpkg_installed (net_close_test names ENet's headers and enet.lib).
# The copy of the folders leaves .lib files out on purpose, so exactly the ones the settings name are copied here, from this repository's own
# vcpkg_installed, which a build of the game fills. (Added 2026-11-07, for net_close_test.) Every name is checked first, and nothing is
# copied unless all are fine. Other words in the settings go to the compiler, which reads them itself, the same as when the Tests check
# builds the test (tests/build.ps1): this copies or changes nothing by them.
function Copy-SettingsLibraries([string]$settings, [string]$root, [string]$copyRoot, [string]$settingsName)
{
    if (-not (Test-Path -LiteralPath $settings)) { return }
    $words = @(Get-Content -LiteralPath $settings | ForEach-Object { $_ -split '\s+' } | Where-Object { $_ })
    $names = @($words | ForEach-Object { $_ -replace '^/I', '' } | Where-Object { $_ -cmatch '^vcpkg_installed[\\/]' })

    foreach ($named in $names) {
        $why = Get-SettingsNameRefusal $named $root $settingsName
        if ($why) { throw $why }
        if (-not (Test-Inside (Join-Path $root $named) $root) -or -not (Test-Inside (Join-Path $copyRoot $named) $copyRoot)) {
            throw "$settingsName names $named, which isn't inside the repository, or wouldn't be inside the copy"
        }
    }

    foreach ($named in $names) {
        # Not $from and $to: PowerShell reads a name the same in capitals or small letters, so those would be -From and -To above,
        # which hold whole numbers, and a path can't go in one.
        $libraryFrom = Join-Path $root $named
        $libraryTo = Join-Path $copyRoot $named
        if (Test-Path -LiteralPath $libraryFrom -PathType Container) {
            # /XJ leaves out the links inside the folder, as for the folders a list copies
            & robocopy $libraryFrom $libraryTo /E /XJ /NFL /NDL /NJH /NJS /NP | Out-Null
            if ($LASTEXITCODE -ge 8) { throw "Couldn't copy $named" }
        }
        else {
            New-Item -ItemType Directory -Force (Split-Path -Parent $libraryTo) | Out-Null
            Copy-Item -LiteralPath $libraryFrom -Destination $libraryTo
        }
    }
    $global:LASTEXITCODE = 0
}

# A library a list names under "libraries", to copy from this repository's vcpkg_installed: a file a test loads while it runs, instead of
# linking it, so the test's settings, which go to the compiler, can't name it (shader_test loads Microsoft's D3DX that way). It keeps the
# rule for names (Get-NameRefusal), is inside vcpkg_installed, is no link and in none (Test-HasLink), and is a file that is there. (Added
# 2026-11-21, #163.)
function Get-LibraryRefusal([object]$named, [string]$root)
{
    $why = Get-NameRefusal $named
    if ($why) { return "The list names the library '$(Show-Name $named)', which $why" }
    if ($named -cnotmatch '\Avcpkg_installed[\\/]') { return "The list names the library $named, which isn't inside vcpkg_installed" }
    if (Test-HasLink $root $named) { return "The list names the library $named, which is a link, or inside one, and what is copied is in the repository itself" }
    $libraryFrom = Join-Path $root $named
    if (Test-Path -LiteralPath $libraryFrom -PathType Container) { return "The list names the library $named, which is a folder, and a list's libraries are files" }
    if (-not (Test-Path -LiteralPath $libraryFrom -PathType Leaf)) { return "The list names the library $named, and it isn't there. Build the game once, so vcpkg puts its libraries in vcpkg_installed." }
    return ''
}

# Copies the libraries a list names under "libraries" into the copy of the code, each to the same place in it. Every name is checked
# first, and nothing is copied unless all are fine. The copy of the folders leaves every .dll out on purpose, so these are the only ones
# in it. A list with no "libraries" copies none.
function Copy-ListLibraries([object]$list, [string]$root, [string]$copyRoot)
{
    foreach ($named in $list.libraries) {
        $why = Get-LibraryRefusal $named $root
        if ($why) { throw $why }
        if (-not (Test-Inside (Join-Path $root $named) $root) -or -not (Test-Inside (Join-Path $copyRoot $named) $copyRoot)) {
            throw "The list names the library $named, which isn't inside the repository, or wouldn't be inside the copy"
        }
    }

    foreach ($named in $list.libraries) {
        $libraryTo = Join-Path $copyRoot $named
        New-Item -ItemType Directory -Force (Split-Path -Parent $libraryTo) | Out-Null
        Copy-Item -LiteralPath (Join-Path $root $named) -Destination $libraryTo
    }
}

# -CheckRefusals: shows that the checks above refuse each name they should, for its own reason, and take the good ones. It makes a made-up
# repository in a folder of its own, with made-up files in it (text, since nothing here is built or run), and asks about names made to be
# refused. It also gives whole lists, and whole settings, to the two copying functions, to show nothing is copied unless every name is
# fine. Nothing is really copied in this check: a function given the same name as a program or a command is run instead of it, so the
# robocopy and Copy-Item defined inside it only write down where they were asked to copy to. Gives back 1 if any answer is wrong, 0 if all
# are right.
function Test-Refusals
{
    # Outside the automatic checks, in a folder of this copy of the repository's own, so a check in a copy beside this one can't empty it
    $copy = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
    $outside = Split-Path -Parent $copy
    $root = if ($env:RUNNER_TEMP) { Join-Path $env:RUNNER_TEMP 'avp-takeouts-refusals' } else { Join-Path $outside "AvP-test-out\takeouts-refusals\$(Split-Path -Leaf $copy)" }

    # The folder is emptied each time, so only one this made is: it leaves a note in it saying so
    $note = 'made by the take-outs refusal check.txt'
    if (Test-Path -LiteralPath $root) {
        if (-not (Test-Path -LiteralPath (Join-Path $root $note) -PathType Leaf)) { throw "$root is there already, and this didn't make it, so it's left alone" }
        Remove-Item -LiteralPath $root -Recurse -Force
    }
    $made = Join-Path $root 'repository'
    $copyRoot = Join-Path $root 'copy'
    foreach ($folder in @('tests', 'dependencies\made-up\src', '.github', 'vcpkg_installed\x86-windows\include', 'vcpkg_installed\x86-windows\lib', 'vcpkg_installed\x86-windows\bin')) {
        New-Item -ItemType Directory -Force (Join-Path $made $folder) | Out-Null
    }
    New-Item -ItemType Directory -Force $copyRoot | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $root $note), 'tests/takeouts/run.ps1 -CheckRefusals makes this folder, and empties it the next time it runs.')
    foreach ($file in @('made-up.cfg', 'tests\made-up.cpp', 'vcpkg_installed\x86-windows\lib\made-up.lib', 'vcpkg_installed\x86-windows\bin\made-up.dll')) {
        [System.IO.File]::WriteAllText((Join-Path $made $file), 'A made-up file: only its name is checked.')
    }

    # Two links (junctions, which Windows lets anyone make) from inside the made-up repository to a made-up folder beside it, in the same
    # folder this check empties. If they can't be made, the check fails: it can't show what it's for.
    $beside = Join-Path $root 'beside'
    New-Item -ItemType Directory -Force $beside | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $beside 'made-up.cpp'), 'A made-up file beside the repository.')
    New-Item -ItemType Junction -Path (Join-Path $made 'linked') -Target $beside | Out-Null
    New-Item -ItemType Junction -Path (Join-Path $made 'vcpkg_installed\x86-windows\linked') -Target $beside | Out-Null

    $script:wrongAnswers = 0
    function Answer([string]$what, [string]$reason, [string]$said)
    {
        if ($reason -eq '' -and $said -eq '') { Write-Host "ok      $what"; return }
        if ($reason -ne '' -and $said.Contains($reason)) { Write-Host "ok      $what"; return }
        if ($reason -eq '') { Write-Host "FAILED  $what. It was refused: '$said'" }
        elseif ($said -eq '') { Write-Host "FAILED  $what. It was let through" }
        else { Write-Host "FAILED  $what. It was refused, but not for that reason: '$said'" }
        $script:wrongAnswers++
    }

    # Each function is asked inside try, so one that stops with an error gives that error as its answer, and the check goes on
    function Ask([scriptblock]$question) { try { return "$(& $question)" } catch { return "$_" } }

    $dots = 'a part made of dots alone'
    $plain = "a part that isn't plain"
    $colon = 'has a :'
    $atRoot = 'starts at a root'
    $notName = "isn't a name"
    $device = 'a part Windows reads as a device'
    $link = 'is a link, or inside one'
    $fullName = Join-Path $made 'tests'

    Write-Host 'Names of tests, and so of lists:'
    $cases = @(
        @{ Name = 'rim_test'; Taken = $true; What = 'a test''s name is taken' },
        @{ Name = "rim_test`n"; Taken = $false; What = 'a name ending in a line end is refused' },
        @{ Name = '..\rim_test'; Taken = $false; What = 'and one with anything but letters, digits and _' }
    )
    foreach ($case in $cases) {
        $said = Ask { Test-TestName $case.Name }
        if ($said -eq "$($case.Taken)") { Write-Host "ok      $($case.What)" } else { Write-Host "FAILED  $($case.What). It said: '$said'"; $script:wrongAnswers++ }
    }

    Write-Host 'Names a list copies:'
    $cases = @(
        @{ Name = 'tests'; Reason = ''; What = 'a folder at the top of the repository is taken' },
        @{ Name = 'dependencies/made-up/src'; Reason = ''; What = 'and a folder inside one' },
        @{ Name = 'dependencies\made-up\src'; Reason = ''; What = 'and the same written with \' },
        @{ Name = '.github'; Reason = ''; What = 'and a folder whose name starts with a dot' },
        @{ Name = 'made-up.cfg'; Reason = ''; What = 'and a file at the top, by its plain name' },
        @{ Name = '..'; Reason = $dots; What = 'a name that is .. is refused' },
        @{ Name = 'tests\..\..'; Reason = $dots; What = 'and one with .. parts after a folder' },
        @{ Name = 'tests/../tests'; Reason = $dots; What = 'and one with a .. part that comes back to a folder that is there' },
        @{ Name = '.'; Reason = $dots; What = 'a name that is . is refused' },
        @{ Name = 'tests/...'; Reason = $dots; What = 'a part of three dots is refused' },
        @{ Name = $fullName; Reason = $colon; What = 'a name with a drive in it is refused' },
        @{ Name = '\tests'; Reason = $atRoot; What = 'a name starting with \ is refused' },
        @{ Name = '/tests'; Reason = $atRoot; What = 'and with /' },
        @{ Name = 'tests.'; Reason = $plain; What = 'a name ending in a dot is refused' },
        @{ Name = 'tests '; Reason = $plain; What = 'and one ending in a space' },
        @{ Name = "tests`n"; Reason = $plain; What = 'and one ending in a line end' },
        @{ Name = 'NUL'; Reason = $device; What = 'a name Windows reads as a device is refused' },
        @{ Name = 'tests/con.txt'; Reason = $device; What = 'and one whose part before a dot is a device, in small letters' },
        @{ Name = 'linked'; Reason = $link; What = 'a link to a folder outside the repository is refused' },
        @{ Name = 'tests/made-up.cpp'; Reason = 'only from the top of the repository'; What = 'a file deeper in the repository is refused' },
        @{ Name = 'missing'; Reason = 'the repository has no such folder or file'; What = 'a name that isn''t there is refused' },
        @{ Name = 7; Reason = $notName; What = 'a number is refused' },
        @{ Name = ''; Reason = $notName; What = 'an empty name is refused' }
    )
    foreach ($case in $cases) { Answer $case.What $case.Reason (Ask { Get-CopyRefusal $case.Name $made }) }

    Write-Host 'Names in a test''s settings, under vcpkg_installed:'
    $cases = @(
        @{ Name = 'vcpkg_installed/x86-windows/lib/made-up.lib'; Reason = ''; What = 'a library vcpkg built is taken' },
        @{ Name = 'vcpkg_installed\x86-windows\lib\made-up.lib'; Reason = ''; What = 'and the same written with \' },
        @{ Name = 'vcpkg_installed/x86-windows/include'; Reason = ''; What = 'and a folder of headers' },
        @{ Name = 'vcpkg_installed/../tests'; Reason = $dots; What = 'a name with a .. part is refused' },
        @{ Name = 'vcpkg_installed/x86-windows/.../lib'; Reason = $dots; What = 'and one with a part of three dots' },
        @{ Name = 'vcpkg_installed/x86-windows/lib/made-up.lib:x'; Reason = $colon; What = 'a name with a : is refused' },
        @{ Name = 'vcpkg_installed/x86-windows/lib/made-up.lib.'; Reason = $plain; What = 'a name ending in a dot is refused' },
        @{ Name = 'vcpkg_installed/x86-windows/lib /made-up.lib'; Reason = $plain; What = 'and a part ending in a space' },
        @{ Name = 'vcpkg_installed/x86-windows/lib/AUX.lib'; Reason = $device; What = 'a name with a device in it is refused' },
        @{ Name = 'vcpkg_installed/x86-windows/linked/made-up.cpp'; Reason = $link; What = 'a name through a link to outside the repository is refused' },
        @{ Name = 'vcpkg_installed/x86-windows/lib/missing.lib'; Reason = "and it isn't there"; What = 'a name that isn''t there is refused' }
    )
    foreach ($case in $cases) { Answer $case.What $case.Reason (Ask { Get-SettingsNameRefusal $case.Name $made 'The settings' }) }

    Write-Host 'Files a row changes, in the copy:'
    $cases = @(
        @{ Name = 'tests/made-up.cpp'; Reason = ''; What = 'a file in the copy is taken' },
        @{ Name = 'tests\made-up.cpp'; Reason = ''; What = 'and the same written with \' },
        @{ Name = 'tests/../tests/made-up.cpp'; Reason = $dots; What = 'a name with a .. part is refused' },
        @{ Name = (Join-Path $made 'tests\made-up.cpp'); Reason = $colon; What = 'a name with a drive in it is refused' },
        @{ Name = '\tests\made-up.cpp'; Reason = $atRoot; What = 'a name starting with \ is refused' },
        @{ Name = 'tests/made-up.cpp.'; Reason = $plain; What = 'a name ending in a dot is refused' },
        @{ Name = 'tests/COM1.cpp'; Reason = $device; What = 'a name with a device in it is refused' },
        @{ Name = 'linked/made-up.cpp'; Reason = $link; What = 'a file through a link to outside the copy is refused' },
        @{ Name = 'tests/missing.cpp'; Reason = "isn't in the copy"; What = 'a file the copy doesn''t have is refused' },
        @{ Name = ''; Reason = $notName; What = 'an empty name is refused' }
    )
    foreach ($case in $cases) { Answer $case.What $case.Reason (Ask { Get-RowFileRefusal $case.Name $made }) }

    Write-Host 'Libraries a list names, under vcpkg_installed:'
    $cases = @(
        @{ Name = 'vcpkg_installed/x86-windows/bin/made-up.dll'; Reason = ''; What = 'a library vcpkg built is taken' },
        @{ Name = 'vcpkg_installed\x86-windows\bin\made-up.dll'; Reason = ''; What = 'and the same written with \' },
        @{ Name = 'tests/made-up.cpp'; Reason = "isn't inside vcpkg_installed"; What = 'a file outside vcpkg_installed is refused' },
        @{ Name = 'vcpkg_installed/../tests/made-up.cpp'; Reason = $dots; What = 'a name with a .. part is refused' },
        @{ Name = 'vcpkg_installed/x86-windows/bin/made-up.dll:x'; Reason = $colon; What = 'a name with a : is refused' },
        @{ Name = 'vcpkg_installed/x86-windows/bin/made-up.dll.'; Reason = $plain; What = 'a name ending in a dot is refused' },
        @{ Name = 'vcpkg_installed/x86-windows/bin/NUL.dll'; Reason = $device; What = 'a name with a device in it is refused' },
        @{ Name = 'vcpkg_installed/x86-windows/linked/made-up.cpp'; Reason = $link; What = 'a name through a link to outside the repository is refused' },
        @{ Name = 'vcpkg_installed/x86-windows/bin'; Reason = 'which is a folder'; What = 'a folder is refused: a list''s libraries are files' },
        @{ Name = 'vcpkg_installed/x86-windows/bin/missing.dll'; Reason = "and it isn't there"; What = 'a library that isn''t there is refused' },
        @{ Name = 5; Reason = $notName; What = 'a number is refused' },
        @{ Name = ''; Reason = $notName; What = 'an empty name is refused' }
    )
    foreach ($case in $cases) { Answer $case.What $case.Reason (Ask { Get-LibraryRefusal $case.Name $made }) }

    Write-Host 'A path has to be inside its folder:'
    $cases = @(
        @{ Path = (Join-Path $copyRoot 'tests'); Inside = $true; What = 'a folder in the copy is inside it' },
        @{ Path = (Join-Path $copyRoot 'tests\..\..\elsewhere'); Inside = $false; What = 'a path that leaves the copy is not' },
        @{ Path = "$copyRoot-other\tests"; Inside = $false; What = 'and a folder beside the copy whose name starts the same is not' },
        @{ Path = $copyRoot; Inside = $false; What = 'and the copy itself is not inside it' }
    )
    foreach ($case in $cases) {
        $said = Ask { Test-Inside $case.Path $copyRoot }
        if ($said -eq "$($case.Inside)") { Write-Host "ok      $($case.What)" } else { Write-Host "FAILED  $($case.What). It said: '$said'"; $script:wrongAnswers++ }
    }

    # From here on, robocopy and Copy-Item are these two, so nothing is copied: they write down where they were asked to copy to
    $script:copiedTo = @()
    $script:folderCopies = 0; $script:linksLeftOut = 0
    function robocopy { $script:copiedTo += "$($args[1])"; $script:folderCopies++; if ($args -ccontains '/XJ') { $script:linksLeftOut++ }; $global:LASTEXITCODE = 1 }
    function Copy-Item([string]$LiteralPath, [string]$Destination) { $script:copiedTo += $Destination }

    # Copies a whole list or whole settings, and says what came of it: the refusal, or where each thing went, by its path in the copy
    function Ask-Copying([scriptblock]$copying)
    {
        $script:copiedTo = @()
        $said = Ask $copying
        # Each place is read the way Windows will read it, .. parts and all, before it's said whether it's in the copy
        $places = @($script:copiedTo | ForEach-Object {
            $full = try { [System.IO.Path]::GetFullPath($_) } catch { $_ }
            if ("$full".StartsWith("$copyRoot\", [StringComparison]::OrdinalIgnoreCase)) { "$full".Substring($copyRoot.Length + 1) } else { "OUTSIDE THE COPY: $_" }
        })
        if ($said) { return "$said$(if ($places.Count) { " (and it had already copied to: $($places -join ', '))" })" }
        return "copied to: $($places -join ', ')"
    }

    Write-Host 'Whole lists, and whole settings:'
    $cases = @(
        @{ Json = '{"copy": ["tests", "made-up.cfg"]}'; Said = 'copied to: tests, made-up.cfg'; What = 'a good list copies each thing it names, to the same place in the copy' },
        @{ Json = '{"copy": [".."]}'; Said = 'refused'; What = 'a list copying .. is refused, and nothing is copied' },
        @{ Json = '{"copy": ["tests", ".."]}'; Said = 'refused'; What = 'a list with one bad name copies nothing, not even the good names before it' },
        @{ Json = '{"copy": [{"name": "tests"}]}'; Said = 'refused'; What = 'a list naming something that isn''t a name is refused' }
    )
    foreach ($case in $cases) {
        $said = Ask-Copying { Copy-ListEntries ($case.Json | ConvertFrom-Json) $made $copyRoot }
        $right = if ($case.Said -eq 'refused') { $said -ne '' -and -not $said.StartsWith('copied to:') -and -not $said.Contains('already copied') } else { $said -eq $case.Said }
        if ($right) { Write-Host "ok      $($case.What)" } else { Write-Host "FAILED  $($case.What). It said: '$said'"; $script:wrongAnswers++ }
    }

    $settings = Join-Path $root 'made-up.args'
    $cases = @(
        @{ Words = '/Ivcpkg_installed/x86-windows/include vcpkg_installed/x86-windows/lib/made-up.lib /W4'; Said = 'copied to: vcpkg_installed\x86-windows\include, vcpkg_installed\x86-windows\lib\made-up.lib'; What = 'good settings copy each library they name, to the same place in the copy' },
        @{ Words = '/Ivcpkg_installed/x86-windows/include vcpkg_installed/x86-windows/lib/made-up.lib.'; Said = 'refused'; What = 'settings with one bad name copy nothing, not even the good names before it' }
    )
    foreach ($case in $cases) {
        [System.IO.File]::WriteAllText($settings, $case.Words)
        $said = Ask-Copying { Copy-SettingsLibraries $settings $made $copyRoot 'The settings' }
        $right = if ($case.Said -eq 'refused') { $said -ne '' -and -not $said.StartsWith('copied to:') -and -not $said.Contains('already copied') } else { $said -eq $case.Said }
        if ($right) { Write-Host "ok      $($case.What)" } else { Write-Host "FAILED  $($case.What). It said: '$said'"; $script:wrongAnswers++ }
    }

    $cases = @(
        @{ Json = '{"libraries": ["vcpkg_installed/x86-windows/bin/made-up.dll"]}'; Said = 'copied to: vcpkg_installed\x86-windows\bin\made-up.dll'; What = 'a list''s library is copied to the same place in the copy' },
        @{ Json = '{"libraries": ["vcpkg_installed/x86-windows/bin/made-up.dll", "vcpkg_installed/../tests/made-up.cpp"]}'; Said = 'refused'; What = 'a list with one bad library copies none, not even the good one before it' },
        @{ Json = '{"libraries": "vcpkg_installed/x86-windows/bin/made-up.dll:x"}'; Said = 'refused'; What = 'one bad library on its own, not in a list, is refused too' },
        @{ Json = '{"copy": ["tests"]}'; Said = 'copied to: '; What = 'a list with no libraries copies none, and that is fine' }
    )
    foreach ($case in $cases) {
        $said = Ask-Copying { Copy-ListLibraries ($case.Json | ConvertFrom-Json) $made $copyRoot }
        $right = if ($case.Said -eq 'refused') { $said -ne '' -and -not $said.StartsWith('copied to:') -and -not $said.Contains('already copied') } else { $said -eq $case.Said }
        if ($right) { Write-Host "ok      $($case.What)" } else { Write-Host "FAILED  $($case.What). It said: '$said'"; $script:wrongAnswers++ }
    }

    # Every folder copied above was asked to leave out the links inside it (robocopy's /XJ), so all that comes in is in the repository itself
    $what = 'every folder is copied leaving out the links inside it'
    if ($script:folderCopies -gt 0 -and $script:linksLeftOut -eq $script:folderCopies) { Write-Host "ok      $what" }
    else { Write-Host "FAILED  $what. $($script:linksLeftOut) of $($script:folderCopies) copies of a folder were asked to (/XJ)"; $script:wrongAnswers++ }

    Write-Host "$(if ($script:wrongAnswers) { "$($script:wrongAnswers) answers were WRONG" } else { 'Every answer was right' })"
    return $(if ($script:wrongAnswers) { 1 } else { 0 })
}

if ($CheckRefusals) { exit (Test-Refusals) }

# ---- The run

# A list's name is a test's name, which is only letters, digits and underscores, so it can't name a file anywhere else
if (-not $Test) { throw 'Name a test, such as rim_test, or ask for -CheckRefusals' }
if (-not (Test-TestName $Test)) { throw "A list's name is letters, digits and underscores only, and this isn't: $Test" }

$repository = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$rowsFile = Join-Path $PSScriptRoot "$Test.json"
if (-not (Test-Path $rowsFile)) { throw "There's no list of take-outs at $rowsFile" }

$base = if ($env:RUNNER_TEMP) { Join-Path $env:RUNNER_TEMP 'avp-takeouts' } else { Join-Path (Split-Path -Parent $repository) "AvP-test-out\takeouts\$(Split-Path -Leaf $repository)" }
$tree = Join-Path $base "$Test\tree"
$out = Join-Path $base "$Test\out"
if (Test-Path -LiteralPath (Join-Path $base $Test)) { Remove-Item -LiteralPath (Join-Path $base $Test) -Recurse -Force }
New-Item -ItemType Directory -Force $tree | Out-Null

# These bytes go in and out exactly as they are, whatever they are, and line endings are put back as they were found.
$bytes = [System.Text.Encoding]::GetEncoding(28591)
$list = [System.IO.File]::ReadAllText($rowsFile, [System.Text.Encoding]::UTF8) | ConvertFrom-Json

# What the list copies, the libraries the test's settings name, and the ones the list names (Copy-ListEntries, Copy-SettingsLibraries and
# Copy-ListLibraries, above)
Copy-ListEntries $list $repository $tree
Copy-SettingsLibraries (Join-Path $repository "tests\$Test.args") $repository $tree "tests\$Test.args"
Copy-ListLibraries $list $repository $tree

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
    if (Test-Path -LiteralPath $out) { Remove-Item -LiteralPath $out -Recurse -Force }
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
        # The file a row changes has to be in the copy (Get-RowFileRefusal, above)
        $file = "$($change.file)"
        $why = Get-RowFileRefusal $change.file $tree
        if ($why) {
            Write-Host "#$r $($name): CAN'T RUN, $why"
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
