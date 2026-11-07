# Checks that a test really notices when the code it guards is broken. Written 2026-11-05 by Melanikus for the AvP port.
#
# README.md, beside this file, says what this is for. Each row of tests/takeouts/<test>.json is one small change to a copy of the code. For each, this builds the test against the
# changed copy and runs it. The test should fail. A row it doesn't fail on means that check isn't really being tested. The repository itself is never touched.
#
# Run it from anywhere, in PowerShell:
#   tests/takeouts/run.ps1 <test>                  for example: tests/takeouts/run.ps1 rim_test
#   tests/takeouts/run.ps1 <test> -From 3 -To 5    only those rows
#
# The copy and the programs land in a folder called AvP-test-out next to the repository, or under $env:RUNNER_TEMP in the automatic checks. Not the temp folder: antivirus programs distrust
# new programs that appear there. It gives back a non-zero result if any row wasn't noticed, or couldn't be run.

param(
    [Parameter(Mandatory = $true)][string]$Test,
    [int]$From = 0,
    [int]$To = -1,
    [int]$SecondsForEachRun = 120
)

$ErrorActionPreference = 'Stop'
trap { Write-Host "Could not run the take-outs: $_"; exit 2 }

$repository = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$rowsFile = Join-Path $PSScriptRoot "$Test.json"
if (-not (Test-Path $rowsFile)) { throw "There's no list of take-outs at $rowsFile" }

$base = if ($env:RUNNER_TEMP) { Join-Path $env:RUNNER_TEMP 'avp-takeouts' } else { Join-Path (Split-Path -Parent $repository) 'AvP-test-out\takeouts' }
$tree = Join-Path $base "$Test\tree"
$out = Join-Path $base "$Test\out"
if (Test-Path (Join-Path $base $Test)) { Remove-Item -Recurse -Force (Join-Path $base $Test) }
New-Item -ItemType Directory -Force $tree | Out-Null

# These bytes go in and out exactly as they are, whatever they are, and line endings are put back as they were found.
$bytes = [System.Text.Encoding]::GetEncoding(28591)
$list = [System.IO.File]::ReadAllText($rowsFile, [System.Text.Encoding]::UTF8) | ConvertFrom-Json

# Only the sources, not what earlier builds left behind
foreach ($folder in $list.copy) {
    & robocopy (Join-Path $repository $folder) (Join-Path $tree $folder) /E /NFL /NDL /NJH /NJS /NP /XF *.obj *.pdb *.exe *.ilk *.iobj *.ipdb *.dll *.lib *.exp *.idb *.tlog *.recipe *.log | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "Couldn't copy $folder" }
}
$global:LASTEXITCODE = 0

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio with the C++ tools was not found' }

# Visual Studio's own PowerShell module sets the compiler up in this window, the way its setup batch file does in a command window, so nothing has to be written to disk to run it.
# The antivirus distrusts a script that writes another to run, and took one of ours. The setup looks for vswhere on the path, so that goes on first. -arch=x86 builds 32-bit programs,
# the same as the game.
$env:PATH = "$(Split-Path -Parent $vswhere);$env:PATH"
Import-Module (Join-Path $visualStudio 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $visualStudio -SkipAutomaticLocation -DevCmdArguments '-arch=x86 -host_arch=amd64 -no_logo' | Out-Null

# Runs one of the compiler's programs in a folder and gives back its exit code. What it says goes in a file, and anything it says as an error in that file's .errors. It's started this way,
# and not by its name on its own, because Windows PowerShell takes anything a program writes as an error as the script failing.
function Invoke-Tool([string]$program, [string]$arguments, [string]$folder, [string]$saysTo)
{
    $found = (Get-Command $program -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
    $started = @{ FilePath = $found; WorkingDirectory = $folder; NoNewWindow = $true; PassThru = $true; RedirectStandardOutput = $saysTo; RedirectStandardError = "$saysTo.errors" }

    # Start-Process refuses an empty list of arguments, and a program run on its own has none
    if ($arguments) { $started.ArgumentList = $arguments }

    $process = Start-Process @started
    $handle = $process.Handle    # without asking for this, Windows PowerShell can't read the exit code afterwards
    $process.WaitForExit()
    return $process.ExitCode
}

function Build-And-Run {
    if (Test-Path $out) { Remove-Item -Recurse -Force $out }
    New-Item -ItemType Directory -Force $out | Out-Null
    Invoke-Tool 'cl.exe' "/nologo @tests\$Test.args `"/Fe:$out\t.exe`" `"/Fo:$out/`"" $tree (Join-Path $out 'build.txt') | Out-Null
    $built = Test-Path (Join-Path $out 't.exe')
    if (-not $built) { return @{ Built = $false; Exit = -1; Words = (Get-Content (Join-Path $out 'build.txt') -TotalCount 3) -join ' ' } }

    $said = Join-Path $out 'said.txt'
    $process = Start-Process -FilePath (Join-Path $out 't.exe') -WorkingDirectory $tree -NoNewWindow -PassThru -RedirectStandardOutput $said -RedirectStandardError (Join-Path $out 'errors.txt')
    $handle = $process.Handle    # without asking for this, Windows PowerShell can't read the exit code afterwards
    if (-not $process.WaitForExit($SecondsForEachRun * 1000)) {
        $process.Kill()
        $process.WaitForExit()
        return @{ Built = $true; Exit = 'ran on until it was stopped'; Words = '' }
    }

    $failed = @(Select-String -Path $said -Pattern 'FAILED' -SimpleMatch | Where-Object { $_.Line -notmatch 'checks FAILED$' })
    $first = if ($failed.Count) { $failed[0].Line.Trim() } else { '' }
    return @{ Built = $true; Exit = $process.ExitCode; Words = "$($failed.Count) failed. $first"; Ok = (@(Select-String -Path $said -Pattern '^\s*ok').Count) }
}

$first = Build-And-Run
if (-not $first.Built -or $first.Exit -ne 0) { throw "With nothing changed the test should pass, and it doesn't: $($first.Exit) $($first.Words)" }
Write-Host "With nothing changed, $Test passes $($first.Ok) checks"

$rows = @($list.rows)
$last = if ($To -ge 0) { [Math]::Min($To, $rows.Count - 1) } else { $rows.Count - 1 }
$noticed = 0; $missed = 0; $notRun = 0

for ($r = $From; $r -le $last; $r++) {
    $row = $rows[$r]
    $kept = @{}
    $matched = $true

    foreach ($change in $row.changes) {
        $path = Join-Path $tree $change.file
        if (-not $kept.ContainsKey($path)) { $kept[$path] = [System.IO.File]::ReadAllText($path, $bytes) }

        $text = [System.IO.File]::ReadAllText($path, $bytes)
        $windows = $text.Contains("`r`n")
        $plain = $text.Replace("`r`n", "`n")
        $at = $plain.IndexOf($change.from, [StringComparison]::Ordinal)

        if ($at -lt 0 -or $plain.IndexOf($change.from, $at + 1, [StringComparison]::Ordinal) -ge 0) {
            Write-Host "#$r $($row.name): it should match exactly once, and doesn't: $($change.from)"
            $matched = $false
            continue
        }

        $changed = $plain.Substring(0, $at) + $change.to + $plain.Substring($at + $change.from.Length)
        if ($windows) { $changed = $changed.Replace("`n", "`r`n") }
        [System.IO.File]::WriteAllText($path, $changed, $bytes)
    }

    if (-not $matched) {
        $notRun++
    }
    else {
        $result = Build-And-Run
        if (-not $result.Built) {
            Write-Host "#$r $($row.name): it didn't build: $($result.Words)"
            $notRun++
        }
        elseif ($result.Exit -eq 0) {
            Write-Host "#$r NOT NOTICED $($row.name)"
            $missed++
        }
        else {
            Write-Host "#$r noticed     $($row.name): exit $($result.Exit), $($result.Words)"
            $noticed++
        }
    }

    foreach ($path in $kept.Keys) { [System.IO.File]::WriteAllText($path, $kept[$path], $bytes) }
}

Write-Host "$noticed noticed, $missed not noticed, $notRun not run"
exit $(if ($missed -or $notRun) { 1 } else { 0 })
