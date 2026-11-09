# Tries tools/hudson/compare-pictures.ps1 on pictures made for the purpose: a run where everything is as it should be, then each way it can be wrong. tests/hudson_compare_test.cpp runs this, so that it counts as a test.
#
# Each wrong run has to be noticed on the right line, not only by the exit code, or a mistake on another line could hide that one isn't being checked at all. Two rows change known pixels and check the exact count, because
# a mistake in where a row starts moves both pictures alike, and every other row here would still pass.
#
# -ScriptFile is there so that a broken copy of the script can be tried instead, to prove this test notices.

param([string]$ScriptFile = (Join-Path (Split-Path -Parent $PSScriptRoot) 'tools\hudson\compare-pictures.ps1'))

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

$top = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'AvP-test-out' }
$root = Join-Path $top "hudson-compare-test-$PID"
$powershell = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
$script:failed = 0

# A plain grey picture with a coloured patch. $marks are single pixels set on top of it, each as x, y.
function Make-Picture([string]$path, [int]$tint, [int]$patchSize, $format, [int]$width, [int]$height, $marks)
{
    $bitmap = New-Object System.Drawing.Bitmap $width, $height, $format
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.Clear([System.Drawing.Color]::FromArgb(255, 100, 100, 100))

    if ($patchSize -gt 0)
    {
        $brush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, $tint, 100, 100))
        $graphics.FillRectangle($brush, 10, 10, $patchSize, $patchSize)
        $brush.Dispose()
    }

    $graphics.Dispose()

    foreach ($mark in $marks)
    {
        $bitmap.SetPixel($mark[0], $mark[1], [System.Drawing.Color]::FromArgb(255, 255, 0, 255))
    }

    $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
}

# Each way of drawing is the plain picture with a patch of its own, so that each pair that should differ does. Anything in $changes replaces one: its tint and patch size, or nothing to leave the picture out.
function Make-Run([string]$name, [hashtable]$changes, $format = [System.Drawing.Imaging.PixelFormat]::Format32bppRgb, [int]$width = 200, [int]$height = 100, [hashtable]$marks = @{})
{
    $run = Join-Path $root $name
    $pictures = Join-Path $run 'pictures'
    New-Item -ItemType Directory -Force $pictures | Out-Null

    $usual = @{
        'gouraud' = @(10, 0); 'perpixel' = @(130, 30); 'normals' = @(160, 40); 'relief' = @(190, 50); 'relief-flip' = @(220, 60);
        'relief-tilt' = @(250, 70); 'relief-depth0' = @(160, 40); 'relief-again' = @(190, 50); 'normals-again' = @(160, 40)
    }

    foreach ($case in $usual.Keys)
    {
        $values = if ($changes.ContainsKey($case)) { $changes[$case] } else { $usual[$case] }

        if ($null -ne $values)
        {
            $picture = Join-Path $pictures "room1-view1-$case.png"
            $theseMarks = if ($marks.ContainsKey($case)) { $marks[$case] } else { @() }

            Make-Picture $picture $values[0] $values[1] $format $width $height $theseMarks
        }
    }

    return $run
}

function Expect([string]$what, [int]$wantedExit, [string]$run, [string]$lineMustSay)
{
    $words = & $powershell -NoProfile -ExecutionPolicy Bypass -File $ScriptFile -RunFolder $run | Out-String
    $gotExit = $LASTEXITCODE
    $good = ($gotExit -eq $wantedExit) -and (-not $lineMustSay -or $words -match $lineMustSay)

    Write-Host ("  {0}  {1}" -f $(if ($good) { 'ok' } else { 'FAILED' }), $what)

    if (-not $good)
    {
        Write-Host "    exit $gotExit, wanted $wantedExit, and the words should have matched: $lineMustSay"
        Write-Host $words
        $script:failed++
    }
}

function ExpectTwoRuns([string]$what, [int]$wantedExit, [string]$first, [string]$second, [int]$limit, [string]$lineMustSay)
{
    $words = & $powershell -NoProfile -ExecutionPolicy Bypass -File $ScriptFile -FirstRun $first -SecondRun $second -Limit $limit | Out-String
    $gotExit = $LASTEXITCODE
    $good = ($gotExit -eq $wantedExit) -and (-not $lineMustSay -or $words -match $lineMustSay)

    Write-Host ("  {0}  {1}" -f $(if ($good) { 'ok' } else { 'FAILED' }), $what)

    if (-not $good)
    {
        Write-Host "    exit $gotExit, wanted $wantedExit, and the words should have matched: $lineMustSay"
        Write-Host $words
        $script:failed++
    }
}

try
{
    Write-Host 'Reading the pictures from a run:'

    Expect 'everything as it should be' 0 (Make-Run 'good' @{}) 'every line is as it should be'

    Expect 'relief no different from normals' 1 (Make-Run 'no-relief' @{ 'relief' = @(160, 40); 'relief-again' = @(160, 40) }) 'normals\s+relief\s+should be different.*NOT AS IT SHOULD BE'
    Expect 'the flip changes nothing' 1 (Make-Run 'no-flip' @{ 'relief-flip' = @(190, 50) }) 'relief\s+relief-flip\s+should be different.*NOT AS IT SHOULD BE'
    Expect 'depth 0 far from normals' 1 (Make-Run 'depth' @{ 'relief-depth0' = @(255, 70) }) 'normals\s+relief-depth0\s+should be close.*NOT AS IT SHOULD BE'
    Expect 'relief twice differs' 1 (Make-Run 'noise-relief' @{ 'relief-again' = @(250, 50) }) 'relief\s+relief-again\s+should be same.*NOT AS IT SHOULD BE'
    Expect 'normals twice differs' 1 (Make-Run 'noise-normals' @{ 'normals-again' = @(250, 40) }) 'normals\s+normals-again\s+should be same.*NOT AS IT SHOULD BE'
    Expect 'a difference of a level or two is no difference' 0 (Make-Run 'small' @{ 'relief-again' = @(191, 50) }) ''

    # These depend on a live light reaching the wall. A wall lit only by the level's baked light looks the same in each, and that's no fault, so each is the same here and the run still passes.
    Expect 'the tilt changing nothing has no verdict' 0 (Make-Run 'no-tilt' @{ 'relief-tilt' = @(190, 50) }) 'relief\s+relief-tilt\s+should be needs-light.*no verdict'
    Expect 'per pixel and normals the same has no verdict' 0 (Make-Run 'no-normals' @{ 'perpixel' = @(160, 40) }) 'perpixel\s+normals\s+should be needs-light.*no verdict'
    Expect 'Gouraud and per pixel the same has no verdict' 0 (Make-Run 'no-perpixel' @{ 'gouraud' = @(130, 30) }) 'gouraud\s+perpixel\s+should be needs-light.*no verdict'

    # One pixel in each corner, in a picture whose rows take up exactly a multiple of four bytes, and in one whose rows need filling out to get there. The count has to be exact. All four corners, because a row of the wrong
    # width drifts further each row, so it can land on some of them by luck, and it runs out before it reaches the end of the last row.
    $format24 = [System.Drawing.Imaging.PixelFormat]::Format24bppRgb
    $format32 = [System.Drawing.Imaging.PixelFormat]::Format32bppRgb
    $corners32 = @{ 'relief-again' = @(@(0, 0), @(199, 0), @(0, 99), @(199, 99)) }
    $corners24 = @{ 'relief-again' = @(@(0, 0), @(200, 0), @(0, 99), @(200, 99)) }

    Expect 'exactly the four pixels changed, one in each corner' 0 (Make-Run 'corners-32' @{} $format32 200 100 $corners32) 'relief\s+relief-again\s+should be same\s+4 pixels differ'
    Expect 'and the same with rows that need filling out' 0 (Make-Run 'corners-24' @{} $format24 201 100 $corners24) 'relief\s+relief-again\s+should be same\s+4 pixels differ'

    Expect 'a picture missing' 2 (Make-Run 'missing' @{ 'relief-flip' = $null }) 'relief-flip\.png is missing'

    $damaged = Make-Run 'damaged' @{}
    Set-Content -LiteralPath (Join-Path $damaged 'pictures\room1-view1-normals.png') -Value 'this is not a picture' -Encoding ASCII
    Expect "a picture that can't be read" 2 $damaged "normals\.png can't be read"

    $smaller = Make-Run 'smaller' @{}
    Make-Picture (Join-Path $smaller 'pictures\room1-view1-relief-again.png') 190 50 $format32 150 100 @()
    Expect 'the second picture smaller' 2 $smaller "can't be compared"

    # Bigger is the harder one: nothing runs off the end, so only the check on the sizes can notice.
    $wider = Make-Run 'wider' @{}
    Make-Picture (Join-Path $wider 'pictures\room1-view1-relief-again.png') 190 50 $format32 250 100 @()
    Expect 'the second picture wider' 2 $wider "can't be compared"

    $taller = Make-Run 'taller' @{}
    Make-Picture (Join-Path $taller 'pictures\room1-view1-relief-again.png') 190 50 $format32 200 150 @()
    Expect 'the second picture taller' 2 $taller "can't be compared"

    $deeper = Make-Run 'deeper' @{}
    Make-Picture (Join-Path $deeper 'pictures\room1-view1-relief-again.png') 190 50 $format24 200 100 @()
    Expect 'the second picture with more bytes for each pixel' 2 $deeper "can't be compared"

    # The same the other way round. Here nothing runs off the end either, so only the check on the depth can notice.
    $shallower = Make-Run 'shallower' @{}
    Make-Picture (Join-Path $shallower 'pictures\room1-view1-relief-again.png') 190 50 $format32 200 100 @()
    Make-Picture (Join-Path $shallower 'pictures\room1-view1-relief.png') 190 50 $format24 200 100 @()
    Expect 'the first picture with fewer bytes for each pixel' 2 $shallower "can't be compared"

    Expect 'no pictures folder' 2 (Join-Path $root 'nothing-here') 'no pictures folder'

    $empty = Join-Path $root 'empty'
    New-Item -ItemType Directory -Force (Join-Path $empty 'pictures') | Out-Null
    Expect 'a folder with no pictures from a comparing run' 2 $empty 'no pictures from a run with compare=relief'

    Write-Host 'Comparing the pictures of two runs:'

    $runOne = Make-Run 'two-runs-one' @{}
    $runTwo = Make-Run 'two-runs-two' @{}

    ExpectTwoRuns 'two runs with the same pictures differ nowhere' 0 $runOne $runTwo 2 'room1-view1-relief\.png\s+0 pixels differ by more than 2 \(0\.000%\), most\s+0, average 0\.000'

    $cornersRun = Make-Run 'two-runs-corners' @{} $format32 200 100 $corners32
    ExpectTwoRuns 'it counts exactly the pixels that changed, one in each corner' 0 $runOne $cornersRun 2 'room1-view1-relief-again\.png\s+4 pixels differ by more than 2 \(0\.020%\), most 155'
    ExpectTwoRuns 'and only in that picture' 0 $runOne $cornersRun 2 'room1-view1-relief\.png\s+0 pixels differ'

    # The patch in relief is 50 pixels square, and this one is 5 levels from it
    $tintRun = Make-Run 'two-runs-tint' @{ 'relief' = @(195, 50) }
    ExpectTwoRuns 'a difference of 5 is past a limit of 4' 0 $runOne $tintRun 4 'room1-view1-relief\.png\s+2500 pixels differ by more than 4 \(12\.500%\), most\s+5'
    ExpectTwoRuns 'and not past a limit of 5' 0 $runOne $tintRun 5 'room1-view1-relief\.png\s+0 pixels differ by more than 5 \(0\.000%\), most\s+5'

    ExpectTwoRuns 'a picture only in the second run' 2 (Make-Run 'two-runs-short' @{ 'relief-flip' = $null }) $runOne 2 'room1-view1-relief-flip\.png is only in one'
    ExpectTwoRuns 'a picture only in the first run' 2 $runOne (Make-Run 'two-runs-short-too' @{ 'relief-flip' = $null }) 2 'room1-view1-relief-flip\.png is only in one'
    ExpectTwoRuns 'no pictures folder in the first' 2 (Join-Path $root 'nothing-here') $runOne 2 'no pictures folder'
    ExpectTwoRuns 'no pictures folder in the second' 2 $runOne (Join-Path $root 'nothing-here') 2 'no pictures folder'
    ExpectTwoRuns 'no pictures at all' 2 $empty $empty 2 'no pictures in'

    $damagedTwo = Make-Run 'two-runs-damaged' @{}
    Set-Content -LiteralPath (Join-Path $damagedTwo 'pictures\room1-view1-normals.png') -Value 'this is not a picture' -Encoding ASCII
    ExpectTwoRuns "a picture that can't be read" 2 $runOne $damagedTwo 2 "normals\.png can't be read"

    $sizedTwo = Make-Run 'two-runs-sized' @{}
    Make-Picture (Join-Path $sizedTwo 'pictures\room1-view1-relief.png') 190 50 $format32 250 100 @()
    ExpectTwoRuns 'two pictures of different sizes' 2 $runOne $sizedTwo 2 "called room1-view1-relief\.png can't be compared"
}
finally
{
    # Only ever the folder made above, which has this program's number in its name
    if (Test-Path -LiteralPath $root)
    {
        Remove-Item -LiteralPath $root -Recurse -Force
    }
}

if ($script:failed -gt 0)
{
    Write-Host "SOME CHECKS FAILED: $($script:failed)"
    exit 1
}

Write-Host 'All checks passed'
exit 0
