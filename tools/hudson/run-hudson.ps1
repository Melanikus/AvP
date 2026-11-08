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

The window goes on the second screen, where it covers nothing anyone's using, and the game does that itself from the moment the window is made. -Screen main leaves it on the main screen, which is for comparing the two. With no second screen, it stays on the main one, and the log says so. The log's "the window is at" line says where it went. -WindowSlot and -WindowsTogether are for several games at once, and run-net-pair.ps1 sets them.

A run with -WhileInUse makes a film of itself, an mp4 kept with the run's other files, and the film= line of hudson-result.txt says where. -Record pictures keeps a film as well. -NoFilm or -Record numbers stops it. A film's pictures slow the frames, so a run that measures time has none, and -Record video, which asks for one by name, needs -TimingsDontMatter and -WhileInUse. Nothing deletes a film: one left behind by a run that was cut short goes to runs\films-left-behind when the next run clears the way.

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
  40       -Route asked for a route that isn't there, or that has something wrong with it
  31       -Lighting or -Shadows asked for something this machine can't draw
  32       -Wall asked for a wall this level hasn't
  33       -Strategy asked for one this level hasn't, or there's nowhere to stand to see it
  34       the strategy was asked to go on, and its state, or that of what he was looking at, never changed
  35       strategies were asked for in a network game
  36       -Combat couldn't make its alien, or there was nowhere in sight of him to put it
  37       the game stopped before the run had finished: its window was closed, or the graphics card failed
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
    [int]$Together = 0,     # 2 to 8: a watched fight has this many aliens at once, a few metres apart, each followed once a second. Needs -Watch alien
    [ValidateSet('', 'side', 'eyes')][string]$WatchFrom = '', # where a watched fight is filmed from: side (beside the alien, as without it) or eyes (his own eyes, turned to the alien)
    [string]$AlienAt = '', # x,y,z in the level's own mm, where an m stands for a minus sign: the fight's alien is made just above it
    [string]$StandAt = '', # x,y,z the same way: he stands there
    [switch]$AlienWays,    # the game's own -alienways: an alien that knows where he is takes the way the alien ways map finds, where that's in the build
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
    [int]$CoopEnd = 0
)
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'clear-run-folder.ps1')

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
if ($NetPartner -and -not $Net) { Stop-Here "-NetPartner only means something with -Net" }

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
if ($running.Count -gt 0) { Stop-Here "the game is already running: $($running[0].ProcessName)" }

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

$arguments = "-dev -statusoff$staticSwitches$tryingOut$alienWaysSwitch -hudson level=$Level frames=$Frames seconds=$Seconds idle=$IdleSeconds"
if ($Rooms -gt 0) { $arguments += " rooms=$Rooms" }
if ($Views -gt 0) { $arguments += " views=$Views" }
if ($Hold -gt 0) { $arguments += " hold=$Hold" }
if ($HoldSeconds -gt 0) { $arguments += " holdseconds=$HoldSeconds" }
if ($TourFrom) { $arguments += " tourfrom=$TourFrom" }
if ($Nav -gt 0) { $arguments += " nav=$Nav" }
if ($Route) { $arguments += " route=$Route" }
if ($Pace) { $arguments += " pace=$Pace" }
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
if ($Coop) { $arguments += " coop=yes" }
if ($BotFight) { $arguments += " botfight=$BotFight" }
if ($NoPictures) { $arguments += " pictures=no" }
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
if ($AlienAt) { $arguments += " alienat=$($AlienAt -replace 'm', '-')" }
if ($StandAt) { $arguments += " standat=$($StandAt -replace 'm', '-')" }
Write-Host "run-hudson: $exePath $arguments"

# Taken before it starts, so they're of what actually ran.
$exeFingerprint = (Get-FileHash -LiteralPath $exePath -Algorithm SHA256).Hash
$settingsFingerprint = (Get-FileHash -LiteralPath $settings -Algorithm SHA256).Hash

# Nothing is left from the run before. A run that crashes writes nothing, and an old result would look like this run's. Then the settings, which the game finds there.
$refusal = Clear-HudsonRunFolder $folder
if ($refusal) { Stop-Here "the run's folder can't be cleared: $refusal" }

$saves = Join-Path $folder 'HudsonRun\My Games\Aliens versus Predator'
New-Item -ItemType Directory -Force $saves | Out-Null
Copy-Item -LiteralPath $settings -Destination (Join-Path $saves 'AliensVsPredator.cfg')

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

Set-Content -LiteralPath (Join-Path $kept 'run.txt') -Encoding ASCII -Value (@("exe=$Exe", "sha256=$exeFingerprint", "settings_sha256=$settingsFingerprint", "arguments=$arguments", "exit=$exitCode", "log_errors=$logErrors") + $rateLines + $pictureLines)

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
