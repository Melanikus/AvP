#Requires -Version 4.0
<#
#198: asks running Hudson games to end now, cleanly, as if each had reached its time: its result is written and its film finished, so
nothing is lost. Closing a game's window doesn't promise that. Written 2026-11-10 by Melanikus (Third) for the AvP port.

  powershell -ExecutionPolicy Bypass -File tools\hudson\stop-run.ps1 -GameFolder C:\Games\AvP-net-a, C:\Games\AvP-net-b

Give the folder of each game to stop: both of a pair, to stop the pair. It puts an empty hudson-stop file in each game's own run folder
(stop-file.ps1 says where, and what makes it refuse). Each game looks for it once a second and ends with exit code 38 and
"ended=it was stopped by hand" in its result. run-hudson.ps1 and run-net-pair.ps1 then keep the run's files as they always do.

Exit code: 0 if every folder got its file, 1 if any was refused (each says why), 100 for nothing given.
#>
param([string[]]$GameFolder = @())

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'stop-file.ps1')

if ($GameFolder.Count -eq 0)
{
    Write-Host 'stop-run: give -GameFolder, the folder of each game to stop'
    exit 100
}

$refused = 0

foreach ($folder in $GameFolder)
{
    $why = Add-HudsonStopFile $folder

    if ($why)
    {
        Write-Host "stop-run: nothing written for $folder`: $why"
        $refused++
    }
    else
    {
        Write-Host "stop-run: asked the game in $folder to stop. It ends within about a second, then finishes its film"
    }
}

exit $(if ($refused) { 1 } else { 0 })
