# Tries tools/hudson/clear-run-folder.ps1 on folders made for the purpose. tests/hudson_clear_folder_test.cpp runs this, so that it counts with the other tests. Written 2026-11-05 by Melanikus for the AvP port.
#
# That function deletes files from a path built from a command line, so each bad folder here has to be refused for its own reason, with nothing deleted anywhere. Most of these make a real link with
# mklink /J, which needs no special rights, and leave a file on the far side of it. That file has to survive whatever happens.
#
# -FunctionFile is there so that a broken copy of the function can be tried instead, to prove this test notices.

param([string]$FunctionFile = (Join-Path (Split-Path -Parent $PSScriptRoot) 'tools\hudson\clear-run-folder.ps1'))

$ErrorActionPreference = 'Stop'

. $FunctionFile
# #198's stop file, which a run's folder can hold and the clearing has to take away (added 2026-11-10 by Melanikus (Third))
. (Join-Path (Split-Path -Parent $PSScriptRoot) 'tools\hudson\stop-file.ps1')

$top = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'AvP-test-out' }
$root = Join-Path $top "hudson-clear-test-$PID"
$failed = 0

function Check([bool]$good, [string]$what)
{
    Write-Host ("  " + $(if ($good) { 'ok' } else { 'FAILED' }) + "  $what")

    if (-not $good)
    {
        $script:failed++
    }
}

function Add-File([string]$path)
{
    New-Item -ItemType Directory -Force (Split-Path -Parent $path) | Out-Null
    Set-Content -LiteralPath $path -Value 'something' -Encoding ASCII
}

function Add-Link([string]$link, [string]$target)
{
    New-Item -ItemType Directory -Force (Split-Path -Parent $link) | Out-Null
    cmd /c mklink /J "$link" "$target" | Out-Null

    if (-not (Test-Path -LiteralPath $link))
    {
        throw "couldn't make the link $link"
    }
}

# A game folder as a run leaves it, with a file outside everything that must always survive
function New-GameFolder([string]$name)
{
    $game = Join-Path $root $name
    $saves = Join-Path $game 'HudsonRun\My Games\Aliens versus Predator'

    Add-File (Join-Path $game 'AvPx.exe')
    Add-File (Join-Path $game 'runs\2026-11-05-120000\avp_log.txt')
    Add-File (Join-Path $saves 'AliensVsPredator.cfg')
    Add-File (Join-Path $saves 'CONFIG.CFG')
    Add-File (Join-Path $saves 'avp_log.txt')
    Add-File (Join-Path $saves 'hudson-result.txt')
    Add-File (Join-Path $saves 'User_Profiles\profile.prf')
    Add-File (Join-Path $saves 'MPConfig\game.cfg')
    Add-File (Join-Path $saves 'pictures\room1-view1.png')

    return $game
}

function SavesOf([string]$game)
{
    return Join-Path $game 'HudsonRun\My Games\Aliens versus Predator'
}

# The second copy's folder: a run with -SecondCopy keeps its files there (save_folder.h, kSaveFolderSecondCopyName). Added 2026-11-10 by
# Melanikus (Third) for the AvP port, after a second copy's second run was refused for having this folder.
function SecondSavesOf([string]$game)
{
    return Join-Path $game 'HudsonRun\My Games\Aliens versus Predator (second copy)'
}

# What a run with -SecondCopy leaves, as B's folder had it on 2026-11-10
function Add-SecondCopyFiles([string]$game)
{
    $second = SecondSavesOf $game

    Add-File (Join-Path $second 'AliensVsPredator.cfg')
    Add-File (Join-Path $second 'avp_log.txt')
    Add-File (Join-Path $second 'hudson-result.txt')
    Add-File (Join-Path $second 'User_Profiles\New Profile.prf')
    Add-File (Join-Path $second 'MPConfig\game.cfg')
    Add-File (Join-Path $second 'pictures\access-asked.png')
}

# The second copy's files are all still there: the check for a refusal in a folder the second copy has run in
function SecondNothingWent([string]$game)
{
    $second = SecondSavesOf $game

    return (Test-Path -LiteralPath (Join-Path $second 'avp_log.txt')) -and (Test-Path -LiteralPath (Join-Path $second 'User_Profiles\New Profile.prf'))
}

# Nothing at all went: the check for every refusal
function NothingWent([string]$game)
{
    $saves = SavesOf $game

    return (Test-Path -LiteralPath (Join-Path $saves 'avp_log.txt')) -and (Test-Path -LiteralPath (Join-Path $saves 'User_Profiles\profile.prf')) -and (Test-Path -LiteralPath (Join-Path $game 'AvPx.exe'))
}

function Refused([string]$game, [string]$wanted, [string]$what)
{
    $said = Clear-HudsonRunFolder $game

    Check ($said -like $wanted) "$what is refused, saying why [$said]"
    Check (NothingWent $game) "and nothing went"
}

try
{
    New-Item -ItemType Directory -Force $root | Out-Null

    # ---- what it's for ----

    $game = New-GameFolder 'normal'
    $saves = SavesOf $game
    Set-ItemProperty -LiteralPath (Join-Path $saves 'CONFIG.CFG') -Name IsReadOnly -Value $true

    $said = Clear-HudsonRunFolder $game

    Check ($null -eq $said) 'a folder as a run left it is cleared'
    Check (@(Get-ChildItem -LiteralPath $saves -Force -File).Count -eq 0) 'the game''s files have gone, a read-only one too'
    Check (-not (Test-Path -LiteralPath (Join-Path $saves 'User_Profiles\profile.prf'))) 'and the profiles'
    Check (-not (Test-Path -LiteralPath (Join-Path $saves 'MPConfig\game.cfg'))) 'and the multiplayer settings'
    Check (-not (Test-Path -LiteralPath (Join-Path $saves 'pictures\room1-view1.png'))) 'and the last run''s pictures'
    Check (Test-Path -LiteralPath (Join-Path $saves 'User_Profiles')) 'the folders stay'
    Check (Test-Path -LiteralPath (Join-Path $game 'AvPx.exe')) 'the game isn''t touched'
    Check (Test-Path -LiteralPath (Join-Path $game 'runs\2026-11-05-120000\avp_log.txt')) 'and nor are the copies of earlier runs'
    Check ($null -eq (Clear-HudsonRunFolder $game)) 'clearing it again is fine'

    $fresh = Join-Path $root 'fresh'
    Add-File (Join-Path $fresh 'AvPx.exe')
    Check ($null -eq (Clear-HudsonRunFolder $fresh)) 'a game that has never had a run has nothing to clear'
    Check (-not (Test-Path -LiteralPath (Join-Path $fresh 'HudsonRun'))) 'and nothing gets made'

    # ---- a film is never deleted ----

    $filmName = 'hudson-video-2026-11-05-120000.mp4'
    $game = New-GameFolder 'film left behind'
    $saves = SavesOf $game
    Add-File (Join-Path $saves $filmName)
    $said = Clear-HudsonRunFolder $game
    Check ($null -eq $said) "a folder with a film left in it is cleared [$said]"
    Check (Test-Path -LiteralPath (Join-Path $game "runs\films-left-behind\$filmName")) 'and the film is kept, beside the runs'
    Check (-not (Test-Path -LiteralPath (Join-Path $saves $filmName))) 'and it has left, or the next run would take it for its own'
    Check (-not (Test-Path -LiteralPath (Join-Path $saves 'avp_log.txt'))) 'and the rest has gone as usual'

    # the place it would go already has one of that name, so it can't be moved
    $game = New-GameFolder 'film that can''t move'
    Add-File (Join-Path (SavesOf $game) $filmName)
    Add-File (Join-Path $game "runs\films-left-behind\$filmName")
    Refused $game '*a film couldn''t be moved to*so nothing has been deleted*' 'a film that can''t be moved'
    Check (Test-Path -LiteralPath (Join-Path (SavesOf $game) $filmName)) 'and the film is still where it was'

    # ---- the wrong place ----

    Check ((Clear-HudsonRunFolder (Join-Path $root 'nothing here')) -like '*isn''t a folder') 'a folder that isn''t there is refused'
    Check ((Clear-HudsonRunFolder (Join-Path $game 'AvPx.exe')) -like '*isn''t a folder') 'and so is a file'

    Refused (New-GameFolder 'steamapps\common\AvP') '*looks like a Steam folder' 'a Steam folder'
    Refused (New-GameFolder 'SteamApps2') '*looks like a Steam folder' 'one in other capitals'

    # ---- links, each with a file on the far side ----

    $outside = Join-Path $root 'outside'
    $precious = Join-Path $outside 'precious.txt'
    Add-File $precious

    $game = New-GameFolder 'link in profiles'
    Remove-Item -LiteralPath (Join-Path (SavesOf $game) 'MPConfig\game.cfg')
    Remove-Item -LiteralPath (Join-Path (SavesOf $game) 'MPConfig')
    Add-Link (Join-Path (SavesOf $game) 'MPConfig') $outside
    Refused $game '*is a link to somewhere else' 'a link where a folder should be'
    Check (Test-Path -LiteralPath $precious) 'and the file through it survives'

    $game = New-GameFolder 'link among files'
    Add-Link (Join-Path (SavesOf $game) 'somewhere') $outside
    Refused $game '*is a link to somewhere else' 'a link among the files'
    Check (Test-Path -LiteralPath $precious) 'and the file through it survives'

    # the run folder itself: all of it is on the far side, so the far side has to look just like a run's
    $farSide = New-GameFolder 'far side'
    $game = Join-Path $root 'link is the run folder'
    Add-File (Join-Path $game 'AvPx.exe')
    Add-Link (Join-Path $game 'HudsonRun') (Join-Path $farSide 'HudsonRun')
    $said = Clear-HudsonRunFolder $game
    Check ($said -like '*is a link to somewhere else') "the run folder being a link is refused [$said]"
    Check (Test-Path -LiteralPath (Join-Path (SavesOf $farSide) 'avp_log.txt')) 'and what it points at survives'

    $farSide = New-GameFolder 'far side of my games'
    $game = Join-Path $root 'link is my games'
    Add-File (Join-Path $game 'AvPx.exe')
    Add-Link (Join-Path $game 'HudsonRun\My Games') (Join-Path $farSide 'HudsonRun\My Games')
    $said = Clear-HudsonRunFolder $game
    Check ($said -like '*is a link to somewhere else') "a link further down is refused too [$said]"
    Check (Test-Path -LiteralPath (Join-Path (SavesOf $farSide) 'avp_log.txt')) 'and what it points at survives'

    # the game folder, or a folder on the way to it
    $farSide = New-GameFolder 'far side of the game'
    $game = Join-Path $root 'link is the game'
    Add-Link $game $farSide
    $said = Clear-HudsonRunFolder $game
    Check ($said -like '*is a link to somewhere else') "the game folder being a link is refused [$said]"
    Check (Test-Path -LiteralPath (Join-Path (SavesOf $farSide) 'avp_log.txt')) 'and what it points at survives'

    $farSide = New-GameFolder 'far side above\game'
    $above = Join-Path $root 'link above'
    Add-Link $above (Join-Path $root 'far side above')
    $said = Clear-HudsonRunFolder (Join-Path $above 'game')
    Check ($said -like '*is a link to somewhere else') "a folder on the way to the game being a link is refused [$said]"
    Check (Test-Path -LiteralPath (Join-Path (SavesOf $farSide) 'avp_log.txt')) 'and what it points at survives'

    # ---- what a run wouldn't have left ----

    $game = New-GameFolder 'unknown folder'
    Add-File (Join-Path (SavesOf $game) 'something new\file.txt')
    Refused $game '*a folder a run wouldn''t have made' 'a folder it doesn''t know'
    Check (Test-Path -LiteralPath (Join-Path (SavesOf $game) 'something new\file.txt')) 'and what''s in it survives'

    $game = New-GameFolder 'folder in profiles'
    Add-File (Join-Path (SavesOf $game) 'User_Profiles\deeper\file.txt')
    Refused $game '*a folder a run wouldn''t have made' 'a folder inside the profiles'

    $game = New-GameFolder 'file in run folder'
    Add-File (Join-Path $game 'HudsonRun\left here.txt')
    Refused $game '*a file a run wouldn''t have left' 'a file directly in the run folder'
    Check (Test-Path -LiteralPath (Join-Path $game 'HudsonRun\left here.txt')) 'and it survives'

    $game = New-GameFolder 'file in my games'
    Add-File (Join-Path $game 'HudsonRun\My Games\left here.txt')
    Refused $game '*a file a run wouldn''t have left' 'a file in My Games'

    $game = New-GameFolder 'folder in run folder'
    Add-File (Join-Path $game 'HudsonRun\another\file.txt')
    Refused $game '*a folder a run wouldn''t have made' 'another folder in the run folder'

    # ---- the second copy's folder (-SecondCopy) ----

    # only the second copy has run here, as in a pair's joiner folder: its second run has to start as cleanly as its first
    $game = Join-Path $root 'second copy only'
    Add-File (Join-Path $game 'AvPx.exe')
    Add-SecondCopyFiles $game
    $second = SecondSavesOf $game
    $said = Clear-HudsonRunFolder $game
    Check ($null -eq $said) "a folder only the second copy has run in is cleared [$said]"
    Check (@(Get-ChildItem -LiteralPath $second -Recurse -Force -File).Count -eq 0) 'every one of the second copy''s files has gone'
    Check ((Test-Path -LiteralPath (Join-Path $second 'User_Profiles')) -and (Test-Path -LiteralPath (Join-Path $second 'MPConfig')) -and (Test-Path -LiteralPath (Join-Path $second 'pictures'))) 'its folders stay'
    Check (Test-Path -LiteralPath (Join-Path $game 'AvPx.exe')) 'the game isn''t touched'
    Check ($null -eq (Clear-HudsonRunFolder $game)) 'clearing it again is fine'

    # both copies have run here
    $game = New-GameFolder 'both copies'
    Add-SecondCopyFiles $game
    $said = Clear-HudsonRunFolder $game
    Check ($null -eq $said) "a folder both copies have run in is cleared [$said]"
    Check (@(Get-ChildItem -LiteralPath (Join-Path $game 'HudsonRun') -Recurse -Force -File).Count -eq 0) 'every file of both copies has gone'
    Check (Test-Path -LiteralPath (Join-Path $game 'runs\2026-11-05-120000\avp_log.txt')) 'and the copies of earlier runs are still there'

    # a film the second copy left is kept, as the game's own is
    $game = Join-Path $root 'second copy film'
    Add-File (Join-Path $game 'AvPx.exe')
    Add-SecondCopyFiles $game
    Add-File (Join-Path (SecondSavesOf $game) $filmName)
    $said = Clear-HudsonRunFolder $game
    Check ($null -eq $said) "a second copy's folder with a film left in it is cleared [$said]"
    Check (Test-Path -LiteralPath (Join-Path $game "runs\films-left-behind\$filmName")) 'and its film is kept, beside the runs'

    # the second copy's folder is held to the same rules as the game's own, each refusal with nothing deleted in either
    $game = New-GameFolder 'second copy unknown folder'
    Add-SecondCopyFiles $game
    Add-File (Join-Path (SecondSavesOf $game) 'something new\file.txt')
    Refused $game '*a folder a run wouldn''t have made' 'a folder the second copy''s folder shouldn''t have'
    Check (SecondNothingWent $game) 'and nothing of the second copy''s went'
    Check (Test-Path -LiteralPath (Join-Path (SecondSavesOf $game) 'something new\file.txt')) 'and what''s in it survives'

    $game = New-GameFolder 'second copy folder in profiles'
    Add-SecondCopyFiles $game
    Add-File (Join-Path (SecondSavesOf $game) 'User_Profiles\deeper\file.txt')
    Refused $game '*a folder a run wouldn''t have made' 'a folder inside the second copy''s profiles'
    Check (SecondNothingWent $game) 'and nothing of the second copy''s went'

    $game = New-GameFolder 'second copy link'
    Add-SecondCopyFiles $game
    Add-Link (Join-Path (SecondSavesOf $game) 'somewhere') $outside
    Refused $game '*is a link to somewhere else' 'a link in the second copy''s folder'
    Check (SecondNothingWent $game) 'and nothing of the second copy''s went'
    Check (Test-Path -LiteralPath $precious) 'and the file through it survives'

    $farSide = New-GameFolder 'far side of the second copy'
    Add-SecondCopyFiles $farSide
    $game = New-GameFolder 'second copy is a link'
    Add-Link (SecondSavesOf $game) (SecondSavesOf $farSide)
    Refused $game '*is a link to somewhere else' 'the second copy''s folder being a link'
    Check (SecondNothingWent $farSide) 'and what it points at survives'

    # only the second copy's own name: a name like it is still a folder a run wouldn't have made
    $game = New-GameFolder 'third copy'
    Add-File (Join-Path $game 'HudsonRun\My Games\Aliens versus Predator (third copy)\avp_log.txt')
    Refused $game '*a folder a run wouldn''t have made' 'a folder named like a copy the game doesn''t have'

    # ---- #198: the stop file (stop-file.ps1), written only in a run's own folders ----

    $game = New-GameFolder 'stop one'
    $said = Add-HudsonStopFile $game
    $stop = Join-Path (SavesOf $game) 'hudson-stop'
    Check ($null -eq $said) "a running game's folder gets a stop file [$said]"
    Check ((Test-Path -LiteralPath $stop -PathType Leaf) -and (Get-Item -LiteralPath $stop).Length -eq 0) 'an empty plain file, in its run folder'
    Check (-not (Test-Path -LiteralPath (Join-Path $game 'hudson-stop'))) 'and nowhere else'
    $before = (Get-Item -LiteralPath $stop).LastWriteTimeUtc
    Start-Sleep -Milliseconds 1100
    Check ($null -eq (Add-HudsonStopFile $game)) 'asking again is fine'
    Check ((Get-Item -LiteralPath $stop).LastWriteTimeUtc -gt $before) 'and the file is written again, so its date is new, as the game wants'
    Check ($null -eq (Clear-HudsonRunFolder $game)) 'the next run''s clearing'
    Check (-not (Test-Path -LiteralPath $stop)) 'takes the stop file away, so it can''t stop that run'

    $game = New-GameFolder 'stop both copies'
    Add-SecondCopyFiles $game
    Check ($null -eq (Add-HudsonStopFile $game)) 'a folder both copies run in'
    Check ((Test-Path -LiteralPath (Join-Path (SavesOf $game) 'hudson-stop')) -and (Test-Path -LiteralPath (Join-Path (SecondSavesOf $game) 'hudson-stop'))) 'gets one in each copy''s folder'

    $game = Join-Path $root 'stop second only'
    Add-File (Join-Path $game 'AvPx.exe')
    Add-SecondCopyFiles $game
    Check ($null -eq (Add-HudsonStopFile $game)) 'a joiner''s folder, the second copy''s alone'
    Check ((Test-Path -LiteralPath (Join-Path (SecondSavesOf $game) 'hudson-stop')) -and -not (Test-Path -LiteralPath (SavesOf $game))) 'gets one there, and no folder is made for the other'

    $fresh = Join-Path $root 'stop no run'
    Add-File (Join-Path $fresh 'AvPx.exe')
    Check ((Add-HudsonStopFile $fresh) -like '*has no run folder*') 'a game with no run folder is refused'
    Check (-not (Test-Path -LiteralPath (Join-Path $fresh 'HudsonRun'))) 'and nothing is made'
    Check ((Add-HudsonStopFile (Join-Path $root 'stop nothing here')) -like '*isn''t a folder') 'a folder that isn''t there is refused'
    Check ((Add-HudsonStopFile (New-GameFolder 'steamapps\stop')) -like '*looks like a Steam folder') 'a Steam folder is refused'

    $game = New-GameFolder 'stop that is a folder'
    New-Item -ItemType Directory -Force (Join-Path (SavesOf $game) 'hudson-stop') | Out-Null
    Check ((Add-HudsonStopFile $game) -like '*a folder or a link*') 'a hudson-stop that''s a folder is refused'

    $farSide = New-GameFolder 'stop far side'
    $game = Join-Path $root 'stop through a link'
    Add-File (Join-Path $game 'AvPx.exe')
    Add-Link (Join-Path $game 'HudsonRun') (Join-Path $farSide 'HudsonRun')
    Check ((Add-HudsonStopFile $game) -like '*is a link to somewhere else') 'a run folder that''s a link is refused'
    Check (-not (Test-Path -LiteralPath (Join-Path (SavesOf $farSide) 'hudson-stop'))) 'and nothing is written on its far side'

    $farSide = New-GameFolder 'stop far side of the copy'
    $game = New-GameFolder 'stop copy is a link'
    Add-Link (SecondSavesOf $game) (SavesOf $farSide)
    Check ((Add-HudsonStopFile $game) -like '*is a link to somewhere else') 'a copy''s folder that''s a link is refused'
    Check (-not (Test-Path -LiteralPath (Join-Path (SavesOf $farSide) 'hudson-stop')) -and -not (Test-Path -LiteralPath (Join-Path (SavesOf $game) 'hudson-stop'))) 'and nothing is written anywhere, not even in the other folder'
}
catch
{
    Check $false "the test itself stopped: $_"
}
finally
{
    # Links go first and only as links, so that clearing up can't reach through one. rmdir on a link takes the link and leaves what it points at.
    # A message from cmd isn't a reason to stop here, so these stay inside its own command line, and clearing up carries on past anything that goes wrong.
    $ErrorActionPreference = 'Continue'

    if (Test-Path -LiteralPath $root)
    {
        $links = @(cmd /c "dir /s /b /al `"$root`" 2>nul")

        foreach ($link in ($links | Sort-Object Length -Descending))
        {
            cmd /c "rmdir `"$link`" 2>nul"
        }

        if (@(cmd /c "dir /s /b /al `"$root`" 2>nul").Count -eq 0)
        {
            Get-ChildItem -LiteralPath $root -Recurse -Force -File | ForEach-Object { Set-ItemProperty -LiteralPath $_.FullName -Name IsReadOnly -Value $false }
            Remove-Item -LiteralPath $root -Recurse -Force
        }
        else
        {
            Write-Host "  left $root alone, because it still has links in it"
        }
    }
}

Write-Host $(if ($failed) { "SOME CHECKS FAILED: $failed" } else { 'All checks passed' })
exit $(if ($failed) { 1 } else { 0 })
