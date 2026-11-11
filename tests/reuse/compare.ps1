# The nightly run's check, which reuses nothing: every test lookup.ps1 -Dry said it WOULD have reused must have passed tonight. If one
# didn't, the key missed something that changes a result, and this fails, naming the test, so the key can be fixed: a card, and nothing
# hidden. Written 2026-11-11 by Melanikus for the AvP port, as step 4 of the CI plan (it was record.ps1 -Compare until the records moved to
# a job of their own). It only reads, and can only fail.
#
#   compare.ps1 -Work <the folder with plan.json and results.json>

param([Parameter(Mandatory = $true)][string]$Work)

$ErrorActionPreference = 'Stop'
$plan = Get-Content -LiteralPath (Join-Path $Work 'plan.json') -Raw | ConvertFrom-Json
$resultsFile = Join-Path $Work 'results.json'
$results = if (Test-Path -LiteralPath $resultsFile) { Get-Content -LiteralPath $resultsFile -Raw | ConvertFrom-Json } else { $null }
$wrong = @(foreach ($one in @($plan.wouldReuse | Where-Object { $_ })) {
    $fresh = if ($results) { $results.tests.PSObject.Properties[$one.test] } else { $null }
    if (-not $fresh -or -not $fresh.Value.passed) { "$($one.test) (its record from run $($one.run) said it passed; tonight it $(if ($fresh) { 'failed' } else { "didn't run" }))" }
})
"Tonight's run against what would have been reused: $(@($plan.wouldReuse | Where-Object { $_ }).Count) tests would have been, and $($wrong.Count) of them didn't pass here"
if ($wrong.Count -gt 0) { throw "A reused result would have been wrong, so a key missed something: $($wrong -join '; '). Open a card and fix the key; nothing is hidden." }
