# What a test's result depends on, written down so that a run can reuse an earlier result only when none of it has changed. Written
# 2026-11-11 by Melanikus for the AvP port, as step 4 of the CI plan (the owner: "can't we take the last ran execution if we're 100%
# sure it doesn't affect the code?"). Modified the same day after Secondary's attacker's reading (AvP-docs, review-ci-reuse-4a-secondary.md):
# sources only from avpx_main; everything read from the commit itself, never from the disk; and the class taken from the built program's
# own import table as well as from its text.
#
# THE RULE OVER ALL OF THIS: a reused result can only make a run faster, never weaker. Anything that can't be worked out here means the
# test runs.
#
# WHERE EVERYTHING IS READ FROM. Every file is read from the commit being checked (HEAD), through git, never from the disk: a test runs
# with every right on the machine, and what it leaves on the disk is never believed. The list of names is HEAD's, the texts are HEAD's,
# and every hash is of HEAD's own bytes.
#
# THE CLASSES. A test's compiled files are its .cpp, everything its .args names, its stand-ins, and every header the compiler said it
# read (cl /showIncludes). Its class is the stricter of two answers, the text's and the program's:
#   by its text:     A if nothing in those files opens a file, reads a clock, the environment, chance or the build's own date or time,
#                    links a library, loads one, makes a thread or a process; B if something opens a file; C otherwise
#   by its program:  its import table (dumpbin /imports), against tests/reuse/imports-allowed.txt: A if every import is on the list's
#                    "allowed" part, B if the rest are all on its "files" part, C if anything else
#   A  its result depends only on what it compiled: reused when every compiled file is the same
#   B  it can open a file while it runs: until Windows' own record of what it opened is proven (the plan's step 5), its key also holds
#      the whole tree, so it's reused only on an identical tree
#   C  always runs
# Words in comments and in quotes don't count for the text, which is why the program's own imports are asked too.
#
# THE KEY. One SHA-256 over lines of "part<TAB>hash", in a fixed order, so a miss can say which part changed: schema and kind; the
# machine (the runner's image, Visual Studio's version and tools, the Windows SDKs); the libraries' inputs; the check's own files; the list
# of every name in the commit; each compiled file by its contents; the program's imports; and for class B, the tree.

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'

$script:kSchema = 3
# The files that make and read the records. A source run must have these byte for byte as this commit has them (lookup.ps1), and they
# are a part of every key.
$script:kMachinery = @('.github/workflows/tests.yml', 'tests/build.ps1', 'tests/every_test', 'tests/reuse')
# What the libraries are built from (they're built from source each time, so their inputs, not their bytes)
$script:kLibraryInputs = @('vcpkg.json', 'vcpkg-configuration.json', 'vcpkg-overlay-ports', 'vcpkg-overlay-triplets', '.github/actions/fetch-test-libraries')
# The only branch whose runs are sources (Secondary's C1: a branch's own tests run with every right in the job that records)
$script:kSourceBranch = 'avpx_main'

# Ways to open a file, or to look at a folder, while running (class B by the text). A stream or a folder listing counts by its type's
# name alone, as it's declared before it's used (std::ifstream file(...)); a function, by its name and a bracket. The key rows try both.
$script:kFileWays = '\b(ifstream|ofstream|fstream|wifstream|wofstream|basic_ifstream|basic_fstream|filebuf|directory_iterator|recursive_directory_iterator|directory_entry)\b|\b(fopen|_wfopen|_wfopen_s|fopen_s|freopen|_fsopen|_wfsopen|CreateFile[AW]?|CreateFile2|OpenFile|FindFirstFile[AW]?|FindFirstFileEx[AW]?|GetPrivateProfile\w*|_open|_wopen|_sopen\w*|_wsopen\w*|GetFileAttributes\w*|PathFileExists\w*|ReadSource\w*|SourceSlice\w*|ReadWholeFile\w*|D3DX\w*FromFile\w*|tmpfile|tmpnam|_tempnam|remove|_wremove|rename|_wrename|_unlink|DeleteFile[AW]?|MoveFile\w*|CopyFile\w*)\s*\(|std::filesystem|<filesystem>|<fstream>'
# A clock, the environment, chance, the build's own time, the machine, a library loaded by hand, a thread or a process (class C by the text)
$script:kOtherWays = '\b(getenv|_wgetenv|_dupenv_s|_wdupenv_s|getenv_s|GetEnvironmentVariable[AW]?|GetEnvironmentStrings\w*|GetTickCount\w*|QueryPerformanceCounter|QueryPerformanceFrequency|timeGetTime|GetSystemTime\w*|GetLocalTime|time|_time64|_time32|clock|rand|srand|rand_s|BCrypt\w*|CryptGenRandom|GetCurrentProcessId|GetCurrentThreadId|GetUserName\w*|GetComputerName\w*|GetSystemInfo|GetNativeSystemInfo|setlocale|_wsetlocale|Reg\w+Key\w*|RegQueryValue\w*|RegGetValue\w*|GetModuleHandle\w*|GetProcAddress|LoadLibrary\w*|CreateThread|_beginthread\w*|CreateProcess\w*|ShellExecute\w*|system|_wsystem|_popen|_wpopen|__rdtsc|__rdtscp|_rdrand\w*)\s*\(|\b(random_device|steady_clock|system_clock|high_resolution_clock|std::thread|std::jthread|std::async|hardware_concurrency)\b|__DATE__|__TIME__|__TIMESTAMP__|#\s*pragma\s+comment\s*\(\s*lib'

# ---- Reading the commit, never the disk

# git cat-file --batch, started once a run: each file of HEAD by its name, as bytes. Clear-HashCache forgets everything it read (for the
# key rows, which change the commit).
$script:reader = $null
$script:readerRoot = ''
$script:bytes = @{}
$script:names = $null
$script:modes = $null
$kMostBytes = 64MB   # no file of ours is near this; an answer bigger than it isn't believed (Secondary's R2, PO)

# Replacement objects are never followed: a .git/refs/replace entry could make a name give other bytes (Secondary's R1). The records are
# worked out in a job of their own, on a machine no test has run on, so this is a second line, not the first.
$env:GIT_NO_REPLACE_OBJECTS = '1'

function Clear-HashCache
{
    if ($script:reader) { try { $script:reader.StandardInput.Close(); $script:reader.WaitForExit(5000) | Out-Null } catch { } }
    $script:reader = $null
    $script:readerRoot = ''
    $script:bytes = @{}
    $script:names = $null
    $script:modes = $null
}

function Get-HeadBytes([string]$root, [string]$path)
{
    if ($path -cnotmatch '^[A-Za-z0-9_./+-]+$' -or $path.StartsWith('/') -or ($path -split '/') -contains '..') { throw "Not a file of the repository by its name: $path" }
    if ($script:readerRoot -cne $root) { Clear-HashCache }
    if ($script:bytes.ContainsKey($path)) { return $script:bytes[$path] }
    if (-not $script:reader)
    {
        $start = [System.Diagnostics.ProcessStartInfo]::new('git')
        $start.Arguments = "-C `"$root`" cat-file --batch"
        $start.RedirectStandardInput = $true
        $start.RedirectStandardOutput = $true
        $start.UseShellExecute = $false
        $script:reader = [System.Diagnostics.Process]::Start($start)
        $script:readerRoot = $root
    }
    $script:reader.StandardInput.Write("HEAD:$path`n")
    $script:reader.StandardInput.Flush()
    $out = $script:reader.StandardOutput.BaseStream
    # The answer's first line: "<object> <kind> <size>", followed by that many bytes and a line end; or "<what was asked> missing" alone
    $line = [System.Collections.Generic.List[byte]]::new()
    while ($true) { $b = $out.ReadByte(); if ($b -lt 0) { throw 'git stopped answering' }; if ($b -eq 10) { break }; $line.Add([byte]$b); if ($line.Count -gt 300) { throw "git's answer didn't start as it should" } }
    $header = [System.Text.Encoding]::ASCII.GetString($line.ToArray())
    if ($header -cmatch ' missing$') { $script:bytes[$path] = $null; return $null }
    if ($header -cnotmatch '^[0-9a-f]{40,64} (blob|tree|commit|tag) ([0-9]+)$') { throw "git's answer didn't start as it should: $header" }
    $kind = $Matches[1]
    $size = [int64]$Matches[2]
    if ($size -gt $kMostBytes) { throw "$path is $size bytes, more than any file of ours, so it isn't believed" }
    # The whole body is read whatever it is, so the next answer starts where it should (Secondary's R2); only a file's is kept
    $data = [byte[]]::new($size)
    $got = 0
    while ($got -lt $size) { $n = $out.Read($data, $got, [int]($size - $got)); if ($n -le 0) { throw 'git stopped in the middle of an answer' }; $got += $n }
    if ($out.ReadByte() -ne 10) { throw "git's answer didn't end as it should" }
    if ($kind -cne 'blob') { $script:bytes[$path] = $null; return $null }
    $script:bytes[$path] = $data
    return $data
}

function Get-HeadText([string]$root, [string]$path)
{
    $data = Get-HeadBytes $root $path
    if ($null -eq $data) { return $null }
    return [System.Text.Encoding]::UTF8.GetString($data)
}

function Get-TextHash([string]$text) { return Get-BytesHash ([System.Text.Encoding]::UTF8.GetBytes($text)) }
function Get-BytesHash([byte[]]$data)
{
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return (($sha.ComputeHash($data) | ForEach-Object { $_.ToString('x2') }) -join '') }
    finally { $sha.Dispose() }
}

# A file of HEAD by its hash, or throws if HEAD hasn't got it
function Get-HeadHash([string]$root, [string]$path)
{
    $data = Get-HeadBytes $root $path
    if ($null -eq $data) { throw "$path isn't in this commit" }
    return Get-BytesHash $data
}

# Every name in HEAD, in a fixed order
function Get-HeadNames([string]$root)
{
    if ($script:readerRoot -cne $root) { Clear-HashCache; $script:readerRoot = $root }
    if ($null -eq $script:names)
    {
        # "<mode> <kind> <object>`t<name>" a line: the mode says whether it's a plain file (100644, 100755), a link (120000) or another
        # repository (160000)
        $found = @(& git -C $root ls-tree -r HEAD 2>$null)
        if ($LASTEXITCODE -ne 0) { throw "git couldn't list this commit's files" }
        $modes = @{}
        foreach ($line in $found) { if ($line -cmatch '^([0-7]{6}) \S+ \S+\t(.+)$') { $modes[$Matches[2]] = $Matches[1] } }
        $sorted = [string[]]@($modes.Keys)
        [Array]::Sort($sorted, [StringComparer]::Ordinal)
        $script:names = $sorted
        $script:modes = $modes
    }
    return $script:names
}

# A name's mode in HEAD: 100644 or 100755 for a plain file, 120000 for a link, 160000 for another repository, '' if HEAD hasn't got it
function Get-HeadMode([string]$root, [string]$path)
{
    [void](Get-HeadNames $root)
    if ($script:modes.ContainsKey($path)) { return $script:modes[$path] }
    return ''
}

# Every name in HEAD under some paths (a file or a folder each)
function Get-HeadFiles([string]$root, [string[]]$paths)
{
    $all = Get-HeadNames $root
    return [string[]]@($all | Where-Object { $name = $_; @($paths | Where-Object { $name -ceq $_ -or $name.StartsWith("$_/") }).Count -gt 0 })
}

# One hash for many files: each file's name and its own hash, a line each, in a fixed order
function Get-FilesHash([string]$root, [string[]]$files)
{
    $lines = foreach ($file in $files) { "$file`t$(Get-HeadHash $root $file)" }
    return Get-TextHash (($lines -join "`n") + "`n")
}

# What a run changed in the tree, if anything: the files git sees as changed or new. A run that changed anything writes no records.
function Get-DirtyFiles([string]$root)
{
    $said = @(& git -C $root status --porcelain --untracked-files=all 2>$null)
    if ($LASTEXITCODE -ne 0) { return @("git couldn't say whether the tree changed") }
    return [string[]]@($said | Where-Object { $_ })
}

# ---- The parts every key shares

# What machine the tests are built and run on, in one line: the runner's image, Visual Studio's version and tools, the Windows SDKs,
# and how git puts the files on its disk (its version and core.autocrlf: the compiler reads the files as checked out, not as committed).
# The Unit tests job writes it down before any test runs; the job that works the records out, on another machine, is given that line.
function Get-MachineWords([string]$root)
{
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vsVersion = ''
    $tools = ''
    if (Test-Path -LiteralPath $vswhere)
    {
        $visualStudio = "$(& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)".Trim()
        $vsVersion = "$(& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion)".Trim()
        $toolsFile = Join-Path $visualStudio 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt'
        if (Test-Path -LiteralPath $toolsFile) { $tools = (Get-Content -LiteralPath $toolsFile -Raw).Trim() }
    }
    $kits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Include'
    $sdks = if (Test-Path -LiteralPath $kits) { (@(Get-ChildItem -LiteralPath $kits -Directory | ForEach-Object { $_.Name }) | Sort-Object) -join ',' } else { '' }
    $gitVersion = "$(& git --version 2>$null)".Trim()
    $autocrlf = "$(& git -C $root config --get core.autocrlf 2>$null)".Trim()
    return "image=$($env:ImageOS) $($env:ImageVersion);vs=$vsVersion;tools=$tools;sdks=$sdks;git=$gitVersion;autocrlf=$autocrlf"
}

# The parts every key shares. $machine is the line Get-MachineWords gave on the machine the tests ran on; left out, it's this machine's.
function Get-SharedParts([string]$root, [string]$machine = '')
{
    if (-not $machine) { $machine = Get-MachineWords $root }
    if ($machine -cnotmatch '^image=[^\r\n]*$') { throw "The machine's words aren't in their shape: $machine" }

    $names = Get-HeadNames $root
    # How git puts the files on the disk is part of what the compiler reads (line endings, Secondary's R3): every .gitattributes in HEAD
    $attributes = [string[]]@($names | Where-Object { $_ -ceq '.gitattributes' -or $_.EndsWith('/.gitattributes') })
    $tree = "$(& git -C $root rev-parse 'HEAD^{tree}' 2>$null)".Trim()
    if ($LASTEXITCODE -ne 0 -or $tree -cnotmatch '^[0-9a-f]{40}$') { throw "git couldn't say this commit's tree" }

    return [ordered]@{
        machine = Get-TextHash $machine
        machineSays = $machine
        libraries = Get-FilesHash $root (Get-HeadFiles $root $script:kLibraryInputs)
        machinery = Get-FilesHash $root (Get-HeadFiles $root $script:kMachinery)
        checkout = Get-FilesHash $root $attributes
        names = Get-TextHash (($names -join "`n") + "`n")
        tree = $tree
    }
}

# A test's own files: its .cpp and .args, its stand-ins, and every file of the commit its .args names
function Get-OwnFiles([string]$root, [string]$test)
{
    if ($test -cnotmatch '^[A-Za-z0-9_]+$') { throw "A test's name is letters, digits and underscores only, and this isn't: $test" }
    $names = Get-HeadNames $root
    $known = [System.Collections.Generic.HashSet[string]]::new([string[]]$names, [StringComparer]::Ordinal)
    $files = [System.Collections.Generic.List[string]]::new()
    foreach ($file in @("tests/$test.cpp", "tests/$test.args")) { if ($known.Contains($file)) { $files.Add($file) } }
    foreach ($file in (Get-HeadFiles $root @("tests/$($test)_stubs"))) { $files.Add($file) }
    $settings = Get-HeadText $root "tests/$test.args"
    if ($null -ne $settings)
    {
        foreach ($word in @($settings -split '\s+' | Where-Object { $_ -and -not $_.StartsWith('/') -and -not $_.StartsWith('-') -and -not $_.StartsWith('@') }))
        {
            $file = $word -replace '\\', '/'
            if ($known.Contains($file)) { $files.Add($file) }
        }
    }
    return $files
}

# ---- The classes

$script:order = @{ A = 0; B = 1; C = 2 }
function Get-Stricter([string]$one, [string]$other) { if ($script:order[$one] -ge $script:order[$other]) { return $one }; return $other }

# By the text of its compiled files. See the top of this file.
function Get-TextClass([string]$root, [string]$test, [string[]]$compiled)
{
    $settings = Get-HeadText $root "tests/$test.args"
    if ($null -eq $settings) { return 'C' }
    # A library linked in: its code isn't read here
    if ($settings -match '(?i)\.lib(\s|$)|vcpkg_installed') { return 'C' }
    $class = 'A'
    foreach ($file in $compiled)
    {
        if ($file -notmatch '(?i)\.(c|cc|cpp|cxx|h|hh|hpp|hxx|inl|ipp)$') { continue }
        $text = Get-HeadText $root $file
        if ($null -eq $text) { return 'C' }
        # Comments and what's in quotes are taken out, so a word there doesn't count
        $code = [regex]::Replace($text, '/\*[\s\S]*?\*/', ' ')
        $code = [regex]::Replace($code, '//[^\n]*', ' ')
        $code = [regex]::Replace($code, '"(?:[^"\\\n]|\\.)*"', '""')
        if ($code -match $script:kOtherWays) { return 'C' }
        if ($code -match $script:kFileWays) { $class = 'B' }
    }
    return $class
}

# The allow-list: "allowed DLL!Function" lines (what a test can call and still be class A) and "files DLL!Function" lines (what makes it
# class B at most). Read from HEAD.
function Get-ImportRules([string]$root)
{
    $text = Get-HeadText $root 'tests/reuse/imports-allowed.txt'
    if ($null -eq $text) { throw "tests/reuse/imports-allowed.txt isn't in this commit" }
    $rules = @{ allowed = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase); files = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase) }
    foreach ($line in ($text -split "`n"))
    {
        $line = $line.Trim()
        if (-not $line -or $line.StartsWith('#')) { continue }
        if ($line -cnotmatch '^(allowed|files) ([A-Za-z0-9_.-]+\.[dD][lL][lL])!(\S+)$') { throw "tests/reuse/imports-allowed.txt has a line that isn't a rule: $line" }
        [void]$rules[$Matches[1]].Add("$($Matches[2])!$($Matches[3])")
    }
    return $rules
}

# By the program's own imports, "DLL!Function" a line, as tests/build.ps1 -ShowIncludes writes them
function Get-ImportClass([string[]]$imports, $rules)
{
    if ($imports.Count -eq 0) { return 'C' }   # a list that's empty wasn't read: every program imports something
    $class = 'A'
    foreach ($import in $imports)
    {
        if ($rules.allowed.Contains($import)) { continue }
        if ($rules.files.Contains($import)) { $class = 'B'; continue }
        return 'C'
    }
    return $class
}

# ---- The key

# A test's key, and every part of it with its own hash. $includes are the repository files the compiler said it read for this test, and
# $imports the program's imports, both from the run that made the result. A part that can't be worked out throws, and then nothing is reused.
function Get-ReuseKey([string]$root, [string]$test, [string]$kind, [string[]]$includes, [string[]]$imports, $shared, $rules)
{
    if ($kind -cnotmatch '^(unit)$') { throw "There's no kind of result called $kind" }
    $names = [System.Collections.Generic.HashSet[string]]::new([string[]](Get-HeadNames $root), [StringComparer]::Ordinal)
    $compiled = [System.Collections.Generic.SortedSet[string]]::new([StringComparer]::Ordinal)
    foreach ($file in (Get-OwnFiles $root $test)) { [void]$compiled.Add($file) }
    foreach ($file in $includes)
    {
        if ($file -cnotmatch '^[A-Za-z0-9_./+-]+$' -or $file.StartsWith('/') -or ($file -split '/') -contains '..') { throw "An include isn't a file of the repository by its name: $file" }
        if (-not $names.Contains($file)) { throw "$test read $file when it was built, and this commit hasn't got it" }
        [void]$compiled.Add($file)
    }
    foreach ($import in $imports) { if ($import -cnotmatch '^[A-Za-z0-9_.-]+\.[dD][lL][lL]![A-Za-z0-9_?@$.#<>-]+$') { throw "An import isn't a DLL's name and a function's: $import" } }
    $files = [string[]]@($compiled)
    # Only plain files: a link's bytes are the name of what it points to, and another repository's are only its commit, so neither says
    # what the compiler read (Secondary's R3)
    foreach ($file in $files)
    {
        $mode = Get-HeadMode $root $file
        if ($mode -cne '100644' -and $mode -cne '100755') { throw "$file isn't a plain file in this commit (its mode is $mode), so what $test compiled can't be said by its bytes" }
    }
    $textClass = Get-TextClass $root $test $files
    $importClass = Get-ImportClass $imports $rules
    $class = Get-Stricter $textClass $importClass
    $sortedImports = [string[]]@($imports)
    [Array]::Sort($sortedImports, [StringComparer]::Ordinal)

    $parts = [System.Collections.Generic.List[object]]::new()
    $parts.Add(@('schema', (Get-TextHash "$script:kSchema")))
    $parts.Add(@('kind', (Get-TextHash $kind)))
    $parts.Add(@('test', (Get-TextHash $test)))
    $parts.Add(@('class', (Get-TextHash $class)))
    $parts.Add(@('machine', $shared.machine))
    $parts.Add(@('libraries', $shared.libraries))
    $parts.Add(@('machinery', $shared.machinery))
    $parts.Add(@('checkout', $shared.checkout))
    $parts.Add(@('names', $shared.names))
    foreach ($file in $files) { $parts.Add(@("file:$file", (Get-HeadHash $root $file))) }
    $parts.Add(@('imports', (Get-TextHash (($sortedImports -join "`n") + "`n"))))
    if ($class -ceq 'B') { $parts.Add(@('tree', (Get-TextHash $shared.tree))) }

    $text = (@($parts | ForEach-Object { "$($_[0])`t$($_[1])" }) -join "`n") + "`n"
    return [pscustomobject]@{ test = $test; kind = $kind; class = $class; textClass = $textClass; importClass = $importClass; key = (Get-TextHash $text); parts = $parts
        includes = [string[]]@($includes); imports = $sortedImports }
}

# The first part two keys differ in, for the log's "why it ran"
function Get-FirstDifference($mine, [object[]]$theirs)
{
    $theirMap = @{}
    foreach ($part in $theirs) { $theirMap[[string]$part[0]] = [string]$part[1] }
    foreach ($part in $mine.parts)
    {
        $name = [string]$part[0]
        if (-not $theirMap.ContainsKey($name)) { return "$name is new" }
        if ($theirMap[$name] -cne [string]$part[1]) { return "$name changed" }
        $theirMap.Remove($name)
    }
    if ($theirMap.Count -gt 0) { return "$(@($theirMap.Keys)[0]) is gone" }
    return 'nothing'
}

# ---- What lookup.ps1 believes

# Why a run GitHub lists can't be a source, or nothing if it can: a run of tests.yml in this repository, on avpx_main, started by a push
# or by the nightly schedule, that succeeded. The key rows try each reason.
function Get-SourceRunRefusal($run, [string]$repo, [string]$thisRun)
{
    if (@('push', 'schedule') -cnotcontains "$($run.event)") { return "it was started by $($run.event), not by a push or the nightly run" }
    if ("$($run.head_branch)" -cne $script:kSourceBranch) { return "it was a run on $($run.head_branch), not on $script:kSourceBranch" }
    if ("$($run.conclusion)" -cne 'success') { return "it didn't succeed ($($run.conclusion))" }
    if ("$($run.head_repository.full_name)" -cne $repo -or "$($run.repository.full_name)" -cne $repo) { return 'it was a run of another repository' }
    if ("$($run.path)" -cne '.github/workflows/tests.yml') { return "it was a run of another workflow ($($run.path))" }
    if ("$($run.id)" -cnotmatch '^[0-9]+$' -or "$($run.id)" -ceq $thisRun) { return "its number isn't another run's" }
    if ("$($run.head_sha)" -cnotmatch '^[0-9a-f]{40}$') { return "its commit isn't a commit's full hash" }
    return ''
}

# Why a downloaded records file can't be believed, or nothing if it can: everything in the shape record.ps1 writes, said to come from that
# run and commit, on avpx_main; one record out of shape and none of them is believed.
function Get-RecordsRefusal($data, [string]$id, [string]$sha)
{
    if ($null -eq $data -or $data.schema -ne $script:kSchema) { return "its schema isn't $script:kSchema" }
    if ("$($data.run.id)" -cne $id -or "$($data.run.sha)" -cne $sha) { return "it says it's from another run or commit ($($data.run.id) at $($data.run.sha))" }
    if (@('push', 'schedule') -cnotcontains "$($data.run.event)") { return "it says it was made by a run started by $($data.run.event)" }
    if ("$($data.run.ref)" -cne "refs/heads/$script:kSourceBranch") { return "it says it was made on $($data.run.ref)" }
    foreach ($record in @($data.records | Where-Object { $_ }))
    {
        if ("$($record.test)" -cnotmatch '^[A-Za-z0-9_]+$') { return "a record names a test that can't be one: $($record.test)" }
        if ("$($record.key)" -cnotmatch '^[0-9a-f]{64}$') { return "$($record.test)'s key isn't 64 hex digits" }
        if ("$($record.kind)" -cne 'unit' -or "$($record.status)" -cne 'ran') { return "$($record.test)'s record isn't of a test that ran here" }
        if (@('A', 'B', 'C') -cnotcontains "$($record.class)" -or @('A', 'B', 'C') -cnotcontains "$($record.importClass)" -or @('passed', 'failed') -cnotcontains "$($record.result)") { return "$($record.test)'s class or result isn't one of the known ones" }
        foreach ($part in @($record.parts))
        {
            if (@($part).Count -ne 2 -or "$($part[0])" -cnotmatch '^[A-Za-z0-9_:./+-]+$' -or "$($part[1])" -cnotmatch '^[0-9a-f]{40,64}$') { return "a part of $($record.test)'s key isn't a name and a hash" }
        }
        foreach ($include in @($record.includes | Where-Object { $null -ne $_ })) { if ("$include" -cnotmatch '^[A-Za-z0-9_./+-]+$') { return "$($record.test) names something it read that isn't a file's name" } }
        foreach ($import in @($record.imports | Where-Object { $null -ne $_ })) { if ("$import" -cnotmatch '^[A-Za-z0-9_.-]+\.[dD][lL][lL]![A-Za-z0-9_?@$.#<>-]+$') { return "$($record.test) names an import that isn't a DLL's name and a function's" } }
    }
    return ''
}

Export-ModuleMember -Function Clear-HashCache, Get-HeadBytes, Get-HeadText, Get-HeadMode, Get-MachineWords, Get-TextHash, Get-BytesHash, Get-HeadHash, Get-HeadNames, Get-HeadFiles, Get-DirtyFiles, Get-SharedParts, Get-OwnFiles, Get-Stricter, Get-TextClass, Get-ImportRules, Get-ImportClass, Get-ReuseKey, Get-FirstDifference, Get-SourceRunRefusal, Get-RecordsRefusal
Export-ModuleMember -Variable kMachinery, kSourceBranch
