// Modified 2026-11-09 by Melanikus for the AvP port:
//   join-films' four checks moved to tools/hudson/join_films_reads.cpp, because nothing in the game calls them, and the clock's look to
//   hudson_plan.h, because the stamper here and the reader there both need it
//   access=, accesswait=, answer= and answerafter= (hudson_plan.h)
// Modified 2026-11-07 by Melanikus for the AvP port:
//   a siege's players can't be hurt at all, which the game's own protection doesn't manage, since it ends whenever they fire
//   a film's clock can be read back from its corner, to line two films up
//   join-films reads its seconds strictly, and checks a film's rows fit in their buffer before it reads a picture
// Modified 2026-11-06 by Melanikus for the AvP port:
//   siege= has both players of a pair stand in a room against every kind of enemy the host can make, and a film of it has a clock on it
//   every game of a pair run starts with a smoke test, and a fight's idle player looks at the fight
//   a run's window goes on the second screen by the game's own doing, with a part of it for each game
//   killkind=, killat= and killer= have one machine really kill a level's own named NPC
//   a run is held to 60 frames a second unless it asks for another rate, or for none, by name
//   levelfilms= and filmskip= play a level's own films in a run, and stop one partway
//   listplace=, meets=, dieat=, difficulty=, and species=alien
//   coop=yes plays a net= run as a campaign, and coopend= reaches each level's end after so many seconds
//   gunmarine=yes stands a marine holding that gun in front of him on a gun= run
//   gunalien=yes makes an alien in front of him on a gun= run
//   class= starts a gun= run as a skirmish as that marine class
//   gun= tries a marine's gun
//   species=predator lets a gun= run play as a Predator, for trying the staff
//   botfight= has the host make an NPC beside the joiner in a network test
//   friendlyfire=off has the host of a network test turn friendly fire off
// What a Hudson run has been asked to do, read from the command line. Written 2026-11-04 by Melanikus for the AvP port. See hudson_plan.h.

#include "hudson_plan.h"
#include "marine_classes.h"
#include "command_line_word.h"
#include "shoulder_lamp.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

const char *const kHudsonRefusedNotNameAndValue = "a word after -hudson isn't name=value";
const char *const kHudsonRefusedUnknownName = "a word after -hudson has a name it doesn't know";
const char *const kHudsonRefusedAskedTwice = "something after -hudson is there twice";
const char *const kHudsonRefusedNoLevel = "level= is missing";
const char *const kHudsonRefusedLevelTooLong = "level= is too long";
const char *const kHudsonRefusedLevelCharacter = "level= has a character no level's name has";
const char *const kHudsonRefusedNoFrames = "frames= is missing";
const char *const kHudsonRefusedFramesNotANumber = "frames= isn't a number";
const char *const kHudsonRefusedFramesOutOfRange = "frames= is out of its range";
const char *const kHudsonRefusedSecondsNotANumber = "seconds= isn't a number";
const char *const kHudsonRefusedSecondsOutOfRange = "seconds= is out of its range";
const char *const kHudsonRefusedIdleNotANumber = "idle= isn't a number";
const char *const kHudsonRefusedIdleOutOfRange = "idle= is out of its range";
const char *const kHudsonRefusedRoomsNotANumber = "rooms= isn't a number";
const char *const kHudsonRefusedRoomsOutOfRange = "rooms= is out of its range";
const char *const kHudsonRefusedViewsNotANumber = "views= isn't a number";
const char *const kHudsonRefusedViewsOutOfRange = "views= is out of its range";
const char *const kHudsonRefusedViewsWithoutRooms = "views= means nothing without rooms=";
const char *const kHudsonRefusedHoldNotANumber = "hold= isn't a number";
const char *const kHudsonRefusedHoldOutOfRange = "hold= is out of its range";
const char *const kHudsonRefusedNavNotANumber = "nav= isn't a number";
const char *const kHudsonRefusedNavOutOfRange = "nav= is out of its range";
const char *const kHudsonRefusedNavWithoutOneRoom = "nav= needs a marine who starts in one room: rooms=1";
const char *const kHudsonRefusedNavWithWall = "nav= walks away, so it can't go with wall=";
const char *const kHudsonRefusedRouteEmpty = "route= is empty";
const char *const kHudsonRefusedRouteTooLong = "route= is too long";
const char *const kHudsonRefusedRouteCharacter = "route= can only have letters, digits, - and _";
const char *const kHudsonRefusedRouteWithoutOneRoom = "route= needs a marine who starts in one room: rooms=1";
const char *const kHudsonRefusedRouteWithWall = "route= walks away, so it can't go with wall=";
const char *const kHudsonRefusedPaceUnknown = "pace= can only be walk or run";
const char *const kHudsonRefusedMapUnknown = "map= can only be grid or rays";
const char *const kHudsonRefusedMapWithoutWalking = "map= is how he finds his way, so it needs nav= or route=";
const char *const kHudsonRefusedPaceWithoutWalking = "pace= is how fast he walks, so it needs nav= or route=";
const char *const kHudsonRefusedFrameHoldNotOff = "framehold= can only be off";
const char *const kHudsonRefusedFrameHoldWithoutWalking = "framehold= is about a walk, so it needs nav= or route=";
const char *const kHudsonRefusedHoldWithoutRooms = "hold= means nothing without rooms=";
const char *const kHudsonRefusedCoopNotYes = "coop= can only be yes";
const char *const kHudsonRefusedCoopEndNotANumber = "coopend= is how many seconds into each level its end is reached, from 1 to 3600";
const char *const kHudsonRefusedCoopWithoutNet = "coop= and coopend= are for a net= run";
const char *const kHudsonRefusedVsync = "vsync= can only be off";
const char *const kHudsonRefusedBotWeapon = "botweapon= can only be blades or plasma";
const char *const kHudsonRefusedDeathWay = "deathway= has to be a number from 0 to 99";
const char *const kHudsonRefusedRateTest = "ratetest= can only be fall or flame";
const char *const kHudsonRefusedAccessWord = "access= can only be request, check or newid";
const char *const kHudsonRefusedAccessWithNet = "access= stays at the menus to press the Online access page, so it can't be a net= run";
const char *const kHudsonRefusedAccessWaitNotANumber = "accesswait= is a number of seconds";
const char *const kHudsonRefusedAccessWaitOutOfRange = "accesswait= is from 61 seconds, past the list's minute between calls, to 3600";
const char *const kHudsonRefusedAccessWaitWithoutCheck = "accesswait= says when Check again is pressed, so it needs access=check";
const char *const kHudsonRefusedAnswerWord = "answer= can only be letin or no";
const char *const kHudsonRefusedAnswerWithoutHost = "answer= is the host's answer to a player asking to join, so it needs net=host";
const char *const kHudsonRefusedAnswerAfterNotANumber = "answerafter= is a number of seconds";
const char *const kHudsonRefusedAnswerAfterOutOfRange = "answerafter= is from 1 to 15 seconds, well inside the joiner's 20 s wait";
const char *const kHudsonRefusedAnswerAfterWithoutAnswer = "answerafter= says when the host answers, so it needs answer=";
const char *const kHudsonRefusedFindGamesWord = "findgames= can only be yes";
const char *const kHudsonRefusedFindGamesWithoutJoin = "findgames= is a joiner's way in, through Find Games, so it needs net=join";
const char *const kHudsonRefusedPicturesWord = "pictures= can only be no, and only on a net= run";
const char *const kHudsonRefusedKillWords = "killkind= (marine, predator, xenoborg or queen), killat=x,y,z and killer= (host or join) go together, on a net= run with coop=yes";
const char *const kHudsonRefusedFramesPerSecond = "fps= is a number from 10 to 1000, or uncapped";
const char *const kHudsonRefusedBotFightUnknown = "botfight= is alien, marine, flame, predator, plasma, xenoborg, queen, facehugger or sentry";
const char *const kHudsonRefusedBotFightWithoutCoop = "botfight= is for a net= run with coop=yes";
const char *const kHudsonRefusedFriendlyFireNotOff = "friendlyfire= can only be off";
const char *const kHudsonRefusedFriendlyFireWithoutCoop = "friendlyfire= is for a net= run with coop=yes";
const char *const kHudsonRefusedLightingUnknown = "lighting= isn't gouraud, perpixel, normals or relief";
const char *const kHudsonRefusedShadowsUnknown = "shadows= isn't on or off";
const char *const kHudsonRefusedRecordUnknown = "record= isn't numbers, pictures or video";
const char *const kHudsonRefusedLampUnknown = "lamp= isn't on, off or blink";
const char *const kHudsonRefusedLampSpoilsComparing = "the lamp changes what's on the screen, so a run that compares it, with vision= or compare=, can't have lamp=on or lamp=blink";
const char *const kHudsonRefusedLampKindUnknown = "lampkind= isn't real or painted";
const char *const kHudsonRefusedLampAtUnknown = "lampat= isn't shoulder or wide";
const char *const kHudsonRefusedLampWordsWithoutReal = "lampat= is about the real lamp, so it needs lampkind=real";
const char *const kHudsonRefusedVideoMeasuresTime = "record=video slows the frames it records, so a run that measures time can't have it: it needs machine=shared";
const char *const kHudsonRefusedBreakVideoWithoutVideo = "break=video breaks the film, so the run has to have one";
const char *const kHudsonRefusedNetUnknown = "net= isn't host or join";
const char *const kHudsonRefusedNetMeasuresTime = "net= is two games at once on one machine, so it can't measure time: it needs machine=shared";
const char *const kHudsonRefusedNetNotAlone = "net= can't go with rooms=, nav=, route=, strategies=, strategy=, wall=, vision=, compare= or combat=, which all need a game to themselves";
const char *const kHudsonRefusedFilmUnknown = "film= can only say no: a run that shares the machine has a film anyway, and one that measures time can't";
const char *const kHudsonRefusedFilmBothWays = "record=video asks for a film and film=no says not to";
const char *const kHudsonRefusedPicturesWithoutRooms = "record=pictures means nothing without rooms=, a gun or a walk";
const char *const kHudsonRefusedWallEmpty = "wall= has no name";
const char *const kHudsonRefusedWallTooLong = "wall= is too long";
const char *const kHudsonRefusedWallCharacter = "wall= has a character no picture's name has";
const char *const kHudsonRefusedWallWithoutRooms = "wall= means nothing without rooms=";
const char *const kHudsonRefusedWallRankNotANumber = "wallrank= isn't a number";
const char *const kHudsonRefusedWallRankOutOfRange = "wallrank= is out of its range";
const char *const kHudsonRefusedWallRankWithoutWall = "wallrank= means nothing without wall=";
const char *const kHudsonRefusedWallRoomEmpty = "wallroom= has no name";
const char *const kHudsonRefusedWallRoomTooLong = "wallroom= is too long";
const char *const kHudsonRefusedWallRoomCharacter = "wallroom= has a character no room's name has";
const char *const kHudsonRefusedWallRoomWithoutWall = "wallroom= means nothing without wall=";
const char *const kHudsonRefusedHoldSecondsNotANumber = "holdseconds= isn't a number";
const char *const kHudsonRefusedHoldSecondsOutOfRange = "holdseconds= is out of its range";
const char *const kHudsonRefusedHoldTwoWays = "hold= and holdseconds= are two answers to one question";
const char *const kHudsonRefusedTourFromEmpty = "tourfrom= has no name";
const char *const kHudsonRefusedTourFromTooLong = "tourfrom= is too long";
const char *const kHudsonRefusedTourFromCharacter = "tourfrom= has a character no room's name has";
const char *const kHudsonRefusedTourFromWithoutATour = "tourfrom= means nothing without more than one room";
const char *const kHudsonRefusedSteamNotYes = "steam= can only be yes";
const char *const kHudsonRefusedOneJobAtATime = "strategy=, combat=, nav= and more than one room each take over what he does, so a run can only ask for one of them";
const char *const kHudsonRefusedBrightNotYes = "bright= can only be yes";
const char *const kHudsonRefusedBrightWithoutVision = "bright= is for seeing what vision= finds, so it needs vision=";
const char *const kHudsonRefusedGunNotASlot = "gun= has to be a weapon slot, from 1 to 14, or 15 for each in turn";
const char *const kHudsonRefusedSpeciesNotPredator = "species= can only be predator or alien";
const char *const kHudsonRefusedSpeciesWithoutGun = "species= is for trying a gun in his own hands, so it needs gun=";
const char *const kHudsonRefusedGunAlienNotYes = "gunalien= can only be yes";
const char *const kHudsonRefusedGunMarineNotYes = "gunmarine= can only be yes";
const char *const kHudsonRefusedGunMarineWithoutGun = "gunmarine= shows the model for a gun, so it needs gun=";
const char *const kHudsonRefusedPlayNotYes = "play= can only be yes";
const char *const kHudsonRefusedPlayWithSomethingElse = "play= is him playing the level himself, so it can't go with rooms=, gun= or botfight=";
const char *const kHudsonRefusedPlayNetWithoutCoop = "play= with net= is two Hudsons playing a level of a campaign together, so it needs coop=yes";
const char *const kHudsonRefusedGunAlienWithoutGun = "gunalien= makes something for a gun to hit, so it needs gun=";
const char *const kHudsonRefusedMeetsNotYes = "meets= can only be yes";
const char *const kHudsonRefusedDieAtNotANumber = "dieat= isn't a number of seconds from 1 to 3600";
const char *const kHudsonRefusedLevelFilmsNotYes = "levelfilms= can only be yes";
const char *const kHudsonRefusedScreenWord = "screen= is second or main";
const char *const kHudsonRefusedWindowSlot = "windowslot= is a number from 0 to 3, and less than windowsof=";
const char *const kHudsonRefusedWindowsOf = "windowsof= is a number from 1 to 4";
const char *const kHudsonRefusedWindowBig = "windowbig= is the number of the window with the big part of the second screen, and it needs windowsof=2";
const char *const kHudsonRefusedWindowPartsOnMain = "windowslot= and windowsof= share out the second screen, so they mean nothing with screen=main";
const char *const kHudsonRefusedLookNotYesOrNo = "look= is yes or no";
const char *const kHudsonRefusedSiegeEmpty = "siege= has no room name";
const char *const kHudsonRefusedSiegeFromEmpty = "siegefrom= has no room name";
const char *const kHudsonRefusedSiegeFarEmpty = "siegefar= has no room name";
const char *const kHudsonRefusedSiegeFarTooLong = "siegefar= has a room name that's too long";
const char *const kHudsonRefusedCustomMapEmpty = "custommap= has no map name";
const char *const kHudsonRefusedCustomMapName = "custommap= isn't a name a custom map can have (see custom_map_name.h)";
const char *const kHudsonRefusedSiegeFarCharacter = "siegefar= has a character a room name can't have here: write a space as _";
const char *const kHudsonRefusedSiegeFarWithoutSiege = "siegefar= says where a siege's xenoborgs come from, so it needs siege=";
const char *const kHudsonRefusedSiegeFromTooLong = "siegefrom= has a room name that's too long";
const char *const kHudsonRefusedSiegeFromCharacter = "siegefrom= has a character a room name can't have here: write a space as _";
const char *const kHudsonRefusedSiegeFromWithoutSiege = "siegefrom= says where a siege's enemies come from, so it needs siege=";
const char *const kHudsonRefusedRemoteRelayEmpty = "remoterelay= has no server's name";
const char *const kHudsonRefusedRemoteRelayTooLong = "remoterelay= has a name longer than a server's can be";
const char *const kHudsonRefusedRemoteRelayCharacter = "remoterelay= has a character a server's name can't have here: small letters, digits, - and . only";
const char *const kHudsonRefusedRemoteRelayWithoutNet = "remoterelay= says which relay a network run may use, so it needs net=";
const char *const kHudsonRefusedRemoteRelayHostile = "remoterelay= takes a run beyond this machine, where the other end could be anybody's game, so it can't be a hostile= run";
const char *const kHudsonRefusedStepSeconds = "stepseconds= is a whole number of seconds, from 10 to 600";
const char *const kHudsonRefusedStepSecondsWithoutPlay = "stepseconds= is how long a step of play=yes may take, so it needs play=yes";
const char *const kHudsonRefusedSiegeTooLong = "siege= has a room name that's too long";
const char *const kHudsonRefusedSiegeCharacter = "siege= has a character a room name can't have here";
const char *const kHudsonRefusedSiegeNeedsAPair = "siege= is for a pair of games with coop=yes, and it's a fight of its own, so it has no other fight, kill, walk or hostile test";
const char *const kHudsonRefusedLookNeedsAPair = "look= is for a pair of games playing the campaign, with no walk or hostile test of its own";
const char *const kHudsonRefusedFilmSkipNotANumber = "filmskip= isn't a number of seconds from 1 to 3600";
const char *const kHudsonRefusedFilmSkipWithoutFilms = "filmskip= needs levelfilms=yes, or there's no film for it to stop";
const char *const kHudsonRefusedDifficultyUnknown = "difficulty= can only be easy, medium or hard";
const char *const kHudsonRefusedListPlaceNotANumber = "listplace= isn't a place on the list";
const char *const kHudsonRefusedListPlaceWithSpecies = "listplace= chooses the species along with the level, so it can't go with species=";
const char *const kHudsonRefusedListPlaceWithClass = "listplace= chooses what he plays, so it can't go with class=";
const char *const kHudsonRefusedListPlaceWithNet = "listplace= is a skirmish, which is one machine, so it can't go with net=";
const char *const kHudsonRefusedAlienWithGun = "gun= tries a marine's or a Predator's gun, so it can't go with species=alien";
const char *const kHudsonRefusedClassNotOne = "class= can only be shotgun, revolver or m79";
const char *const kHudsonRefusedClassWithoutGun = "class= is for trying a gun as that class, so it needs gun=";
const char *const kHudsonRefusedClassAsPredator = "class= is a marine's class, so it can't go with species=predator";
const char *const kHudsonRefusedGunWithAMarine = "gun= tries a gun in the player's own hands, so a run with a marine for the camera to follow can't do it too";
const char *const kHudsonRefusedFlareNotYes = "flare= can only be yes";
const char *const kHudsonRefusedFlareSpoilsComparing = "a flare changes what's on the screen, so a run that compares it, with vision= or compare=, can't have flare=";
const char *const kHudsonRefusedFlareWithoutHolds = "flare= throws one as each hold starts, so it needs a run that stands in rooms and looks round";
const char *const kHudsonRefusedBreakUnknown = "break= isn't prisms or video";
const char *const kHudsonRefusedBreakWithoutVision = "break= is for seeing that vision= notices, so it needs vision=";
const char *const kHudsonRefusedVisionUnknown = "vision= isn't check";
const char *const kHudsonRefusedVisionWithoutRooms = "vision= means nothing without rooms=";
const char *const kHudsonRefusedVisionWithoutPictures = "vision= reads the pictures only perpixel, normals and relief draw, so it needs lighting= to say one of them";
const char *const kHudsonRefusedVisionWithoutViews = "vision= looks once in each view he holds still for, so it doesn't go with compare=, strategy=, combat=, nav= or route=";
const char *const kHudsonRefusedSizeUnknown = "size= isn't 640x480, 1024x768, 1280x720, 1600x900, 1920x1080, 2560x1440 or 3440x1440";
const char *const kHudsonRefusedShowNotYes = "show= can only be yes";
const char *const kHudsonRefusedShowWithoutSharing = "show=yes takes the focus, so it needs machine=shared, or the first touch would end the run";
const char *const kHudsonRefusedMachineUnknown = "machine= isn't alone or shared";
const char *const kHudsonRefusedWallInManyRooms = "wall= is one place, so it can't go with more than one room";
const char *const kHudsonRefusedStrategiesUnknown = "strategies= isn't list";
const char *const kHudsonRefusedStrategiesWithoutRooms = "strategies= means nothing without rooms=";
const char *const kHudsonRefusedStrategyEmpty = "strategy= has no name";
const char *const kHudsonRefusedStrategyTooLong = "strategy= is too long";
const char *const kHudsonRefusedStrategyCharacter = "strategy= has a character this can't take: write a space as _";
const char *const kHudsonRefusedStrategyWithoutRooms = "strategy= means nothing without rooms=";
const char *const kHudsonRefusedStrategyWithWall = "strategy= and wall= each say where he stands";
const char *const kHudsonRefusedStrategyLookEmpty = "strategylook= has no name";
const char *const kHudsonRefusedStrategyLookTooLong = "strategylook= is too long";
const char *const kHudsonRefusedStrategyLookCharacter = "strategylook= has a character this can't take: write a space as _";
const char *const kHudsonRefusedStrategyLookWithoutStrategy = "strategylook= means nothing without strategy=";
const char *const kHudsonRefusedStrategySecondsNotANumber = "strategyseconds= isn't a number";
const char *const kHudsonRefusedStrategySecondsOutOfRange = "strategyseconds= is out of range";
const char *const kHudsonRefusedStrategySecondsWithoutStrategy = "strategyseconds= means nothing without strategy=";
const char *const kHudsonRefusedCompareUnknown = "compare= isn't relief";
const char *const kHudsonRefusedCompareWithoutPictures = "compare= means nothing without record=pictures";
const char *const kHudsonRefusedCombatUnknown = "combat= isn't alien";
const char *const kHudsonRefusedCombatWithoutRooms = "combat= means nothing without rooms=";
const char *const kHudsonRefusedCombatNotAlone = "combat= is one fight where he stands, so it doesn't go with wall=, compare=, hold=, holdseconds=, more than one room or more than one view";
const char *const kHudsonRefusedAliensNotANumber = "aliens= isn't a number";
const char *const kHudsonRefusedAliensOutOfRange = "aliens= is out of its range";
const char *const kHudsonRefusedAliensWithoutCombat = "aliens= means nothing without combat=";
const char *const kHudsonRefusedWatchUnknown = "watch= isn't alien";
const char *const kHudsonRefusedHoldFireNotYes = "holdfire= can only be yes";
const char *const kHudsonRefusedPlaceNotThreeNumbers = "alienat= and standat= are three whole numbers with commas between them, x,y,z in mm, as the level has them";
const char *const kHudsonRefusedWatchWordsWithoutCombat = "watch=, holdfire=, alienat= and standat= are for a fight, so they need combat=alien";
const char *const kHudsonRefusedHoldFireWithoutWatch = "holdfire= is for watching where an alien goes, so it needs watch=alien: in a fight he isn't watching, an alien nobody hurts is taken away as stalled";
const char *const kHudsonRefusedWatchWithAliens = "watch=alien follows one alien, and its first blow ends the run, so it doesn't go with aliens=";
const char *const kHudsonRefusedWatchFromUnknown = "watchfrom= is side or eyes";
const char *const kHudsonRefusedWatchFromWithoutWatch = "watchfrom= says where the camera watches the alien from, so it needs watch=alien";
const char *const kHudsonRefusedLampSizeNotANumber = "lampsize= is a number from 0.05 to 0.35, as LAMPSPOTSIZE takes it, written like 0.35";
const char *const kHudsonRefusedLampSizeWithoutLamp = "lampsize= is the size of the lamp's spot, so it needs lamp=on or lamp=blink";
const char *const kHudsonRefusedFollowName = "follow= is a name from the level: 1 to 59 letters, digits, _ (for a space) or -";
const char *const kHudsonRefusedWalkPlaceNotThreeNumbers = "walkfrom=, walkto= and lookat= are three whole numbers with commas between them, x,y,z in mm, as the level has them";
const char *const kHudsonRefusedWalkNeedsBoth = "walkfrom= and walkto= go together: one says where he starts, the other where he walks to";
const char *const kHudsonRefusedLookAtWithoutWalk = "lookat= is where he looks once he has walked, so it needs walkfrom= and walkto=";
const char *const kHudsonRefusedTogetherNotANumber = "together= is how many aliens at once, from 2 to 8";
const char *const kHudsonRefusedEveryNotASecond = "every= is seconds from 0.1 to 10, with at most one figure after the dot, like 0.5";
const char *const kHudsonRefusedEveryWithoutTogether = "every= is how long between the aliens of together=, so it needs together= or several places in alienat=";
const char *const kHudsonRefusedTooManyPlaces = "alienat= has at most 8 places, with / between them";
const char *const kHudsonRefusedPlacesWithoutWatch = "alienat= with more than one place is for the aliens of a watched fight, so it needs watch=alien";
const char *const kHudsonRefusedTogetherNotThePlaces = "together= and the number of places in alienat= say different numbers of aliens";
const char *const kHudsonRefusedTogetherWithoutWatch = "together= is how many aliens a watched fight has at once, so it needs watch=alien";
const char *const kHudsonRefusedWalkNotAlone = "walkfrom= has the player walk, so it doesn't go with combat=, strategy=, nav=, route= or rooms=: with rooms= the camera goes to a marine who isn't the player";

static bool IsSpace(char c)
{
	return c == ' ' || c == '\t';
}

// Is this word exactly this? A word doesn't have to end the line, so it's compared by length.
static bool WordIs(const char *word, size_t length, const char *wanted)
{
	return strlen(wanted) == length && strncmp(word, wanted, length) == 0;
}

// Only plain digits count, and few enough of them that the number can't go past what an int holds. There's no sign, no spaces, and nothing after them.
static bool ReadNumber(const char *text, size_t length, int *number);

// Three numbers with commas between them, any of which can be below zero, as a place in a level can be.
static bool ReadPlace(const char *text, size_t length, int *x, int *y, int *z)
{
	int *const parts[3] = { x, y, z };
	size_t at = 0;

	for (int i = 0; i < 3; i++)
	{
		bool const below = at < length && text[at] == '-';

		at += below ? 1 : 0;

		size_t const start = at;

		while (at < length && text[at] != ',')
		{
			at++;
		}

		if (!ReadNumber(text + start, at - start, parts[i]))
		{
			return false;
		}

		*parts[i] = below ? -*parts[i] : *parts[i];

		if (i < 2)
		{
			if (at >= length)
			{
				return false;
			}

			at++;
		}
	}

	return at == length;
}

static bool ReadNumber(const char *text, size_t length, int *number)
{
	if (length == 0 || length > 9)
	{
		return false;
	}

	int value = 0;

	for (size_t i = 0; i < length; i++)
	{
		if (text[i] < '0' || text[i] > '9')
		{
			return false;
		}

		value = value * 10 + (text[i] - '0');
	}

	*number = value;
	return true;
}

// Where -hudson is, as a word on its own. It's the same test as CommandLineHasWord, but this needs the place.
static const char *FindHudsonWord(const char *commandLine)
{
	static const char kWord[] = "-hudson";
	size_t const length = sizeof(kWord) - 1;

	if (!commandLine)
	{
		return 0;
	}

	// Looked for by place, not by a pointer strstr hands back, so every character read is a place inside the line: the one before the
	// word only when there is one, and the one after it only up to the line's end
	size_t const total = strlen(commandLine);

	for (size_t at = 0; at + length <= total; at++)
	{
		if (memcmp(commandLine + at, kWord, length) != 0)
		{
			continue;
		}

		bool const startsClean = at == 0 || IsSpace(commandLine[at - 1]);
		bool const endsClean = at + length == total || IsSpace(commandLine[at + length]);

		if (startsClean && endsClean)
		{
			return commandLine + at + length;
		}
	}

	return 0;
}

bool HudsonPlan_IsAskedFor(const char *commandLine)
{
	return CommandLineHasWord(commandLine, "-dev") && CommandLineHasWord(commandLine, "-hudson");
}

bool HudsonPlan_NameMatches(const char *levelName, const char *word)
{
	for (;; levelName++, word++)
	{
		char a = *levelName, b = *word;

		if (a == 0 || b == 0)
		{
			return a == b;
		}

		if (a >= 'a' && a <= 'z')
		{
			a = (char)(a - 'a' + 'A');
		}

		if (b >= 'a' && b <= 'z')
		{
			b = (char)(b - 'a' + 'A');
		}

		if (a != b && !(b == '_' && a == ' '))
		{
			return false;
		}
	}
}

const char *HudsonPlan_Read(const char *commandLine, HudsonPlan *plan)
{
	memset(plan, 0, sizeof(*plan));
	plan->deathWay = -1;
	plan->seconds = kHudsonDefaultSeconds;
	plan->framesPerSecond = kHudsonDefaultFramesPerSecond;
	plan->idleSeconds = kHudsonDefaultIdleSeconds;
	plan->views = kHudsonDefaultViews;
	plan->wallRank = 1;
	plan->holdFrames = kHudsonDefaultHoldFrames;
	plan->difficulty = 1;
	plan->windowsTogether = 1;
	plan->windowBig = -1;
	plan->width = kHudsonDefaultWidth;
	plan->height = kHudsonDefaultHeight;
	plan->strategySeconds = kHudsonDefaultStrategySeconds;
	plan->combatAliens = 1;
	plan->accessWaitSeconds = kHudsonDefaultAccessWaitSeconds;
	plan->answerAfterSeconds = kHudsonDefaultAnswerAfterSeconds;

	bool seenAccess = false, seenAccessWait = false, seenAnswer = false, seenAnswerAfter = false, seenFindGames = false;
	bool seenHold = false;
	bool seenNav = false, seenRoute = false, seenPace = false, seenFrameHold = false;
	bool seenGun = false;
	bool seenStepSeconds = false;
	bool seenPlayGun = false;
	bool seenSpecies = false;
	bool seenMeets = false;
	bool seenLevelFilms = false;
	bool seenLook = false;
	bool seenSiege = false;
	bool seenSiegeFrom = false;
	bool seenRemoteRelay = false;
	bool seenSiegeFar = false;
	bool seenCustomMap = false;
	bool seenScreen = false;
	bool seenWindowSlot = false;
	bool seenWindowsOf = false;
	bool seenWindowBig = false;
	bool seenDifficulty = false;
	bool seenGunAlien = false;
	bool seenGunMarine = false;
	bool seenClass = false;
	bool seenFilm = false, seenLamp = false, seenLampKind = false, seenLampAt = false, seenBright = false, seenFlare = false, seenBreak = false, seenVision = false, seenSize = false, seenShow = false, seenMachine = false, seenHoldSeconds = false, seenTourFrom = false, seenSteam = false;
	bool seenNet = false;
	bool seenMap = false;
	bool seenStrategies = false, seenStrategy = false, seenStrategySeconds = false, seenStrategyLook = false;

	bool seenWall = false, seenWallRank = false, seenWallRoom = false, seenCompare = false, seenCombat = false, seenAliens = false;
	bool seenWatch = false, seenHoldFire = false, seenAlienAt = false, seenStandAt = false, seenWatchFrom = false;
	bool seenLampSize = false, seenFollow = false, seenWalkFrom = false, seenWalkTo = false, seenLookAt = false, seenTogether = false, seenEvery = false;
	bool seenLevel = false, seenFrames = false, seenSeconds = false, seenIdle = false, seenRooms = false, seenViews = false, seenLighting = false, seenShadows = false, seenRecord = false;
	const char *place = commandLine ? FindHudsonWord(commandLine) : 0;

	while (place)
	{
		while (IsSpace(*place))
		{
			place++;
		}

		// the end of the line, or the next switch, is the end of what's for this one
		if (*place == 0 || *place == '-')
		{
			break;
		}

		size_t wordLength = 0;

		while (place[wordLength] != 0 && !IsSpace(place[wordLength]))
		{
			wordLength++;
		}

		const char *const equals = (const char *)memchr(place, '=', wordLength);

		if (!equals)
		{
			return kHudsonRefusedNotNameAndValue;
		}

		size_t const nameLength = (size_t)(equals - place);
		const char *const value = equals + 1;
		size_t const valueLength = wordLength - nameLength - 1;

		if (WordIs(place, nameLength, "level"))
		{
			if (seenLevel)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenLevel = true;

			if (valueLength > kHudsonLongestLevelName)
			{
				return kHudsonRefusedLevelTooLong;
			}

			for (size_t i = 0; i < valueLength; i++)
			{
				char const c = value[i];

				if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
				{
					return kHudsonRefusedLevelCharacter;
				}
			}

			memcpy(plan->level, value, valueLength);
			plan->level[valueLength] = 0;
		}
		else if (WordIs(place, nameLength, "frames"))
		{
			if (seenFrames)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenFrames = true;

			if (!ReadNumber(value, valueLength, &plan->frames))
			{
				return kHudsonRefusedFramesNotANumber;
			}

			if (plan->frames < 1 || plan->frames > kHudsonMostFrames)
			{
				return kHudsonRefusedFramesOutOfRange;
			}
		}
		else if (WordIs(place, nameLength, "seconds"))
		{
			if (seenSeconds)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenSeconds = true;

			if (!ReadNumber(value, valueLength, &plan->seconds))
			{
				return kHudsonRefusedSecondsNotANumber;
			}

			if (plan->seconds < kHudsonFewestSeconds || plan->seconds > kHudsonMostSeconds)
			{
				return kHudsonRefusedSecondsOutOfRange;
			}
		}
		else if (WordIs(place, nameLength, "idle"))
		{
			if (seenIdle)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenIdle = true;

			if (!ReadNumber(value, valueLength, &plan->idleSeconds))
			{
				return kHudsonRefusedIdleNotANumber;
			}

			if (plan->idleSeconds > kHudsonMostIdleSeconds)
			{
				return kHudsonRefusedIdleOutOfRange;
			}
		}
		else if (WordIs(place, nameLength, "rooms"))
		{
			if (seenRooms)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenRooms = true;

			if (!ReadNumber(value, valueLength, &plan->rooms))
			{
				return kHudsonRefusedRoomsNotANumber;
			}

			if (plan->rooms > kHudsonMostRooms)
			{
				return kHudsonRefusedRoomsOutOfRange;
			}
		}
		else if (WordIs(place, nameLength, "views"))
		{
			if (seenViews)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenViews = true;

			if (!ReadNumber(value, valueLength, &plan->views))
			{
				return kHudsonRefusedViewsNotANumber;
			}

			if (plan->views < 1 || plan->views > kHudsonMostViews)
			{
				return kHudsonRefusedViewsOutOfRange;
			}
		}
		else if (WordIs(place, nameLength, "hold"))
		{
			if (seenHold)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenHold = true;

			if (!ReadNumber(value, valueLength, &plan->holdFrames))
			{
				return kHudsonRefusedHoldNotANumber;
			}

			if (plan->holdFrames < kHudsonFewestHoldFrames || plan->holdFrames > kHudsonMostHoldFrames)
			{
				return kHudsonRefusedHoldOutOfRange;
			}
		}
		else if (WordIs(place, nameLength, "framehold"))
		{
			if (seenFrameHold)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenFrameHold = true;

			if (!WordIs(value, valueLength, "off"))
			{
				return kHudsonRefusedFrameHoldNotOff;
			}

			plan->frameHoldOff = true;
		}
		else if (WordIs(place, nameLength, "map"))
		{
			if (seenMap)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenMap = true;

			if (WordIs(value, valueLength, "grid"))
			{
				plan->mapGrid = true;
			}
			else if (!WordIs(value, valueLength, "rays"))
			{
				return kHudsonRefusedMapUnknown;
			}
		}
		else if (WordIs(place, nameLength, "pace"))
		{
			if (seenPace)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenPace = true;

			if (WordIs(value, valueLength, "run"))
			{
				plan->paceRun = true;
			}
			else if (!WordIs(value, valueLength, "walk"))
			{
				return kHudsonRefusedPaceUnknown;
			}
		}
		else if (WordIs(place, nameLength, "route"))
		{
			if (seenRoute)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenRoute = true;

			if (valueLength == 0)
			{
				return kHudsonRefusedRouteEmpty;
			}

			if (valueLength > kHudsonLongestRouteName)
			{
				return kHudsonRefusedRouteTooLong;
			}

			// It becomes part of a file's name, so nothing that could lead to another folder gets through
			for (size_t i = 0; i < valueLength; i++)
			{
				char const c = value[i];

				if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
				{
					return kHudsonRefusedRouteCharacter;
				}
			}

			memcpy(plan->route, value, valueLength);
			plan->route[valueLength] = 0;
		}
		else if (WordIs(place, nameLength, "nav"))
		{
			if (seenNav)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenNav = true;

			if (!ReadNumber(value, valueLength, &plan->navModules))
			{
				return kHudsonRefusedNavNotANumber;
			}

			if (plan->navModules < kHudsonFewestNavModules || plan->navModules > kHudsonMostNavModules)
			{
				return kHudsonRefusedNavOutOfRange;
			}
		}
		else if (WordIs(place, nameLength, "lighting"))
		{
			if (seenLighting)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenLighting = true;

			if (WordIs(value, valueLength, "gouraud"))
			{
				plan->lighting = kHudsonLightingGouraud;
			}
			else if (WordIs(value, valueLength, "perpixel"))
			{
				plan->lighting = kHudsonLightingPerPixel;
			}
			else if (WordIs(value, valueLength, "normals"))
			{
				plan->lighting = kHudsonLightingNormals;
			}
			else if (WordIs(value, valueLength, "relief"))
			{
				plan->lighting = kHudsonLightingRelief;
			}
			else
			{
				return kHudsonRefusedLightingUnknown;
			}
		}
		else if (WordIs(place, nameLength, "shadows"))
		{
			if (seenShadows)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenShadows = true;

			if (WordIs(value, valueLength, "on"))
			{
				plan->shadows = kHudsonShadowsOn;
			}
			else if (WordIs(value, valueLength, "off"))
			{
				plan->shadows = kHudsonShadowsOff;
			}
			else
			{
				return kHudsonRefusedShadowsUnknown;
			}
		}
		else if (WordIs(place, nameLength, "strategies"))
		{
			if (seenStrategies)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenStrategies = true;

			if (!WordIs(value, valueLength, "list"))
			{
				return kHudsonRefusedStrategiesUnknown;
			}

			plan->listStrategies = true;
		}
		else if (WordIs(place, nameLength, "strategy"))
		{
			if (seenStrategy)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenStrategy = true;

			if (valueLength == 0)
			{
				return kHudsonRefusedStrategyEmpty;
			}

			if (valueLength > kHudsonLongestStrategyName)
			{
				return kHudsonRefusedStrategyTooLong;
			}

			// Only ever compared with the names of the level's strategies, but nothing on the command line is believed.
			for (size_t i = 0; i < valueLength; i++)
			{
				char const c = value[i];

				if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
				{
					return kHudsonRefusedStrategyCharacter;
				}

				plan->strategy[i] = c;
			}

			plan->strategy[valueLength] = 0;
		}
		else if (WordIs(place, nameLength, "strategylook"))
		{
			if (seenStrategyLook)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenStrategyLook = true;

			if (valueLength == 0)
			{
				return kHudsonRefusedStrategyLookEmpty;
			}

			if (valueLength > kHudsonLongestStrategyName)
			{
				return kHudsonRefusedStrategyLookTooLong;
			}

			// Only ever compared with the names of the level's strategies, but nothing on the command line is believed.
			for (size_t i = 0; i < valueLength; i++)
			{
				char const c = value[i];

				if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
				{
					return kHudsonRefusedStrategyLookCharacter;
				}

				plan->strategyLook[i] = c;
			}

			plan->strategyLook[valueLength] = 0;
		}
		else if (WordIs(place, nameLength, "strategyseconds"))
		{
			if (seenStrategySeconds)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenStrategySeconds = true;

			if (!ReadNumber(value, valueLength, &plan->strategySeconds))
			{
				return kHudsonRefusedStrategySecondsNotANumber;
			}

			if (plan->strategySeconds < 1 || plan->strategySeconds > kHudsonMostStrategySeconds)
			{
				return kHudsonRefusedStrategySecondsOutOfRange;
			}
		}
		else if (WordIs(place, nameLength, "wall"))
		{
			if (seenWall)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenWall = true;

			if (valueLength == 0)
			{
				return kHudsonRefusedWallEmpty;
			}

			if (valueLength > kHudsonLongestWallName)
			{
				return kHudsonRefusedWallTooLong;
			}

			// Only ever compared with the names of the level's pictures, but nothing on the command line is believed.
			for (size_t i = 0; i < valueLength; i++)
			{
				char const c = value[i];

				if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
				{
					return kHudsonRefusedWallCharacter;
				}

				plan->wall[i] = c;
			}

			plan->wall[valueLength] = 0;
		}
		else if (WordIs(place, nameLength, "wallrank"))
		{
			if (seenWallRank)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenWallRank = true;

			if (!ReadNumber(value, valueLength, &plan->wallRank))
			{
				return kHudsonRefusedWallRankNotANumber;
			}

			if (plan->wallRank < 1 || plan->wallRank > kHudsonMostWallRank)
			{
				return kHudsonRefusedWallRankOutOfRange;
			}
		}
		else if (WordIs(place, nameLength, "wallroom"))
		{
			if (seenWallRoom)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenWallRoom = true;

			if (valueLength == 0)
			{
				return kHudsonRefusedWallRoomEmpty;
			}

			if (valueLength > kHudsonLongestWallName)
			{
				return kHudsonRefusedWallRoomTooLong;
			}

			// Only ever compared with the names of the level's rooms, but nothing on the command line is believed.
			for (size_t i = 0; i < valueLength; i++)
			{
				char const c = value[i];

				if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
				{
					return kHudsonRefusedWallRoomCharacter;
				}

				plan->wallRoom[i] = c;
			}

			plan->wallRoom[valueLength] = 0;
		}
		else if (WordIs(place, nameLength, "combat"))
		{
			if (seenCombat)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenCombat = true;

			if (!WordIs(value, valueLength, "alien"))
			{
				return kHudsonRefusedCombatUnknown;
			}

			plan->combatAlien = true;
		}
		else if (WordIs(place, nameLength, "watch"))
		{
			if (seenWatch)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenWatch = true;

			if (!WordIs(value, valueLength, "alien"))
			{
				return kHudsonRefusedWatchUnknown;
			}

			plan->watchAlien = true;
		}
		else if (WordIs(place, nameLength, "together"))
		{
			if (seenTogether)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenTogether = true;

			if (!ReadNumber(value, valueLength, &plan->watchTogether) || plan->watchTogether < 2 || plan->watchTogether > kHudsonMostTogether)
			{
				return kHudsonRefusedTogetherNotANumber;
			}
		}
		else if (WordIs(place, nameLength, "every"))
		{
			if (seenEvery)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenEvery = true;

			// Read by hand: one or two figures, and then perhaps a dot and one more, so 0.5 is 5 tenths. Nothing else, so no "1e9" and no comma for a dot.
			size_t const dot = (valueLength >= 3 && value[valueLength - 2] == '.') ? valueLength - 2 : valueLength;
			bool shaped = dot >= 1 && dot <= 2 && (dot == valueLength || isdigit((unsigned char)value[valueLength - 1]));
			int tenths = 0;

			for (size_t i = 0; shaped && i < dot; i++)
			{
				shaped = isdigit((unsigned char)value[i]) != 0;
				tenths = tenths * 10 + (value[i] - '0');
			}

			tenths = tenths * 10 + ((shaped && dot < valueLength) ? value[valueLength - 1] - '0' : 0);

			if (!shaped || tenths < 1 || tenths > 100)
			{
				return kHudsonRefusedEveryNotASecond;
			}

			plan->watchEveryMs = tenths * 100;
		}
		else if (WordIs(place, nameLength, "watchfrom"))
		{
			if (seenWatchFrom)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenWatchFrom = true;

			// side is the camera beside the alien (the same as not saying it); eyes is his own eyes, where he stands
			if (WordIs(value, valueLength, "eyes"))
			{
				plan->watchFromEyes = true;
			}
			else if (!WordIs(value, valueLength, "side"))
			{
				return kHudsonRefusedWatchFromUnknown;
			}
		}
		else if (WordIs(place, nameLength, "holdfire"))
		{
			if (seenHoldFire)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenHoldFire = true;

			if (!WordIs(value, valueLength, "yes"))
			{
				return kHudsonRefusedHoldFireNotYes;
			}

			plan->holdFire = true;
		}
		else if (WordIs(place, nameLength, "alienat"))
		{
			if (seenAlienAt)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenAlienAt = true;

			// One place, or several with / between them: the first is the watched alien's, the others are together='s, in order
			size_t start = 0;

			plan->alienAtPlaces = 0;

			while (start <= valueLength)
			{
				size_t end = start;

				while (end < valueLength && value[end] != '/')
				{
					end++;
				}

				if (plan->alienAtPlaces >= kHudsonMostTogether)
				{
					return kHudsonRefusedTooManyPlaces;
				}

				int *const x = plan->alienAtPlaces == 0 ? &plan->alienAtX : &plan->alienAtMore[plan->alienAtPlaces - 1][0];
				int *const y = plan->alienAtPlaces == 0 ? &plan->alienAtY : &plan->alienAtMore[plan->alienAtPlaces - 1][1];
				int *const z = plan->alienAtPlaces == 0 ? &plan->alienAtZ : &plan->alienAtMore[plan->alienAtPlaces - 1][2];

				if (!ReadPlace(value + start, end - start, x, y, z))
				{
					return kHudsonRefusedPlaceNotThreeNumbers;
				}

				plan->alienAtPlaces++;
				start = end + 1;
			}

			plan->alienAtSaid = true;
		}
		else if (WordIs(place, nameLength, "standat"))
		{
			if (seenStandAt)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenStandAt = true;

			if (!ReadPlace(value, valueLength, &plan->standAtX, &plan->standAtY, &plan->standAtZ))
			{
				return kHudsonRefusedPlaceNotThreeNumbers;
			}

			plan->standAtSaid = true;
		}
		else if (WordIs(place, nameLength, "follow"))
		{
			if (seenFollow)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenFollow = true;

			if (valueLength == 0 || valueLength > kHudsonLongestStrategyName)
			{
				return kHudsonRefusedFollowName;
			}

			// Only ever compared with the names of the level's own things, but nothing on the command line is believed
			for (size_t i = 0; i < valueLength; i++)
			{
				char const c = value[i];

				if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
				{
					return kHudsonRefusedFollowName;
				}

				plan->follow[i] = c;
			}
		}
		else if (WordIs(place, nameLength, "walkfrom") || WordIs(place, nameLength, "walkto") || WordIs(place, nameLength, "lookat"))
		{
			bool const from = WordIs(place, nameLength, "walkfrom");
			bool const to = WordIs(place, nameLength, "walkto");
			bool *const seen = from ? &seenWalkFrom : to ? &seenWalkTo : &seenLookAt;
			int *const x = from ? &plan->walkFromX : to ? &plan->walkToX : &plan->lookAtX;
			int *const y = from ? &plan->walkFromY : to ? &plan->walkToY : &plan->lookAtY;
			int *const z = from ? &plan->walkFromZ : to ? &plan->walkToZ : &plan->lookAtZ;

			if (*seen)
			{
				return kHudsonRefusedAskedTwice;
			}

			*seen = true;

			if (!ReadPlace(value, valueLength, x, y, z))
			{
				return kHudsonRefusedWalkPlaceNotThreeNumbers;
			}
		}
		else if (WordIs(place, nameLength, "aliens"))
		{
			if (seenAliens)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenAliens = true;

			if (!ReadNumber(value, valueLength, &plan->combatAliens))
			{
				return kHudsonRefusedAliensNotANumber;
			}

			if (plan->combatAliens < 1 || plan->combatAliens > kHudsonMostAliens)
			{
				return kHudsonRefusedAliensOutOfRange;
			}
		}
		else if (WordIs(place, nameLength, "holdseconds"))
		{
			if (seenHoldSeconds)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenHoldSeconds = true;

			if (!ReadNumber(value, valueLength, &plan->holdSeconds))
			{
				return kHudsonRefusedHoldSecondsNotANumber;
			}

			if (plan->holdSeconds < 1 || plan->holdSeconds > kHudsonMostHoldSeconds)
			{
				return kHudsonRefusedHoldSecondsOutOfRange;
			}
		}
		else if (WordIs(place, nameLength, "tourfrom"))
		{
			if (seenTourFrom)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenTourFrom = true;

			if (valueLength == 0)
			{
				return kHudsonRefusedTourFromEmpty;
			}

			if (valueLength > kHudsonLongestWallName)
			{
				return kHudsonRefusedTourFromTooLong;
			}

			// Only ever compared with the names of the level's rooms, but nothing on the command line is believed.
			for (size_t i = 0; i < valueLength; i++)
			{
				char const c = value[i];

				if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
				{
					return kHudsonRefusedTourFromCharacter;
				}

				plan->tourFrom[i] = c;
			}
		}
		else if (WordIs(place, nameLength, "steam"))
		{
			if (seenSteam)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenSteam = true;

			if (!WordIs(value, valueLength, "yes"))
			{
				return kHudsonRefusedSteamNotYes;
			}

			plan->steamFolderAllowed = true;
		}
		else if (WordIs(place, nameLength, "coop"))
		{
			if (plan->coop)
			{
				return kHudsonRefusedAskedTwice;
			}

			if (!WordIs(value, valueLength, "yes"))
			{
				return kHudsonRefusedCoopNotYes;
			}

			plan->coop = true;
		}
		else if (WordIs(place, nameLength, "typed"))
		{
			if (plan->typed)
			{
				return kHudsonRefusedAskedTwice;
			}

			if (!WordIs(value, valueLength, "yes"))
			{
				return kHudsonRefusedCoopNotYes;
			}

			plan->typed = true;
		}
		else if (WordIs(place, nameLength, "hostile"))
		{
			if (plan->hostile)
			{
				return kHudsonRefusedAskedTwice;
			}

			if (WordIs(value, valueLength, "drain"))
			{
				plan->hostileKind = kHudsonHostileDrain;
			}
			else if (WordIs(value, valueLength, "watch"))
			{
				plan->hostileKind = kHudsonHostileWatch;
			}
			else if (WordIs(value, valueLength, "hold"))
			{
				plan->hostileKind = kHudsonHostileHold;
			}
			else if (WordIs(value, valueLength, "flood"))
			{
				plan->hostileKind = kHudsonHostileFlood;
			}
			else if (!WordIs(value, valueLength, "yes"))
			{
				return kHudsonRefusedCoopNotYes;
			}

			plan->hostile = true;
		}
		else if (WordIs(place, nameLength, "coopwalk"))
		{
			if (plan->coopWalk)
			{
				return kHudsonRefusedAskedTwice;
			}

			if (!WordIs(value, valueLength, "yes"))
			{
				return kHudsonRefusedCoopNotYes;
			}

			plan->coopWalk = true;
		}
		else if (WordIs(place, nameLength, "friendlyfire"))
		{
			if (plan->noFriendlyFire)
			{
				return kHudsonRefusedAskedTwice;
			}

			if (!WordIs(value, valueLength, "off"))
			{
				return kHudsonRefusedFriendlyFireNotOff;
			}

			plan->noFriendlyFire = true;
		}
		else if (WordIs(place, nameLength, "fps"))
		{
			if (WordIs(value, valueLength, "uncapped"))
			{
				plan->framesPerSecond = 0;
			}
			else if (!ReadNumber(value, valueLength, &plan->framesPerSecond) || plan->framesPerSecond < kHudsonFewestFramesPerSecond || plan->framesPerSecond > kHudsonMostFramesPerSecond)
			{
				return kHudsonRefusedFramesPerSecond;
			}
		}
		else if (WordIs(place, nameLength, "botsingle"))
		{
			if (!WordIs(value, valueLength, "yes"))
			{
				return kHudsonRefusedBotFightUnknown;
			}

			plan->botSingle = true;
		}
		else if (WordIs(place, nameLength, "vsync"))
		{
			if (!WordIs(value, valueLength, "off"))
			{
				return kHudsonRefusedVsync;
			}

			plan->vsyncOff = true;
		}
		else if (WordIs(place, nameLength, "access"))
		{
			// Added 2026-11-09 by Melanikus for the AvP port: see kHudsonAccess... in hudson_plan.h
			if (seenAccess)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenAccess = true;

			if (WordIs(value, valueLength, "request"))
			{
				plan->accessPress = kHudsonAccessRequest;
			}
			else if (WordIs(value, valueLength, "check"))
			{
				plan->accessPress = kHudsonAccessCheck;
			}
			else if (WordIs(value, valueLength, "newid"))
			{
				plan->accessPress = kHudsonAccessNewId;
			}
			else
			{
				return kHudsonRefusedAccessWord;
			}
		}
		else if (WordIs(place, nameLength, "accesswait"))
		{
			if (seenAccessWait)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenAccessWait = true;

			if (!ReadNumber(value, valueLength, &plan->accessWaitSeconds))
			{
				return kHudsonRefusedAccessWaitNotANumber;
			}

			if (plan->accessWaitSeconds < kHudsonLeastAccessWaitSeconds || plan->accessWaitSeconds > kHudsonMostAccessWaitSeconds)
			{
				return kHudsonRefusedAccessWaitOutOfRange;
			}
		}
		else if (WordIs(place, nameLength, "answer"))
		{
			// Added 2026-11-09 by Melanikus for the AvP port: see kHudsonAnswer... in hudson_plan.h
			if (seenAnswer)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenAnswer = true;

			if (WordIs(value, valueLength, "letin"))
			{
				plan->answerPress = kHudsonAnswerLetIn;
			}
			else if (WordIs(value, valueLength, "no"))
			{
				plan->answerPress = kHudsonAnswerSayNo;
			}
			else
			{
				return kHudsonRefusedAnswerWord;
			}
		}
		else if (WordIs(place, nameLength, "findgames"))
		{
			// Added 2026-11-09 by Melanikus for the AvP port: see findGames in hudson_plan.h
			if (seenFindGames)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenFindGames = true;

			if (!WordIs(value, valueLength, "yes"))
			{
				return kHudsonRefusedFindGamesWord;
			}

			plan->findGames = true;
		}
		else if (WordIs(place, nameLength, "answerafter"))
		{
			if (seenAnswerAfter)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenAnswerAfter = true;

			if (!ReadNumber(value, valueLength, &plan->answerAfterSeconds))
			{
				return kHudsonRefusedAnswerAfterNotANumber;
			}

			if (plan->answerAfterSeconds < 1 || plan->answerAfterSeconds > kHudsonMostAnswerAfterSeconds)
			{
				return kHudsonRefusedAnswerAfterOutOfRange;
			}
		}
		else if (WordIs(place, nameLength, "ratetest"))
		{
			if (WordIs(value, valueLength, "fall"))
			{
				plan->rateTest = kHudsonRateTestFall;
			}
			else if (WordIs(value, valueLength, "flame"))
			{
				plan->rateTest = kHudsonRateTestFlame;
			}
			else
			{
				return kHudsonRefusedRateTest;
			}
		}
		else if (WordIs(place, nameLength, "ghostlog"))
		{
			if (!WordIs(value, valueLength, "yes"))
			{
				return kHudsonRefusedBotFightUnknown;
			}

			plan->ghostLog = true;
		}
		else if (WordIs(place, nameLength, "botown"))
		{
			if (!WordIs(value, valueLength, "yes"))
			{
				return kHudsonRefusedBotFightUnknown;
			}

			plan->botOwn = true;
		}
		else if (WordIs(place, nameLength, "deathway"))
		{
			if (!ReadNumber(value, valueLength, &plan->deathWay) || plan->deathWay < 0 || plan->deathWay > 99)
			{
				return kHudsonRefusedDeathWay;
			}
		}
		else if (WordIs(place, nameLength, "botweapon"))
		{
			if (WordIs(value, valueLength, "blades"))
			{
				plan->botWeapon = kHudsonBotWeaponBlades;
			}
			else if (WordIs(value, valueLength, "plasma"))
			{
				plan->botWeapon = kHudsonBotWeaponPlasma;
			}
			else
			{
				return kHudsonRefusedBotWeapon;
			}
		}
		else if (WordIs(place, nameLength, "botstay"))
		{
			if (!WordIs(value, valueLength, "yes"))
			{
				return kHudsonRefusedBotFightUnknown;
			}

			plan->botStay = true;
		}
		else if (WordIs(place, nameLength, "pictures"))
		{
			if (!WordIs(value, valueLength, "no"))
			{
				return kHudsonRefusedPicturesWord;
			}

			plan->noNetPictures = true;
		}
		else if (WordIs(place, nameLength, "killkind"))
		{
			if (WordIs(value, valueLength, "marine"))
			{
				plan->killKind = kHudsonBotFightMarine;
			}
			else if (WordIs(value, valueLength, "predator"))
			{
				plan->killKind = kHudsonBotFightPredator;
			}
			else if (WordIs(value, valueLength, "xenoborg"))
			{
				plan->killKind = kHudsonBotFightXenoborg;
			}
			else if (WordIs(value, valueLength, "queen"))
			{
				plan->killKind = kHudsonBotFightQueen;
			}
			else
			{
				return kHudsonRefusedKillWords;
			}
		}
		else if (WordIs(place, nameLength, "killat"))
		{
			if (!ReadPlace(value, valueLength, &plan->killX, &plan->killY, &plan->killZ))
			{
				return kHudsonRefusedKillWords;
			}

			plan->killAtSaid = true;
		}
		else if (WordIs(place, nameLength, "killer"))
		{
			if (WordIs(value, valueLength, "host"))
			{
				plan->killerIsHost = true;
			}
			else if (WordIs(value, valueLength, "nobody"))
			{
				plan->killerIsNobody = true;
			}
			else if (!WordIs(value, valueLength, "join"))
			{
				return kHudsonRefusedKillWords;
			}

			plan->killerSaid = true;
		}
		else if (WordIs(place, nameLength, "botfight"))
		{
			if (plan->botFight != kHudsonBotFightNone)
			{
				return kHudsonRefusedAskedTwice;
			}

			if (WordIs(value, valueLength, "alien"))
			{
				plan->botFight = kHudsonBotFightAlien;
			}
			else if (WordIs(value, valueLength, "marine"))
			{
				plan->botFight = kHudsonBotFightMarine;
			}
			else if (WordIs(value, valueLength, "predator"))
			{
				plan->botFight = kHudsonBotFightPredator;
			}
			else if (WordIs(value, valueLength, "xenoborg"))
			{
				plan->botFight = kHudsonBotFightXenoborg;
			}
			else if (WordIs(value, valueLength, "queen"))
			{
				plan->botFight = kHudsonBotFightQueen;
			}
			else if (WordIs(value, valueLength, "facehugger"))
			{
				plan->botFight = kHudsonBotFightFacehugger;
			}
			else if (WordIs(value, valueLength, "sentry"))
			{
				plan->botFight = kHudsonBotFightSentry;
			}
			else if (WordIs(value, valueLength, "plasma"))
			{
				plan->botFight = kHudsonBotFightPlasma;
			}
			else if (WordIs(value, valueLength, "flame"))
			{
				plan->botFight = kHudsonBotFightFlame;
			}
			else
			{
				return kHudsonRefusedBotFightUnknown;
			}
		}
		else if (WordIs(place, nameLength, "coopend"))
		{
			if (plan->coopEndSeconds != 0)
			{
				return kHudsonRefusedAskedTwice;
			}

			if (!ReadNumber(value, valueLength, &plan->coopEndSeconds) || plan->coopEndSeconds < 1 || plan->coopEndSeconds > 3600)
			{
				return kHudsonRefusedCoopEndNotANumber;
			}
		}
		else if (WordIs(place, nameLength, "net"))
		{
			if (seenNet)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenNet = true;

			if (WordIs(value, valueLength, "host"))
			{
				plan->netMode = kHudsonNetHost;
			}
			else if (WordIs(value, valueLength, "join"))
			{
				plan->netMode = kHudsonNetJoin;
			}
			else
			{
				return kHudsonRefusedNetUnknown;
			}
		}
		else if (WordIs(place, nameLength, "film"))
		{
			if (seenFilm)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenFilm = true;

			if (!WordIs(value, valueLength, "no"))
			{
				return kHudsonRefusedFilmUnknown;
			}
		}
		else if (WordIs(place, nameLength, "lamp"))
		{
			if (seenLamp)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenLamp = true;

			if (WordIs(value, valueLength, "on"))
			{
				plan->lampOn = true;
			}
			else if (WordIs(value, valueLength, "blink"))
			{
				plan->lampBlink = true;
			}
			else if (!WordIs(value, valueLength, "off"))
			{
				return kHudsonRefusedLampUnknown;
			}
		}
		else if (WordIs(place, nameLength, "lampkind"))
		{
			if (seenLampKind)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenLampKind = true;

			if (WordIs(value, valueLength, "real"))
			{
				plan->lampReal = true;
			}
			else if (!WordIs(value, valueLength, "painted"))
			{
				return kHudsonRefusedLampKindUnknown;
			}
		}
		else if (WordIs(place, nameLength, "lampat"))
		{
			if (seenLampAt)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenLampAt = true;

			if (WordIs(value, valueLength, "wide"))
			{
				plan->lampWide = true;
			}
			else if (!WordIs(value, valueLength, "shoulder"))
			{
				return kHudsonRefusedLampAtUnknown;
			}
		}
		else if (WordIs(place, nameLength, "lampsize"))
		{
			if (seenLampSize)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenLampSize = true;

			// Read by hand: "0." and one or two digits, so 0.35 is 35 hundredths. Nothing else, so no "1e9" and no comma for a dot. The limits are the lamp's own.
			int const fewest = (int)(kShoulderLampFewestSpotSize * 100.0 + 0.5);
			int const most = (int)(kShoulderLampMostSpotSize * 100.0 + 0.5);
			bool const shaped = (valueLength == 3 || valueLength == 4) && value[0] == '0' && value[1] == '.' && isdigit((unsigned char)value[2]) && (valueLength == 3 || isdigit((unsigned char)value[3]));
			int const hundredths = shaped ? (value[2] - '0') * 10 + (valueLength == 4 ? value[3] - '0' : 0) : 0;

			if (!shaped || hundredths < fewest || hundredths > most)
			{
				return kHudsonRefusedLampSizeNotANumber;
			}

			plan->lampSizeHundredths = hundredths;
		}
		else if (WordIs(place, nameLength, "bright"))
		{
			if (seenBright)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenBright = true;

			if (!WordIs(value, valueLength, "yes"))
			{
				return kHudsonRefusedBrightNotYes;
			}

			plan->brightWorld = true;
		}
		else if (WordIs(place, nameLength, "gun"))
		{
			if (seenGun)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenGun = true;

			if (!ReadNumber(value, valueLength, &plan->gunSlot) || plan->gunSlot < 1 || plan->gunSlot > 15)
			{
				return kHudsonRefusedGunNotASlot;
			}
		}
		else if (WordIs(place, nameLength, "playgun"))
		{
			if (seenPlayGun)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenPlayGun = true;

			// One gun, so not 15, which is gun='s "each in turn"
			if (!ReadNumber(value, valueLength, &plan->playGunSlot) || plan->playGunSlot < 1 || plan->playGunSlot > 14)
			{
				return kHudsonRefusedGunNotASlot;
			}
		}
		else if (WordIs(place, nameLength, "stepseconds"))
		{
			if (seenStepSeconds)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenStepSeconds = true;

			// How long one step of play=yes may take before he's moved to it: shorter for a film that should move on, never so short that a
			// step he's really walking is cut, nor longer than ten minutes
			if (!ReadNumber(value, valueLength, &plan->playStepSeconds) || plan->playStepSeconds < 10 || plan->playStepSeconds > 600)
			{
				return kHudsonRefusedStepSeconds;
			}
		}
		else if (WordIs(place, nameLength, "species"))
		{
			if (seenSpecies)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenSpecies = true;

			if (WordIs(value, valueLength, "predator"))
			{
				plan->playAsPredator = true;
			}
			else if (WordIs(value, valueLength, "alien"))
			{
				plan->playAsAlien = true;
			}
			else
			{
				return kHudsonRefusedSpeciesNotPredator;
			}
		}
		else if (WordIs(place, nameLength, "meets"))
		{
			if (seenMeets)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenMeets = true;

			if (!WordIs(value, valueLength, "yes"))
			{
				return kHudsonRefusedMeetsNotYes;
			}

			plan->meets = true;
		}
		else if (WordIs(place, nameLength, "screen"))
		{
			if (seenScreen)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenScreen = true;

			if (WordIs(value, valueLength, "main"))
			{
				plan->windowOnMainScreen = true;
			}
			else if (!WordIs(value, valueLength, "second"))
			{
				return kHudsonRefusedScreenWord;
			}
		}
		else if (WordIs(place, nameLength, "windowslot"))
		{
			if (seenWindowSlot)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenWindowSlot = true;

			if (!ReadNumber(value, valueLength, &plan->windowSlot))
			{
				return kHudsonRefusedWindowSlot;
			}
		}
		else if (WordIs(place, nameLength, "windowbig"))
		{
			if (seenWindowBig)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenWindowBig = true;

			// ReadNumber reads figures only, so the number is never below 0
			if (!ReadNumber(value, valueLength, &plan->windowBig) || plan->windowBig > 1)
			{
				return kHudsonRefusedWindowBig;
			}
		}
		else if (WordIs(place, nameLength, "windowsof"))
		{
			if (seenWindowsOf)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenWindowsOf = true;

			if (!ReadNumber(value, valueLength, &plan->windowsTogether) || plan->windowsTogether < 1 || plan->windowsTogether > 4)
			{
				return kHudsonRefusedWindowsOf;
			}
		}
		else if (WordIs(place, nameLength, "custommap"))
		{
			if (seenCustomMap)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenCustomMap = true;

			if (valueLength == 0)
			{
				return kHudsonRefusedCustomMapEmpty;
			}

			// The same rule as a map's file name. Whether there's such a map is for the game to say, from the maps it finds.
			if (valueLength > kCustomMapNameMost)
			{
				return kHudsonRefusedCustomMapName;
			}

			memcpy(plan->customMap, value, valueLength);
			plan->customMap[valueLength] = 0;

			if (!CustomMapName_IsSafe(plan->customMap, sizeof(plan->customMap)))
			{
				plan->customMap[0] = 0;
				return kHudsonRefusedCustomMapName;
			}
		}
		else if (WordIs(place, nameLength, "siegefar"))
		{
			if (seenSiegeFar)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenSiegeFar = true;

			if (valueLength == 0)
			{
				return kHudsonRefusedSiegeFarEmpty;
			}

			if (valueLength > kHudsonLongestWallName)
			{
				return kHudsonRefusedSiegeFarTooLong;
			}

			// Only ever compared with the names of the level's rooms, but nothing on the command line is believed.
			for (size_t i = 0; i < valueLength; i++)
			{
				char const c = value[i];

				if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
				{
					return kHudsonRefusedSiegeFarCharacter;
				}

				plan->siegeFar[i] = c;
			}
		}
		else if (WordIs(place, nameLength, "siegefrom"))
		{
			if (seenSiegeFrom)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenSiegeFrom = true;

			if (valueLength == 0)
			{
				return kHudsonRefusedSiegeFromEmpty;
			}

			if (valueLength > kHudsonLongestWallName)
			{
				return kHudsonRefusedSiegeFromTooLong;
			}

			// Only ever compared with the names of the level's rooms, but nothing on the command line is believed.
			for (size_t i = 0; i < valueLength; i++)
			{
				char const c = value[i];

				if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
				{
					return kHudsonRefusedSiegeFromCharacter;
				}

				plan->siegeFrom[i] = c;
			}
		}
		else if (WordIs(place, nameLength, "remoterelay"))
		{
			// Added 2026-11-09 by Melanikus for the AvP port (server #14): see remoteRelay in hudson_plan.h
			if (seenRemoteRelay)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenRemoteRelay = true;

			if (valueLength == 0)
			{
				return kHudsonRefusedRemoteRelayEmpty;
			}

			if (valueLength > kHudsonLongestRelayName)
			{
				return kHudsonRefusedRemoteRelayTooLong;
			}

			// A server's name, in small letters. networking.cpp checks it again as one (Relay_IsServerName): nothing on the command line is believed.
			for (size_t i = 0; i < valueLength; i++)
			{
				char const c = value[i];

				if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.'))
				{
					return kHudsonRefusedRemoteRelayCharacter;
				}

				plan->remoteRelay[i] = c;
			}
		}
		else if (WordIs(place, nameLength, "siege"))
		{
			if (seenSiege)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenSiege = true;

			if (valueLength == 0)
			{
				return kHudsonRefusedSiegeEmpty;
			}

			if (valueLength > kHudsonLongestWallName)
			{
				return kHudsonRefusedSiegeTooLong;
			}

			// Only ever compared with the names of the level's rooms, but nothing on the command line is believed.
			for (size_t i = 0; i < valueLength; i++)
			{
				char const c = value[i];

				if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
				{
					return kHudsonRefusedSiegeCharacter;
				}

				plan->siegeRoom[i] = c;
			}
		}
		else if (WordIs(place, nameLength, "look"))
		{
			if (seenLook)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenLook = true;

			if (WordIs(value, valueLength, "yes"))
			{
				plan->lookAllTheTime = true;
			}
			else if (WordIs(value, valueLength, "no"))
			{
				plan->lookAllTheTime = false;
			}
			else
			{
				return kHudsonRefusedLookNotYesOrNo;
			}
		}
		else if (WordIs(place, nameLength, "levelfilms"))
		{
			if (seenLevelFilms)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenLevelFilms = true;

			if (!WordIs(value, valueLength, "yes"))
			{
				return kHudsonRefusedLevelFilmsNotYes;
			}

			plan->levelFilms = true;
		}
		else if (WordIs(place, nameLength, "filmskip"))
		{
			if (plan->filmSkipSeconds != 0)
			{
				return kHudsonRefusedAskedTwice;
			}

			if (!ReadNumber(value, valueLength, &plan->filmSkipSeconds) || plan->filmSkipSeconds < 1 || plan->filmSkipSeconds > 3600)
			{
				return kHudsonRefusedFilmSkipNotANumber;
			}
		}
		else if (WordIs(place, nameLength, "dieat"))
		{
			if (plan->dieAtSeconds != 0)
			{
				return kHudsonRefusedAskedTwice;
			}

			if (!ReadNumber(value, valueLength, &plan->dieAtSeconds) || plan->dieAtSeconds < 1 || plan->dieAtSeconds > 3600)
			{
				return kHudsonRefusedDieAtNotANumber;
			}
		}
		else if (WordIs(place, nameLength, "difficulty"))
		{
			if (seenDifficulty)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenDifficulty = true;

			if (WordIs(value, valueLength, "easy"))
			{
				plan->difficulty = 0;
			}
			else if (WordIs(value, valueLength, "medium"))
			{
				plan->difficulty = 1;
			}
			else if (WordIs(value, valueLength, "hard"))
			{
				plan->difficulty = 2;
			}
			else
			{
				return kHudsonRefusedDifficultyUnknown;
			}
		}
		else if (WordIs(place, nameLength, "listplace"))
		{
			if (plan->listPlaceAsked)
			{
				return kHudsonRefusedAskedTwice;
			}

			plan->listPlaceAsked = true;

			// Only a sensible size here. Whether it's a place on the list, and the right one for level=, takes the game's own list.
			if (!ReadNumber(value, valueLength, &plan->listPlace) || plan->listPlace > 999)
			{
				return kHudsonRefusedListPlaceNotANumber;
			}

			plan->meets = true;
		}
		else if (WordIs(place, nameLength, "gunmarine"))
		{
			if (seenGunMarine)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenGunMarine = true;

			if (!WordIs(value, valueLength, "yes"))
			{
				return kHudsonRefusedGunMarineNotYes;
			}

			plan->gunMarine = true;
		}
		else if (WordIs(place, nameLength, "gunalien"))
		{
			if (seenGunAlien)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenGunAlien = true;

			if (!WordIs(value, valueLength, "yes"))
			{
				return kHudsonRefusedGunAlienNotYes;
			}

			plan->gunAlien = true;
		}
		else if (WordIs(place, nameLength, "class"))
		{
			if (seenClass)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenClass = true;

			if (WordIs(value, valueLength, "shotgun"))
			{
				plan->classSlider = kClassSliderFirstNew;
			}
			else if (WordIs(value, valueLength, "revolver"))
			{
				plan->classSlider = kClassSliderFirstNew + 1;
			}
			else if (WordIs(value, valueLength, "m79"))
			{
				plan->classSlider = kClassSliderFirstNew + 2;
			}
			else
			{
				return kHudsonRefusedClassNotOne;
			}
		}
		else if (WordIs(place, nameLength, "play"))
		{
			if (plan->play)
			{
				return kHudsonRefusedAskedTwice;
			}

			if (!WordIs(value, valueLength, "yes"))
			{
				return kHudsonRefusedPlayNotYes;
			}

			plan->play = true;
		}
		else if (WordIs(place, nameLength, "flare"))
		{
			if (seenFlare)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenFlare = true;

			if (!WordIs(value, valueLength, "yes"))
			{
				return kHudsonRefusedFlareNotYes;
			}

			plan->flareEachHold = true;
		}
		else if (WordIs(place, nameLength, "break"))
		{
			if (seenBreak)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenBreak = true;

			if (WordIs(value, valueLength, "prisms"))
			{
				plan->breakPrisms = true;
			}
			else if (WordIs(value, valueLength, "video"))
			{
				plan->breakVideo = true;
			}
			else
			{
				return kHudsonRefusedBreakUnknown;
			}
		}
		else if (WordIs(place, nameLength, "vision"))
		{
			if (seenVision)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenVision = true;

			if (!WordIs(value, valueLength, "check"))
			{
				return kHudsonRefusedVisionUnknown;
			}

			plan->vision = true;
		}
		else if (WordIs(place, nameLength, "size"))
		{
			if (seenSize)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenSize = true;
			plan->sizeAsked = true;

			// A short list of whole words, and not two numbers to read, because nothing on the command line is believed, and these are the only sizes anyone has a use for.
			// 640x480, 1024x768 and 3440x1440 are for the HUD's pictures: the screen it was made for, the one whose look the HUD keeps, and an ultrawide.
			if (WordIs(value, valueLength, "1280x720"))
			{
				plan->width = 1280;
				plan->height = 720;
			}
			else if (WordIs(value, valueLength, "1600x900"))
			{
				plan->width = 1600;
				plan->height = 900;
			}
			else if (WordIs(value, valueLength, "1920x1080"))
			{
				plan->width = 1920;
				plan->height = 1080;
			}
			else if (WordIs(value, valueLength, "2560x1440"))
			{
				plan->width = 2560;
				plan->height = 1440;
			}
			else if (WordIs(value, valueLength, "640x480"))
			{
				plan->width = 640;
				plan->height = 480;
			}
			else if (WordIs(value, valueLength, "1024x768"))
			{
				plan->width = 1024;
				plan->height = 768;
			}
			else if (WordIs(value, valueLength, "3440x1440"))
			{
				plan->width = 3440;
				plan->height = 1440;
			}
			else
			{
				return kHudsonRefusedSizeUnknown;
			}
		}
		else if (WordIs(place, nameLength, "show"))
		{
			if (seenShow)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenShow = true;

			if (!WordIs(value, valueLength, "yes"))
			{
				return kHudsonRefusedShowNotYes;
			}

			plan->showWindow = true;
		}
		else if (WordIs(place, nameLength, "machine"))
		{
			if (seenMachine)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenMachine = true;

			if (WordIs(value, valueLength, "shared"))
			{
				plan->machineShared = true;
			}
			else if (!WordIs(value, valueLength, "alone"))
			{
				return kHudsonRefusedMachineUnknown;
			}
		}
		else if (WordIs(place, nameLength, "compare"))
		{
			if (seenCompare)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenCompare = true;

			if (!WordIs(value, valueLength, "relief"))
			{
				return kHudsonRefusedCompareUnknown;
			}

			plan->compareRelief = true;
		}
		else if (WordIs(place, nameLength, "record"))
		{
			if (seenRecord)
			{
				return kHudsonRefusedAskedTwice;
			}

			seenRecord = true;

			if (WordIs(value, valueLength, "pictures"))
			{
				plan->pictures = true;
			}
			else if (WordIs(value, valueLength, "video"))
			{
				plan->recordVideo = true;
			}
			else if (!WordIs(value, valueLength, "numbers"))
			{
				return kHudsonRefusedRecordUnknown;
			}
		}
		else
		{
			return kHudsonRefusedUnknownName;
		}

		place += wordLength;
	}

	if (!seenLevel || plan->level[0] == 0)
	{
		return kHudsonRefusedNoLevel;
	}

	if (!seenFrames)
	{
		return kHudsonRefusedNoFrames;
	}

	// Nobody is there to look, so asking for this can only be a mistake about what was asked for.
	if (seenViews && plan->rooms == 0)
	{
		return kHudsonRefusedViewsWithoutRooms;
	}

	if (plan->showWindow && !plan->machineShared)
	{
		return kHudsonRefusedShowWithoutSharing;
	}

	if ((plan->coop || plan->coopWalk || (plan->coopEndSeconds != 0 && !plan->listPlaceAsked && !plan->customMap[0])) && plan->netMode == kHudsonNetNone)
	{
		return kHudsonRefusedCoopWithoutNet;
	}

	if (plan->noNetPictures && plan->netMode == kHudsonNetNone)
	{
		return kHudsonRefusedPicturesWord;
	}

	// All three, or none, and only where there's another machine and the level plays as single player. coop=yes is refused above without
	// net=, so needing coop=yes here needs another machine too.
	if ((plan->killKind != kHudsonBotFightNone || plan->killAtSaid || plan->killerSaid) && (plan->killKind == kHudsonBotFightNone || !plan->killAtSaid || !plan->killerSaid || !plan->coop))
	{
		return kHudsonRefusedKillWords;
	}

	if (plan->botFight != kHudsonBotFightNone && !plan->botSingle && (plan->netMode == kHudsonNetNone || !plan->coop))
	{
		return kHudsonRefusedBotFightWithoutCoop;
	}

	if (plan->botSingle && (plan->botFight == kHudsonBotFightNone || plan->netMode != kHudsonNetNone))
	{
		return kHudsonRefusedBotFightWithoutCoop;
	}

	// coop= already needs net=, so this covers a run with no network too
	if (plan->noFriendlyFire && !plan->coop)
	{
		return kHudsonRefusedFriendlyFireWithoutCoop;
	}

	if (plan->netMode != kHudsonNetNone)
	{
		if (!plan->machineShared)
		{
			return kHudsonRefusedNetMeasuresTime;
		}

		if (plan->rooms > 0 || plan->navModules > 0 || plan->route[0] != 0 || plan->listStrategies || plan->strategy[0] != 0 || plan->wall[0] != 0 || plan->vision || plan->compareRelief || plan->combatAlien)
		{
			return kHudsonRefusedNetNotAlone;
		}
	}

	// The lamp is off in every run unless it asks with lamp=on or lamp=blink. The user, 2026-11-08: "Hudson tests weirdly show the marine shoulder lamp all the time,
	// turn it off". The painted lamp is drawn from wherever the camera is, so in a run whose camera follows something else it lit that instead. A run that compares
	// what's on the screen can't ask for it, because it would quietly spoil the comparison.
	if (seenLamp && (plan->lampOn || plan->lampBlink) && (plan->vision || plan->compareRelief))
	{
		return kHudsonRefusedLampSpoilsComparing;
	}

	// A size for a lamp that isn't on would say something about the run that isn't so
	if (seenLampSize && !plan->lampOn && !plan->lampBlink)
	{
		return kHudsonRefusedLampSizeWithoutLamp;
	}

	if (plan->lampWide && !plan->lampReal)
	{
		return kHudsonRefusedLampWordsWithoutReal;
	}

	// A run that shares the machine films itself unless it says otherwise, because the user wants to see them, and it only reads what's already drawn, so it can't spoil a run that compares pictures. There are two ways to say no: film=no, and record=numbers,
	// which means numbers and nothing else. A run that measures time has no film, because a film slows the frames it records, and asking for one by name there is refused just below.
	if (plan->recordVideo && seenFilm)
	{
		return kHudsonRefusedFilmBothWays;
	}

	bool const saidOnlyNumbers = seenRecord && !plan->pictures && !plan->recordVideo;

	if (plan->machineShared && !seenFilm && !saidOnlyNumbers)
	{
		plan->recordVideo = true;
	}

	if (plan->recordVideo && !plan->machineShared)
	{
		return kHudsonRefusedVideoMeasuresTime;
	}

	if (plan->breakVideo && !plan->recordVideo)
	{
		return kHudsonRefusedBreakVideoWithoutVideo;
	}

	if (seenHold && seenHoldSeconds)
	{
		return kHudsonRefusedHoldTwoWays;
	}

	if (seenHoldSeconds && plan->rooms == 0)
	{
		return kHudsonRefusedHoldWithoutRooms;
	}

	if (seenTourFrom && plan->rooms < 2)
	{
		return kHudsonRefusedTourFromWithoutATour;
	}

	if (seenHold && plan->rooms == 0)
	{
		return kHudsonRefusedHoldWithoutRooms;
	}

	if (seenNav && plan->rooms != 1)
	{
		return kHudsonRefusedNavWithoutOneRoom;
	}

	if (seenNav && plan->wall[0])
	{
		return kHudsonRefusedNavWithWall;
	}

	if (seenPace && !seenNav && !seenRoute)
	{
		return kHudsonRefusedPaceWithoutWalking;
	}

	if (seenMap && !seenNav && !seenRoute)
	{
		return kHudsonRefusedMapWithoutWalking;
	}

	if (seenFrameHold && !seenNav && !seenRoute)
	{
		return kHudsonRefusedFrameHoldWithoutWalking;
	}

	if (seenRoute && plan->rooms != 1)
	{
		return kHudsonRefusedRouteWithoutOneRoom;
	}

	if (seenRoute && plan->wall[0])
	{
		return kHudsonRefusedRouteWithWall;
	}

	// A route says where to go, so unless nav= says to stop sooner, he goes on until it ends.
	if (seenRoute && !seenNav)
	{
		plan->navModules = kHudsonMostNavModules;
	}

	// A walk takes its own two pictures, once it has got where it was going: see "A WALK" in hudson.cpp
	if (plan->pictures && plan->rooms == 0 && plan->gunSlot == 0 && !(seenWalkFrom && seenWalkTo))
	{
		return kHudsonRefusedPicturesWithoutRooms;
	}

	if (seenStrategies && plan->rooms == 0)
	{
		return kHudsonRefusedStrategiesWithoutRooms;
	}

	if (seenStrategy && plan->rooms == 0)
	{
		return kHudsonRefusedStrategyWithoutRooms;
	}

	if (seenStrategy && seenWall)
	{
		return kHudsonRefusedStrategyWithWall;
	}

	if (seenStrategyLook && !seenStrategy)
	{
		return kHudsonRefusedStrategyLookWithoutStrategy;
	}

	if (seenStrategySeconds && !seenStrategy)
	{
		return kHudsonRefusedStrategySecondsWithoutStrategy;
	}

	if (seenWall && plan->rooms == 0)
	{
		return kHudsonRefusedWallWithoutRooms;
	}

	if (seenWall && plan->rooms > 1)
	{
		return kHudsonRefusedWallInManyRooms;
	}

	if (seenWallRank && !seenWall)
	{
		return kHudsonRefusedWallRankWithoutWall;
	}

	if (seenWallRoom && !seenWall)
	{
		return kHudsonRefusedWallRoomWithoutWall;
	}

	if (plan->compareRelief && !plan->pictures)
	{
		return kHudsonRefusedCompareWithoutPictures;
	}

	if (plan->combatAlien && plan->rooms == 0)
	{
		return kHudsonRefusedCombatWithoutRooms;
	}

	// A fight is him standing where the level starts him, for as long as it takes. Holds and views would be asking for something it won't do.
	if (plan->combatAlien && (seenWall || plan->compareRelief || seenHold || seenHoldSeconds || plan->rooms > 1 || (seenViews && plan->views != 1)))
	{
		return kHudsonRefusedCombatNotAlone;
	}

	if ((seenStrategy ? 1 : 0) + (plan->combatAlien ? 1 : 0) + ((seenNav || seenRoute) ? 1 : 0) + (plan->rooms > 1 ? 1 : 0) > 1)
	{
		return kHudsonRefusedOneJobAtATime;
	}

	// Watching an alien is a fight he doesn't take part in, so each of these only means something in a fight
	if ((seenWatch || seenHoldFire || seenAlienAt || seenStandAt || seenWatchFrom || seenTogether) && !plan->combatAlien)
	{
		return kHudsonRefusedWatchWordsWithoutCombat;
	}

	if (plan->holdFire && !plan->watchAlien)
	{
		return kHudsonRefusedHoldFireWithoutWatch;
	}

	// every= needs several aliens: from together=, or from several places in alienat=
	if (seenEvery && !seenTogether && plan->alienAtPlaces <= 1)
	{
		return kHudsonRefusedEveryWithoutTogether;
	}

	// Several places in alienat= are together='s aliens: they say how many, or have to agree with together=
	if (plan->alienAtPlaces > 1 && !plan->watchAlien)
	{
		return kHudsonRefusedPlacesWithoutWatch;
	}

	if (plan->alienAtPlaces > 1 && seenTogether && plan->watchTogether != plan->alienAtPlaces)
	{
		return kHudsonRefusedTogetherNotThePlaces;
	}

	if (plan->alienAtPlaces > 1 && !seenTogether)
	{
		plan->watchTogether = plan->alienAtPlaces;
	}

	if (seenTogether && !plan->watchAlien)
	{
		return kHudsonRefusedTogetherWithoutWatch;
	}

	if (seenWatchFrom && !plan->watchAlien)
	{
		return kHudsonRefusedWatchFromWithoutWatch;
	}

	// A walk is the player walking, so it needs both ends, and nothing else moving him or the camera. rooms= of any number hands the camera to a marine and makes the
	// player only a watcher, who can walk into nothing: on 2026-11-08 a walk run with rooms=1 looked round its room and ended before the walk began.
	if (seenWalkFrom != seenWalkTo)
	{
		return kHudsonRefusedWalkNeedsBoth;
	}

	plan->walkSaid = seenWalkFrom && seenWalkTo;
	plan->lookAtSaid = seenLookAt;

	if (seenLookAt && !plan->walkSaid)
	{
		return kHudsonRefusedLookAtWithoutWalk;
	}

	// A fight needs rooms=1 (above), so rooms > 0 covers a walk with a fight too
	if (plan->walkSaid && (seenStrategy || seenNav || seenRoute || plan->rooms > 0))
	{
		return kHudsonRefusedWalkNotAlone;
	}

	// One alien is followed, and its first blow ends the run, so aliens made one after another would never be seen
	if (plan->watchAlien && seenAliens)
	{
		return kHudsonRefusedWatchWithAliens;
	}

	if (seenAliens && !plan->combatAlien)
	{
		return kHudsonRefusedAliensWithoutCombat;
	}

	if (plan->brightWorld && !plan->vision)
	{
		return kHudsonRefusedBrightWithoutVision;
	}

	if (plan->gunSlot && plan->rooms)
	{
		return kHudsonRefusedGunWithAMarine;
	}

	if (plan->playAsAlien && plan->gunSlot != 0)
	{
		return kHudsonRefusedAlienWithGun;
	}

	if (plan->windowOnMainScreen && (seenWindowSlot || seenWindowsOf || seenWindowBig))
	{
		return kHudsonRefusedWindowPartsOnMain;
	}

	if (seenWindowBig && plan->windowsTogether != 2)
	{
		return kHudsonRefusedWindowBig;
	}

	if (plan->windowSlot >= plan->windowsTogether)
	{
		return kHudsonRefusedWindowSlot;
	}

	// A walk and a hostile test turn him themselves, and a game with no campaign has an old test that does too
	bool const aPairOnTheCampaign = plan->netMode != kHudsonNetNone && plan->coop && !plan->coopWalk && !plan->hostile;

	if (!plan->play && seenStepSeconds)
	{
		return kHudsonRefusedStepSecondsWithoutPlay;
	}

	// The Online access presses (added 2026-11-09): an access run never starts a network game, accesswait= belongs to access=check alone,
	// and the host's answer to a player asking to join is a host's
	if (seenAccess && plan->netMode != kHudsonNetNone)
	{
		return kHudsonRefusedAccessWithNet;
	}

	if (seenAccessWait && plan->accessPress != kHudsonAccessCheck)
	{
		return kHudsonRefusedAccessWaitWithoutCheck;
	}

	if (seenAnswer && plan->netMode != kHudsonNetHost)
	{
		return kHudsonRefusedAnswerWithoutHost;
	}

	if (seenAnswerAfter && !seenAnswer)
	{
		return kHudsonRefusedAnswerAfterWithoutAnswer;
	}

	if (seenFindGames && plan->netMode != kHudsonNetJoin)
	{
		return kHudsonRefusedFindGamesWithoutJoin;
	}

	if (seenSiegeFrom && !seenSiege)
	{
		return kHudsonRefusedSiegeFromWithoutSiege;
	}

	if (seenRemoteRelay && plan->netMode == kHudsonNetNone)
	{
		return kHudsonRefusedRemoteRelayWithoutNet;
	}

	// A hostile run sends the other game bad messages on purpose, of every kind (yes, drain, watch, hold, flood). Through a relay beyond this
	// machine, the other end could be anybody's game, so none of them ever goes there (Primary's read of this line, 2026-11-09).
	if (seenRemoteRelay && plan->hostile)
	{
		return kHudsonRefusedRemoteRelayHostile;
	}

	if (seenSiegeFar && !seenSiege)
	{
		return kHudsonRefusedSiegeFarWithoutSiege;
	}

	if (seenSiege)
	{
		if (!aPairOnTheCampaign || plan->botFight != kHudsonBotFightNone || plan->killKind != kHudsonBotFightNone)
		{
			return kHudsonRefusedSiegeNeedsAPair;
		}

		// Their shots are for the enemy, and neither should stand in the way of the other's
		plan->noFriendlyFire = true;
	}

	if (seenLook && !aPairOnTheCampaign)
	{
		return kHudsonRefusedLookNeedsAPair;
	}

	if (!seenLook && aPairOnTheCampaign)
	{
		bool const aFight = plan->botFight != kHudsonBotFightNone || plan->killKind != kHudsonBotFightNone;

		plan->lookAllTheTime = aFight;
	}

	if (plan->filmSkipSeconds != 0 && !plan->levelFilms)
	{
		return kHudsonRefusedFilmSkipWithoutFilms;
	}

	if (((plan->playAsPredator || plan->playAsAlien) && plan->gunSlot == 0 && !plan->meets) && !plan->play)
	{
		return kHudsonRefusedSpeciesWithoutGun;
	}

	if (plan->listPlaceAsked && (plan->playAsPredator || plan->playAsAlien))
	{
		return kHudsonRefusedListPlaceWithSpecies;
	}

	if (plan->listPlaceAsked && plan->classSlider)
	{
		return kHudsonRefusedListPlaceWithClass;
	}

	if (plan->listPlaceAsked && plan->netMode != kHudsonNetNone)
	{
		return kHudsonRefusedListPlaceWithNet;
	}

	if (plan->gunMarine && plan->gunSlot == 0)
	{
		return kHudsonRefusedGunMarineWithoutGun;
	}

	if (plan->gunAlien && plan->gunSlot == 0)
	{
		return kHudsonRefusedGunAlienWithoutGun;
	}

	if (plan->play && (plan->rooms > 0 || plan->gunSlot != 0 || plan->botFight != 0))
	{
		return kHudsonRefusedPlayWithSomethingElse;
	}

	if (plan->play && plan->netMode != kHudsonNetNone && !plan->coop)
	{
		return kHudsonRefusedPlayNetWithoutCoop;
	}

	if (plan->classSlider && plan->gunSlot == 0)
	{
		return kHudsonRefusedClassWithoutGun;
	}

	if (plan->classSlider && plan->playAsPredator)
	{
		return kHudsonRefusedClassAsPredator;
	}

	if (plan->flareEachHold && (plan->vision || plan->compareRelief))
	{
		return kHudsonRefusedFlareSpoilsComparing;
	}

	if (plan->flareEachHold && (plan->rooms == 0 || plan->combatAlien || plan->navModules || plan->strategy[0] || plan->listStrategies))
	{
		return kHudsonRefusedFlareWithoutHolds;
	}

	if (plan->breakPrisms && !plan->vision)
	{
		return kHudsonRefusedBreakWithoutVision;
	}

	if (plan->vision && plan->rooms == 0)
	{
		return kHudsonRefusedVisionWithoutRooms;
	}

	if (plan->vision && plan->lighting != kHudsonLightingPerPixel && plan->lighting != kHudsonLightingNormals && plan->lighting != kHudsonLightingRelief)
	{
		return kHudsonRefusedVisionWithoutPictures;
	}

	if (plan->vision && (plan->compareRelief || seenStrategy || plan->combatAlien || seenNav || seenRoute))
	{
		return kHudsonRefusedVisionWithoutViews;
	}

	// Someone may be watching a run that shares the machine. Pictures stay the usual size, so that they compare with the last ones.
	if (!seenSize && plan->machineShared && !plan->pictures)
	{
		plan->width = kHudsonWatchedWidth;
		plan->height = kHudsonWatchedHeight;
	}

	return 0;
}

// Relief and its twin are there twice each, on purpose. Two pictures of the same thing show how much pictures differ with nothing changed, because the level's lights can flicker and its textures can move. That's
// what any other difference has to be measured against.
static const HudsonCompareCase kCompareCases[] =
{
	{ "gouraud",       kHudsonLightingGouraud,  false, false, false },
	{ "perpixel",      kHudsonLightingPerPixel, false, false, false },
	{ "normals",       kHudsonLightingNormals,  false, false, false },
	{ "relief",        kHudsonLightingRelief,   false, false, false },
	{ "relief-flip",   kHudsonLightingRelief,   true,  false, false },
	{ "relief-tilt",   kHudsonLightingRelief,   false, true,  false },
	{ "relief-depth0", kHudsonLightingRelief,   false, false, true  },
	{ "relief-again",  kHudsonLightingRelief,   false, false, false },
	{ "normals-again", kHudsonLightingNormals,  false, false, false }
};

int HudsonPlan_CompareCaseCount()
{
	return (int)(sizeof(kCompareCases) / sizeof(kCompareCases[0]));
}

const HudsonCompareCase &HudsonPlan_CompareCase(int number)
{
	// An hour's work later, a number past the end shouldn't read whatever's there
	if (number < 0 || number >= HudsonPlan_CompareCaseCount())
	{
		number = 0;
	}

	return kCompareCases[number];
}

bool HudsonPlan_WallSpot(const int (*corners)[3], int count, double faceX, double faceY, double faceZ, HudsonWallSpot *spot)
{
	if (count < 3)
	{
		return false;
	}

	double const faceLength = sqrt(faceX * faceX + faceY * faceY + faceZ * faceZ);
	double const levelLength = sqrt(faceX * faceX + faceZ * faceZ);

	// sin(30 degrees) is a half: a wall upright within 30 degrees faces no more than that up or down
	if (faceLength <= 0.0 || levelLength <= 0.0 || fabs(faceY) > 0.5 * faceLength)
	{
		return false;
	}

	double const outX = faceX / levelLength, outZ = faceZ / levelLength;
	double middleX = 0.0, middleY = 0.0, middleZ = 0.0;
	double leastAlong = 0.0, mostAlong = 0.0, leastY = 0.0, mostY = 0.0;

	for (int i = 0; i < count; i++)
	{
		double const x = corners[i][0], y = corners[i][1], z = corners[i][2];

		middleX += x;
		middleY += y;
		middleZ += z;

		// how far along the wall this corner is: along the level direction square to the way it faces
		double const along = x * outZ - z * outX;

		leastAlong = (i == 0 || along < leastAlong) ? along : leastAlong;
		mostAlong = (i == 0 || along > mostAlong) ? along : mostAlong;
		leastY = (i == 0 || y < leastY) ? y : leastY;
		mostY = (i == 0 || y > mostY) ? y : mostY;
	}

	double const wide = mostAlong - leastAlong, tall = mostY - leastY;

	if (wide < kHudsonSmallestWallSide || tall < kHudsonSmallestWallSide)
	{
		return false;
	}

	spot->wide = wide;
	spot->tall = tall;
	spot->size = wide * tall;
	spot->x = (int)floor(middleX / count + outX * kHudsonWallDistance + 0.5);
	spot->y = (int)floor(middleY / count + 0.5);
	spot->z = (int)floor(middleZ / count + outZ * kHudsonWallDistance + 0.5);
	spot->towardsX = (int)floor(-outX * kHudsonViewDistance + 0.5);
	spot->towardsZ = (int)floor(-outZ * kHudsonViewDistance + 0.5);

	return true;
}

static char Lowercase(char c)
{
	return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

bool HudsonPlan_PictureIsNamed(const char *path, const char *word)
{
	if (!path || !word || !word[0])
	{
		return false;
	}

	const char *name = path;

	for (const char *c = path; *c; c++)
	{
		if (*c == '/' || *c == '\\')
		{
			name = c + 1;
		}
	}

	size_t i = 0;

	for (; word[i]; i++)
	{
		if (Lowercase(name[i]) != Lowercase(word[i]) || name[i] == 0)
		{
			return false;
		}
	}

	return name[i] == 0 || name[i] == '.';
}

// As many as a run can ask for, kHudsonMostViews. The first three are the ones that matter, and a run needs no more to judge a wall.
struct WallView
{
	int degrees;
	int step;      // how far along the wall, in millimetres
};

static const WallView kWallViews[kHudsonMostViews] =
{
	{ 0, 0 },
	{ 35, 0 },
	{ 0, 1000 },
	{ -35, 0 },
	{ 0, -1000 },
	{ 70, 0 },
	{ -70, 0 },
	{ 0, 2000 }
};

// A number past either end is the first view, so it can't read past the list.
static const WallView &WallViewNumber(int view)
{
	return kWallViews[(view < 0 || view >= kHudsonMostViews) ? 0 : view];
}

int HudsonPlan_WallViewDegrees(int view)
{
	return WallViewNumber(view).degrees;
}

int HudsonPlan_WallViewStep(int view)
{
	return WallViewNumber(view).step;
}

void HudsonPlan_WallViewPlace(int view, int towardsX, int towardsZ, int *x, int *z)
{
	double const length = sqrt((double)towardsX * towardsX + (double)towardsZ * towardsZ);
	double const step = (double)HudsonPlan_WallViewStep(view);

	if (length <= 0.0)
	{
		*x = 0;
		*z = 0;
		return;
	}

	*x = (int)floor((double)towardsZ / length * step + 0.5);
	*z = (int)floor(-(double)towardsX / length * step + 0.5);
}

double HudsonPlan_OnePictureCovers(double wallLength, double pictureCoordinatesRange, double pictureSize)
{
	return (pictureCoordinatesRange > 0.0) ? wallLength * pictureSize / pictureCoordinatesRange : 0.0;
}

void HudsonPlan_WallViewDirection(int view, int towardsX, int towardsZ, int *x, int *z)
{
	double const turn = 3.14159265358979323846 * (double)HudsonPlan_WallViewDegrees(view) / 180.0;

	// to the nearest whole number, so that straight at the wall is exactly that
	*x = (int)floor((double)towardsX * cos(turn) + (double)towardsZ * sin(turn) + 0.5);
	*z = (int)floor((double)towardsZ * cos(turn) - (double)towardsX * sin(turn) + 0.5);
}

void HudsonPlan_ViewDirection(int view, int views, int *x, int *z)
{
	double const turn = 2.0 * 3.14159265358979323846 * (double)view / (double)views;

	// to the nearest whole number, so that straight along an axis is exactly that
	*x = (int)floor(sin(turn) * (double)kHudsonViewDistance + 0.5);
	*z = (int)floor(cos(turn) * (double)kHudsonViewDistance + 0.5);
}

// Within about two degrees. A marine turns to face a point in small steps, and the steps are rounded, so he won't be exactly on it. A quarter of a degree would be too fine, and two is too little to see.
bool HudsonPlan_IsFacing(int forwardX, int forwardZ, int wantedX, int wantedZ)
{
	double const lengths = sqrt((double)forwardX * forwardX + (double)forwardZ * forwardZ) * sqrt((double)wantedX * wantedX + (double)wantedZ * wantedZ);

	if (lengths <= 0.0)
	{
		return false;
	}

	return ((double)forwardX * wantedX + (double)forwardZ * wantedZ) / lengths >= 0.9994;
}

void HudsonPlan_WindowPlace(int areaLeft, int areaTop, int areaWidth, int areaHeight, int slot, int together, int *left, int *top, int *width, int *height)
{
	int const across = together > 1 ? 2 : 1;
	int const down = together > 2 ? 2 : 1;
	int const partWidth = areaWidth / across;
	int const partHeight = areaHeight / down;

	*left = areaLeft + (slot % across) * partWidth;
	*top = areaTop + (slot / across) * partHeight;

	if (*width <= 0 || *height <= 0)
	{
		*width = partWidth;
		*height = partHeight;
	}
	else if (*width > partWidth || *height > partHeight)
	{
		// Whichever of its width and height is tighter decides how much smaller it gets
		if ((long long)partWidth * *height <= (long long)partHeight * *width)
		{
			*height = (int)((long long)*height * partWidth / *width);
			*width = partWidth;
		}
		else
		{
			*width = (int)((long long)*width * partHeight / *height);
			*height = partHeight;
		}
	}
}

void HudsonPlan_WindowPart(int areaLeft, int areaTop, int areaWidth, int areaHeight, bool big, int *left, int *top, int *width, int *height)
{
	int const bigWidth = (int)((long long)areaWidth * 3 / 4);

	*left = big ? areaLeft : areaLeft + bigWidth;
	*top = areaTop;
	*width = big ? bigWidth : areaWidth - bigWidth;
	*height = areaHeight;
}

int HudsonPlan_SmokeVerdict(const HudsonSmokeFacts &facts, const char **why)
{
	*why = "";

	if (facts.dead)
	{
		*why = "he died before it was done";
		return kHudsonSmokeFailed;
	}

	if (!facts.bodyHere)
	{
		if (facts.millisecondsWaiting < kHudsonSmokeMostWaitForABody)
		{
			return kHudsonSmokeWaiting;
		}

		*why = "the other player's body never came";
		return kHudsonSmokeFailed;
	}

	if (facts.millisecondsSeen < kHudsonSmokeLeastLook)
	{
		return kHudsonSmokeWaiting;
	}

	if (facts.facing && facts.drawn && facts.wayClear)
	{
		return kHudsonSmokePassed;
	}

	if (facts.millisecondsSeen < kHudsonSmokeMostLook)
	{
		return kHudsonSmokeWaiting;
	}

	*why = !facts.facing ? "he never came to face the other player" : !facts.drawn ? "the other player's body never came up on the screen" : "something stands between them";
	return kHudsonSmokeFailed;
}

void HudsonPlan_StampNumber(unsigned char *pixels, int pitch, int width, int height, unsigned long number)
{
	const unsigned char (&kFigures)[10][5] = kHudsonStampFigures;
	static const int kFiguresInANumber = kHudsonStampFiguresInANumber;
	static const int kSquare = kHudsonStampSquare;
	static const int kFromTheEdge = kHudsonStampFromTheEdge;
	int const across = kSquare * 4 * kFiguresInANumber;
	int const down = kSquare * 5;

	if (!pixels || width <= 0 || height <= 0 || pitch < width * 4)
	{
		return;
	}

	for (int y = 0; y < down && kFromTheEdge + y < height; y++)
	{
		unsigned char *const row = pixels + (kFromTheEdge + y) * pitch;

		for (int x = 0; x < across && kFromTheEdge + x < width; x++)
		{
			// The figures from the left, so the highest comes first: dividing by ten once for each place to its right
			int const place = x / (kSquare * 4);
			unsigned long figure = number;

			for (int right = kFiguresInANumber - 1 - place; right > 0; right--)
			{
				figure /= 10;
			}

			int const square = (x / kSquare) % 4;
			bool const lit = square < 3 && ((kFigures[figure % 10][y / kSquare] >> (2 - square)) & 1) != 0;
			unsigned char *const pixel = row + (kFromTheEdge + x) * 4;

			pixel[0] = pixel[1] = pixel[2] = lit ? 255 : 0;
			pixel[3] = 255;
		}
	}
}

bool HudsonPlan_PlayersCannotDie(const HudsonPlan &plan)
{
	return plan.siegeRoom[0] != 0;
}

int HudsonPlan_LookStep(int current, int wanted, int mostThisFrame)
{
	int way = (wanted - current) & 4095;

	if (way > 2048)
	{
		way -= 4096;
	}

	int const size = way < 0 ? -way : way;
	int step = 1 + size / 8;

	if (step > mostThisFrame)
	{
		step = mostThisFrame;
	}

	if (step > size)
	{
		step = size;
	}

	return (current + (way < 0 ? -step : step)) & 4095;
}

bool HudsonPlan_SizeFits(int width, int height, int screenWidth, int screenHeight)
{
	return width <= screenWidth && height + kHudsonRoomForTheWindowsFrame <= screenHeight;
}

bool HudsonPlan_FolderIsAllowed(const char *folder, const char *refusal, const HudsonPlan &plan)
{
	return folder && (!HudsonPlan_FolderIsRefused(folder) || (!refusal && plan.steamFolderAllowed));
}

bool HudsonPlan_FolderIsRefused(const char *folder)
{
	static const char kSteamFolder[] = "steamapps";
	size_t const length = sizeof(kSteamFolder) - 1;

	if (!folder)
	{
		return true;
	}

	for (const char *place = folder; *place; place++)
	{
		size_t matched = 0;

		while (matched < length && place[matched] && tolower((unsigned char)place[matched]) == kSteamFolder[matched])
		{
			matched++;
		}

		if (matched == length)
		{
			return true;
		}
	}

	return false;
}
