# Finds the earlier results this run may reuse, and checks each one before it's believed. Written 2026-11-11 by Melanikus for the AvP
# port, as step 4 of the CI plan, at PO's go: "any doubt, it runs"; sources only from runs on avpx_main (after Secondary's attacker's
# reading, C1: a branch's own tests run with every right in the job that records, so nothing there can vouch for a branch's results); the
# workflow and these scripts byte for byte the same; the attestation checked with --deny-self-hosted-runners.
#
#   lookup.ps1 -Root <the repository> -Work <folder> [-Dry]   (it also writes <work>\machine.txt: this machine's words, see Reuse.psm1)
#
# It's run from a copy of tests/reuse taken from HEAD, so it's told where the repository is. It writes <work>\plan.json, and
# <work>\skip.txt for tests/build.ps1 -Skip: the tests not to build, as their result is reused. With -Dry (the nightly run), nothing is
# reused: plan.json then says what WOULD have been, for record.ps1 -Compare.
#
# A RUN IS A SOURCE ONLY IF ALL OF THESE HOLD (a cached "pass" is input, AvP Tao 3):
#   1. it's a run of tests.yml in this repository, ON AVPX_MAIN, started by a push or by the nightly schedule, never by hand or by a
#      pull request, and it succeeded (Reuse.psm1, Get-SourceRunRefusal)
#   2. its commit is one this commit was made from (git merge-base --is-ancestor; the job checks out the whole history)
#   3. at its commit, the workflow file and tests/build.ps1, tests/every_test/ and tests/reuse/ are byte for byte this commit's
#   4. its records file is attested by this repository's own tests.yml on avpx_main, at that commit, on GitHub's own runners
#      (gh attestation verify --cert-identity --source-ref --source-digest --deny-self-hosted-runners)
#   5. the file says it was made by that run, at that commit, on avpx_main, and is in the shape record.ps1 writes (Get-RecordsRefusal)
# AND A RECORD IS REUSED ONLY IF: it says the test ran there and passed; and its key, worked out again HERE from this commit's files, the
# record's list of what the compiler read, the record's list of the program's imports, and every test program that ran in that job (so
# an edit to any test that could have forged it drops it, Secondary's T2), is the same, with a class of A or B (the stricter of what the
# text and the imports say). The newest such record for each test wins. If anything at all goes wrong, nothing is
# reused, and the log says why.

param(
    [Parameter(Mandatory = $true)][string]$Root,
    [Parameter(Mandatory = $true)][string]$Work,
    [switch]$Dry
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Reuse.psm1') -Force
New-Item -ItemType Directory -Force $Work | Out-Null
$kMostSources = 20
$repo = "$env:GITHUB_REPOSITORY"

function Write-Plan([object[]]$reused, [object[]]$notes, [string]$verified)
{
    $plan = [ordered]@{ dry = [bool]$Dry; reused = @(if (-not $Dry) { $reused }); wouldReuse = @(if ($Dry) { $reused }); notes = @($notes); verifiedFile = $verified }
    [System.IO.File]::WriteAllText((Join-Path $Work 'plan.json'), ($plan | ConvertTo-Json -Depth 5))
    [System.IO.File]::WriteAllLines((Join-Path $Work 'skip.txt'), [string[]]@(if (-not $Dry) { $reused | ForEach-Object { $_.test } }))
}

# This machine's words, written down before any test runs, for the job that works the records out on another machine (record.ps1)
$machine = Get-MachineWords $Root
[System.IO.File]::WriteAllText((Join-Path $Work 'machine.txt'), $machine)
"This machine: $machine"

$notes = [System.Collections.Generic.List[string]]::new()
try
{
    if ($repo -cnotmatch '^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$') { throw "GITHUB_REPOSITORY isn't a repository's name: $repo" }
    if ("$env:GITHUB_RUN_ID" -cnotmatch '^[0-9]+$') { throw "GITHUB_RUN_ID isn't a run's number" }

    # gh must know every flag the checks use, or an older one could read a flag as something else (as tools/install-build.ps1 checks)
    $verifyHelp = (gh attestation verify --help) -join "`n"
    foreach ($flag in '--cert-identity', '--source-ref', '--source-digest', '--deny-self-hosted-runners') {
        if (-not $verifyHelp.Contains($flag)) { throw "This gh doesn't know $flag, so no attestation can be checked here" }
    }

    # Only runs on avpx_main are asked for, and each is checked again below, whatever GitHub's own filter did
    $runs = @((gh api "repos/$repo/actions/workflows/tests.yml/runs?branch=$kSourceBranch&status=success&per_page=100" | ConvertFrom-Json).workflow_runs)
    if ($LASTEXITCODE -ne 0) { throw "GitHub didn't list the earlier runs" }
    $candidates = @($runs | Where-Object { (Get-SourceRunRefusal $_ $repo "$env:GITHUB_RUN_ID") -ceq '' })
    "$($runs.Count) successful runs on $kSourceBranch listed, $($candidates.Count) of them started by a push or the nightly run in this repository"

    $verified = ''
    $decided = @{}
    $reused = [System.Collections.Generic.List[object]]::new()
    $shared = Get-SharedParts $Root $machine
    $rules = Get-ImportRules $Root
    $sources = 0
    foreach ($run in $candidates)
    {
        if ($sources -ge $kMostSources) { break }
        $id = "$($run.id)"; $sha = "$($run.head_sha)"

        & git -C $Root cat-file -e "$sha^{commit}" 2>$null
        if ($LASTEXITCODE -ne 0) { $notes.Add("run $id ($sha): that commit isn't in this history"); continue }
        & git -C $Root merge-base --is-ancestor $sha HEAD
        if ($LASTEXITCODE -ne 0) { $notes.Add("run $id ($sha): not a commit this one was made from"); continue }
        & git -C $Root diff --quiet $sha HEAD -- @($kMachinery)
        if ($LASTEXITCODE -ne 0) { $notes.Add("run $id ($sha): its workflow or reuse scripts aren't this commit's"); continue }
        $sources++

        $folder = Join-Path $Work "source-$id"
        if (Test-Path -LiteralPath $folder) { Remove-Item -Recurse -Force -LiteralPath $folder }
        gh run download $id --repo $repo --name reuse-records --dir $folder 2>&1 | Out-Null
        $file = Join-Path $folder 'records.json'
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $file)) { $notes.Add("run $id ($sha): no records to download (none kept, or past 30 days)"); continue }

        gh attestation verify $file --repo $repo --cert-identity "https://github.com/$repo/.github/workflows/tests.yml@refs/heads/$kSourceBranch" --source-ref "refs/heads/$kSourceBranch" --source-digest $sha --deny-self-hosted-runners 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) { $notes.Add("run $id ($sha): its records' attestation didn't check out, so none is believed"); continue }
        if (-not $verified) { $verified = $file }   # for the key rows' check that one byte changed fails the attestation

        # The file, read strictly: anything not in the shape record.ps1 writes, and none of it is believed
        $data = Get-Content -LiteralPath $file -Raw | ConvertFrom-Json
        $refusal = Get-RecordsRefusal $data $id $sha
        if ($refusal) { $notes.Add("run $id ($sha): its records aren't believed, as $refusal"); continue }

        foreach ($record in @($data.records | Where-Object { $_ }))
        {
            $test = "$($record.test)"
            if ($decided.ContainsKey($test)) { continue }
            if ($null -eq (Get-HeadBytes $Root "tests/$test.args")) { $decided[$test] = 'gone'; continue }
            if ($record.result -cne 'passed') { $decided[$test] = "it failed in run $id"; continue }
            try { $mine = Get-ReuseKey $Root $test 'unit' ([string[]]@($record.includes | Where-Object { $null -ne $_ })) ([string[]]@($record.imports | Where-Object { $null -ne $_ })) $shared $rules ([string[]]@($record.ranWith | Where-Object { $null -ne $_ })) }
            catch { $decided[$test] = "its key couldn't be worked out here: $_"; continue }
            if ($mine.class -ceq 'C') { $decided[$test] = "class C (text $($mine.textClass), imports $($mine.importClass)): always runs"; continue }
            if ($mine.key -cne "$($record.key)") { $decided[$test] = "run $id's record differs: $(Get-FirstDifference $mine @($record.parts))"; continue }
            $decided[$test] = "reused from run $id"
            $reused.Add([ordered]@{ test = $test; run = $id; sha = $sha; key = $mine.key; class = $mine.class })
        }
    }

    foreach ($test in @($decided.Keys | Sort-Object)) { if ($decided[$test] -notlike 'reused*') { $notes.Add("${test}: runs here, as $($decided[$test])") } }
    Write-Plan $reused.ToArray() $notes.ToArray() $verified
    "$(if ($Dry) { 'Would reuse' } else { 'Reusing' }) $($reused.Count) results, from $sources source runs:"
    foreach ($one in $reused) { "  $($one.test): class $($one.class), from run $($one.run) at $($one.sha)" }
    $notes | ForEach-Object { "  note: $_" }
}
catch
{
    # Any doubt, it runs: nothing is reused, and the log says why
    Write-Plan @() @("nothing is reused: $_") ''
    "Nothing is reused, as: $_"
}
exit 0
