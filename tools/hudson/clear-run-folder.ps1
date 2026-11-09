# The one thing a Hudson run deletes, kept in a small file of its own so that tests/hudson_clear_folder_test.ps1 can try it on folders made for the purpose. Written 2026-11-05 by Melanikus for the AvP port.
#
# WHY IT DELETES ANYTHING: two runs have to start the same, so nothing may be left from the run before. The game keeps a run's files in a folder beside itself, <game folder>\HudsonRun.
#
# WHAT GOES: plain files, in exactly these four folders. The folders themselves stay.
#
#   HudsonRun\My Games\Aliens versus Predator
#   HudsonRun\My Games\Aliens versus Predator\User_Profiles
#   HudsonRun\My Games\Aliens versus Predator\MPConfig
#   HudsonRun\My Games\Aliens versus Predator\pictures
#
# WHAT CAN'T GO: anything else. Nothing is deleted by going down through folders, so there's no way to wander out of these four. The copies a run keeps, in <game folder>\runs, are outside them.
#
# A FILM NEVER GOES, because the user wants all of them kept. run-hudson.ps1 moves a film out after every run, so one is only still here if a run was cut short before it could. It's moved to <game folder>\runs\films-left-behind instead of being deleted, and
# it can't stay where it is, or the next run would take it for its own. If it can't be moved, nothing is deleted at all and it says why.
#
# WHAT STOPS IT: the path is built from something typed on a command line, so it looks before it deletes anything. If the game folder is a Steam folder, or if the game folder or anything on the way to those files is
# a link to somewhere else, or if something is there that a run wouldn't have left, it deletes nothing at all and says why. A link is the real danger: going through one would delete files that aren't in this folder.

function Test-HudsonIsLink($item)
{
    return ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0
}

# Gives back why it won't delete anything, or nothing if it deleted what it should. Finding no run folder at all is fine: there's nothing to clear.
function Clear-HudsonRunFolder([string]$GameFolder)
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

    # the game folder itself, and every folder on the way to it
    for ($place = $game; $place; $place = $place.Parent)
    {
        if (Test-HudsonIsLink $place)
        {
            return "$($place.FullName) is a link to somewhere else"
        }
    }

    $runFolder = Join-Path $game.FullName 'HudsonRun'

    if (-not (Test-Path -LiteralPath $runFolder))
    {
        return $null
    }

    # What each folder may hold. Only the last four may hold plain files, and those are what goes.
    $saves = 'HudsonRun\My Games\Aliens versus Predator'
    $rules = @(
        @{ Folder = 'HudsonRun';                  Folders = @('My Games');                  Files = $false },
        @{ Folder = 'HudsonRun\My Games';         Folders = @('Aliens versus Predator');    Files = $false },
        @{ Folder = $saves;                       Folders = @('User_Profiles', 'MPConfig', 'pictures'); Files = $true },
        @{ Folder = "$saves\User_Profiles";       Folders = @();                            Files = $true },
        @{ Folder = "$saves\MPConfig";            Folders = @();                            Files = $true },
        @{ Folder = "$saves\pictures";            Folders = @();                            Files = $true }
    )

    if (Test-HudsonIsLink (Get-Item -LiteralPath $runFolder -Force))
    {
        return "$runFolder is a link to somewhere else"
    }

    $toDelete = @()
    $filmsToKeep = @()

    foreach ($rule in $rules)
    {
        $folder = Join-Path $game.FullName $rule.Folder

        if (-not (Test-Path -LiteralPath $folder -PathType Container))
        {
            continue
        }

        foreach ($item in @(Get-ChildItem -LiteralPath $folder -Force))
        {
            if (Test-HudsonIsLink $item)
            {
                return "$($item.FullName) is a link to somewhere else"
            }

            if ($item.PSIsContainer)
            {
                if ($rule.Folders -notcontains $item.Name)
                {
                    return "$($item.FullName) is a folder a run wouldn't have made"
                }
            }
            elseif ($rule.Files)
            {
                if ($item.Name -like 'hudson-video-*.mp4')
                {
                    $filmsToKeep += $item.FullName
                }
                else
                {
                    $toDelete += $item.FullName
                }
            }
            else
            {
                return "$($item.FullName) is a file a run wouldn't have left"
            }
        }
    }

    # The films first, so that if one can't be moved nothing has been deleted yet
    if ($filmsToKeep.Count -gt 0)
    {
        $leftBehind = Join-Path $game.FullName 'runs\films-left-behind'

        try
        {
            New-Item -ItemType Directory -Force $leftBehind -ErrorAction Stop | Out-Null

            foreach ($film in $filmsToKeep)
            {
                Move-Item -LiteralPath $film -Destination $leftBehind -ErrorAction Stop
            }
        }
        catch
        {
            return "a film couldn't be moved to $leftBehind, so nothing has been deleted: $_"
        }
    }

    # Only now, when all of it has been looked at. One at a time and by its whole name, and only files.
    foreach ($file in $toDelete)
    {
        Remove-Item -LiteralPath $file -Force
    }

    return $null
}
