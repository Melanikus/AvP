# Runs one batch of take-out lists, one after another, on one machine, for the Take-outs check. Written 2026-11-11 by Melanikus for
# the AvP port, as the second step of the CI plan. share.ps1, beside this file, says how the lists are shared into batches.
#
# Each list, or piece of one, runs in a PowerShell of its own, through run.ps1, so nothing one leaves behind reaches the next:
# Visual Studio's setup, which run.ps1 makes each time, adds to the path, and by the third time in one window it was too long
# (SANITIZE-RUN-1). Every one of them runs, even after one fails. At the end this says how each went and how long it took, and gives
# back 1 if any didn't pass, naming each.
#
#   tests/takeouts/run-batch.ps1 -Items 'rim_test:0:-1 hudson_plan_test:0:197'
#
# An item is a list's name, its first row and its last (-1 for "to its end"), as share.ps1 writes them. Nothing else is taken.

param(
    [Parameter(Mandatory = $true)][string]$Items
)

$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$shell = Join-Path $PSHOME $(if ($PSVersionTable.PSEdition -eq 'Core') { 'pwsh.exe' } else { 'powershell.exe' })

$work = @($Items -split '\s+' | Where-Object { $_ })
if ($work.Count -eq 0) { throw 'This batch has no lists in it' }

$parsed = foreach ($item in $work) {
    if ($item -cnotmatch '^([A-Za-z0-9_]+):([0-9]+):(-1|[0-9]+)$') { throw "An item is a list's name, its first row and its last, and this isn't: $item" }
    $name = $Matches[1]; $from = [int]$Matches[2]; $to = [int]$Matches[3]
    if ($to -ge 0 -and $to -lt $from) { throw "$item ends before it starts" }
    if (-not (Test-Path -LiteralPath (Join-Path $repository "tests/takeouts/$name.json"))) { throw "There's no list of take-outs called $name" }
    [pscustomobject]@{ item = $item; list = $name; from = $from; to = $to }
}

$results = [System.Collections.Generic.List[object]]::new()
foreach ($one in $parsed) {
    $rows = if ($one.to -lt 0) { "rows $($one.from) to the end" } else { "rows $($one.from) to $($one.to)" }
    "===== $($one.list), $rows"
    $started = Get-Date
    if (-not (Test-Path -LiteralPath (Join-Path $repository "tests/$($one.list).args"))) {
        "tests/takeouts/$($one.list).json is a list for a test that isn't here: there's no tests/$($one.list).args"
        $code = 1
    }
    else {
        # -From:N and -To:N, with the colon, so a last row of -1 can't be read as the name of a switch
        & $shell -NoProfile -NonInteractive -File (Join-Path $PSScriptRoot 'run.ps1') $one.list "-From:$($one.from)" "-To:$($one.to)"
        $code = $LASTEXITCODE
    }
    $minutes = ((Get-Date) - $started).TotalMinutes
    $results.Add([pscustomobject]@{ list = $one.list; rows = $rows; passed = ($code -eq 0); code = $code; minutes = $minutes })
    # One line a list, in the same words each time, so its minutes can be read back into .github/takeouts-minutes.json
    "take-outs: $($one.list), $($rows): $(if ($code -eq 0) { 'passed' } else { "DIDN'T PASS (exit $code)" }), in $([Math]::Round($minutes, 1)) minutes"
}

$failed = @($results | Where-Object { -not $_.passed })
''
"This batch: $($results.Count - $failed.Count) of $($results.Count) passed"
foreach ($r in $results) { "  $(if ($r.passed) { 'passed' } else { "DIDN'T PASS" })  $($r.list), $($r.rows), $([Math]::Round($r.minutes, 1)) minutes" }
if ($env:GITHUB_STEP_SUMMARY) {
    $summary = @('| List | Rows | How it went | Minutes |', '|---|---|---|---|')
    $summary += @($results | ForEach-Object { "| $($_.list) | $($_.rows) | $(if ($_.passed) { 'passed' } else { "didn't pass" }) | $([Math]::Round($_.minutes, 1)) |" })
    $summary | Out-File -LiteralPath $env:GITHUB_STEP_SUMMARY -Append -Encoding utf8
}
if ($failed.Count -gt 0) {
    "$($failed.Count) of this batch's lists didn't pass: $(@($failed | ForEach-Object { "$($_.list) ($($_.rows))" }) -join ', '). Each one's own lines above say which row, and why."
    exit 1
}
exit 0
