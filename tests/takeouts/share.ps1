# Shares the take-out lists a run is to try out into a few batches, one machine each, for the Take-outs check. Written 2026-11-11 by
# Melanikus for the AvP port, as the second step of the CI plan (the owner: "Be smarter about it so buildschecks are faster").
#
# Why: one machine for each list made 150 jobs a run. On 2026-11-11, runs of this check on six heads at once took every runner the
# account has, and new runs couldn't start. Most lists take under 2 minutes, so most of each job was a machine starting up.
#
# How:
# - Each list's minutes come from .github/takeouts-minutes.json, the time it took when it last ran on a machine of its own. A list not
#   there counts as 2 minutes. The table only decides how the batches are balanced, never whether a list runs.
# - A list longer than $LongestPiece is cut into pieces of its rows, as run.ps1 -From and -To run them. Each piece builds the unchanged
#   test once more, first, to see that it passes: that's run.ps1's own first step.
# - The pieces go into batches, the longest first, each into the batch with the fewest minutes so far. There are as many batches as
#   it takes to keep each near $BatchMinutes, but never more than $MostBatches, and at least one.
# - Each batch is given a time limit of about three times its minutes, at least 30, so a lost runner frees its place.
# Nothing here runs a list: run-batch.ps1 does, one batch at a time.
#
#   tests/takeouts/share.ps1 -Lists 'rim_test glint_test'      prints the batches, and the line GitHub reads
#
# The last line it prints is "batches=" and the batches in JSON, for the workflow's output. It's written by hand, as the workflow writes
# its own: every list name is letters, digits and underscores (checked here again), and every other value is a whole number.

param(
    [Parameter(Mandatory = $true)][string]$Lists,
    [ValidateRange(1, 30)][int]$MostBatches = 8,
    [ValidateRange(5, 120)][int]$BatchMinutes = 25,
    [ValidateRange(5, 120)][int]$LongestPiece = 20
)

$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$kUnknownMinutes = 2.0

$names = @($Lists -split '\s+' | Where-Object { $_ })
foreach ($name in $names) {
    if ($name -cnotmatch '^[A-Za-z0-9_]+$') { throw "A list's name is letters, digits and underscores only, and this isn't: $name" }
    if (-not (Test-Path -LiteralPath (Join-Path $repository "tests/takeouts/$name.json"))) { throw "There's no list of take-outs called $name" }
}
if ($names.Count -eq 0) { throw 'No lists were given to share out' }

# The minutes each list took when it last ran on its own
$known = @{}
$table = Join-Path $repository '.github/takeouts-minutes.json'
if (Test-Path -LiteralPath $table) {
    foreach ($entry in ((Get-Content -LiteralPath $table -Raw | ConvertFrom-Json).minutes.PSObject.Properties)) {
        $value = [double]$entry.Value
        if ($value -lt 0 -or $value -gt 1000) { throw "$table says $($entry.Name) takes $value minutes, which can't be right" }
        $known[$entry.Name] = $value
    }
}

# Each list, or each piece of a long one, with its first and last row (-1 for "to its end", as run.ps1 takes it)
$pieces = [System.Collections.Generic.List[object]]::new()
foreach ($name in $names) {
    $minutes = if ($known.ContainsKey($name)) { $known[$name] } else { $kUnknownMinutes }
    $rowCount = @((Get-Content -LiteralPath (Join-Path $repository "tests/takeouts/$name.json") -Raw | ConvertFrom-Json).rows).Count
    $parts = [int][Math]::Ceiling($minutes / $LongestPiece)
    if ($parts -le 1 -or $rowCount -le 1) {
        $pieces.Add([pscustomobject]@{ list = $name; from = 0; to = -1; minutes = $minutes })
        continue
    }
    $parts = [Math]::Min($parts, $rowCount)
    $rowsEach = [int][Math]::Ceiling($rowCount / $parts)
    for ($from = 0; $from -lt $rowCount; $from += $rowsEach) {
        $to = [Math]::Min($rowCount - 1, $from + $rowsEach - 1)
        $pieces.Add([pscustomobject]@{ list = $name; from = $from; to = $to; minutes = $minutes * ($to - $from + 1) / $rowCount })
    }
}

$total = 0.0
foreach ($piece in $pieces) { $total += $piece.minutes }
$count = [int][Math]::Max(1, [Math]::Min($MostBatches, [Math]::Ceiling($total / $BatchMinutes)))

# The longest first, each into the batch with the fewest minutes so far (the first such batch, when two have the same). The order is
# fixed by the minutes, then the name, then the first row, so the same lists always make the same batches.
$batches = @(for ($i = 0; $i -lt $count; $i++) { [pscustomobject]@{ items = [System.Collections.Generic.List[object]]::new(); minutes = 0.0 } })
$ordered = @($pieces | Sort-Object -Property @{ Expression = 'minutes'; Descending = $true }, @{ Expression = 'list'; Descending = $false }, @{ Expression = 'from'; Descending = $false })
foreach ($piece in $ordered) {
    $emptiest = 0
    for ($i = 1; $i -lt $count; $i++) { if ($batches[$i].minutes -lt $batches[$emptiest].minutes) { $emptiest = $i } }
    $batches[$emptiest].items.Add($piece)
    $batches[$emptiest].minutes += $piece.minutes
}

$entries = [System.Collections.Generic.List[string]]::new()
for ($i = 0; $i -lt $count; $i++) {
    $batch = $batches[$i]
    $minutes = [int][Math]::Ceiling($batch.minutes)
    $limit = [int][Math]::Max(30, 3 * $minutes)
    $items = @($batch.items | ForEach-Object { "$($_.list):$($_.from):$($_.to)" }) -join ' '
    "Batch $($i + 1) of ${count}: about $minutes minutes, a limit of ${limit}: $items"
    $entries.Add("{`"n`":$($i + 1),`"of`":$count,`"minutes`":$minutes,`"limit`":$limit,`"items`":`"$items`"}")
}
"$($names.Count) lists, $($pieces.Count) pieces, about $([int][Math]::Ceiling($total)) minutes in all, in $count batches"
"batches=[$($entries -join ',')]"
