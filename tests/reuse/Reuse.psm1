# What a test's result depends on, written down so that a run can reuse an earlier result only when none of it has changed. Written
# 2026-11-11 by Melanikus for the AvP port, as step 4 of the CI plan (the owner: "can't we take the last ran execution if we're 100%
# sure it doesn't affect the code?").
#
# THE RULE OVER ALL OF THIS: a reused result can only make a run faster, never weaker. Anything that can't be worked out here means the
# test runs.
#
# THE CLASSES. A test's compiled files are its .cpp, everything its .args names, its stand-ins, and every header the compiler said it
# read (cl /showIncludes). By what those files use:
#   A  nothing that opens a file, reads a clock, the environment or chance, or the time of the build: its result depends only on what
#      it compiled, so it can be reused when every one of those files is the same.
#   B  something that can open a file while it runs: until Windows' own record of what it opened is proven (the plan's step 5), its key
#      also holds the whole tree, so it's reused only on an identical tree.
#   C  a clock, the environment, chance, the build's own date or time, or a library it links (a library's own code isn't read here, and
#      the network ones read a clock and chance): it always runs.
# Words in comments and in quotes don't count. Anything in doubt goes to B or C, never A.
#
# THE KEY. One SHA-256 over lines of "part<TAB>hash", in a fixed order, so a miss can say which part changed:
#   schema and kind; the machine (the runner's image, Visual Studio's version and tools, the Windows SDKs); the libraries' inputs
#   (vcpkg.json, the overlay ports, the action that fetches them); the check's own files (the workflow, build.ps1, tests/every_test/,
#   tests/reuse/); the list of every tracked file's name; each compiled file by its contents; and for class B, the tree.

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'

$script:kSchema = 1
# The files that make and read the records. A source run must have these byte for byte as this commit has them (lookup.ps1), and they
# are a part of every key.
$script:kMachinery = @('.github/workflows/tests.yml', 'tests/build.ps1', 'tests/every_test', 'tests/reuse')
# What the libraries are built from (they're built from source each time, so their inputs, not their bytes)
$script:kLibraryInputs = @('vcpkg.json', 'vcpkg-overlay-ports', '.github/actions/fetch-test-libraries')

# Ways to open a file, or to look at a folder, while running (class B). A stream or a folder listing counts by its type's name alone,
# as it's declared before it's used (std::ifstream file(...)); a function, by its name and a bracket. The key rows try both.
$script:kFileWays = '\b(ifstream|ofstream|fstream|wifstream|wofstream|basic_ifstream|basic_fstream|filebuf|directory_iterator|recursive_directory_iterator|directory_entry)\b|\b(fopen|_wfopen|_wfopen_s|fopen_s|freopen|_fsopen|_wfsopen|CreateFile[AW]?|CreateFile2|OpenFile|FindFirstFile[AW]?|FindFirstFileEx[AW]?|LoadLibrary[AW]?|LoadLibraryEx[AW]?|GetPrivateProfile\w*|_open|_wopen|_sopen\w*|_wsopen\w*|GetFileAttributes\w*|PathFileExists\w*|ReadSource\w*|SourceSlice\w*|ReadWholeFile\w*|D3DX\w*FromFile\w*)\s*\(|std::filesystem|<filesystem>|<fstream>'
# A clock, the environment, chance, the build's own time (class C)
$script:kOtherWays = '\b(getenv|_wgetenv|_dupenv_s|_wdupenv_s|getenv_s|GetEnvironmentVariable[AW]?|GetTickCount\w*|QueryPerformanceCounter|timeGetTime|GetSystemTime\w*|GetLocalTime|time|_time64|clock|rand|srand|rand_s|random_device|BCryptGenRandom|CryptGenRandom|steady_clock|system_clock|high_resolution_clock|GetCurrentProcessId|GetCurrentThreadId)\s*\(|\b(random_device|steady_clock|system_clock|high_resolution_clock)\b|__DATE__|__TIME__|__TIMESTAMP__'

function Get-TextHash([string]$text)
{
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return (($sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($text)) | ForEach-Object { $_.ToString('x2') }) -join '') }
    finally { $sha.Dispose() }
}

# A file's hash, kept for the rest of the run: the same header is read for many tests, and nothing changes the files while this runs.
# Clear-HashCache forgets them, for the key rows, which do change files.
$script:hashes = @{}
function Get-FileHashHex([string]$path)
{
    $full = [System.IO.Path]::GetFullPath($path)
    if (-not $script:hashes.ContainsKey($full)) { $script:hashes[$full] = (Get-FileHash -Algorithm SHA256 -LiteralPath $full).Hash.ToLowerInvariant() }
    return $script:hashes[$full]
}
function Clear-HashCache { $script:hashes = @{} }

# Every tracked file under a path (a file or a folder), by its name from the top of the repository, in a fixed order
function Get-TrackedFiles([string]$root, [string[]]$paths)
{
    $found = @(& git -C $root ls-files -- @($paths) 2>$null)
    if ($LASTEXITCODE -ne 0) { throw "git couldn't list the tracked files under $($paths -join ', ')" }
    $sorted = [string[]]@($found | Where-Object { $_ })
    [Array]::Sort($sorted, [StringComparer]::Ordinal)
    return $sorted
}

# One hash for many files: each file's name and its own hash, a line each, in a fixed order
function Get-FilesHash([string]$root, [string[]]$files)
{
    $lines = foreach ($file in $files) { "$file`t$(Get-FileHashHex (Join-Path $root $file))" }
    return Get-TextHash (($lines -join "`n") + "`n")
}

# The parts every key shares, worked out once a run: the machine, the libraries' inputs, the check's own files, the names of all the
# tracked files, and the tree
function Get-SharedParts([string]$root)
{
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $visualStudio = ''
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
    $machine = "image=$($env:ImageOS) $($env:ImageVersion);vs=$vsVersion;tools=$tools;sdks=$sdks"

    $names = Get-TrackedFiles $root @('.')
    $tree = "$(& git -C $root rev-parse 'HEAD^{tree}' 2>$null)".Trim()
    if ($LASTEXITCODE -ne 0 -or $tree -cnotmatch '^[0-9a-f]{40}$') { throw "git couldn't say this commit's tree" }

    return [ordered]@{
        machine = Get-TextHash $machine
        machineSays = $machine
        libraries = Get-FilesHash $root (Get-TrackedFiles $root $script:kLibraryInputs)
        machinery = Get-FilesHash $root (Get-TrackedFiles $root $script:kMachinery)
        names = Get-TextHash (($names -join "`n") + "`n")
        tree = $tree
        visualStudio = $visualStudio
    }
}

# A test's own files: its .cpp and .args, its stand-ins, and every file of the repository its .args names
function Get-OwnFiles([string]$root, [string]$test)
{
    if ($test -cnotmatch '^[A-Za-z0-9_]+$') { throw "A test's name is letters, digits and underscores only, and this isn't: $test" }
    $files = [System.Collections.Generic.List[string]]::new()
    foreach ($file in @("tests/$test.cpp", "tests/$test.args")) { if (Test-Path -LiteralPath (Join-Path $root $file)) { $files.Add($file) } }
    foreach ($file in (Get-TrackedFiles $root @("tests/$($test)_stubs"))) { $files.Add($file) }
    $settings = Join-Path $root "tests/$test.args"
    if (Test-Path -LiteralPath $settings)
    {
        foreach ($word in @((Get-Content -LiteralPath $settings -Raw) -split '\s+' | Where-Object { $_ -and -not $_.StartsWith('/') -and -not $_.StartsWith('-') -and -not $_.StartsWith('@') }))
        {
            $file = $word -replace '\\', '/'
            if (Test-Path -LiteralPath (Join-Path $root $file) -PathType Leaf) { $files.Add($file) }
        }
    }
    return $files
}

# The class of a test, from its compiled files (its own, and what the compiler said it read). See the top of this file.
function Get-ReuseClass([string]$root, [string]$test, [string[]]$compiled)
{
    $settings = Join-Path $root "tests/$test.args"
    if (-not (Test-Path -LiteralPath $settings)) { return 'C' }
    # A library linked in: its code isn't read here
    if ((Get-Content -LiteralPath $settings -Raw) -match '(?i)\.lib(\s|$)|vcpkg_installed') { return 'C' }
    $class = 'A'
    foreach ($file in $compiled)
    {
        if ($file -notmatch '(?i)\.(c|cc|cpp|cxx|h|hh|hpp|hxx|inl|ipp)$') { continue }
        $path = Join-Path $root $file
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return 'C' }
        $text = [System.IO.File]::ReadAllText($path)
        # Comments and what's in quotes are taken out, so a word there doesn't count
        $code = [regex]::Replace($text, '/\*[\s\S]*?\*/', ' ')
        $code = [regex]::Replace($code, '//[^\n]*', ' ')
        $code = [regex]::Replace($code, '"(?:[^"\\\n]|\\.)*"', '""')
        if ($code -match $script:kOtherWays) { return 'C' }
        if ($code -match $script:kFileWays) { $class = 'B' }
    }
    return $class
}

# A test's key, and every part of it with its own hash. $includes are the repository files the compiler said it read for this test,
# from the run that made the result. A part that can't be worked out throws, and the caller then doesn't reuse.
function Get-ReuseKey([string]$root, [string]$test, [string]$kind, [string[]]$includes, $shared)
{
    if ($kind -cnotmatch '^(unit)$') { throw "There's no kind of result called $kind" }
    $compiled = [System.Collections.Generic.SortedSet[string]]::new([StringComparer]::Ordinal)
    foreach ($file in (Get-OwnFiles $root $test)) { [void]$compiled.Add($file) }
    foreach ($file in $includes)
    {
        if ($file -cnotmatch '^[A-Za-z0-9_./+-]+$' -or $file.StartsWith('/') -or ($file -split '/') -contains '..') { throw "An include isn't a file of the repository by its name: $file" }
        if (-not (Test-Path -LiteralPath (Join-Path $root $file) -PathType Leaf)) { throw "$test read $file when it was built, and it isn't there now" }
        [void]$compiled.Add($file)
    }
    $files = [string[]]@($compiled)
    $class = Get-ReuseClass $root $test $files

    $parts = [System.Collections.Generic.List[object]]::new()
    $parts.Add(@('schema', (Get-TextHash "$script:kSchema")))
    $parts.Add(@('kind', (Get-TextHash $kind)))
    $parts.Add(@('test', (Get-TextHash $test)))
    $parts.Add(@('class', (Get-TextHash $class)))
    $parts.Add(@('machine', $shared.machine))
    $parts.Add(@('libraries', $shared.libraries))
    $parts.Add(@('machinery', $shared.machinery))
    $parts.Add(@('names', $shared.names))
    foreach ($file in $files) { $parts.Add(@("file:$file", (Get-FileHashHex (Join-Path $root $file)))) }
    if ($class -ceq 'B') { $parts.Add(@('tree', (Get-TextHash $shared.tree))) }

    $text = (@($parts | ForEach-Object { "$($_[0])`t$($_[1])" }) -join "`n") + "`n"
    return [pscustomobject]@{ test = $test; kind = $kind; class = $class; key = (Get-TextHash $text); parts = $parts; includes = [string[]]@($includes) }
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

# Why a run GitHub lists can't be a source, or nothing if it can: lookup.ps1's checks 1 and 2, and the run's own numbers. The key rows
# (key-rows.ps1) try each reason.
function Get-SourceRunRefusal($run, [string]$repo, [string]$thisRun)
{
    if (@('push', 'workflow_dispatch') -cnotcontains "$($run.event)") { return "it was started by $($run.event), not by a push or by hand" }
    if ("$($run.conclusion)" -cne 'success') { return "it didn't succeed ($($run.conclusion))" }
    if ("$($run.head_repository.full_name)" -cne $repo -or "$($run.repository.full_name)" -cne $repo) { return 'it was a run of another repository' }
    if ("$($run.path)" -cne '.github/workflows/tests.yml') { return "it was a run of another workflow ($($run.path))" }
    if ("$($run.id)" -cnotmatch '^[0-9]+$' -or "$($run.id)" -ceq $thisRun) { return "its number isn't another run's" }
    if ("$($run.head_sha)" -cnotmatch '^[0-9a-f]{40}$') { return "its commit isn't a commit's full hash" }
    $branch = "$($run.head_branch)"
    if ($branch -cnotmatch '^[A-Za-z0-9._/-]+$' -or $branch.Contains('..') -or $branch.StartsWith('-')) { return "its branch isn't a branch's name: $branch" }
    return ''
}

# Why a downloaded records file can't be believed, or nothing if it can: lookup.ps1's check 6. Everything must be in the shape
# record.ps1 writes, said to come from that run and commit; one record out of shape and none of them is believed.
function Get-RecordsRefusal($data, [string]$id, [string]$sha)
{
    if ($null -eq $data -or $data.schema -ne $script:kSchema) { return "its schema isn't $script:kSchema" }
    if ("$($data.run.id)" -cne $id -or "$($data.run.sha)" -cne $sha) { return "it says it's from another run or commit ($($data.run.id) at $($data.run.sha))" }
    if (@('push', 'workflow_dispatch') -cnotcontains "$($data.run.event)") { return "it says it was made by a run started by $($data.run.event)" }
    foreach ($record in @($data.records))
    {
        if ("$($record.test)" -cnotmatch '^[A-Za-z0-9_]+$') { return "a record names a test that can't be one: $($record.test)" }
        if ("$($record.key)" -cnotmatch '^[0-9a-f]{64}$') { return "$($record.test)'s key isn't 64 hex digits" }
        if ("$($record.kind)" -cne 'unit' -or "$($record.status)" -cne 'ran') { return "$($record.test)'s record isn't of a test that ran here" }
        if (@('A', 'B', 'C') -cnotcontains "$($record.class)" -or @('passed', 'failed') -cnotcontains "$($record.result)") { return "$($record.test)'s class or result isn't one of the known ones" }
        foreach ($part in @($record.parts))
        {
            if (@($part).Count -ne 2 -or "$($part[0])" -cnotmatch '^[A-Za-z0-9_:./+-]+$' -or "$($part[1])" -cnotmatch '^[0-9a-f]{40,64}$') { return "a part of $($record.test)'s key isn't a name and a hash" }
        }
        foreach ($include in @($record.includes)) { if ("$include" -cnotmatch '^[A-Za-z0-9_./+-]+$') { return "$($record.test) names something it read that isn't a file's name" } }
    }
    return ''
}

Export-ModuleMember -Function Get-TextHash, Get-FileHashHex, Clear-HashCache, Get-TrackedFiles, Get-SharedParts, Get-OwnFiles, Get-ReuseClass, Get-ReuseKey, Get-FirstDifference, Get-SourceRunRefusal, Get-RecordsRefusal
Export-ModuleMember -Variable kMachinery
