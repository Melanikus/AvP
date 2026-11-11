#Requires -Version 4.0
<#
Runs the game once with nobody at the keyboard, and says how it went. 3dc/avp/hudson.cpp explains what a run is. This script makes the checks that have to come first, so that nobody has to remember them.

  powershell -ExecutionPolicy Bypass -File tools\hudson\run-hudson.ps1 -GameFolder C:\Games\AvP-test -Level derelict -Frames 300

With a marine for the camera to follow, standing in one room and looking two ways. -Frames is then the most it may take:

  powershell -ExecutionPolicy Bypass -File tools\hudson\run-hudson.ps1 -GameFolder C:\Games\AvP-test -Level derelict -Frames 5000 -Rooms 1 -Views 2

Saying how the world is drawn, and saving a picture of each view. -Lighting is gouraud, perpixel, normals or relief, -Shadows is on or off, and -Record is numbers or pictures. Saving a picture stops the game for a moment, so a run with pictures says nothing about timings:

  powershell -ExecutionPolicy Bypass -File tools\hudson\run-hudson.ps1 -GameFolder C:\Games\AvP-test -Level derelict -Frames 5000 -Rooms 1 -Lighting normals -Shadows on -Record pictures

Standing in front of the biggest wall showing a picture, and drawing the same view each way there is, with a picture of each. -WallRank 2 is the next biggest wall, and -WallRoom hole only looks in that room. -Views 3 is straight at the wall, then turned, then straight at it again from a step along it. tools\hudson\compare-pictures.ps1 reads the pictures afterwards:

  powershell -ExecutionPolicy Bypass -File tools\hudson\run-hudson.ps1 -GameFolder C:\Games\AvP-test -Level derelict -Frames 20000 -Rooms 1 -Views 3 -Wall hive01 -Compare relief -Record pictures -Lighting relief -Shadows off

Going through the level room by room, in the order they join up, looking round each. Nobody has to know the rooms' names:

  powershell -ExecutionPolicy Bypass -File tools\hudson\run-hudson.ps1 -GameFolder C:\Games\AvP-test -Level derelict -Frames 500000 -Seconds 600 -Rooms 12 -Views 4 -HoldSeconds 3 -WhileInUse -TimingsDontMatter

Trying the route that draws walls from a fixed buffer. -Static is scene, for the route, or compare, for the route with each report frame drawn both ways and compared, or drop, which is compare with half the walls left out of the new route on purpose, to prove the comparison finds holes. -Hold 600 makes a hold long enough to be sure of a whole report inside it. The result's static_frames says how many frames took the route, out of how many asked. Not on derelict where he starts: that room has the game's only mirror, and the route refuses a mirrored frame:

  powershell -ExecutionPolicy Bypass -File tools\hudson\run-hudson.ps1 -GameFolder C:\Games\AvP-test -Level invasion -Frames 20000 -Rooms 1 -Views 4 -Hold 600 -Lighting perpixel -Shadows on -Static scene

A run has the marine's shoulder lamp off unless it says -Lamp on (the user's word, 2026-11-08). Runs that compare what's on the screen (-Vision, -Compare) can't have it, and hudson-result.txt and the log always say which. -Lamp blink switches it
every second and a half. -LampKind real makes it a real light with real shadows, in the three per-pixel lighting modes, with -LampAt wide to sit it further out.

-Flare has him throw a flare as each hold starts, on a run that looks round and doesn't compare what's on the screen. Give it a -HoldSeconds long enough for the flare to land. hudson-result.txt says how many really went.
-Net host or -Net join makes this one of two games on this machine, on its own address only. tools\hudson\run-net-pair.ps1 runs both, and that's the way to use it. -NetPartner is the other game's folder: it's the only other game this will start alongside.

Asking the relays' list for access, as a player does, in a run of its own with no -Net: it stays at the menus, presses the Online access page, saves a picture of it, and ends there. -Access request, -Access check (with -AccessWait, seconds to wait first, 61 or more) or -Access newid. It needs -Relay and -ServersLine for the server it asks, and -SecondCopy for the second copy's own identity. The host of a pair can answer a player asking to join with -Answer letin or no (-AnswerAfter, its seconds), and the joiner can find the host through Find Games with -FindGames; run-net-pair.ps1 passes those as -HostAnswer and -JoinFindGames:

  powershell -ExecutionPolicy Bypass -File tools\hudson\run-hudson.ps1 -GameFolder C:\Games\AvP-test -Level derelict -Frames 6000 -Relay -ServersLine "127.0.0.1:8443:27500 <fingerprint> Local" -Access request

The window goes on the second screen, where it covers nothing anyone's using, and the game does that itself from the moment the window is made. -Screen main leaves it on the main screen, which is for comparing the two. With no second screen, it stays on the main one, and the log says so. The log's "the window is at" line says where it went. -WindowSlot and -WindowsTogether are for several games at once, and run-net-pair.ps1 sets them.

A run with -WhileInUse makes a film of itself, an mp4 kept with the run's other files, and the film= line of hudson-result.txt says where. -Record pictures keeps a film as well. -NoFilm or -Record numbers stops it. A film's pictures slow the frames, so a run that measures time has none, and -Record video, which asks for one by name, needs -TimingsDontMatter and -WhileInUse. Nothing deletes a film: one left behind by a run that was cut short goes to runs\films-left-behind when the next run clears the way.

-FilmMenus starts the film at the menus instead of the level, for showing them: it's cut into clips, each finished at a page the menus picture, or at the level's first frame, and named after it, like hudson-video-<when>-clip02-join-list.mp4 beside join-list.png. The last clip, to the run's end, has its number alone. -PageHold 4 holds each of those pages 4 seconds first, so the clip shows it (not the joiner's "let in", which the host is waiting on). Every clip goes in the run's folder with a video= line in run.txt.

-WhileInUse is for runs that don't measure time, so it needs -TimingsDontMatter as well. It doesn't wait for the machine to be idle, and touching the machine doesn't end the run. The result line says machine=shared, and touched=yes if anyone did. Every other promise stands: the window doesn't take the focus, the mouse is left alone, there's no sound, and it only runs in a test copy.

Walking through the level by himself, until he's reached 5 new modules. The log's "nav:" lines say what he did, and hudson-result.txt counts what he walked into apart from any time he had to be moved:

  powershell -ExecutionPolicy Bypass -File tools\hudson\run-hudson.ps1 -GameFolder C:\Games\AvP-test -Level derelict -Frames 40000 -Rooms 1 -Nav 5

Following a route through the level and pressing what it says, from hudson-routes\derelict.txt beside the game. make-route.ps1 makes that file:

  powershell -ExecutionPolicy Bypass -File tools\hudson\run-hudson.ps1 -GameFolder C:\Games\AvP-test -Level derelict -Frames 400000 -Rooms 1 -Route derelict

Watching an alien find its way to him, from A to B, filmed, with him holding his fire, and with #109's ways where the build has them (-AlienWays):

  powershell -ExecutionPolicy Bypass -File tools\hudson\run-hudson.ps1 -GameFolder C:\Games\AvP-test -Level derelict -Frames 30000 -Seconds 400 -Rooms 1 -Combat alien -Watch alien -HoldFire -AlienAt m43880,5490,m310070 -StandAt m50790,5490,m267890 -WhileInUse -TimingsDontMatter -AlienWays

Add -WatchFrom eyes to film it from his own eyes instead, turned to the alien, as a player standing there would see it come.

What it won't do:
- Run from a Steam folder. A run is for a test copy of the game, and this script empties folders, so it never goes there. The game refuses as well, unless steam=yes is on its own command line, which is for a run the user starts from Steam. This script doesn't pass that word.
- Run while the game is already running, or while a compiler is running, because then its timings would mean nothing. -TimingsDontMatter leaves the compiler check out.

The game itself checks that nobody has touched the machine for -IdleSeconds before it starts, and it ends the moment anyone does. So this script doesn't look at that. It just says what the game said.

Every run starts the same way. Before each one, this empties the folder the game keeps a run's files in, and copies the settings in tools\hudson\AliensVsPredator.cfg there. clear-run-folder.ps1 says exactly what it deletes, and what makes it refuse.

What's left behind: a folder called runs\<date and time> inside the game folder, with a copy of the game's log and result, and a file called run.txt saying exactly what was started: the arguments, a fingerprint (SHA-256) of the exe and of the settings, so that two runs can be shown to be the same, and how many lines of the log are errors. A good run should have none. The next run clears the game's own copies, but not these.

The exit code here is the game's own, so another script can read it:
  0        finished
  21       the arguments were refused
  22       that isn't the exact name of one of the game's own levels
  23       this copy of the game is one a run must not touch. The game writes nothing at all in that case, so there's no log or result to keep
  24       someone had touched the machine too recently, so it didn't start. Try again later
  25       someone touched the machine, so it ended there
  26       it went past its time limit
  27       the game wanted to ask someone something
  28       the marine couldn't be made, or he's gone
  29       the marine never settled, or he moved during a hold
  30       -Frames ran out before the marine had finished
  40       -Route asked for a route that isn't there, or that has something wrong with it. Or -Play has no list of steps, or the log says why it was refused
  41       walking, he fell further than the game lets a marine survive. The log's "HE FELL TOO FAR" line says where. test-deaths.ps1 proves this one
  42       walking or -Play, the player died. test-deaths.ps1 proves this one too
  43       a route or -Play's list of steps was over, and the game doesn't say the level is complete. A crossing only counts if hudson-result.txt says nav_crossed=yes
  44       the game says the level is complete, and he wasn't at the end of the route. The log says what the route still had ahead of him
  45       -Play: the game ended the level, and he was playing it himself. The log says how many steps worked
  46       -Play: there was no grid for him to find his way on
  47       -Play: the game ended the level, but only with cheats, so it does not show a player can. play_cheats= in the result counts them, and each is a "play: cheat" line in the log
  48       -Play in a pair: the other one's body never came, or the two never looked at each other, so neither played on alone. The log's "play:" lines say which
  31       -Lighting or -Shadows asked for something this machine can't draw
  32       -Wall asked for a wall this level hasn't
  33       -Strategy asked for one this level hasn't, or there's nowhere to stand to see it
  34       the strategy was asked to go on, and its state, or that of what he was looking at, never changed
  35       strategies were asked for in a network game
  36       -Combat couldn't make its alien, or there was nowhere in sight of him to put it
  37       the game stopped before the run had finished: its window was closed, or the graphics card failed
  38       stopped by hand: tools\hudson\stop-run.ps1 asked it to end (#198). It ended as at its time limit, so its result and film are whole
  0x3421   the level didn't load
  0xffff   an assert failed
  anything else is a crash, as Windows reports it, or from this script:
  100      this script wouldn't start it
  101      it didn't end in time, so this script ended it
#>
param(
    [Parameter(Mandatory = $true)][string]$GameFolder,
    [Parameter(Mandatory = $true)][string]$Level,
    [Parameter(Mandatory = $true)][int]$Frames,
    [int]$Seconds = 1800,
    [int]$IdleSeconds = 120,
    [int]$Rooms = 0,
    [int]$Views = 0,
    [int]$Hold = 0,
    [int]$HoldSeconds = 0,
    [string]$TourFrom = '',
    [int]$Nav = 0,
    [string]$Route = '',
    [string]$Pace = '',
    [string]$Map = '',
    [string]$FrameHold = '',
    [string]$Lighting = '',
    [string]$Shadows = '',
    [string]$Record = '',
    [string]$Wall = '',
    [int]$WallRank = 0,
    [string]$WallRoom = '',
    [string]$Compare = '',
    [string]$Strategies = '',
    [string]$Strategy = '',
    [int]$StrategySeconds = 0,
    [string]$StrategyLook = '',
    [string]$Combat = '',
    [string]$Static = '',
    [string]$ShadowSwitches = '',
    [int]$Aliens = 0,
    [string]$Watch = '',   # alien: in a fight, the camera follows the alien from the side and the log says where it goes. Needs -Combat alien
    [switch]$HoldFire,     # in a watched fight he never fires, so that the alien reaches him
    [string]$Every = '',    # seconds between the aliens of -Together, like 0.5, as a level's generator sends them. Left out, they all come at once
    [int]$Together = 0,     # 2 to 8: a watched fight has this many aliens at once, a few metres apart, each followed once a second. Needs -Watch alien
    [ValidateSet('', 'side', 'eyes')][string]$WatchFrom = '', # where a watched fight is filmed from: side (beside the alien, as without it) or eyes (his own eyes, turned to the alien)
    [string]$AlienAt = '', # x,y,z in the level's own mm, where an m stands for a minus sign: the fight's alien is made just above it. Several, with / between them, are -Together's aliens
    [string]$StandAt = '', # x,y,z the same way: he stands there
    [switch]$AlienWays,    # the game's own -alienways: an alien that knows where he is takes the way the alien ways map finds, where that's in the build
    [string]$AlienWaysSwitches = '', # more of the game's own -alienways... developer switches, like -alienwaysgrid or -alienwaysgrid=1, as the build names them. Only -alienways switches, letters and an optional =number. Needs -AlienWays
    [int]$AlienWaysWatch = -1, # the game's -alienwayswatch=N, 0 to 63: the alien with the guide's place N gets a log line every frame, where that's in the build. Needs -AlienWays
    [int]$AlienWaysAim = -1, # the game's -alienwaysaim=N, 0 to 10000 mm: how far along its way an alien aims, where that's in the build. Left out, the build's own. Needs -AlienWays
    [string]$UserCopy = '', # the folder of a copy of the game the user is playing at the same time. A game running from there, or from inside it, doesn't stop this run; any other still does. Needs -WhileInUse
    [string]$Exe = 'AvPx.exe',
    [switch]$TimingsDontMatter,
    [switch]$WhileInUse,
    [switch]$Show,
    [string]$Size = '',
    [ValidateSet('second', 'main')][string]$Screen = 'second',
    [ValidateRange(0, 3)][int]$WindowSlot = 0,
    [ValidateRange(1, 4)][int]$WindowsTogether = 1,
    [ValidateRange(-1, 1)][int]$WindowBig = -1,
    [string]$Vision = '',
    [string]$Break = '',
    [string]$Lamp = '',
    [string]$LampKind = '',
    [string]$LampAt = '',
    [string]$LampSize = '', # how big the lamp's spot is, 0.05 to 0.35, written like 0.35. Needs -Lamp on or blink. The user's alien films have 0.35, the most it goes to
    [string]$Follow = '',   # the name of one thing in the level (an NPC, a door, a switch): the log says its place and state once a second
    [string]$WalkFrom = '', # x,y,z in the level's own mm, where an m stands for a minus sign: he's put here and walks to -WalkTo. With -DieAt, only after the level starts again
    [string]$WalkTo = '',
    [string]$LookAt = '',   # x,y,z the same way: once he's walked, he looks here, and with -Record pictures a picture comes 2 and 5 seconds later
    [switch]$Bright,
    [switch]$Flare,
    [int]$Gun = 0,
    [string]$Species = '',
    [int]$DieAt = 0,       # he dies this many seconds in, once in the run, and "presses a key" three seconds later
    [string]$Difficulty = '', # easy, medium or hard, in single player or for a campaign
    [switch]$Meets,        # the log says what the player meets when the level starts, for comparing runs
    [switch]$LevelFilms,   # a level's own films play, which a run otherwise skips
    [string]$SiegeFar = '',  # the room a siege's xenoborgs are made in, further off so they have time to wake, with _ for a space in its name
    [string]$SiegeFrom = '', # the room a siege's enemies are made in, with _ for a space in its name. Left out, the far side of the siege's own room
    [string]$CustomMap = '', # a custom map made for single player, by its name in avp_rifs\Custom, played instead of -Level, which still says who plays
    [string]$Siege = '',   # a pair stands in the room of this name, armed and unable to die, against every kind of enemy the host can make. Needs -Coop
    [ValidateSet('', 'yes', 'no')][string]$Look = '', # a player of a pair looks at what's going on. Left out, a fight or a kill looks all the time, and otherwise each level's start does
    [int]$FilmSkip = 0,    # a film stops this many seconds in, as if a key had been pressed. Needs -LevelFilms
    [int]$ListPlace = -1,  # a skirmish on this place of the co-op level list, as the level row would start it
    [string]$Class = '',   # a gun= run starts as a skirmish, as this marine class: shotgun, revolver or m79
    [switch]$GunMarine,    # a gun= run stands a marine holding that gun in front of him, for pictures of what other players see
    [switch]$Play,         # he plays the level himself, on foot. It reads <level>.play from HudsonChains in the game folder
    [int]$PlayGun = 0,     # with -Play, the slot of the gun he's given and holds, from 1 to 14: 10 is the shotgun
    [int]$StepSeconds = 0, # with -Play, how long one step may take before he's moved to it (a logged cheat), 10 to 600. 0 is the game's 180
    [switch]$GunAlien,     # a gun= run makes an alien in front of him, for a throw or a shot to hit
    [switch]$NoFilm,
    [string]$Net = '',
    [string]$NetPartner = '',
    [string]$NetSecondPartner = '',
    [switch]$Coop,
    [string]$BotFight = '',
    [switch]$NoPictures,
    [string]$KillKind = '',   # with -KillAt x,y,z, where an m stands for a minus sign, and -Killer host or join: a machine really kills the level's own NPC of this kind placed nearest there
    [string]$KillAt = '',
    [string]$Killer = '',
    [switch]$BotStay,
    [switch]$BotOwn,
    [switch]$GhostLog,
    [string]$BotWeapon = '',
    [int]$DeathWay = -1,
    [string]$RateTest = '',
    [switch]$VsyncOff,
    [switch]$BotSingle,
    [int]$Fps = 60,
    [switch]$Uncapped,
    [switch]$NoFriendlyFire,
    [switch]$CoopWalk,
    [switch]$Hostile,
    [string]$HostileKind = '',
    [switch]$Typed,
    [int]$CoopEnd = 0,
    [switch]$Relay,             # server #14: host or join through the relay server -ServersLine names (-relayhost or -relayjoin, by -Net)
    # Modified 2026-11-10 by Melanikus for the AvP port (#175): the server is one servers.cfg line, which the run writes beside bin (relay-words.ps1)
    [string]$ServersLine = '',  # that line, "<address>[:<list port>:<relay port>] <fingerprint> <name>": 127.0.0.1 for the server on this machine. Needs -Relay
    [string]$RemoteRelay = '',  # server #14 beyond this machine: the line's address, one IPv4 address, given on purpose. Needs -Relay
    # (The v2 draft's switch went on 2026-11-10, #175 PR 3a: every relay run is version 2, sealed)
    [switch]$SecondCopy,          # -secondcopy: the second copy of the game on this machine, with its own settings, log and online identity
    # Added 2026-11-10 by Melanikus for the AvP port, to see how the menus are drawn at a screen size (menu_shape.h):
    [switch]$MenuPictures,        # menus=pictures: a run with no -Net that stays at the menus, pictures main, Multiplayer, Online access and Options, and the intro's copyright screen and logo, and ends
    # Added 2026-11-10 by Melanikus (Third) for the AvP port: the presses Hudson knows (hudson_plan.cpp's access=, accesswait=, answer=,
    # answerafter= and findgames=), so a run can use them. Each is checked below to the shape and range Hudson itself takes
    [string]$Access = '',         # request, check or newid: a run with no -Net that stays at the menus and presses the Online access page's Request access, Check again, or Make a new identity. Needs -Relay, for the list's lines
    [int]$AccessWait = 0,         # with -Access check: seconds before Check again is pressed, 61 to 3600, past the list's minute between calls
    [string]$Answer = '',         # letin or no: with -Net host, the host's answer to a player asking to join, pressed on the page's own rows
    [int]$AnswerAfter = 0,        # with -Answer: seconds before the host answers, 1 to 15, well inside the joiner's 20 s wait. Left out, Hudson's own 3
    [switch]$FindGames,           # with -Net join: the joiner finds the host through Find Games, as a player does
    [switch]$FilmMenus,           # film=menus: the film starts at the menus, cut into a clip for each page they picture (added 2026-11-10 by Melanikus (Third))
    [int]$PageHold = 0,           # with -FilmMenus: seconds each pictured page is held first, 1 to 10, so the film shows it
    # Added 2026-11-10 by Melanikus (Third) for the AvP port, for the owner's rehearsal of beta.2's screens (hudson_plan.h's yespage= and
    # hostrow=)
    [string]$YesPage = '',        # yes or no: with -Access request or check, the run sees the yes page the Online access row opens, as a player's first time does, and presses that row. The yes holds for the run alone
    [string]$HostRow = ''         # online or athome: with -Net host, the host hosts as a player does, through the Multiplayer page's Host row, Start game and Start, with the server's wait if it hasn't answered
)
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'clear-run-folder.ps1')
# -RemoteRelay's and -ServersLine's own rules, and the servers.cfg a run writes, the same in run-net-pair.ps1
. (Join-Path $PSScriptRoot 'relay-words.ps1')

function Stop-Here([string]$why)
{
    Write-Host "run-hudson: not starting: $why"
    exit 100
}

$folder = (Get-Item -LiteralPath $GameFolder).FullName
$exePath = Join-Path $folder $Exe
$settings = Join-Path $PSScriptRoot 'AliensVsPredator.cfg'

if ($folder -match 'steamapps') { Stop-Here "$folder looks like a Steam folder" }
if ($Record -eq 'video' -and -not ($TimingsDontMatter -and $WhileInUse)) { Stop-Here "-Record video slows the frames it records, so it has to be asked for together with -TimingsDontMatter and -WhileInUse" }
if ($WhileInUse -and -not $TimingsDontMatter) { Stop-Here "-WhileInUse means the timings mean nothing, so it has to be asked for together with -TimingsDontMatter" }
if ($Exe -ne (Split-Path -Leaf $Exe)) { Stop-Here "-Exe has to be a file in the game folder, and $Exe is a path" }
if (-not (Test-Path -LiteralPath $settings)) { Stop-Here "there's no $settings" }
if (-not (Test-Path -LiteralPath $exePath)) { Stop-Here "there's no $Exe in $folder" }

if ($Net -and -not $WhileInUse) { Stop-Here "-Net is two games at once on one machine, so it needs -TimingsDontMatter and -WhileInUse" }
# Modified 2026-11-10 by Melanikus (Third): or an -Access run, which asks the relays' list alone (the presses, below)
if ($Relay -and $Net -ne 'host' -and $Net -ne 'join' -and -not $Access) { Stop-Here "-Relay is for a network run's host or joiner, or an -Access run, so it needs -Net host, -Net join or -Access" }
if ($RemoteRelay -and -not $Relay) { Stop-Here "-RemoteRelay says which relay -Relay goes through, so it needs -Relay" }
$relayWordsRefusal = Get-HudsonRemoteRelayRefusal $RemoteRelay
if ($relayWordsRefusal) { Stop-Here $relayWordsRefusal }
# The relay server (modified 2026-11-10 by Melanikus for the AvP port, #175): one servers.cfg line, to its shape, on this machine unless
# -RemoteRelay gives its address. The game reads it again by its own rules.
if ($Relay -and -not $ServersLine) { Stop-Here "-Relay needs -ServersLine: the game finds its server only in servers.cfg, and the run writes that one line there" }
if ($ServersLine -and -not $Relay) { Stop-Here "-ServersLine is the server -Relay goes through, so it needs -Relay" }
$serversLineRefusal = if ($ServersLine) { Get-HudsonServersLineRefusal $ServersLine $RemoteRelay } else { '' }
if ($serversLineRefusal) { Stop-Here $serversLineRefusal }
# No hostile kind of run ever goes beyond this machine: the other end could be anybody's game (the game's plan reader refuses the same)
if ($RemoteRelay -and ($Hostile -or $HostileKind)) { Stop-Here "-Hostile sends bad messages on purpose, so it never goes through -RemoteRelay, beyond this machine" }
if ($NetPartner -and -not $Net) { Stop-Here "-NetPartner only means something with -Net" }
# Protocol version 2's draft through the relay (added 2026-11-09 by Melanikus (Fifth) for the AvP port; its key line went with #175, as the
# servers.cfg line proves the relay's keys, and every relay run is version 2 since #175 PR 3a)
if ($SecondCopy -and $Net -ne 'join' -and -not $Access) { Stop-Here "-SecondCopy is the joiner of a pair on this machine, or the second copy's own -Access run, so it needs -Net join or -Access" }
# The presses (added 2026-11-10 by Melanikus (Third) for the AvP port): each to Hudson's own words and ranges (hudson_plan.cpp), and only
# with the kind of run it belongs to. -cmatch, because Hudson's words are small letters and PowerShell's -match ignores case
if ($Access -and $Access -cnotmatch '\A(request|check|newid)\z') { Stop-Here "-Access is request, check or newid" }
if ($Access -and $Net) { Stop-Here "-Access stays at the menus to press the Online access page, so it's a run with no -Net" }
if ($Access -and -not $Relay) { Stop-Here "-Access asks the relays' list, so it needs -Relay, for the list's lines in the run's settings" }
if ($AccessWait -ne 0 -and $Access -cne 'check') { Stop-Here "-AccessWait says when Check again is pressed, so it needs -Access check" }
if ($AccessWait -ne 0 -and ($AccessWait -lt 61 -or $AccessWait -gt 3600)) { Stop-Here "-AccessWait is from 61 seconds, past the list's minute between calls, to 3600" }
if ($Answer -and $Answer -cnotmatch '\A(letin|no)\z') { Stop-Here "-Answer is letin or no" }
if ($Answer -and $Net -ne 'host') { Stop-Here "-Answer is the host's answer to a player asking to join, so it needs -Net host" }
if ($AnswerAfter -ne 0 -and -not $Answer) { Stop-Here "-AnswerAfter says when the host answers, so it needs -Answer" }
if ($AnswerAfter -ne 0 -and ($AnswerAfter -lt 1 -or $AnswerAfter -gt 15)) { Stop-Here "-AnswerAfter is from 1 to 15 seconds, well inside the joiner's 20 s wait" }
if ($FindGames -and $Net -ne 'join') { Stop-Here "-FindGames is a joiner's way in, through Find Games, so it needs -Net join" }
# The showcase's film (added 2026-11-10 by Melanikus (Third) for the AvP port): Hudson's own rules for film=menus and pagehold=
# (hudson_plan.cpp), checked here first so a run that would be refused never empties its folder
if ($FilmMenus -and -not $WhileInUse) { Stop-Here "-FilmMenus starts the film at the menus, and only a run with -WhileInUse has a film" }
if ($FilmMenus -and ($NoFilm -or $Record -eq 'numbers')) { Stop-Here "-FilmMenus asks for a film, and -NoFilm or -Record numbers says there's none" }
if ($PageHold -ne 0 -and -not $FilmMenus) { Stop-Here "-PageHold holds the menus' pages for the film, so it needs -FilmMenus" }
if ($PageHold -ne 0 -and ($PageHold -lt 1 -or $PageHold -gt 10)) { Stop-Here "-PageHold is from 1 to 10 seconds" }
# The yes page and the Host row (added 2026-11-10 by Melanikus (Third) for the AvP port): Hudson's own rules for yespage= and hostrow=
# (hudson_plan.cpp), and the Host row is on the page only when servers.cfg names a server
if ($YesPage -and $YesPage -cnotmatch '\A(yes|no)\z') { Stop-Here "-YesPage is yes or no" }
if ($YesPage -and $Access -cne 'request' -and $Access -cne 'check') { Stop-Here "-YesPage is the yes page the Online access row opens, so it needs -Access request or -Access check" }
if ($HostRow -and $HostRow -cnotmatch '\A(online|athome)\z') { Stop-Here "-HostRow is online or athome" }
if ($HostRow -and $Net -ne 'host') { Stop-Here "-HostRow is a host's way in, through the Multiplayer page's Host row, so it needs -Net host" }
if ($HostRow -and -not $Relay) { Stop-Here "-HostRow presses the Host row, which is on the page only when servers.cfg names a server, so it needs -Relay and -ServersLine" }

# Room on the run's disk, before anything there changes (Primary's read, 2026-11-09): a run keeps its pictures, maps, logs and film, and
# nothing else watches the disk. The game's own film stops at 1 GB left (hudson_video.h, kHudsonVideoDiskToLeave) and is at most 2 GB
# (kHudsonVideoMostBytes), so a run that may film needs both; one with -NoFilm needs the 1 GB only. A disk that won't say how much room it
# has is refused too: not knowing isn't room.
$mostFilmBytes = 2000000000
$leaveBytes = 1000000000
$freeBytes = -1
try { $freeBytes = [System.IO.DriveInfo]::new([System.IO.Path]::GetPathRoot($folder)).AvailableFreeSpace } catch { $freeBytes = -1 }
$needBytes = $leaveBytes + $(if ($NoFilm) { 0 } else { $mostFilmBytes })
if ($freeBytes -lt 0) { Stop-Here "the run's disk won't say how much room it has, so the run doesn't start" }
if ($freeBytes -lt $needBytes) { Stop-Here "the run's disk has $([math]::Floor($freeBytes / 1MB)) MB free, and a run needs $([math]::Floor($needBytes / 1MB)) MB, so it doesn't start" }

$partner = ''

if ($NetPartner)
{
    if (-not (Test-Path -LiteralPath $NetPartner -PathType Container)) { Stop-Here "-NetPartner isn't a folder: $NetPartner" }
    $partner = (Get-Item -LiteralPath $NetPartner).FullName
    if ($partner -eq $folder) { Stop-Here "-NetPartner has to be another folder, because each game keeps its log and its result in its own" }
    if ($partner -match 'steamapps') { Stop-Here "-NetPartner looks like a Steam folder" }
}

# -UserCopy: the user may be playing their own copy while a run shares the machine. A game running from that folder, or from anything inside it, is let be.
# PowerShell's names don't care about capitals, so this one is $userCopyFolder: a $userCopy here would be the setting itself, and emptying it would undo it.
$userCopyFolder = ''

if ($UserCopy)
{
    if (-not $WhileInUse) { Stop-Here "-UserCopy is a run sharing the machine with someone's game, so it needs -WhileInUse" }
    if (-not (Test-Path -LiteralPath $UserCopy -PathType Container)) { Stop-Here "-UserCopy isn't a folder: $UserCopy" }
    $userCopyFolder = (Get-Item -LiteralPath $UserCopy).FullName.TrimEnd('\')
    $runFolder = $folder.TrimEnd('\')

    # Neither may be inside the other, or one of this run's own copies could be taken for the user's
    if ($userCopyFolder -eq $runFolder -or $runFolder -like "$userCopyFolder\*" -or $userCopyFolder -like "$runFolder\*") { Stop-Here "-UserCopy and the run's own folder can't be the same, or one inside the other" }
}

$running = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -match '^(AvPx|debug_AvPx|AvP_Classic|AvP)$' -and -not ($partner -and $_.Path -like "$partner\*") -and -not ($NetSecondPartner -and $_.Path -like "$NetSecondPartner\*") -and -not ($userCopyFolder -and $_.Path -like "$userCopyFolder\*") })
# Which copy it is goes in the words, because a pair once found one that was neither of its own and nobody knew whose. Only a test folder is named: the user's own game could be anywhere,
# their Steam folder among them, and no log or output of ours ever names that.
if ($running.Count -gt 0)
{
    $where = $running[0].Path
    $whose = if ($where -and $where -like "$(Split-Path -Parent $folder)\AvP-*") { "the test folder $(Split-Path -Leaf (Split-Path -Parent $where))" } else { "outside the test folders" }
    Stop-Here "the game is already running: $($running[0].ProcessName), $whose, started $($running[0].StartTime)"
}

if (-not $TimingsDontMatter)
{
    $building = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -match '^(cl|link|MSBuild|fxc)$' })
    if ($building.Count -gt 0) { Stop-Here "$($building[0].ProcessName) is running, so the timings would mean nothing" }
}

# -statusoff keeps the tag off the screen, because it costs a little to draw, and that's not what's being measured. An older build that doesn't know it leaves it alone.
# These switches go before -hudson, because the game refuses anything after it that isn't name=value.
$staticSwitches = ''
switch ($Static)
{
    '' { }
    'scene' { $staticSwitches = ' -staticscene' }
    'compare' { $staticSwitches = ' -staticscene -staticcompare' }
    'drop' { $staticSwitches = ' -staticscene -staticcompare -staticcomparedrop' }
    default { Stop-Here "-Static has to be scene, compare or drop, and $Static isn't" }
}

# The game's own switches for trying out shadows, such as -shadowlights=8. Only those, because anything else could change what a run is without its result saying so. run.txt has them.
if ($ShadowSwitches -and $ShadowSwitches -notmatch '^-shadow[a-z]+(=\d+)?( -shadow[a-z]+(=\d+)?)*$') { Stop-Here "-ShadowSwitches only takes the game's -shadow switches, and $ShadowSwitches isn't" }
$tryingOut = if ($ShadowSwitches) { " $ShadowSwitches" } else { '' }
$alienWaysSwitch = if ($AlienWays) { ' -alienways' } else { '' }
if ($AlienWaysAim -ge 0 -and (-not $AlienWays -or $AlienWaysAim -gt 10000)) { Stop-Here "-AlienWaysAim is 0 to 10000 mm, and it needs -AlienWays" }
if ($AlienWaysAim -ge 0) { $alienWaysSwitch += " -alienwaysaim=$AlienWaysAim" }
if ($AlienWaysWatch -ge 0 -and (-not $AlienWays -or $AlienWaysWatch -gt 63)) { Stop-Here "-AlienWaysWatch is 0 to 63, a place in the guide's list, and it needs -AlienWays" }
if ($AlienWaysWatch -ge 0) { $alienWaysSwitch += " -alienwayswatch=$AlienWaysWatch" }
if ($AlienWaysSwitches -and (-not $AlienWays -or $AlienWaysSwitches -notmatch '^-alienways[a-z]+(=\d+)?( -alienways[a-z]+(=\d+)?)*$')) { Stop-Here "-AlienWaysSwitches only takes the game's -alienways... switches, each letters with an optional =number, and it needs -AlienWays" }
if ($AlienWaysSwitches) { $alienWaysSwitch += " $AlienWaysSwitches" }

# -Relay's word is one of the game's own switches, so it goes before -hudson: Hudson's own words (name=value) end at the next word that
# starts with -, and one in among them would cut them short
# An -Access run has the list's lines in its settings and no game to host or join, so it gets neither switch.
# A host with -Answer gets -askjoiners, which hosts as -relayhost does but asks about each joiner (relay_auto.h). -relayhost lets every
# joiner in without asking (relay_auto.cpp), and the game would never show the page -Answer presses, nor its check code: the nyc3 pair of
# 2026-11-10 had answer=letin and no host-asked.png for that reason (modified 2026-11-10 by Melanikus (Third) for the AvP port)
# A host with -HostRow gets neither, as it hosts through the page's rows, not by itself as the menus start; and a joiner with -Typed joins a
# game at home by its typed address on this machine, so it gets no -relayjoin either, though servers.cfg names a server (added 2026-11-10
# by Melanikus (Third) for the AvP port)
$relaySwitch = if (-not $Relay -or -not $Net) { '' } elseif ($Net -eq 'host' -and $HostRow) { '' } elseif ($Net -eq 'host' -and $Answer) { ' -askjoiners' } elseif ($Net -eq 'host') { ' -relayhost' } elseif ($Typed) { '' } else { ' -relayjoin' }
# -secondcopy is the game's own switch too, so it goes before -hudson as well (added 2026-11-09 by Melanikus (Fifth))
if ($SecondCopy) { $relaySwitch += ' -secondcopy' }
$arguments = "-dev -statusoff$staticSwitches$tryingOut$alienWaysSwitch$relaySwitch -hudson level=$Level frames=$Frames seconds=$Seconds idle=$IdleSeconds"
if ($MenuPictures) { $arguments += ' menus=pictures' }
if ($Rooms -gt 0) { $arguments += " rooms=$Rooms" }
if ($Views -gt 0) { $arguments += " views=$Views" }
if ($Hold -gt 0) { $arguments += " hold=$Hold" }
if ($HoldSeconds -gt 0) { $arguments += " holdseconds=$HoldSeconds" }
if ($TourFrom) { $arguments += " tourfrom=$TourFrom" }
if ($Nav -gt 0) { $arguments += " nav=$Nav" }
if ($Route) { $arguments += " route=$Route" }
if ($Pace) { $arguments += " pace=$Pace" }
if ($Map) { $arguments += " map=$Map" }
if ($FrameHold) { $arguments += " framehold=$FrameHold" }
if ($Lighting) { $arguments += " lighting=$Lighting" }
if ($Shadows) { $arguments += " shadows=$Shadows" }
if ($Record) { $arguments += " record=$Record" }
if ($Wall) { $arguments += " wall=$Wall" }
if ($WallRank -gt 0) { $arguments += " wallrank=$WallRank" }
if ($WallRoom) { $arguments += " wallroom=$WallRoom" }
if ($Compare) { $arguments += " compare=$Compare" }
if ($WhileInUse) { $arguments += " machine=shared" }
if ($Show) { $arguments += " show=yes" }
if ($Size) { $arguments += " size=$Size" }
if ($Vision) { $arguments += " vision=$Vision" }
if ($Break) { $arguments += " break=$Break" }
if ($NoFilm) { $arguments += " film=no" }
if ($FilmMenus) { $arguments += " film=menus" }
if ($PageHold -ne 0) { $arguments += " pagehold=$PageHold" }
if ($Net) { $arguments += " net=$Net" }
if ($Lamp) { $arguments += " lamp=$Lamp" }
if ($LampKind) { $arguments += " lampkind=$LampKind" }
if ($LampAt) { $arguments += " lampat=$LampAt" }
if ($LampSize) { $arguments += " lampsize=$LampSize" }
if ($Follow) { $arguments += " follow=$Follow" }
if ($WalkFrom) { $arguments += " walkfrom=$($WalkFrom -replace 'm', '-')" }
if ($WalkTo) { $arguments += " walkto=$($WalkTo -replace 'm', '-')" }
if ($LookAt) { $arguments += " lookat=$($LookAt -replace 'm', '-')" }
if ($Bright) { $arguments += " bright=yes" }
if ($Flare) { $arguments += " flare=yes" }
if ($Gun -gt 0) { $arguments += " gun=$Gun" }
if ($Species) { $arguments += " species=$Species" }
if ($DieAt -gt 0) { $arguments += " dieat=$DieAt" }
if ($Difficulty) { $arguments += " difficulty=$Difficulty" }
if ($Meets) { $arguments += " meets=yes" }
if ($LevelFilms) { $arguments += " levelfilms=yes" }
if ($Look) { $arguments += " look=$Look" }
if ($CustomMap) { $arguments += " custommap=$CustomMap" }
if ($Siege) { $arguments += " siege=$Siege" }
if ($SiegeFrom) { $arguments += " siegefrom=$SiegeFrom" }
if ($SiegeFar) { $arguments += " siegefar=$SiegeFar" }
if ($Screen -eq 'main') { $arguments += " screen=main" }
if ($Screen -eq 'second' -and $WindowsTogether -gt 1) { $arguments += " windowslot=$WindowSlot windowsof=$WindowsTogether" }
if ($Screen -eq 'second' -and $WindowsTogether -eq 2 -and $WindowBig -ge 0) { $arguments += " windowbig=$WindowBig" }
if ($FilmSkip -gt 0) { $arguments += " filmskip=$FilmSkip" }
if ($ListPlace -ge 0) { $arguments += " listplace=$ListPlace" }
if ($Class) { $arguments += " class=$Class" }
if ($GunAlien) { $arguments += " gunalien=yes" }
if ($Play) { $arguments += " play=yes" }
if ($PlayGun -gt 0) { $arguments += " playgun=$PlayGun" }
# The game checks it again (stepseconds=, hudson_plan.cpp); a number outside its range would only be refused there, after the folders are emptied
if ($StepSeconds -ne 0 -and ($StepSeconds -lt 10 -or $StepSeconds -gt 600)) { Stop-Here "-StepSeconds is a whole number of seconds, from 10 to 600" }
if ($StepSeconds -ne 0 -and -not $Play) { Stop-Here "-StepSeconds is how long a step of -Play may take, so it needs -Play" }
if ($StepSeconds -gt 0) { $arguments += " stepseconds=$StepSeconds" }
if ($Coop) { $arguments += " coop=yes" }
if ($Access) { $arguments += " access=$Access" }
if ($AccessWait -ne 0) { $arguments += " accesswait=$AccessWait" }
if ($Answer) { $arguments += " answer=$Answer" }
if ($AnswerAfter -ne 0) { $arguments += " answerafter=$AnswerAfter" }
if ($FindGames) { $arguments += " findgames=yes" }
if ($YesPage) { $arguments += " yespage=$YesPage" }
if ($HostRow) { $arguments += " hostrow=$HostRow" }
if ($BotFight) { $arguments += " botfight=$BotFight" }
if ($NoPictures) { $arguments += " pictures=no" }
# remoterelay= keeps a network run's game traffic to that one relay, and Hudson takes it only with net=. An -Access run through a server
# beyond this machine has no game traffic: it calls the list, at the address its settings name, so it gets the settings' lines alone
if ($RemoteRelay -and $Net) { $arguments += " remoterelay=$RemoteRelay" }
if ($KillKind) { $arguments += " killkind=$KillKind killat=$($KillAt -replace 'm', '-') killer=$Killer" }
if ($BotStay) { $arguments += " botstay=yes" }
if ($BotOwn) { $arguments += " botown=yes" }
if ($GhostLog) { $arguments += " ghostlog=yes" }
if ($BotWeapon) { $arguments += " botweapon=$BotWeapon" }
if ($DeathWay -ge 0) { $arguments += " deathway=$DeathWay" }
if ($RateTest) { $arguments += " ratetest=$RateTest" }
if ($VsyncOff) { $arguments += " vsync=off" }
if ($BotSingle) { $arguments += " botsingle=yes" }
if ($Uncapped -and $PSBoundParameters.ContainsKey('Fps')) { Stop-Here "-Uncapped and -Fps ask for different things" }
if ($Uncapped) { $arguments += " fps=uncapped" } else { $arguments += " fps=$Fps" }
if ($NoFriendlyFire) { $arguments += " friendlyfire=off" }
if ($CoopWalk) { $arguments += " coopwalk=yes" }
if ($Hostile) { $arguments += " hostile=yes" }
if ($HostileKind) { $arguments += " hostile=$HostileKind" }
if ($Typed) { $arguments += " typed=yes" }
if ($CoopEnd -gt 0) { $arguments += " coopend=$CoopEnd" }
if ($GunMarine) { $arguments += " gunmarine=yes" }
if ($Strategies) { $arguments += " strategies=$Strategies" }
if ($Strategy) { $arguments += " strategy=$Strategy" }
if ($StrategySeconds -gt 0) { $arguments += " strategyseconds=$StrategySeconds" }
if ($StrategyLook) { $arguments += " strategylook=$StrategyLook" }
if ($Combat) { $arguments += " combat=$Combat" }
if ($Aliens -gt 0) { $arguments += " aliens=$Aliens" }
if ($Watch) { $arguments += " watch=$Watch" }
if ($HoldFire) { $arguments += " holdfire=yes" }
if ($WatchFrom) { $arguments += " watchfrom=$WatchFrom" }
if ($Together -gt 0) { $arguments += " together=$Together" }
if ($Every) { $arguments += " every=$Every" }
if ($AlienAt) { $arguments += " alienat=$($AlienAt -replace 'm', '-')" }
if ($StandAt) { $arguments += " standat=$($StandAt -replace 'm', '-')" }
Write-Host "run-hudson: $exePath $arguments"

# Taken before it starts, so they're of what actually ran.
$exeFingerprint = (Get-FileHash -LiteralPath $exePath -Algorithm SHA256).Hash
$settingsFingerprint = (Get-FileHash -LiteralPath $settings -Algorithm SHA256).Hash

# Nothing is left from the run before. A run that crashes writes nothing, and an old result would look like this run's. Then the settings, which the game finds there.
$refusal = Clear-HudsonRunFolder $folder
if ($refusal) { Stop-Here "the run's folder can't be cleared: $refusal" }

# -SecondCopy: the game keeps the second copy's files in a folder of its own name (save_folder.h, kSaveFolderSecondCopyName), inside the
# run's folder too, so everything here is read from there (modified 2026-11-09 by Melanikus (Fifth) for the AvP port)
$saves = Join-Path $folder $(if ($SecondCopy) { 'HudsonRun\My Games\Aliens versus Predator (second copy)' } else { 'HudsonRun\My Games\Aliens versus Predator' })
New-Item -ItemType Directory -Force $saves | Out-Null
Copy-Item -LiteralPath $settings -Destination (Join-Path $saves 'AliensVsPredator.cfg')

# -Relay's server (modified 2026-11-10 by Melanikus for the AvP port, #175, in place of the -dev settings RelayServer, RelayListPort,
# RelayListPin, RelayServerName and RelayKey): the one servers.cfg line the game reads, beside bin\, written only into a copy made for
# Hudson's runs (relay-words.ps1). A run without -Relay takes away the one a run left, so it reaches no server. The line was checked to its
# shape above. With -RemoteRelay, the game is told the line's address with remoterelay= too, and uses that relay alone (networking.cpp,
# TheRemoteRelay).
$serversRefusal = Set-HudsonServersFile $folder $ServersLine
if ($serversRefusal) { Stop-Here $serversRefusal }

# Its own folder, because the game finds its data from the folder it starts in. The game's own limit should always come first: this one is for a game too stuck to end itself.
$process = Start-Process -FilePath $exePath -ArgumentList $arguments -WorkingDirectory $folder -PassThru

$limitMilliseconds = ($Seconds + 60) * 1000
$exitCode = 101

if ($process.WaitForExit($limitMilliseconds))
{
    $exitCode = $process.ExitCode
}
else
{
    Write-Host 'run-hudson: it went past even its own limit, so this is ending it. That is a finding.'
    Stop-Process -Id $process.Id -Force
}

$kept = Join-Path $folder ('runs\' + (Get-Date -Format 'yyyy-MM-dd-HHmmss'))
New-Item -ItemType Directory -Force $kept | Out-Null
$log = Join-Path $saves 'avp_log.txt'
$logErrors = if (Test-Path -LiteralPath $log) { @(Select-String -LiteralPath $log -SimpleMatch 'Error: ').Count } else { 'no log' }

# The pictures go with the run they're from, because the next run clears them. Each has its fingerprint in run.txt, so two can be seen to be the same without opening them.
$pictures = Join-Path $saves 'pictures'
$pictureLines = @()

if (Test-Path -LiteralPath $pictures -PathType Container)
{
    $keptPictures = Join-Path $kept 'pictures'
    New-Item -ItemType Directory -Force $keptPictures | Out-Null

    foreach ($picture in @(Get-ChildItem -LiteralPath $pictures -File | Sort-Object Name))
    {
        Copy-Item -LiteralPath $picture.FullName -Destination $keptPictures
        $pictureLines += "picture=$($picture.Name) sha256=$((Get-FileHash -LiteralPath $picture.FullName -Algorithm SHA256).Hash)"
    }
}

# A film goes with the run too. It's moved rather than copied, because it can be 2 GB, and its fingerprint goes in run.txt like a picture's.
$filmKept = ''

if (Test-Path -LiteralPath $saves -PathType Container)
{
    foreach ($film in @(Get-ChildItem -LiteralPath $saves -Filter 'hudson-video-*.mp4' -File))
    {
        Move-Item -LiteralPath $film.FullName -Destination $kept
        $filmKept = Join-Path $kept $film.Name
        $pictureLines += "video=$($film.Name) sha256=$((Get-FileHash -LiteralPath (Join-Path $kept $film.Name) -Algorithm SHA256).Hash)"
    }
}

# What rate this run really reached, beside what it asked for, so nobody reads a result as if it went at the rate asked.
$rateLines = @()
$resultHere = Join-Path $saves 'hudson-result.txt'
if (Test-Path -LiteralPath $resultHere) { $rateLines = @(Get-Content -LiteralPath $resultHere | Where-Object { $_ -like 'frames_each_second_asked=*' }) }

Set-Content -LiteralPath (Join-Path $kept 'run.txt') -Encoding ASCII -Value (@("exe=$Exe", "sha256=$exeFingerprint", "settings_sha256=$settingsFingerprint", "arguments=$arguments", "servers_line=$ServersLine", "exit=$exitCode", "log_errors=$logErrors") + $rateLines + $pictureLines)

# What a walk keeps of its map and paths. Only a walk has them, so it isn't news if they're not there.
foreach ($name in @('hudson-map.txt', 'hudson-ways.txt', 'hudson-trail.txt'))
{
    $file = Join-Path $saves $name

    if (Test-Path -LiteralPath $file)
    {
        Move-Item -LiteralPath $file -Destination $kept
    }
}

foreach ($name in @('avp_log.txt', 'hudson-result.txt'))
{
    $file = Join-Path $saves $name

    if (Test-Path -LiteralPath $file)
    {
        Copy-Item -LiteralPath $file -Destination $kept
    }
    else
    {
        Write-Host "run-hudson: no $name"
    }
}

# Where the window went, for a run that had it moved: the game writes it as hudson-window.txt (hudson.cpp, kWindowFileName). This once named
# window-place.txt, which nothing writes, so it was never kept (Primary's read, 2026-11-09)
$windowNotes = Join-Path $saves 'hudson-window.txt'
if (Test-Path -LiteralPath $windowNotes) { Copy-Item -LiteralPath $windowNotes -Destination $kept }

$result = Join-Path $kept 'hudson-result.txt'

# The game said where it was putting the film, and this has moved it since, so the result has to say where it is now. It goes back in the same encoding it was read in, so that nothing else in the file changes.
if (Test-Path -LiteralPath $result)
{
    $filmLine = if ($filmKept) { "film=$filmKept" } else { 'film=none' }
    $lines = @(Get-Content -LiteralPath $result | ForEach-Object { if ($_ -like 'film=*') { $filmLine } else { $_ } })
    Set-Content -LiteralPath $result -Encoding Default -Value $lines
}

# A game of a pair run says how its smoke test went, and that goes in run.txt too, where a reader looks first. A run that wrote nothing can't count as a pass.
if ($Net)
{
    $smoke = if (Test-Path -LiteralPath $result) { @(Get-Content -LiteralPath $result | Where-Object { $_ -like 'smoke=*' }) | Select-Object -First 1 } else { $null }
    Add-Content -LiteralPath (Join-Path $kept 'run.txt') -Encoding ASCII -Value $(if ($smoke) { 'smoke: ' + $smoke.Substring(6) } else { 'smoke: failed: this run wrote nothing about it' })
}

if (Test-Path -LiteralPath $result) { Get-Content -LiteralPath $result | ForEach-Object { Write-Host "run-hudson: $_" } }

Write-Host "run-hudson: exit code $exitCode, $logErrors error lines in the log. Kept in $kept"
exit $exitCode
