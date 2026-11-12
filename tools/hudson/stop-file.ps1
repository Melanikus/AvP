# #198, a clean early stop: puts a hudson-stop file in a running game's own run folder, which asks that run to end as if it had reached
# its time, so its result is written and its film finished (hudson.cpp, AStopFileAsksToEnd). Kept in a small file of its own so that
# tests/hudson_clear_folder_test.ps1 can try it on folders made for the purpose. Written 2026-11-10 by Melanikus (Third) for the AvP port.
#
# WHERE IT WRITES: only in the run folders a run itself has made, "HudsonRun\My Games\Aliens versus Predator" and the second copy's
# "...(second copy)", each that's there. Nowhere else. The file is empty: only that it's there counts, never what's in it.
#
# WHAT STOPS IT: the path is built from something typed on a command line, so it looks before it writes. A Steam folder, a link anywhere
# on the way (a link would write somewhere else), no run folder at all, or a hudson-stop that's a folder or a link: it writes nothing and
# says why. A plain hudson-stop already there is written again, so its date is new: the game counts only one newer than its run.
#
# The next run's clearing (clear-run-folder.ps1) deletes it, as it deletes every plain file in those folders.

function Test-HudsonStopIsLink($item)
{
    return ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0
}

# Gives back why it wrote nothing, or nothing once it has written the file in every run folder that's there
function Add-HudsonStopFile([string]$GameFolder)
{
    if (-not (Test-Path -LiteralPath $GameFolder -PathType Container))
    {
        return "$GameFolder isn't a folder"
    }

    $game = Get-Item -LiteralPath $GameFolder -Force

    if ($game.FullName -match 'steamapps')
    {
        return "$($game.FullName) looks like a Steam folder"
    }

    for ($place = $game; $place; $place = $place.Parent)
    {
        if (Test-HudsonStopIsLink $place)
        {
            return "$($place.FullName) is a link to somewhere else"
        }
    }

    # Every folder on the way down, each looked at before anything is written
    $targets = @()

    foreach ($name in @('Aliens versus Predator', 'Aliens versus Predator (second copy)'))
    {
        $way = @('HudsonRun', 'HudsonRun\My Games', "HudsonRun\My Games\$name")
        $there = $true

        foreach ($step in $way)
        {
            $path = Join-Path $game.FullName $step

            if (-not (Test-Path -LiteralPath $path))
            {
                $there = $false
                break
            }

            $item = Get-Item -LiteralPath $path -Force

            if (Test-HudsonStopIsLink $item)
            {
                return "$path is a link to somewhere else"
            }

            if (-not $item.PSIsContainer)
            {
                return "$path isn't a folder"
            }
        }

        if (-not $there)
        {
            continue
        }

        $stop = Join-Path (Join-Path $game.FullName "HudsonRun\My Games\$name") 'hudson-stop'

        if (Test-Path -LiteralPath $stop)
        {
            $old = Get-Item -LiteralPath $stop -Force

            if ($old.PSIsContainer -or (Test-HudsonStopIsLink $old))
            {
                return "$stop is there already, and it's a folder or a link, not the plain file a stop is"
            }
        }

        $targets += $stop
    }

    if ($targets.Count -eq 0)
    {
        return "$($game.FullName) has no run folder: has a run started there?"
    }

    foreach ($stop in $targets)
    {
        # Written new each time, empty, so its date is now
        [IO.File]::WriteAllText($stop, '')
    }

    return $null
}
