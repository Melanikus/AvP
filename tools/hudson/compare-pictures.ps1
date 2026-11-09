#Requires -Version 4.0
<#
Reads the pictures from a Hudson run with compare=relief, and says whether they show what each way of drawing should. Written 2026-11-05 by Melanikus for the AvP port.

  powershell -ExecutionPolicy Bypass -File tools\hudson\compare-pictures.ps1 -RunFolder C:\Games\AvP-test\runs\2026-11-05-140000

THE IDEA: a person looking at two pictures can talk themselves into seeing a difference. This counts. The run draws one view several ways, with nothing moving, so whatever differs between two of its pictures comes from
the way they were drawn. Two of the ways are drawn twice, and those pairs show how much pictures differ with nothing changed, because a level's lights can flicker. That's what the others are measured against.

Each line says how many pixels differ, how far the most different one is, and the average difference over the whole picture, all out of 255. Then it says whether that's what should happen:

  same        the two should match, because nothing changed between them
  close       the two should match apart from rounding, because they're the same picture drawn by different shaders
  different   the two should clearly differ
  needs-light the two differ only if a live light reaches the wall, so this line has no verdict, only numbers. A wall lit only by the light baked into the level looks the same in these, and that isn't a fault

Reading the numbers needs care, because they say only that the pictures differ, not that they differ correctly. Look at the pictures as well. A map that's the wrong way round still makes a difference.

Gouraud against per pixel is a needs-light line for a further reason: with shadows off, Gouraud is the view at full brightness and per pixel has its dimming, so they'd differ whatever the lighting did.

The exit code is 0 if every line is as it should be, 1 if any isn't, and 2 if a picture is missing or can't be read.

It can also compare two runs, picture by picture. Each picture has to be in both, with the same name, as the same view has in any two runs:

  powershell -ExecutionPolicy Bypass -File tools\hudson\compare-pictures.ps1 -FirstRun C:\Games\AvP-test\runs\one -SecondRun C:\Games\AvP-test\runs\two -Limit 2

Each line says how many pixels differ by more than -Limit, out of 255, how far the most different one is, and the average difference. This way gives no verdicts, because what's right depends on what's being compared: most
often, whether two runs that should differ, differ by no more than two runs that shouldn't. The exit code is 0 if every picture could be compared, and 2 if not.
#>
[CmdletBinding(DefaultParameterSetName = 'OneRun')]
param(
    [Parameter(Mandatory = $true, ParameterSetName = 'OneRun')][string]$RunFolder,
    [Parameter(Mandatory = $true, ParameterSetName = 'TwoRuns')][string]$FirstRun,
    [Parameter(Mandatory = $true, ParameterSetName = 'TwoRuns')][string]$SecondRun,
    [Parameter(ParameterSetName = 'TwoRuns')][ValidateRange(0, 255)][int]$Limit = 2
)
$ErrorActionPreference = 'Stop'

# Without this, reading past the end of a list gives nothing, which would count here as no difference at all. With it, that's an error, and the script stops there with a non-zero exit code. Nothing should reach it,
# because a picture read here is always whole and the sizes are checked first, but a tool that says "no difference" must not be able to say so by accident.
Set-StrictMode -Version 3.0

Add-Type -AssemblyName System.Drawing

# All the limits below are guesses until a real run has shown how much pictures differ by themselves. The numbers on each line are what to trust until then.

# A pixel counts as different only past this, out of 255. A level's own flicker and a shader's rounding are usually within it, and it's what stops either being counted.
$pixelCountsAsDifferentPast = 2

# "different" needs at least this many pixels differing: one in two hundred, a piece of the picture anyone could point to, not a few stray ones.
$differentNeedsThisShare = 0.005

# "close" allows this much on average over the whole picture, and this many pixels that are further off than rounding could make them.
$closeAllowsAverage = 2.0
$closeAllowsThisShareFarOff = 0.001
$farOffIsPast = 8

# "same" allows what's left over from the level's own flicker: this share of the picture.
$sameAllowsThisShare = 0.002

# The pairs: the first, the second, and what should be true of them.
$pairs = @(
    @('relief', 'relief-again', 'same'),
    @('normals', 'normals-again', 'same'),
    @('normals', 'relief', 'different'),
    @('relief', 'relief-flip', 'different'),
    @('normals', 'relief-depth0', 'close'),
    @('gouraud', 'perpixel', 'needs-light'),
    @('perpixel', 'normals', 'needs-light'),
    @('relief', 'relief-tilt', 'needs-light')
)

# Every pixel of a picture, as the plain bytes of a BMP: the picture's own format goes in, and this comes out the same for any two pictures of the same size and depth.
function Read-Picture([string]$path)
{
    $bitmap = New-Object System.Drawing.Bitmap $path

    try
    {
        $memory = New-Object System.IO.MemoryStream

        try
        {
            $bitmap.Save($memory, [System.Drawing.Imaging.ImageFormat]::Bmp)
            $bytes = $memory.ToArray()
        }
        finally
        {
            $memory.Dispose()
        }
    }
    finally
    {
        $bitmap.Dispose()
    }

    # In a BMP's header: where the pixels start, its width, its height, and how many bits each pixel takes.
    $start = [BitConverter]::ToInt32($bytes, 10)
    $width = [BitConverter]::ToInt32($bytes, 18)
    $height = [Math]::Abs([BitConverter]::ToInt32($bytes, 22))
    $bitsPerPixel = [BitConverter]::ToInt16($bytes, 28)

    return @{ Bytes = $bytes; Start = $start; Width = $width; Height = $height; BytesPerPixel = [int]($bitsPerPixel / 8) }
}

function Compare-Two($first, $second)
{
    if ($first.Width -ne $second.Width -or $first.Height -ne $second.Height -or $first.BytesPerPixel -ne $second.BytesPerPixel)
    {
        return $null
    }

    [int]$colours = [Math]::Min($first.BytesPerPixel, 3)   # a fourth byte is alpha, which means nothing on a picture of the screen
    [int]$step = $first.BytesPerPixel

    # A BMP's rows are each rounded up to a whole number of four bytes. Floor matters: PowerShell's [int] rounds to the nearest, which would make most rows four bytes too long.
    [int]$rowBytes = [int][Math]::Floor(([int64]$first.Width * $step + 3) / 4) * 4

    [int]$threshold = $pixelCountsAsDifferentPast
    [int]$farOff = $farOffIsPast
    [byte[]]$a = $first.Bytes
    [byte[]]$b = $second.Bytes
    [int64]$total = 0
    [int]$different = 0
    [int]$veryDifferent = 0
    [int]$largest = 0

    for ([int]$row = 0; $row -lt $first.Height; $row++)
    {
        [int]$place = $first.Start + $row * $rowBytes

        for ([int]$column = 0; $column -lt $first.Width; $column++)
        {
            [int]$mostInThisPixel = 0

            for ([int]$colour = 0; $colour -lt $colours; $colour++)
            {
                [int]$difference = [int]$a[$place + $colour] - [int]$b[$place + $colour]

                if ($difference -lt 0) { $difference = -$difference }

                $total += $difference

                if ($difference -gt $mostInThisPixel) { $mostInThisPixel = $difference }
            }

            if ($mostInThisPixel -gt $threshold) { $different++ }
            if ($mostInThisPixel -gt $farOff) { $veryDifferent++ }
            if ($mostInThisPixel -gt $largest) { $largest = $mostInThisPixel }

            $place += $step
        }
    }

    [int64]$pixels = [int64]$first.Width * $first.Height

    return @{ Different = $different; VeryDifferent = $veryDifferent; Largest = $largest; Average = ([double]$total / ($pixels * $colours)); Pixels = $pixels }
}

function Is-AsItShouldBe([string]$expected, $result)
{
    switch ($expected)
    {
        'same' { return ($result.Different / $result.Pixels) -le $sameAllowsThisShare }
        'close' { return $result.Average -le $closeAllowsAverage -and ($result.VeryDifferent / $result.Pixels) -le $closeAllowsThisShareFarOff }
        'different' { return ($result.Different / $result.Pixels) -ge $differentNeedsThisShare }
        'needs-light' { return $true }
    }

    return $false
}

function Show-Number([double]$number)
{
    # The invariant culture, so that a decimal point is the same on every machine: this machine's own is a comma.
    return $number.ToString('0.000', [Globalization.CultureInfo]::InvariantCulture)
}

if ($PSCmdlet.ParameterSetName -eq 'TwoRuns')
{
    $pixelCountsAsDifferentPast = $Limit
    $names = @{}

    foreach ($runFolderHere in @($FirstRun, $SecondRun))
    {
        # The marine's shoulder lamp flickers, so two pictures with it on can't match, and the difference would mean nothing
        $result = Join-Path $runFolderHere 'hudson-result.txt'

        if ((Test-Path -LiteralPath $result) -and (Select-String -LiteralPath $result -Pattern '(^| )lamp=(on|blink)( |$)' -Quiet))
        {
            Write-Host "compare-pictures: $runFolderHere had the shoulder lamp on, so its pictures can't be compared. Run it with -Lamp off"
            exit 2
        }

        $folder = Join-Path $runFolderHere 'pictures'

        if (-not (Test-Path -LiteralPath $folder -PathType Container))
        {
            Write-Host "compare-pictures: there's no pictures folder in $runFolderHere"
            exit 2
        }

        $names[$runFolderHere] = @(Get-ChildItem -LiteralPath $folder -Filter '*.png' | ForEach-Object { $_.Name } | Sort-Object)
    }

    $firstNames = $names[$FirstRun]
    $secondNames = if ($FirstRun -eq $SecondRun) { $firstNames } else { $names[$SecondRun] }

    if ($firstNames.Count -eq 0)
    {
        Write-Host "compare-pictures: there are no pictures in $FirstRun"
        exit 2
    }

    $onlyInOne = @($firstNames | Where-Object { $secondNames -notcontains $_ }) + @($secondNames | Where-Object { $firstNames -notcontains $_ })

    if ($onlyInOne.Count -gt 0)
    {
        Write-Host "compare-pictures: $($onlyInOne[0]) is only in one of the two runs"
        exit 2
    }

    foreach ($name in $firstNames)
    {
        try
        {
            $first = Read-Picture (Join-Path $FirstRun "pictures\$name")
            $second = Read-Picture (Join-Path $SecondRun "pictures\$name")
        }
        catch
        {
            Write-Host "compare-pictures: $name can't be read"
            exit 2
        }

        $result = Compare-Two $first $second

        if ($null -eq $result)
        {
            Write-Host "compare-pictures: the two pictures called $name can't be compared: they aren't the same size or depth"
            exit 2
        }

        Write-Host ("  {0,-34} {1,8} pixels differ by more than {2} ({3}%), most {4,3}, average {5}" -f $name, $result.Different, $Limit, (Show-Number (100.0 * $result.Different / $result.Pixels)), $result.Largest, (Show-Number $result.Average))
    }

    exit 0
}

$pictures = Join-Path $RunFolder 'pictures'

if (-not (Test-Path -LiteralPath $pictures -PathType Container))
{
    Write-Host "compare-pictures: there's no pictures folder in $RunFolder"
    exit 2
}

$views = @(Get-ChildItem -LiteralPath $pictures -Filter 'room1-view*-relief.png' | ForEach-Object { $_.Name -replace '-relief\.png$', '' } | Sort-Object)

if ($views.Count -eq 0)
{
    Write-Host "compare-pictures: there are no pictures from a run with compare=relief in $pictures"
    exit 2
}

$anyWrong = $false

foreach ($view in $views)
{
    Write-Host $view
    $held = @{}

    foreach ($pair in $pairs)
    {
        foreach ($name in $pair[0..1])
        {
            if (-not $held.ContainsKey($name))
            {
                $path = Join-Path $pictures "$view-$name.png"

                if (-not (Test-Path -LiteralPath $path -PathType Leaf))
                {
                    Write-Host "compare-pictures: $view-$name.png is missing"
                    exit 2
                }

                try
                {
                    $held[$name] = Read-Picture $path
                }
                catch
                {
                    Write-Host "compare-pictures: $view-$name.png can't be read"
                    exit 2
                }
            }
        }

        $result = Compare-Two $held[$pair[0]] $held[$pair[1]]

        if ($null -eq $result)
        {
            Write-Host "compare-pictures: $($pair[0]) and $($pair[1]) can't be compared: they aren't the same size or depth"
            exit 2
        }

        $good = Is-AsItShouldBe $pair[2] $result

        if (-not $good) { $anyWrong = $true }

        $share = Show-Number (100.0 * $result.Different / $result.Pixels)
        $average = Show-Number $result.Average
        $verdict = if ($pair[2] -eq 'needs-light') { 'no verdict' } elseif ($good) { 'as it should be' } else { 'NOT AS IT SHOULD BE' }

        Write-Host ("  {0,-14} {1,-14} should be {2,-11} {3,8} pixels differ ({4}%), most {5,3}, average {6}: {7}" -f $pair[0], $pair[1], $pair[2], $result.Different, $share, $result.Largest, $average, $verdict)
    }
}

if ($anyWrong)
{
    Write-Host 'compare-pictures: something is not as it should be'
    exit 1
}

Write-Host 'compare-pictures: every line is as it should be'
exit 0
