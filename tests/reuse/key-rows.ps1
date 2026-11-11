# Proves the reuse key notices every kind of change, and that lookup.ps1's checks refuse what they should. Written 2026-11-11 by Melanikus
# for the AvP port, as step 4 of the CI plan: "a take-out-style check where changing any one input changes the hash" (PO). It runs in
# every Tests run, and is never itself reused.
#
#   tests/reuse/key-rows.ps1 [-Verified <an attested records file lookup.ps1 checked>]
#
# KEY ROWS: in a small repository made in a temp folder, a made-up test is set up with a header that includes another, stand-ins,
# settings, the check's own files and the libraries' inputs. Its key is worked out; then one thing at a time is changed, and the key must
# change, and the first part that differs must be the one changed. With nothing changed, the key must stay the same. The class rows
# check that a way to open a file makes a test B, a clock or a library C, and the same words in a comment or in quotes don't count.
# REFUSAL ROWS: lookup.ps1's checks of a source run and of its records file, tried on made-up ones, each refused for its own reason.
# With -Verified, one byte of a real attested file is changed, and gh attestation verify must then refuse it.
# Each row says ok or FAILED; it gives back 1 if any row failed.

param([string]$Verified = '', [string]$Repository = "$env:GITHUB_REPOSITORY")

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Reuse.psm1') -Force
$script:failures = 0
$script:passes = 0
function Check([bool]$passed, [string]$what) { if ($passed) { $script:passes++; "ok      $what" } else { $script:failures++; "FAILED  $what" } }

$base = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { Join-Path (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))) 'AvP-test-out' }
$work = Join-Path $base 'reuse-key-rows'
if (Test-Path -LiteralPath $work) { Remove-Item -Recurse -Force -LiteralPath $work }
New-Item -ItemType Directory -Force $work | Out-Null

function Write-File([string]$name, [string]$text)
{
    $path = Join-Path $work $name
    New-Item -ItemType Directory -Force (Split-Path -Parent $path) | Out-Null
    [System.IO.File]::WriteAllText($path, $text)
}
# git's own warnings go to its error stream, which Windows PowerShell would take as this script failing, so they're let through quietly here
function Invoke-Git { $was = $ErrorActionPreference; $ErrorActionPreference = 'Continue'; & git @args 2>&1 | Out-Null; $ErrorActionPreference = $was }
function Commit([string]$what)
{
    Invoke-Git -C $work add -A
    Invoke-Git -C $work -c user.name=key-rows -c user.email=key-rows@localhost commit -q -m $what
    Clear-HashCache
}

# The made-up repository: a test that includes a.h, which includes b.h, with a stand-in and settings, and the check's own files
Invoke-Git init -q $work
Invoke-Git -C $work config core.autocrlf false
Write-File 'tests/zz_key_test.cpp' "#include `"a.h`"`nint main() { return Twice(2) == 4 ? 0 : 1; }`n"
Write-File 'tests/zz_key_test.args' "/MD /D_MSVC_STL_HARDENING=1 /Itests/every_test /FIevery_test.h /I3dc tests/zz_key_test.cpp 3dc/twice.cpp`n"
Write-File 'tests/zz_key_test_stubs/stand_in.h' "inline int StandIn() { return 1; }`n"
Write-File '3dc/a.h' "#include `"b.h`"`ninline int Twice(int n) { return Add(n, n); }`n"
Write-File '3dc/b.h' "inline int Add(int a, int b) { return a + b; }`n"
Write-File '3dc/twice.cpp' "int Unused() { return 0; }`n"
Write-File 'docs/elsewhere.md' "Nothing a test compiles.`n"
Write-File '.github/workflows/tests.yml' "name: Tests`n"
Write-File 'tests/build.ps1' "# build`n"
Write-File 'tests/every_test/every_test.h' "#pragma once`n"
Write-File 'tests/reuse/Reuse.psm1' "# the module`n"
Write-File 'vcpkg.json' "{ `"builtin-baseline`": `"0000000000000000000000000000000000000000`" }`n"
Write-File 'vcpkg-overlay-ports/enet/portfile.cmake' "# a port`n"
Write-File '.github/actions/fetch-test-libraries/action.yml' "name: Fetch`n"
Commit 'start'

$includes = @('3dc/a.h', '3dc/b.h')
$env:ImageOS = 'win25'
$env:ImageVersion = '1.0'
function Key([string]$test = 'zz_key_test', [string[]]$read = $includes) { Get-ReuseKey $work $test 'unit' $read (Get-SharedParts $work) }
$first = Key

# --- the key rows: one change at a time, then back
$second = Key
Check ($second.key -ceq $first.key -and (Get-FirstDifference $second $first.parts) -ceq 'nothing') 'with nothing changed, the key stays the same'
Check ($first.class -ceq 'A') "the made-up test is class A (it's $($first.class))"

$rows = @(
    @{ what = 'a header two deep (b.h)'; file = '3dc/b.h'; part = 'file:3dc/b.h' },
    @{ what = 'a header the test includes (a.h)'; file = '3dc/a.h'; part = 'file:3dc/a.h' },
    @{ what = "the test's .cpp"; file = 'tests/zz_key_test.cpp'; part = 'file:tests/zz_key_test.cpp' },
    @{ what = "the test's .args"; file = 'tests/zz_key_test.args'; part = 'file:tests/zz_key_test.args' },
    @{ what = 'a source the .args names'; file = '3dc/twice.cpp'; part = 'file:3dc/twice.cpp' },
    @{ what = 'a stand-in'; file = 'tests/zz_key_test_stubs/stand_in.h'; part = 'file:tests/zz_key_test_stubs/stand_in.h' },
    @{ what = 'the workflow'; file = '.github/workflows/tests.yml'; part = 'machinery' },
    @{ what = 'build.ps1'; file = 'tests/build.ps1'; part = 'machinery' },
    @{ what = 'tests/every_test/'; file = 'tests/every_test/every_test.h'; part = 'machinery' },
    @{ what = 'tests/reuse/'; file = 'tests/reuse/Reuse.psm1'; part = 'machinery' },
    @{ what = 'vcpkg.json'; file = 'vcpkg.json'; part = 'libraries' },
    @{ what = 'an overlay port'; file = 'vcpkg-overlay-ports/enet/portfile.cmake'; part = 'libraries' },
    @{ what = "the libraries' action"; file = '.github/actions/fetch-test-libraries/action.yml'; part = 'libraries' }
)
foreach ($row in $rows)
{
    $path = Join-Path $work $row.file
    $was = [System.IO.File]::ReadAllText($path)
    [System.IO.File]::WriteAllText($path, $was + "// one more line`n")
    Commit "change $($row.file)"
    $changed = Key
    Check ($changed.key -cne $first.key -and (Get-FirstDifference $changed $first.parts) -ceq "$($row.part) changed") "a change to $($row.what) changes the key, and the first part named is $($row.part) (it says: $(Get-FirstDifference $changed $first.parts))"
    [System.IO.File]::WriteAllText($path, $was)
    Commit "back $($row.file)"
    Check ((Key).key -ceq $first.key) "  and with it put back, the key is the first one again"
}

$env:ImageVersion = '2.0'
$changed = Key
Check ($changed.key -cne $first.key -and (Get-FirstDifference $changed $first.parts) -ceq 'machine changed') "a new runner image changes the key, by the machine part (it says: $(Get-FirstDifference $changed $first.parts))"
$env:ImageVersion = '1.0'

Write-File 'docs/new-file.md' "A file added anywhere.`n"
Commit 'a new file'
$changed = Key
Check ($changed.key -cne $first.key -and (Get-FirstDifference $changed $first.parts) -ceq 'names changed') "a file added anywhere changes the key, by the list of names (it says: $(Get-FirstDifference $changed $first.parts))"
Remove-Item -LiteralPath (Join-Path $work 'docs/new-file.md')
Commit 'the new file gone'

Write-File 'docs/elsewhere.md' "Changed, and still nothing a test compiles.`n"
Commit 'a file no test compiles'
Check ((Key).key -ceq $first.key) 'a class A key ignores a change to a file the test never compiled'

$changed = Key 'zz_key_test' @('3dc/a.h')
Check ($changed.key -cne $first.key) 'a key made from a shorter list of what the compiler read is a different key'

# --- the class rows
Write-File 'tests/zz_file_test.cpp' "#include <fstream>`nint main() { std::ifstream f(`"x`"); return 0; }`n"
Write-File 'tests/zz_file_test.args' "/MD tests/zz_file_test.cpp`n"
Write-File 'tests/zz_clock_test.cpp' "#include <windows.h>`nint main() { return GetTickCount() > 0 ? 0 : 1; }`n"
Write-File 'tests/zz_clock_test.args' "/MD tests/zz_clock_test.cpp`n"
Write-File 'tests/zz_quiet_test.cpp' "// GetTickCount() and std::ifstream in a comment don't count`nint main() { const char *s = `"fopen(`"; return s ? 0 : 1; }`n"
Write-File 'tests/zz_quiet_test.args' "/MD tests/zz_quiet_test.cpp`n"
Write-File 'tests/zz_library_test.cpp' "int main() { return 0; }`n"
Write-File 'tests/zz_library_test.args' "/MD tests/zz_library_test.cpp /link bcrypt.lib`n"
Write-File 'tests/zz_build_time_test.cpp' "int main() { return sizeof(__DATE__) > 1 ? 0 : 1; }`n"
Write-File 'tests/zz_build_time_test.args' "/MD tests/zz_build_time_test.cpp`n"
Commit 'the class tests'
Check ((Key 'zz_file_test' @()).class -ceq 'B') 'a test that opens a file while it runs is class B'
Check ((Key 'zz_clock_test' @()).class -ceq 'C') 'a test that reads a clock is class C, and always runs'
Check ((Key 'zz_quiet_test' @()).class -ceq 'A') 'the same words in a comment or in quotes leave a test class A'
Check ((Key 'zz_library_test' @()).class -ceq 'C') 'a test that links a library is class C'
Check ((Key 'zz_build_time_test' @()).class -ceq 'C') "a test that reads the build's own date is class C"

$fileKey = Key 'zz_file_test' @()
Write-File 'docs/elsewhere.md' "Changed once more.`n"
Commit 'any file at all'
$changed = Key 'zz_file_test' @()
Check ($changed.key -cne $fileKey.key -and (Get-FirstDifference $changed $fileKey.parts) -ceq 'tree changed') "a class B key changes with any file at all, by the tree (it says: $(Get-FirstDifference $changed $fileKey.parts))"

$threw = $false
try { [void](Key 'zz_key_test' @('../outside.h')) } catch { $threw = $true }
Check $threw 'a list of what the compiler read that names a file outside the repository is refused'
$threw = $false
try { [void](Key 'zz_key_test' @('3dc/not-there.h')) } catch { $threw = $true }
Check $threw 'a list that names a file that is no longer there is refused'

# --- the refusal rows: lookup.ps1's checks of a source run, and of its records file
$repo = 'Owner/Repo'
$goodRun = [pscustomobject]@{ id = 11; event = 'push'; conclusion = 'success'; path = '.github/workflows/tests.yml'; head_sha = ('a' * 40); head_branch = 'avpx_main'
    head_repository = [pscustomobject]@{ full_name = $repo }; repository = [pscustomobject]@{ full_name = $repo } }
Check ((Get-SourceRunRefusal $goodRun $repo '99') -ceq '') 'a run started by a push in this repository, that succeeded, can be a source'
$dispatched = $goodRun.PSObject.Copy(); $dispatched.event = 'workflow_dispatch'
Check ((Get-SourceRunRefusal $dispatched $repo '99') -ceq '') 'so can one started by hand'
foreach ($bad in @(
    @{ what = 'a pull request'; set = { param($r) $r.event = 'pull_request' } },
    @{ what = 'a pull request_target'; set = { param($r) $r.event = 'pull_request_target' } },
    @{ what = 'a run that failed'; set = { param($r) $r.conclusion = 'failure' } },
    @{ what = "another repository's head"; set = { param($r) $r.head_repository = [pscustomobject]@{ full_name = 'Someone/Fork' } } },
    @{ what = 'another workflow'; set = { param($r) $r.path = '.github/workflows/other.yml' } },
    @{ what = 'this very run'; set = { param($r) $r.id = 99 } },
    @{ what = 'a commit that is not a full hash'; set = { param($r) $r.head_sha = 'abc' } },
    @{ what = 'a branch with .. in it'; set = { param($r) $r.head_branch = 'a/../b' } },
    @{ what = 'a branch starting with a dash'; set = { param($r) $r.head_branch = '-x' } }))
{
    $run = $goodRun.PSObject.Copy()
    $run.head_repository = $goodRun.head_repository; $run.repository = $goodRun.repository
    & $bad.set $run
    $why = Get-SourceRunRefusal $run $repo '99'
    Check ($why -cne '') "a source run is refused for $($bad.what) ($why)"
}

function Good-Records { [pscustomobject]@{ schema = 1; run = [pscustomobject]@{ id = '11'; sha = ('a' * 40); event = 'push' }; records = @(
    [pscustomobject]@{ test = 'zz_key_test'; key = ('b' * 64); kind = 'unit'; status = 'ran'; class = 'A'; result = 'passed'; parts = @(, @('schema', ('c' * 64))); includes = @('3dc/a.h') }) } }
Check ((Get-RecordsRefusal (Good-Records) '11' ('a' * 40)) -ceq '') 'a records file in the shape record.ps1 writes is believed'
foreach ($bad in @(
    @{ what = 'another schema'; set = { param($d) $d.schema = 2 } },
    @{ what = 'another run'; set = { param($d) $d.run.id = '12' } },
    @{ what = 'another commit'; set = { param($d) $d.run.sha = ('f' * 40) } },
    @{ what = 'a run of a pull request'; set = { param($d) $d.run.event = 'pull_request' } },
    @{ what = 'a test named as a path'; set = { param($d) $d.records[0].test = '../x' } },
    @{ what = "a key that isn't 64 hex digits"; set = { param($d) $d.records[0].key = 'xyz' } },
    @{ what = 'a reused result written as a source'; set = { param($d) $d.records[0].status = 'reused' } },
    @{ what = 'a class that is not one'; set = { param($d) $d.records[0].class = 'D' } },
    @{ what = 'a part that is not a name and a hash'; set = { param($d) $d.records[0].parts = @(, @('schema')) } },
    @{ what = 'an include that is not a file name'; set = { param($d) $d.records[0].includes = @('C:\x.h') } }))
{
    $data = Good-Records
    & $bad.set $data
    $why = Get-RecordsRefusal $data '11' ('a' * 40)
    Check ($why -cne '') "a records file is refused for $($bad.what) ($why)"
}

# --- one byte changed in a real attested file, and the attestation must refuse it
if ($Verified -and (Test-Path -LiteralPath $Verified))
{
    $copy = Join-Path $work 'tampered-records.json'
    $bytes = [System.IO.File]::ReadAllBytes($Verified)
    $bytes[[int]($bytes.Length / 2)] = $bytes[[int]($bytes.Length / 2)] -bxor 1
    [System.IO.File]::WriteAllBytes($copy, $bytes)
    gh attestation verify $copy --repo $Repository --deny-self-hosted-runners 2>&1 | Out-Null
    Check ($LASTEXITCODE -ne 0) 'a records file with one byte changed fails gh attestation verify'
}
else
{
    'note    no attested records file was found by lookup.ps1 this time (the first runs have none), so the changed-byte row waits for one'
}

Remove-Item -Recurse -Force -LiteralPath $work
"$script:passes passed, $script:failures failed"
if ($script:failures -gt 0) { exit 1 }
exit 0
