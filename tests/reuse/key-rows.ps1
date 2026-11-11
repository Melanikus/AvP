# Proves the reuse key notices every kind of change, and that lookup.ps1's checks refuse what they should. Written 2026-11-11 by Melanikus
# for the AvP port, as step 4 of the CI plan: "a take-out-style check where changing any one input changes the hash" (PO). Modified the
# same day after Secondary's attacker's reading: sources only from avpx_main, everything read from the commit, and the import table.
# It runs in every Tests run, and is never itself reused.
#
#   key-rows.ps1 [-Verified <an attested records file lookup.ps1 checked>] [-Repository <owner/name>]
#
# KEY ROWS: in a small repository made in a temp folder, a made-up test is set up with a header that includes another, stand-ins,
# settings, the check's own files, the libraries' inputs and the import allow-list. Its key is worked out; then one thing at a time is
# changed and committed, and the key must change, and the first part that differs must be the one changed. With nothing changed, the key
# must stay the same; a change on the disk that isn't committed must change nothing, and must show as a changed tree.
# CLASS ROWS: by the text (a way to open a file makes a test B; a clock, the environment, a library, a thread, a process make it C; the same
# words in a comment or in quotes don't count) and by the imports (only allowed ones: A; a file function: B; anything else: C).
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
# A second test, which zz_key_test never compiles: it's there for the rows on the tests a key ran with
Write-File 'tests/zz_other_test.cpp' "int main() { return 0; }`n"
Write-File 'tests/zz_other_test.args' "/MD /D_MSVC_STL_HARDENING=1 /Itests/every_test /FIevery_test.h tests/zz_other_test.cpp`n"
Write-File 'tests/zz_other_test_stubs/other.h' "inline int Other() { return 2; }`n"
Write-File 'docs/elsewhere.md' "Nothing a test compiles.`n"
Write-File '.github/workflows/tests.yml' "name: Tests`n"
Write-File 'tests/build.ps1' "# build`n"
Write-File 'tests/every_test/every_test.h' "#pragma once`n"
Write-File 'tests/reuse/Reuse.psm1' "# the module`n"
Write-File 'tests/reuse/imports-allowed.txt' ([System.IO.File]::ReadAllText((Join-Path $PSScriptRoot 'imports-allowed.txt')))
Write-File 'vcpkg.json' "{ `"builtin-baseline`": `"0000000000000000000000000000000000000000`" }`n"
Write-File 'vcpkg-configuration.json' "{ }`n"
Write-File 'vcpkg-overlay-ports/enet/portfile.cmake' "# a port`n"
Write-File 'vcpkg-overlay-triplets/x86-windows-asan.cmake' "# a triplet`n"
Write-File '.github/actions/fetch-test-libraries/action.yml' "name: Fetch`n"
Commit 'start'

$includes = @('3dc/a.h', '3dc/b.h')
$floor = @('API-MS-WIN-CRT-RUNTIME-L1-1-0.DLL!exit', 'KERNEL32.DLL!QueryPerformanceCounter', 'VCRUNTIME140.DLL!memcpy')
$env:ImageOS = 'win25'
$env:ImageVersion = '1.0'
# A key, as record.ps1 and lookup.ps1 work it out. The tests it ran with are the test alone unless they're named.
function Key([string]$test = 'zz_key_test', [string[]]$read = $includes, [string[]]$imports = $floor, [string[]]$together = $null)
{
    if ($null -eq $together) { $together = @($test) }
    Get-ReuseKey $work $test 'unit' $read $imports (Get-SharedParts $work) (Get-ImportRules $work) $together
}
$first = Key

# --- the key rows: one change at a time, then back
$second = Key
Check ($second.key -ceq $first.key -and (Get-FirstDifference $second $first.parts) -ceq 'nothing') 'with nothing changed, the key stays the same'
Check ($first.class -ceq 'A') "the made-up test is class A (it's $($first.class): text $($first.textClass), imports $($first.importClass))"

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
    @{ what = 'the import allow-list'; file = 'tests/reuse/imports-allowed.txt'; part = 'machinery' },
    @{ what = 'vcpkg.json'; file = 'vcpkg.json'; part = 'libraries' },
    @{ what = 'vcpkg-configuration.json'; file = 'vcpkg-configuration.json'; part = 'libraries' },
    @{ what = 'an overlay port'; file = 'vcpkg-overlay-ports/enet/portfile.cmake'; part = 'libraries' },
    @{ what = 'an overlay triplet'; file = 'vcpkg-overlay-triplets/x86-windows-asan.cmake'; part = 'libraries' },
    @{ what = "the libraries' action"; file = '.github/actions/fetch-test-libraries/action.yml'; part = 'libraries' }
)
foreach ($row in $rows)
{
    $path = Join-Path $work $row.file
    $was = [System.IO.File]::ReadAllText($path)
    [System.IO.File]::WriteAllText($path, $was + "# one more line`n")
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

# --- every test that ran in the job is part of every key from it: a test runs with every right on its machine, so it could have made
# any result in its job say "passed" (PO, after Secondary's T2)
$both = @('zz_key_test', 'zz_other_test')
$withOther = Key 'zz_key_test' $includes $floor $both
Check ($withOther.key -cne $first.key -and (Get-FirstDifference $withOther $first.parts) -ceq 'ran-with changed') "a key holds the tests it ran with: with another test there, it's another key (it says: $(Get-FirstDifference $withOther $first.parts))"
Check ((Key 'zz_key_test' $includes $floor @('zz_other_test', 'zz_key_test', 'zz_other_test')).key -ceq $withOther.key) '  in any order, and with a name twice, the same key'
foreach ($row in @(
    @{ what = "the other test's .cpp"; file = 'tests/zz_other_test.cpp' },
    @{ what = "the other test's .args"; file = 'tests/zz_other_test.args' },
    @{ what = "the other test's stand-in"; file = 'tests/zz_other_test_stubs/other.h' }))
{
    $path = Join-Path $work $row.file
    $was = [System.IO.File]::ReadAllText($path)
    [System.IO.File]::WriteAllText($path, $was + "// one more line`n")
    Commit "change $($row.file)"
    $changed = Key 'zz_key_test' $includes $floor $both
    Check ($changed.key -cne $withOther.key -and (Get-FirstDifference $changed $withOther.parts) -ceq 'ran-with changed') "an edit to $($row.what), which zz_key_test never compiles, changes zz_key_test's key, by the tests it ran with (it says: $(Get-FirstDifference $changed $withOther.parts))"
    [System.IO.File]::WriteAllText($path, $was)
    Commit "back $($row.file)"
    Check ((Key 'zz_key_test' $includes $floor $both).key -ceq $withOther.key) '  and with it put back, the key is the same again'
}
Check ((Key 'zz_key_test' $includes $floor @('zz_key_test', 'zz_gone_test')).key -cne $first.key) 'a test that ran with it and is gone now makes another key'
foreach ($bad in @(@{ list = @(); what = 'an empty list' }, @{ list = @('zz_other_test'); what = "a list that hasn't got the test itself" }))
{
    $threw = $false
    try { [void](Get-ReuseKey $work 'zz_key_test' 'unit' $includes $floor (Get-SharedParts $work) (Get-ImportRules $work) ([string[]]$bad.list)) } catch { $threw = $true }
    Check $threw "a key from $($bad.what) of the tests it ran with is refused"
}
Check ((Get-AllTests $work) -join ' ' -ceq 'zz_key_test zz_other_test') "the commit's tests are each tests/<name>.args, and only those ($((Get-AllTests $work) -join ' '))"
Write-File 'tests/zz_other_test_stubs/inner.args' "not a test`n"
Commit 'an .args in a stand-in folder'
Check ((Get-AllTests $work) -join ' ' -ceq 'zz_key_test zz_other_test') "  an .args deeper down, in a stand-in folder, isn't a test"
Remove-Item -LiteralPath (Join-Path $work 'tests/zz_other_test_stubs/inner.args')
Write-File 'tests/zz_shout_test.ARGS' "/MD`n"
Commit 'an .ARGS'
$threw = $false
try { [void](Get-AllTests $work) } catch { $threw = $true }
Check $threw "  a tests/name.ARGS, which Windows would build as a test, is refused, as its name isn't in a test's shape"
Remove-Item -LiteralPath (Join-Path $work 'tests/zz_shout_test.ARGS')
Commit 'no .ARGS'
Check ((Key).key -ceq $first.key) '  and with both gone, the key is the first one again'

$changed = Key 'zz_key_test' @('3dc/a.h')
Check ($changed.key -cne $first.key) 'a key made from a shorter list of what the compiler read is a different key'
$changed = Key 'zz_key_test' $includes ($floor + 'API-MS-WIN-CRT-MATH-L1-1-0.DLL!sqrt')
Check ($changed.key -cne $first.key -and $changed.class -ceq 'A' -and (Get-FirstDifference $changed $first.parts) -ceq 'imports changed') "another allowed import changes the key, by the imports (it says: $(Get-FirstDifference $changed $first.parts))"

# --- the machine's words given by the Unit tests job, and how git puts the files on the disk
$given = Get-ReuseKey $work 'zz_key_test' 'unit' $includes $floor (Get-SharedParts $work 'image=given 1.0;vs=;tools=;sdks=;git=;autocrlf=') (Get-ImportRules $work) @('zz_key_test')
$givenAgain = Get-ReuseKey $work 'zz_key_test' 'unit' $includes $floor (Get-SharedParts $work 'image=given 1.0;vs=;tools=;sdks=;git=;autocrlf=') (Get-ImportRules $work) @('zz_key_test')
$givenOther = Get-ReuseKey $work 'zz_key_test' 'unit' $includes $floor (Get-SharedParts $work 'image=given 2.0;vs=;tools=;sdks=;git=;autocrlf=') (Get-ImportRules $work) @('zz_key_test')
Check ($given.key -ceq $givenAgain.key -and $given.key -cne $givenOther.key -and (Get-FirstDifference $givenOther $given.parts) -ceq 'machine changed') "the machine's words given from the test job make the machine part, and other words change it (it says: $(Get-FirstDifference $givenOther $given.parts))"
$threw = $false
try { [void](Get-SharedParts $work "not the machine's words") } catch { $threw = $true }
Check $threw "words that aren't in the machine's shape are refused"

# The PATH says which programs run as cl, link and dumpbin, so it's in the machine's words; this run's own folders are written as names,
# so the same machine says the same words from one run to the next (PO, after Secondary's T1)
$pathWas = $env:PATH
$tempWas = $env:RUNNER_TEMP
$wordsBefore = Get-MachineWords $work
$env:PATH = "$pathWas;C:\elsewhere"
Check ((Get-MachineWords $work) -cne $wordsBefore) "a folder added to the PATH changes the machine's words"
$env:RUNNER_TEMP = 'C:\run-one'; $env:PATH = "$pathWas;C:\run-one\tools"; $one = Get-MachineWords $work
$env:RUNNER_TEMP = 'C:\run-two'; $env:PATH = "$pathWas;C:\run-two\tools"; $two = Get-MachineWords $work
Check ($one -ceq $two -and $one.Contains('<temp>\tools')) "  and the run's own temp folder in it is written as a name, so two runs say the same"
$env:PATH = $pathWas
$env:RUNNER_TEMP = $tempWas

# CL, _CL_, LINK and _LINK_ add to every compile or link, so a step refuses the build when one is set (PO, after Secondary's T1)
foreach ($name in @('CL', '_CL_', 'LINK', '_LINK_'))
{
    $was = [Environment]::GetEnvironmentVariable($name)
    [Environment]::SetEnvironmentVariable($name, '/nologo')
    $named = @(Get-CompilerEnvironment) -ccontains $name
    [Environment]::SetEnvironmentVariable($name, $null)
    $gone = -not (@(Get-CompilerEnvironment) -ccontains $name)
    [Environment]::SetEnvironmentVariable($name, $was)
    Check ($named -and $gone) "$name set is named as something that adds to every compile or link, and not named once it's gone"
}

Write-File '.gitattributes' "*.h text eol=crlf`n"
Commit 'line endings'
$changed = Key
Check ($changed.key -cne $first.key -and (Get-FirstDifference $changed $first.parts) -ceq 'checkout changed') "a .gitattributes changes the key, by how git puts the files on the disk (it says: $(Get-FirstDifference $changed $first.parts))"
Remove-Item -LiteralPath (Join-Path $work '.gitattributes')
Commit 'no line endings rule'
Check ((Key).key -ceq $first.key) '  and with it gone, the key is the first one again'

# --- git's answers are read whole, whatever they are (Secondary's R2), and only plain files are believed (R3)
Clear-HashCache
Check ($null -eq (Get-HeadBytes $work '3dc')) "a folder's name gives no file's bytes"
$after = Get-HeadText $work '3dc/b.h'
Check ($after -ceq [System.IO.File]::ReadAllText((Join-Path $work '3dc/b.h'))) '  and the next file read after it is read whole and right, as its answer was read to its end'
Check ($null -eq (Get-HeadBytes $work '3dc/not-there.h')) "a name the commit hasn't got gives no bytes"
Check ((Get-HeadText $work '3dc/a.h') -ceq [System.IO.File]::ReadAllText((Join-Path $work '3dc/a.h'))) '  and the next one is right too'
$was = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
$blob = "$('3dc/b.h' | & git -C $work hash-object -w --stdin 2>$null)".Trim()
& git -C $work update-index --add --cacheinfo "120000,$blob,3dc/link.h" 2>&1 | Out-Null
$ErrorActionPreference = $was
Invoke-Git -C $work -c user.name=key-rows -c user.email=key-rows@localhost commit -q -m 'a link'
Clear-HashCache
Check ((Get-HeadMode $work '3dc/link.h') -ceq '120000') 'the made-up link is a link in the commit'
$threw = $false
try { [void](Key 'zz_key_test' @('3dc/a.h', '3dc/b.h', '3dc/link.h')) } catch { $threw = $true }
Check $threw "a list of what the compiler read that names a link is refused: a link's bytes are only where it points"
Invoke-Git -C $work rm -q --cached 3dc/link.h
Invoke-Git -C $work -c user.name=key-rows -c user.email=key-rows@localhost commit -q -m 'no link'
Clear-HashCache
Check ((Key).key -ceq $first.key) '  and with the link gone, the key is the first one again'

# --- two names that differ only by case are two files to git and one to Windows, where the compiler runs: such a commit isn't believed
# (Secondary's T6). Windows can't hold both on its disk, so the second is put straight into the commit, as the link was.
$was = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
$blob = "$('// another B.h' | & git -C $work hash-object -w --stdin 2>$null)".Trim()
& git -C $work -c core.ignorecase=false update-index --add --cacheinfo "100644,$blob,3dc/B.h" 2>&1 | Out-Null
$ErrorActionPreference = $was
Invoke-Git -C $work -c user.name=key-rows -c user.email=key-rows@localhost commit -q -m 'a case twin'
Clear-HashCache
$twins = @(& git -C $work ls-tree --name-only HEAD 3dc/) -join ' '
Check ($twins.Contains('3dc/B.h') -and $twins.Contains('3dc/b.h')) "the made-up commit has 3dc/b.h and 3dc/B.h ($twins)"
$threw = $false
try { [void](Key) } catch { $threw = "$_".Contains('differ only by case') }
Check $threw '  and no key is worked out for it: two names that differ only by case are refused'
Invoke-Git -C $work -c core.ignorecase=false rm -q --cached 3dc/B.h
Invoke-Git -C $work -c user.name=key-rows -c user.email=key-rows@localhost commit -q -m 'no case twin'
Clear-HashCache
Check ((Key).key -ceq $first.key) '  and with the twin gone, the key is the first one again'

# --- what's on the disk and not in the commit is never believed
Clear-HashCache
$header = Join-Path $work '3dc/b.h'
$was = [System.IO.File]::ReadAllText($header)
[System.IO.File]::WriteAllText($header, $was + "// changed on the disk only`n")
Clear-HashCache
Check ((Key).key -ceq $first.key) 'a change on the disk that is not in the commit changes nothing: every file is read from the commit'
Check ((Get-DirtyFiles $work).Count -gt 0) '  and it shows as a changed tree, so a run that did it writes no records'
[System.IO.File]::WriteAllText($header, $was)
Check ((Get-DirtyFiles $work).Count -eq 0) '  and with it put back, the tree is clean again'

# --- the class rows, by the text
$texts = @(
    @{ name = 'zz_file_test'; code = "#include <fstream>`nint main() { std::ifstream f(`"x`"); return 0; }`n"; args = ''; class = 'B'; what = 'a test that opens a file while it runs (a stream declared first) is B' },
    @{ name = 'zz_remove_test'; code = "#include <cstdio>`nint main() { return remove(`"x`"); }`n"; args = ''; class = 'B'; what = 'a test that removes a file is B' },
    @{ name = 'zz_clock_test'; code = "#include <windows.h>`nint main() { return GetTickCount() > 0 ? 0 : 1; }`n"; args = ''; class = 'C'; what = 'a test that reads a clock is C' },
    @{ name = 'zz_user_test'; code = "#include <windows.h>`nint main() { char n[64]; DWORD s = 64; return GetUserNameA(n, &s) ? 0 : 1; }`n"; args = ''; class = 'C'; what = "a test that reads the user's name is C" },
    @{ name = 'zz_locale_test'; code = "#include <clocale>`nint main() { return setlocale(LC_ALL, `"`") ? 0 : 1; }`n"; args = ''; class = 'C'; what = "a test that takes the machine's locale is C" },
    @{ name = 'zz_thread_test'; code = "#include <thread>`nint main() { std::thread t([] {}); t.join(); return 0; }`n"; args = ''; class = 'C'; what = 'a test that makes a thread is C' },
    @{ name = 'zz_load_test'; code = "#include <windows.h>`nint main() { return GetProcAddress(GetModuleHandleA(`"k`"), `"f`") ? 0 : 1; }`n"; args = ''; class = 'C'; what = 'a test that finds a function by its name while it runs is C' },
    @{ name = 'zz_pragma_test'; code = "#pragma comment(lib, `"advapi32.lib`")`nint main() { return 0; }`n"; args = ''; class = 'C'; what = 'a test that links a library from its own text (#pragma comment) is C' },
    @{ name = 'zz_quiet_test'; code = "// GetTickCount() and std::ifstream in a comment don't count`nint main() { const char *s = `"fopen(`"; return s ? 0 : 1; }`n"; args = ''; class = 'A'; what = 'the same words in a comment or in quotes leave a test A' },
    @{ name = 'zz_library_test'; code = "int main() { return 0; }`n"; args = ' /link bcrypt.lib'; class = 'C'; what = 'a test whose settings link a library is C' },
    @{ name = 'zz_build_time_test'; code = "int main() { return sizeof(__DATE__) > 1 ? 0 : 1; }`n"; args = ''; class = 'C'; what = "a test that reads the build's own date is C" }
)
foreach ($t in $texts) { Write-File "tests/$($t.name).cpp" $t.code; Write-File "tests/$($t.name).args" "/MD tests/$($t.name).cpp$($t.args)`n" }
Commit 'the class tests'
foreach ($t in $texts) { $k = Key $t.name @(); Check ($k.textClass -ceq $t.class -and $k.class -ceq $t.class) "$($t.what) (by its text it's $($k.textClass))" }

# --- the class rows, by the imports
$importRows = @(
    @{ add = @(); class = 'A'; what = 'only allowed imports leave a test A' },
    @{ add = @('KERNEL32.DLL!CreateFileW', 'KERNEL32.DLL!ReadFile'); class = 'B'; what = 'a file function makes a test B' },
    @{ add = @('KERNEL32.DLL!GetTickCount'); class = 'C'; what = 'a clock makes a test C' },
    @{ add = @('KERNEL32.DLL!GetSystemInfo'); class = 'C'; what = "the machine's numbers make a test C" },
    @{ add = @('ADVAPI32.DLL!RegOpenKeyExW'); class = 'C'; what = 'the registry makes a test C' },
    @{ add = @('KERNEL32.DLL!CreateThread'); class = 'C'; what = 'a thread makes a test C' },
    @{ add = @('KERNEL32.DLL!GetProcAddress'); class = 'C'; what = 'a function found by name makes a test C' },
    @{ add = @('WS2_32.DLL!connect'); class = 'C'; what = "a DLL that isn't on the list makes a test C" },
    @{ add = @('KERNEL32.DLL!#17'); class = 'C'; what = 'an import by number makes a test C' }
)
foreach ($r in $importRows) { $k = Key 'zz_key_test' $includes ($floor + $r.add); Check ($k.importClass -ceq $r.class -and $k.class -ceq $r.class) "$($r.what) (by its imports it's $($k.importClass))" }
Check ((Key 'zz_key_test' $includes @()).class -ceq 'C') "an empty list of imports means it wasn't read, so the test is C"
Check ((Key 'zz_quiet_test' @() ($floor + 'KERNEL32.DLL!GetTickCount')).class -ceq 'C') 'a test whose text says A and whose imports say C is C: the stricter answer wins'

$fileKey = Key 'zz_file_test' @()
Write-File 'docs/elsewhere.md' "Changed once more.`n"
Commit 'any file at all'
$changed = Key 'zz_file_test' @()
Check ($changed.key -cne $fileKey.key -and (Get-FirstDifference $changed $fileKey.parts) -ceq 'tree changed') "a class B key changes with any file at all, by the tree (it says: $(Get-FirstDifference $changed $fileKey.parts))"

# --- a word of a test's .args that names a file must be that file's exact name in the commit, or nothing is keyed (Secondary's T5)
$argsRows = @(
    @{ name = 'zz_args_case_test'; words = '3DC/twice.cpp'; throws = $true; what = 'a source named in another case' },
    @{ name = 'zz_args_dot_test'; words = './3dc/twice.cpp'; throws = $true; what = 'a source named from ./' },
    @{ name = 'zz_args_quoted_test'; words = '"3dc/twice.cpp"'; throws = $true; what = 'a source named in quotes' },
    @{ name = 'zz_args_up_test'; words = 'tests/../3dc/twice.cpp'; throws = $true; what = 'a source named through ..' },
    @{ name = 'zz_args_missing_test'; words = '3dc/not-there.cpp'; throws = $true; what = "a source the commit hasn't got" },
    @{ name = 'zz_args_bare_test'; words = 'twice.cpp'; throws = $true; what = 'a source named without its folder' },
    @{ name = 'zz_args_response_test'; words = '@tests/more.rsp'; throws = $true; what = 'another file of settings' },
    @{ name = 'zz_args_back_test'; words = '3dc\twice.cpp'; throws = $false; what = "a source named with Windows' \" },
    @{ name = 'zz_args_lib_test'; words = '/link vcpkg_installed/x86-windows/lib/enet.lib bcrypt.lib'; throws = $false; what = 'libraries, which make it C' }
)
foreach ($t in $argsRows) { Write-File "tests/$($t.name).cpp" "int main() { return 0; }`n"; Write-File "tests/$($t.name).args" "/MD tests/$($t.name).cpp $($t.words)`n" }
Commit 'the .args rows'
foreach ($t in $argsRows)
{
    $threw = $false
    $k = $null
    try { $k = Key $t.name @() } catch { $threw = $true }
    if ($t.throws) { Check $threw "an .args with $($t.what) ($($t.words)) is refused: what it compiles can't be keyed" }
    else { Check (-not $threw -and $null -ne $k -and ($t.name -cne 'zz_args_back_test' -or @($k.parts | Where-Object { $_[0] -ceq 'file:3dc/twice.cpp' }).Count -eq 1) -and ($t.name -cne 'zz_args_lib_test' -or $k.class -ceq 'C')) "an .args with $($t.what) ($($t.words)) is keyed$(if ($t.name -ceq 'zz_args_lib_test') { ', as class C' } else { ', with that file in the key' })" }
}

$threw = $false
try { [void](Key 'zz_key_test' @('../outside.h')) } catch { $threw = $true }
Check $threw 'a list of what the compiler read that names a file outside the repository is refused'
$threw = $false
try { [void](Key 'zz_key_test' @('3dc/not-there.h')) } catch { $threw = $true }
Check $threw 'a list that names a file the commit has not got is refused'
$threw = $false
try { [void](Key 'zz_key_test' $includes @('not an import')) } catch { $threw = $true }
Check $threw "a list of imports with a line that isn't a DLL's name and a function's is refused"

# --- the refusal rows: lookup.ps1's checks of a source run, and of its records file
$repo = 'Owner/Repo'
$goodRun = [pscustomobject]@{ id = 11; event = 'push'; conclusion = 'success'; path = '.github/workflows/tests.yml'; head_sha = ('a' * 40); head_branch = 'avpx_main'
    head_repository = [pscustomobject]@{ full_name = $repo }; repository = [pscustomobject]@{ full_name = $repo } }
Check ((Get-SourceRunRefusal $goodRun $repo '99') -ceq '') 'a run on avpx_main started by a push in this repository, that succeeded, can be a source'
$nightly = $goodRun.PSObject.Copy(); $nightly.event = 'schedule'
Check ((Get-SourceRunRefusal $nightly $repo '99') -ceq '') 'so can the nightly run on avpx_main'
foreach ($bad in @(
    @{ what = 'a run started by hand, even on avpx_main'; set = { param($r) $r.event = 'workflow_dispatch' } },
    @{ what = 'a push to another branch'; set = { param($r) $r.head_branch = 'alien-ways-e' } },
    @{ what = 'a pull request'; set = { param($r) $r.event = 'pull_request' } },
    @{ what = 'a pull request_target'; set = { param($r) $r.event = 'pull_request_target' } },
    @{ what = 'a run that failed'; set = { param($r) $r.conclusion = 'failure' } },
    @{ what = "another repository's head"; set = { param($r) $r.head_repository = [pscustomobject]@{ full_name = 'Someone/Fork' } } },
    @{ what = 'another workflow'; set = { param($r) $r.path = '.github/workflows/other.yml' } },
    @{ what = 'this very run'; set = { param($r) $r.id = 99 } },
    @{ what = 'a commit that is not a full hash'; set = { param($r) $r.head_sha = 'abc' } }))
{
    $run = $goodRun.PSObject.Copy()
    $run.head_repository = $goodRun.head_repository; $run.repository = $goodRun.repository
    & $bad.set $run
    $why = Get-SourceRunRefusal $run $repo '99'
    Check ($why -cne '') "a source run is refused for $($bad.what) ($why)"
}

# --- minutes read from a results file: only a plain number from 0 to 600 (Secondary's T4)
foreach ($m in @(
    @{ json = '{"m": 3}'; ok = $true }, @{ json = '{"m": 2.5}'; ok = $true }, @{ json = '{"m": 0}'; ok = $true }, @{ json = '{"m": 600}'; ok = $true },
    @{ json = '{"m": 601}'; ok = $false }, @{ json = '{"m": -1}'; ok = $false }, @{ json = '{"m": "3"}'; ok = $false }, @{ json = '{"m": true}'; ok = $false },
    @{ json = '{"m": null}'; ok = $false }, @{ json = '{"m": [3]}'; ok = $false }, @{ json = '{"m": "2026-11-11T05:41:00Z"}'; ok = $false }))
{
    $value = ($m.json | ConvertFrom-Json).m
    Check ((Test-IsMinutes $value) -eq $m.ok) "minutes read from $($m.json) are $(if ($m.ok) { 'believed' } else { 'not believed' })"
}

function Good-Records { [pscustomobject]@{ schema = 4; none = ''; run = [pscustomobject]@{ id = '11'; sha = ('a' * 40); event = 'push'; ref = 'refs/heads/avpx_main' }; records = @(
    [pscustomobject]@{ test = 'zz_key_test'; key = ('b' * 64); kind = 'unit'; status = 'ran'; class = 'A'; textClass = 'A'; importClass = 'A'; result = 'passed'; minutes = 0.5
        parts = @(, @('schema', ('c' * 64))); includes = @('3dc/a.h'); imports = @('KERNEL32.DLL!QueryPerformanceCounter'); ranWith = @('zz_key_test', 'zz_other_test') }) } }
Check ((Get-RecordsRefusal (Good-Records) '11' ('a' * 40)) -ceq '') 'a records file in the shape record.ps1 writes is believed'
foreach ($bad in @(
    @{ what = 'another schema'; set = { param($d) $d.schema = 1 } },
    @{ what = 'another run'; set = { param($d) $d.run.id = '12' } },
    @{ what = 'another commit'; set = { param($d) $d.run.sha = ('f' * 40) } },
    @{ what = 'a run started by hand'; set = { param($d) $d.run.event = 'workflow_dispatch' } },
    @{ what = 'a run of a pull request'; set = { param($d) $d.run.event = 'pull_request' } },
    @{ what = 'a run on another branch'; set = { param($d) $d.run.ref = 'refs/heads/alien-ways-e' } },
    @{ what = 'a test named as a path'; set = { param($d) $d.records[0].test = '../x' } },
    @{ what = "a key that isn't 64 hex digits"; set = { param($d) $d.records[0].key = 'xyz' } },
    @{ what = 'a reused result written as a source'; set = { param($d) $d.records[0].status = 'reused' } },
    @{ what = 'a class that is not one'; set = { param($d) $d.records[0].class = 'D' } },
    @{ what = 'an import class that is not one'; set = { param($d) $d.records[0].importClass = '' } },
    @{ what = 'a part that is not a name and a hash'; set = { param($d) $d.records[0].parts = @(, @('schema')) } },
    @{ what = 'an include that is not a file name'; set = { param($d) $d.records[0].includes = @('C:\x.h') } },
    @{ what = "an import that isn't a DLL's name and a function's"; set = { param($d) $d.records[0].imports = @('just words') } },
    @{ what = 'minutes written as a word'; set = { param($d) $d.records[0].minutes = '0.5' } },
    @{ what = 'minutes written as a true'; set = { param($d) $d.records[0].minutes = $true } },
    @{ what = 'more than 600 minutes'; set = { param($d) $d.records[0].minutes = 601 } },
    @{ what = 'an empty list of the tests it ran with'; set = { param($d) $d.records[0].ranWith = @() } },
    @{ what = "a list of the tests it ran with that hasn't got it"; set = { param($d) $d.records[0].ranWith = @('zz_other_test') } },
    @{ what = 'a path among the tests it ran with'; set = { param($d) $d.records[0].ranWith = @('zz_key_test', '../x') } }))
{
    $data = Good-Records
    & $bad.set $data
    $why = Get-RecordsRefusal $data '11' ('a' * 40)
    Check ($why -cne '') "a records file is refused for $($bad.what) ($why)"
}

# --- record.ps1 itself, on made-up lists and results kept beside the made-up repository: only a true or false that agrees with the exit
# code, with minutes, is recorded (T4); every record holds every test the commit has (T2); results that name what isn't one of the
# commit's tests, or a test that left something running, give no records
$side = Join-Path $base 'reuse-key-rows-side'
if (Test-Path -LiteralPath $side) { Remove-Item -Recurse -Force -LiteralPath $side }
New-Item -ItemType Directory -Force $side | Out-Null
[System.IO.File]::WriteAllText((Join-Path $side 'machine.txt'), 'image=given 1.0;vs=;tools=;sdks=;git=;autocrlf=')
foreach ($t in @('zz_key_test', 'zz_other_test'))
{
    [System.IO.File]::WriteAllLines((Join-Path $side "$t.includes.txt"), [string[]]@(if ($t -ceq 'zz_key_test') { $includes }))
    [System.IO.File]::WriteAllLines((Join-Path $side "$t.imports.txt"), [string[]]$floor)
}
$shaWas = $env:GITHUB_SHA
$env:GITHUB_SHA = "$(& git -C $work rev-parse HEAD)".Trim()
function Record-With([string]$json)
{
    [System.IO.File]::WriteAllText((Join-Path $side 'results.json'), $json)
    $out = Join-Path $side 'out'
    if (Test-Path -LiteralPath $out) { Remove-Item -Recurse -Force -LiteralPath $out }
    # record.ps1 imports the module again, which would leave this copy's git reader running (and the made-up repository in use), so it's
    # closed first; and record.ps1's own reader is closed after it
    Clear-HashCache
    $said = & (Join-Path $PSScriptRoot 'record.ps1') -Root $work -Lists $side -Results (Join-Path $side 'results.json') -Work $out | Out-String
    Clear-HashCache
    return @{ said = $said; data = (Get-Content -LiteralPath (Join-Path $out 'records.json') -Raw | ConvertFrom-Json) }
}
$got = Record-With '{"tests": {"zz_key_test": {"passed": "true", "exit": 0, "minutes": 0.1}, "zz_other_test": {"passed": true, "exit": 0, "minutes": 0.1}}, "leftBehind": []}'
$recorded = @($got.data.records | Where-Object { $_ })
Check ($recorded.Count -eq 1 -and $recorded[0].test -ceq 'zz_other_test' -and $got.said.Contains('Not recorded: zz_key_test')) "record.ps1 records no test whose ""passed"" is the word ""true"", and records the one that passed"
Check ($recorded.Count -eq 1 -and (@($recorded[0].ranWith) -join ' ') -ceq ((Get-AllTests $work) -join ' ')) "  and that record holds every test the commit has, not only the ones in the results"
$got = Record-With '{"tests": {"zz_key_test": {"passed": true, "exit": 1, "minutes": 0.1}, "zz_other_test": {"passed": true, "exit": 0, "minutes": "0.1"}}, "leftBehind": []}'
Check (@($got.data.records | Where-Object { $_ }).Count -eq 0) "record.ps1 records no test whose ""passed"" disagrees with its exit code, nor one whose minutes are a word"
$got = Record-With '{"tests": {"zz_key_test": {"passed": true, "exit": 0, "minutes": 0.1}, "zz_nobody_test": {"passed": true, "exit": 0, "minutes": 0.1}}, "leftBehind": []}'
Check (@($got.data.records | Where-Object { $_ }).Count -eq 0 -and "$($got.data.none)" -ne '') "record.ps1 writes no records when the results name what isn't one of the commit's tests ($($got.data.none))"
$got = Record-With '{"tests": {"zz_key_test": {"passed": true, "exit": 0, "minutes": 0.1}}, "leftBehind": ["123 C:\\x.exe"]}'
Check (@($got.data.records | Where-Object { $_ }).Count -eq 0 -and "$($got.data.none)" -ne '') "record.ps1 writes no records when a test left something running ($($got.data.none))"
$env:GITHUB_SHA = $shaWas
Remove-Item -Recurse -Force -LiteralPath $side

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

Clear-HashCache
Remove-Item -Recurse -Force -LiteralPath $work
"$script:passes passed, $script:failures failed"
if ($script:failures -gt 0) { exit 1 }
exit 0
