# Decides whether a CodeQL scan passes. CodeQL itself never fails a job over what it finds: it only writes a report, so
# without this script a scan full of problems would still show green.
#
# The scan passes only if all of these are true:
#   - the report exists, and it lists at least one file;
#   - it has no findings at all. Findings marked as "suppressed" still count, because we fix findings, we don't hide them;
#   - CodeQL read every file it should have read;
#   - CodeQL had no trouble reading any of them. Code it could not read was not checked, so it can't count as clean;
#   - CodeQL really read everything, instead of taking one of its pull request shortcuts;
#   - no comment anywhere tells CodeQL to look away.
#
# Every reason to fail is collected first, so one run shows the whole list instead of only the first problem.
[CmdletBinding()]
param(
    # Which scan this is. It sets the list of files we expect to see in the report:
    #   own         = every source file the game's Visual Studio projects compile;
    #   own-all     = every C/C++ file in git, compiled or not;
    #   libs        = the libraries as really compiled: each one must have at least one source file in the report;
    #   all-sources = every C/C++ file of the libraries, compiled or not;
    #   actions     = this repository's workflows: there is no list to compare with, but it must have read something.
    [Parameter(Mandatory)] [ValidateSet('own', 'own-all', 'libs', 'all-sources', 'actions')] [string] $Kind,

    # The folder CodeQL wrote its report (.sarif) into.
    [Parameter(Mandatory)] [string] $SarifFolder,

    # The folder CodeQL scanned. File names in the report are relative to it.
    [Parameter(Mandatory)] [string] $SourceRoot,

    # Where to write the lists a person reads afterwards.
    [Parameter(Mandatory)] [string] $ReportFolder,

    # The list of libraries the game depends on.
    [string] $Manifest = 'vcpkg.json',

    # The Visual Studio solution that builds the game.
    [string] $Solution = 'win32/VS2026/AvP.sln'
)

$ErrorActionPreference = 'Stop'

$failures = [System.Collections.Generic.List[string]]::new()
$sourceTypes = '.c', '.cc', '.cpp', '.cxx'
$headerTypes = '.h', '.hh', '.hpp', '.hxx'

# The report's "notices" are notes from the tool, separate from its findings. Each language puts its own name in front
# (cpp/..., js/..., actions/...), so these patterns match only the end. This one names each file it read.
$fileReadNotice = '/diagnostics/successfully-extracted-files$'

# These notices mean some code was not fully read. read-problems is our own query (.github/codeql/read-problems).
$readProblemNotice = '/diagnostics/(failed-extractor-invocations|extraction-warnings|extraction-errors|read-problems)$'

# Languages CodeQL can read that no job in codeql.yml scans. A file like this would be code nobody reads, so it fails
# the gate. Whoever adds one should add a CodeQL job for its language and take it off this list.
$unscannedTypes = '.cs', '.cshtml', '.razor', '.go', '.java', '.kt', '.kts', '.py', '.rb', '.erb', '.rs', '.swift',
    '.js', '.jsx', '.mjs', '.cjs', '.ts', '.tsx', '.mts', '.cts', '.vue', '.html', '.htm', '.xhtml'

# Helper entries in vcpkg's build folder that hold no library code. D3DX is Microsoft's binary package: it has no source
# to scan, and it is the one exception the project accepts.
$notLibraries = 'detect_compiler', 'dxsdk-d3dx', 'vcpkg-cmake', 'vcpkg-cmake-config'

function Test-FileType([string] $Path, [string[]] $Types) {
    return $Types -contains [System.IO.Path]::GetExtension($Path).ToLowerInvariant()
}

# Turns a location from the report into a plain file name such as 3dc/avp/weapons.cpp.
function Get-ReportPath($Location) {
    $uri = $Location.physicalLocation.artifactLocation.uri
    if ([string]::IsNullOrEmpty($uri)) { return $null }
    return [Uri]::UnescapeDataString($uri).Replace('\', '/')
}

function Write-ReportList([string] $Name, [object[]] $Lines) {
    Set-Content -LiteralPath (Join-Path $ReportFolder $Name) -Value $Lines -Encoding utf8
}

New-Item -ItemType Directory -Force -Path $ReportFolder | Out-Null

# No report means nothing was checked. That must fail, or a scan that broke would look the same as a clean one.
$sarifFiles = @(Get-ChildItem -LiteralPath $SarifFolder -Filter '*.sarif' -File -ErrorAction SilentlyContinue)
if ($sarifFiles.Count -eq 0) {
    Write-Output "::error::CodeQL wrote no report in $SarifFolder, so nothing was checked"
    exit 1
}

$findings = [System.Collections.Generic.List[object]]::new()
$notices = [System.Collections.Generic.List[object]]::new()
foreach ($file in $sarifFiles) {
    $report = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    foreach ($run in $report.runs) {
        foreach ($result in @($run.results)) {
            if ($null -ne $result) { $findings.Add($result) }
        }
        foreach ($invocation in @($run.invocations)) {
            foreach ($notice in @($invocation.toolExecutionNotifications)) {
                if ($null -ne $notice) { $notices.Add($notice) }
            }
        }
    }
}

# Findings: any at all fails the scan, whatever their level.
$findingLines = foreach ($finding in $findings) {
    $place = @($finding.locations)[0]
    $firstLine = ([string] $finding.message.text -split "`n")[0]
    '{0}{1}{2}:{3}{1}{4}' -f $finding.ruleId, "`t", (Get-ReportPath $place), $place.physicalLocation.region.startLine, $firstLine
}
Write-ReportList 'findings.tsv' @($findingLines)
$findingsByRule = @($findings | Group-Object -Property ruleId | Sort-Object -Property Count -Descending)
Write-ReportList 'findings-by-rule.txt' @($findingsByRule | ForEach-Object { '{0,7}  {1}' -f $_.Count, $_.Name })
if ($findings.Count -gt 0) {
    $failures.Add("$($findings.Count) findings ($($findingsByRule.Count) different rules)")
}

# Read problems: the report says some code was not fully read.
$noticesByKind = @($notices | Group-Object -Property { $_.descriptor.id } | Sort-Object -Property Count -Descending)
Write-ReportList 'notices-by-kind.txt' @($noticesByKind | ForEach-Object { '{0,7}  {1}' -f $_.Count, $_.Name })
$readProblems = @($notices | Where-Object { $_.descriptor.id -match $readProblemNotice })
$readProblemLines = foreach ($problem in $readProblems) {
    $place = @($problem.locations)[0]
    $firstLine = ([string] $problem.message.text -split "`n")[0]
    '{0}{1}{2}:{3}{1}{4}' -f $problem.descriptor.id, "`t", (Get-ReportPath $place), $place.physicalLocation.region.startLine, $firstLine
}
Write-ReportList 'read-problems.tsv' @($readProblemLines)
if ($readProblems.Count -gt 0) {
    $failures.Add("$($readProblems.Count) places CodeQL could not fully read (see read-problems.tsv)")
}

# Coverage: which files did CodeQL really read? Upper and lower case are treated as equal because Windows treats them so.
$seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($notice in $notices) {
    if ($notice.descriptor.id -match $fileReadNotice) {
        foreach ($place in @($notice.locations)) {
            $path = Get-ReportPath $place
            if ($null -ne $path) { [void] $seen.Add($path) }
        }
    }
}
Write-ReportList 'files-read.txt' @($seen | Sort-Object)
if ($seen.Count -eq 0) {
    $failures.Add('The report lists no files read, so the scan saw nothing')
}

$expected = @()
if ($Kind -eq 'own') {
    # The project files say exactly what the build compiles, and they don't depend on CodeQL. So if CodeQL lost sight of
    # part of the build, the files it missed show up here even though the build itself worked. A solution file lists its
    # projects with a line per project, and each project lists its files as <ClCompile Include="...">, relative to the
    # project's own folder.
    $root = (Resolve-Path -LiteralPath $SourceRoot).Path.TrimEnd('\', '/')
    $solutionFile = Join-Path $root $Solution
    $projects = @(Select-String -LiteralPath $solutionFile -Pattern '^Project\(.*\) = "[^"]+", "([^"]+\.vcxproj)"' |
        ForEach-Object { $_.Matches[0].Groups[1].Value })
    if ($projects.Count -eq 0) { $failures.Add("No projects found in $Solution") }
    $expected = @(foreach ($relativeProject in $projects) {
            $projectFile = [System.IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $solutionFile) $relativeProject))
            $project = [xml] (Get-Content -LiteralPath $projectFile -Raw)
            foreach ($item in $project.SelectNodes("//*[local-name()='ClCompile'][@Include]")) {
                $file = [System.IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $projectFile) $item.GetAttribute('Include')))
                $file.Substring($root.Length + 1).Replace('\', '/')
            }
        })
    if ($expected.Count -eq 0) { $failures.Add("The projects in $Solution list no source files") }
} elseif ($Kind -eq 'own-all') {
    # Git's list of our files is the truth: anything tracked must have been read. We ask for every file and pick out the
    # C/C++ ones ourselves, because git's own filter would miss a name in capitals like OLDFILE.CPP.
    $tracked = @(git -C $SourceRoot ls-files)
    if ($LASTEXITCODE -ne 0) { $failures.Add('git could not list the tracked files') }
    $expected = @($tracked | Where-Object { (Test-FileType $_ $sourceTypes) -or (Test-FileType $_ $headerTypes) })
    $unscanned = @($tracked | Where-Object { Test-FileType $_ $unscannedTypes })
    Write-ReportList 'languages-not-scanned.txt' $unscanned
    if ($unscanned.Count -gt 0) {
        $failures.Add("$($unscanned.Count) files are in a language CodeQL can read but no job scans (see languages-not-scanned.txt)")
    }
} elseif ($Kind -eq 'all-sources') {
    # installed-include is only there so the libraries' #include lines resolve. It is not a library of its own.
    $root = (Resolve-Path -LiteralPath $SourceRoot).Path.TrimEnd('\', '/')
    $expected = @(Get-ChildItem -LiteralPath $root -Recurse -File |
        Where-Object { (Test-FileType $_.Name $sourceTypes) -or (Test-FileType $_.Name $headerTypes) } |
        ForEach-Object { $_.FullName.Substring($root.Length + 1).Replace('\', '/') } |
        Where-Object { -not $_.StartsWith('installed-include/') })
} elseif ($Kind -eq 'libs') {
    # A compiled build only uses the files the library's build asks for, so there is no exact list to compare with here.
    # What we can demand is that every library was seen, which catches CodeQL losing sight of a library's whole build.
    $libraries = @(Get-ChildItem -LiteralPath $SourceRoot -Directory | Where-Object { $notLibraries -notcontains $_.Name })
    if ($libraries.Count -eq 0) { $failures.Add('No libraries were built') }
    $libraryLines = foreach ($library in $libraries) {
        $count = @($seen | Where-Object {
                $_.StartsWith("$($library.Name)/", [System.StringComparison]::OrdinalIgnoreCase) -and (Test-FileType $_ $sourceTypes)
            }).Count
        if ($count -eq 0) { $failures.Add("CodeQL saw no compiled source for the library $($library.Name)") }
        '{0,7}  {1}' -f $count, $library.Name
    }
    Write-ReportList 'source-files-by-library.txt' @($libraryLines)
}

$missed = @($expected | Where-Object { -not $seen.Contains($_) })
$missedSources = @($missed | Where-Object { Test-FileType $_ $sourceTypes })
$missedHeaders = @($missed | Where-Object { Test-FileType $_ $headerTypes })
Write-ReportList 'files-missed.txt' $missed
if ($missedSources.Count -gt 0) { $failures.Add("$($missedSources.Count) source files were not read (see files-missed.txt)") }
if ($missedHeaders.Count -gt 0) { $failures.Add("$($missedHeaders.Count) header files were not read (see files-missed.txt)") }

# A library that never reached this scan must not pass just because it left no folder behind, so the list of what we
# expect comes from vcpkg.json, not from what happens to be on disk.
if ($Kind -eq 'libs' -or $Kind -eq 'all-sources') {
    $dependencies = (Get-Content -LiteralPath $Manifest -Raw | ConvertFrom-Json).dependencies
    foreach ($dependency in $dependencies) {
        $name = if ($dependency -is [string]) { $dependency } else { $dependency.name }
        if (($notLibraries -notcontains $name) -and -not (Test-Path -LiteralPath (Join-Path $SourceRoot $name))) {
            $failures.Add("The library $name from vcpkg.json never reached this scan")
        }
    }
}

# codeql.yml turns off two shortcuts that make CodeQL look at less on a pull request. Those switches are not a promise:
# a future CodeQL Action could rename one, and the shortcut would quietly come back. So we check what really happened,
# from the notes the Action leaves in this run's temporary folder.
$actionFolder = if ($env:CODEQL_ACTION_TEMP) { $env:CODEQL_ACTION_TEMP } else { $env:RUNNER_TEMP }
if (Test-Path -LiteralPath (Join-Path $actionFolder 'pr-diff-range.json')) {
    $failures.Add('CodeQL only looked at the lines this pull request changed')
}
$actionSettings = Join-Path $actionFolder 'config'
if (-not (Test-Path -LiteralPath $actionSettings)) {
    $failures.Add('Could not confirm that CodeQL read everything: its settings file is missing')
} elseif ((Get-Content -LiteralPath $actionSettings -Raw | ConvertFrom-Json).overlayDatabaseMode -ne 'none') {
    $failures.Add('CodeQL reused part of an older scan instead of reading everything')
}

# A comment can tell CodeQL to look away from a line. We fix findings, so no such comment may exist, in our code or in a
# library's, even on a line with no finding today. These are the forms CodeQL itself accepts (its AlertSuppression
# library): "lgtm[...]" or "codeql[...]" anywhere in a comment, spaces allowed before the bracket, or a plain "lgtm" that
# starts a comment or follows a semicolon. Upper and lower case both count. This looks at every line, not only comments,
# so it can stop on something harmless, and then a person looks. The file list includes the types that C/C++ code pulls
# in with #include, such as .inl, because a comment in one of those works the same way.
$suppressionComment = '\b(lgtm|codeql)\s*\[[^\]]*\]|(//|/\*|#|;)\s*lgtm\b'
$textTypes = if ($Kind -eq 'actions') {
    '.yml', '.yaml'
} else {
    $sourceTypes + $headerTypes + '.inc', '.inl', '.ipp', '.tcc'
}
$suppressions = @(Get-ChildItem -LiteralPath $SourceRoot -Recurse -File |
    Where-Object { Test-FileType $_.Name $textTypes } |
    Select-String -Pattern $suppressionComment)
Write-ReportList 'suppressions.txt' @($suppressions | ForEach-Object { '{0}:{1}{2}{3}' -f $_.Path, $_.LineNumber, "`t", $_.Line.Trim() })
if ($suppressions.Count -gt 0) {
    $failures.Add("$($suppressions.Count) comments tell CodeQL to look away (see suppressions.txt)")
}

# Report: the same few numbers in the log and on the run's summary page.
$summary = @(
    "CodeQL gate ($Kind)",
    "  files read:      $($seen.Count)",
    "  files expected:  $($expected.Count)",
    "  sources missed:  $($missedSources.Count)",
    "  headers missed:  $($missedHeaders.Count)",
    "  findings:        $($findings.Count)",
    "  read problems:   $($readProblems.Count)",
    "  look-away notes: $($suppressions.Count)"
)
Write-ReportList 'summary.txt' $summary
$summary | Write-Output
if ($env:GITHUB_STEP_SUMMARY) {
    @('```') + $summary + @('```') | Out-File -Append -Encoding utf8 -LiteralPath $env:GITHUB_STEP_SUMMARY
}

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) { Write-Output "::error::$failure" }
    exit 1
}
Write-Output 'CodeQL gate passed.'
