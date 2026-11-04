# Checks that no finding falls between two scans.
#
# While CodeQL watches the game being built, it also reads the headers of the libraries the game uses. A finding in one of
# those can't be fixed in our code, so the scan of our code sets it aside (set-aside.tsv) and leaves it to the libraries'
# own scan. That is only safe if the libraries' scan really reported it. Some findings only appear from the game's side:
# a header's helper can be "unused" in the game and used inside the library. Without this check, that finding would be
# counted by neither scan.
#
# It compares the rule and the file name, not the whole path or the line number, because the same header sits in a
# different folder in each scan, and a library's patches can move its lines.
#
# Microsoft's D3DX headers are listed but not judged: they are outside this gate by the project's ruling, and they have no
# library scan to compare with.
[CmdletBinding()]
param(
    # The folder holding the report of the scan of our code.
    [Parameter(Mandatory)] [string] $OwnReport,

    # The folder holding the report of the traced library scan.
    [Parameter(Mandatory)] [string] $LibraryReport
)

$ErrorActionPreference = 'Stop'

# Reads one of the gate's lists. A missing list means that scan didn't finish, so nothing was checked, and that must fail.
function Read-ReportList([string] $Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        Write-Output "::error::$Path is missing, so nothing could be compared"
        exit 1
    }
    return @(Get-Content -LiteralPath $Path | Where-Object { $_ })
}

# "cpp/some-rule" and a path become one key such as "cpp/some-rule|al.h".
function Get-FindingKey([string] $Rule, [string] $Path) {
    $name = ($Path -replace ':\d+$', '').Split('/')[-1]
    return "$Rule|$name"
}

$libraryKeys = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($line in (Read-ReportList (Join-Path $LibraryReport 'findings.tsv'))) {
    $columns = $line -split "`t"
    [void] $libraryKeys.Add((Get-FindingKey $columns[0] $columns[1]))
}

$microsoft = [System.Collections.Generic.List[string]]::new()
$checked = 0
$gaps = [System.Collections.Generic.List[string]]::new()
foreach ($line in (Read-ReportList (Join-Path $OwnReport 'set-aside.tsv'))) {
    $columns = $line -split "`t"
    $rule = $columns[0]
    $place = $columns[1]
    # The list also holds read problems. Only findings, which have a rule such as cpp/..., are compared.
    if ($rule -notmatch '^cpp/' -or $rule -match '^cpp/diagnostics/') { continue }
    if ($place -match '/dxsdk-d3dx/') {
        $microsoft.Add("$rule  $place")
        continue
    }
    $checked++
    if (-not $libraryKeys.Contains((Get-FindingKey $rule $place))) {
        $gaps.Add("$rule  $place")
    }
}

Write-Output "Library header findings from the game's build: $checked checked, $($gaps.Count) reached no library scan"
Write-Output "Microsoft D3DX, listed but not judged: $($microsoft.Count)"
foreach ($item in $microsoft) { Write-Output "  $item" }
if ($gaps.Count -gt 0) {
    foreach ($item in $gaps) { Write-Output "::error::Found only from the game's side: $item" }
    exit 1
}
Write-Output 'Every library header finding reached a library scan.'
