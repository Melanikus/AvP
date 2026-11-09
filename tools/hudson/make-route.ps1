#Requires -Version 4.0
<#
Makes the route file a Hudson run follows, from one level's route list. 3dc/avp/hudson_nav_plan.h says what the file looks like. The game checks every line again when it reads it, so a file this script got wrong
is refused there, and not believed.

  powershell -ExecutionPolicy Bypass -File tools\hudson\make-route.ps1 -From routes\derelict.json -To C:\Games\AvP-test\hudson-routes\derelict.txt

A route list says, for each thing that has to be done in a level, which rooms to walk through and what to do when there. It names things by the names in the level file, and has their places in the world's numbers.
It's worked out from the level's own logic, so a route is what should work, and a run is what shows whether it does.

A step this can't turn into something he can do, like killing something, goes in as "other", and the route stops at that step when he gets there.
#>
param(
    [Parameter(Mandatory = $true)][string]$From,
    [Parameter(Mandatory = $true)][string]$To
)

$ErrorActionPreference = 'Stop'
trap { Write-Host "make-route: $_"; exit 1 }

# Keeps a name to the characters the game takes, because a place's name isn't worth losing a whole route over.
function Name-Of([string]$name) { return ($name -replace '[^a-zA-Z0-9 _.\-]', '_') }

$list = [System.IO.File]::ReadAllText((Resolve-Path -LiteralPath $From).Path) | ConvertFrom-Json
$lines = New-Object System.Collections.ArrayList
[void]$lines.Add("# The way through $($list.level), made from $(Split-Path -Leaf $From). Did the route list reach the end: $($list.finished)")

$lastRoom = ''

foreach ($step in $list.steps) {
    foreach ($walk in $step.walk) {
        # A step's walk starts in the room where the last one finished, and that has nowhere to go through
        if ($walk.through -and $walk.room -ne $lastRoom) {
            [void]$lines.Add("walk $([int]$walk.through.x) $([int]$walk.through.y) $([int]$walk.through.z) $(Name-Of $walk.room)")
        }

        $lastRoom = $walk.room
    }

    switch ($step.do) {
        'press' { [void]$lines.Add("press $([int]$step.place.x) $([int]$step.place.y) $([int]$step.place.z) $(Name-Of $step.name)") }
        'walk into' {
            # What sets it off is a box of air. Its place is the middle of that, and he can only walk on the floor, so the height is the bottom of the box: y goes down.
            $height = if ($step.volume) { [int]$step.volume.max.y } else { [int]$step.place.y }
            [void]$lines.Add("walkinto $([int]$step.place.x) $height $([int]$step.place.z) $(Name-Of $step.name)")
        }
        default { [void]$lines.Add("other $(Name-Of $step.do) $(Name-Of $step.name)") }
    }
}

$folder = Split-Path -Parent $To

if ($folder -and -not (Test-Path -LiteralPath $folder)) {
    New-Item -ItemType Directory -Force $folder | Out-Null
}

[System.IO.File]::WriteAllLines($To, [string[]]$lines, (New-Object System.Text.ASCIIEncoding))
Write-Host "make-route: $To has $($lines.Count - 1) steps"
