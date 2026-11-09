// Modified 2026-11-05 by Melanikus for the AvP port:
//   the rendering page
//   the rendering page's lines for choosing a key
// Modified 2026-11-08 by Melanikus for the AvP port:
//   a game found can be one from the relay server's list: SESSION_DESC says so, with its region and code
// Modified 2026-11-09 by Melanikus for the AvP port:
//   SESSION_DESC keeps the level's name, the game's style and its players, for the games page's columns
//   a row for the host's answer to a request to join (AVPMENU_ELEMENT_JOINANSWER), and the page of games found's Refresh row
//   (AVPMENU_ELEMENT_FINDAGAIN)
//   SESSION_DESC keeps a relay game's host's fingerprint from the list, for protocol version 2's handshake B
#ifndef _included_AvP_Menus_h_
#define _included_AvP_Menus_h_

#include "TextureManager.h"
#include "relay_list_text.h"

extern bool mainMenu;

#define MARINE_DEMO 0
#define PREDATOR_DEMO 0
#define ALIEN_DEMO 0

 // Edmond modified for Mplayer Demo
 #ifdef MPLAYER_DEMO
 #define DEATHMATCH_DEMO 1
 #else
 #define DEATHMATCH_DEMO 0 // more multiplayer-only demo really
 #endif

#ifdef AVP_DEBUG_VERSION
	#define CONSOLE_DEBUGGING_COMMANDS_ACTIVATED 1

	/* bjd 24/11/08 - load all sounds for debug mode.. */
	#define LOAD_SCREAMS_FROM_FASTFILES 1
	#define LOAD_USING_FASTFILES 1

#else //AVP_DEBUG_VERSION

	#ifdef AVP_DEBUG_FOR_FOX
		#define CONSOLE_DEBUGGING_COMMANDS_ACTIVATED 1
	#else
		#define CONSOLE_DEBUGGING_COMMANDS_ACTIVATED 0
	#endif

	#define LOAD_SCREAMS_FROM_FASTFILES 1
	#define LOAD_USING_FASTFILES 1
#endif //AVP_DEBUG_VERSION

#define PLAY_INTRO 1
#define ALLOW_SKIP_INTRO 1

#define SAVE_GAME_ON 1

#include "menu_gfx.hpp"
#include "language.h"

enum MENUSSTATE_ID
{
	MENUSSTATE_MAINMENUS,
	MENUSSTATE_INGAMEMENUS,
	MENUSSTATE_OUTSIDEMENUS,
	MENUSSTATE_STARTGAME,
};

int GetAvPMenuState();

enum AVPMENU_ID
{
	AVPMENU_MAIN,
	AVPMENU_MAIN_WITHCHEATS,
	AVPMENU_DEBUG_MAIN,
	AVPMENU_DEBUG_MAIN_WITHCHEATS,

	AVPMENU_EXITGAME,

	AVPMENU_USERPROFILESELECT,
	AVPMENU_USERPROFILEENTERNAME,
	AVPMENU_USERPROFILEDELETE,

	AVPMENU_SINGLEPLAYER,
	AVPMENU_MARINELEVELS,
	AVPMENU_ALIENLEVELS,
	AVPMENU_PREDATORLEVELS,

	AVPMENU_MULTIPLAYER,
	AVPMENU_MULTIPLAYER_LOBBIEDSERVER,
	AVPMENU_MULTIPLAYER_LOBBIEDCLIENT,
	AVPMENU_MULTIPLAYER_SKIRMISH,
	AVPMENU_MULTIPLAYERSTARTGAME,
	AVPMENU_MULTIPLAYERJOINGAME,
	AVPMENU_MULTIPLAYERSELECTSESSION,
	AVPMENU_MULTIPLAYERJOINGAME2,
	AVPMENU_MULTIPLAYEROPENADDRESS,
	AVPMENU_MULTIPLAYER_LOADIPADDRESS,
	AVPMENU_MULTIPLAYER_CONNECTION,
	AVPMENU_MULTIPLAYER_CONFIG,
	AVPMENU_MULTIPLAYER_CONFIG_JOIN,
	AVPMENU_SKIRMISH_CONFIG,
	AVPMENU_MULTIPLAYER_SPECIES_HOST,
	AVPMENU_MULTIPLAYER_SPECIES_JOIN,

	AVPMENU_MULTIPLAYER_SAVECONFIG,
	AVPMENU_MULTIPLAYER_LOADCONFIG,
	AVPMENU_MULTIPLAYER_DELETECONFIG,

	AVPMENU_MULTIPLAYER_JOINING,

	AVPMENU_CONTROLS,
	AVPMENU_JOYSTICKCONTROLS,

	AVPMENU_INGAMEAVOPTIONS,
	AVPMENU_MAINMENUAVOPTIONS,
	AVPMENU_VIDEOMODE,

	AVPMENU_INGAME,
	AVPMENU_INNETGAME,

	AVPMENU_MARINEKEYCONFIG,
	AVPMENU_PREDATORKEYCONFIG,
	AVPMENU_ALIENKEYCONFIG,
	AVPMENU_OPTIONS,

	AVPMENU_LEVELBRIEFING_BASIC,
	AVPMENU_LEVELBRIEFING_BONUS,

	AVPMENU_CHEATOPTIONS,
	AVPMENU_DETAILLEVELS,

	AVPMENU_LOADGAME,
	AVPMENU_SAVEGAME,

	AVPMENU_INGAMEVIDEOMODE, // bjd - added for ingame resolution changing

	AVPMENU_RENDERING,        // the same page, reached from the main menus or from a level, so that going back goes to where it came from
	AVPMENU_INGAMERENDERING,
	AVPMENU_CUSTOMMAPS,        // the custom maps for single player: see custom_maps.h
	AVPMENU_COOP_HOST,         // the co-op campaign, chosen by a host with others gathering: see coop_menu.h
	AVPMENU_COOP_JOIN,         // the same rows, shown to a joiner
	AVPMENU_COOP_OUT,          // one plain sentence, for a player the co-op game has ended for, and why
	AVPMENU_INCOOPGAME,        // the pause menu in a co-op campaign: the network game's, with a Players row
	AVPMENU_COOP_PLAYERS,      // who's in and alive, from that menu, where a host can remove someone
	AVPMENU_COOP_HOSTQUITS     // a host is asked if they mean to end the game for everyone
};

enum AVPMENU_FONT_ID
{
	AVPMENU_FONT_SMALL,
	AVPMENU_FONT_BIG,
};

/* menu elements */
enum AVPMENU_ELEMENT_ID
{
	AVPMENU_ELEMENT_GOTOMENU,
	AVPMENU_ELEMENT_GOTOMENU_GFX,

	AVPMENU_ELEMENT_TOGGLE,
	AVPMENU_ELEMENT_SLIDER,

	AVPMENU_ELEMENT_USERPROFILE,
	AVPMENU_ELEMENT_ALIENEPISODE,
	AVPMENU_ELEMENT_MARINEEPISODE,
	AVPMENU_ELEMENT_PREDATOREPISODE,
	AVPMENU_ELEMENT_STARTMPGAME,
	AVPMENU_ELEMENT_JOINMPGAME,
	AVPMENU_ELEMENT_JOINLOBBIED,

	AVPMENU_ELEMENT_QUITGAME,

	AVPMENU_ELEMENT_TEXTFIELD,
	AVPMENU_ELEMENT_DUMMYTEXTFIELD,
	AVPMENU_ELEMENT_TEXTFIELD_SMALLWRAPPED,
	AVPMENU_ELEMENT_TEXTSLIDER,
	AVPMENU_ELEMENT_DUMMYTEXTSLIDER,
	AVPMENU_ELEMENT_SPECIES_TEXTSLIDER,

	AVPMENU_ELEMENT_TEXTSLIDER_POINTER,      // text slider with a char** rather than a string index
	AVPMENU_ELEMENT_DUMMYTEXTSLIDER_POINTER, // text slider with a char** rather than a string index

	AVPMENU_ELEMENT_CHEATMODE_TEXTSLIDER,
	AVPMENU_ELEMENT_CHEATMODE_SPECIES_TEXTSLIDER,
	AVPMENU_ELEMENT_CHEATMODE_ENVIRONMENT_TEXTSLIDER,

	AVPMENU_ELEMENT_NUMBERFIELD,
	AVPMENU_ELEMENT_DUMMYNUMBERFIELD,

	AVPMENU_ELEMENT_LISTCHOICE,

	AVPMENU_ELEMENT_CONNECTIONCHOICE,

	AVPMENU_ELEMENT_VIDEOMODE,
	AVPMENU_ELEMENT_VIDEOMODEOK,

	AVPMENU_ELEMENT_KEYCONFIG,
	AVPMENU_ELEMENT_KEYCONFIGOK,
	AVPMENU_ELEMENT_RESETKEYCONFIG,

	AVPMENU_ELEMENT_ENDOFMENU,

	AVPMENU_ELEMENT_RESUMEGAME,
	AVPMENU_ELEMENT_RESTARTGAME,

	AVPMENU_ELEMENT_STARTMARINEDEMO,
	AVPMENU_ELEMENT_STARTPREDATORDEMO,
	AVPMENU_ELEMENT_STARTALIENDEMO,
	AVPMENU_ELEMENT_STARTLEVELWITHCHEAT,

	AVPMENU_ELEMENT_BUTTONSETTING,
	AVPMENU_ELEMENT_LOADMPCONFIG,
	AVPMENU_ELEMENT_SAVEMPCONFIG,
	AVPMENU_ELEMENT_DELETEMPCONFIG,
	AVPMENU_ELEMENT_RESETMPCONFIG,

	AVPMENU_ELEMENT_DIFFICULTYLEVEL,

	AVPMENU_ELEMENT_LOADIPADDRESS,

	AVPMENU_ELEMENT_USERPROFILE_DELETE,

	AVPMENU_ELEMENT_SAVESETTINGS,

	AVPMENU_ELEMENT_SAVEGAME,
	AVPMENU_ELEMENT_LOADGAME,
	AVPMENU_ELEMENT_VIEWKEY,   // a key on the rendering page. Its MaxValue is which of ViewKeyLine it is
	AVPMENU_ELEMENT_CUSTOMMAP_START, // starts the custom map chosen on the Custom maps page
	AVPMENU_ELEMENT_COOP_START, // a host starts the co-op campaign chosen on the page, for everyone with the map
	AVPMENU_ELEMENT_COOP_PLAYER, // one player's line on the Players page: their name and whether they're alive. Nothing happens when it's pressed.
	AVPMENU_ELEMENT_COOP_REMOVE, // a host takes this player out of the game. Its MaxValue is their place on the list of players
	AVPMENU_ELEMENT_JOINANSWER, // the host's answer to a player asking to join (online_screens_page.h): MaxValue 1 lets them in, 0 says no
	AVPMENU_ELEMENT_FINDAGAIN, // the page of games found's Refresh row, and Enter on its failure screen: fetches the open sessions again
};


enum AVPMENU_ELEMENT_INTERACTION_ID
{
	AVPMENU_ELEMENT_INTERACTION_SELECT,
	AVPMENU_ELEMENT_INTERACTION_DECREASE,
	AVPMENU_ELEMENT_INTERACTION_INCREASE,
};

typedef struct
{
	enum AVPMENU_ELEMENT_ID ElementID;

	union
	{
		// bjd - changed enum type to uint32_t
//		enum TEXTSTRING_ID TextDescription;
		uint32_t TextDescription;
//		enum AVPMENUGFX_ID GfxID;
		texID_t textureID;
	};

	union
	{
		// bjd - changed enum type to uint32_t
//		enum AVPMENU_ID MenuToGoTo;
		uint32_t MenuToGoTo;
		int MaxSliderValue;
		int MaxTextLength;
		int MaxValue; //for number fields
	};

	union
	{
		int *ToggleValuePtr;
		int *SliderValuePtr;
		char *TextPtr;
		int *NumberPtr;
		int Value;
	};

	union
	{
		// bjd - changed enum types to uint32_ts
//		enum TEXTSTRING_ID FirstTextSliderString;
//		enum TEXTSTRING_ID NumberFieldUnitsString;
		uint32_t FirstTextSliderString;
		uint32_t NumberFieldUnitsString;
		char** TextSliderStringPointer;
	};

	enum TEXTSTRING_ID HelpString;
	enum TEXTSTRING_ID NumberFieldZeroString; //special string for 0

	int Brightness;

} AVPMENU_ELEMENT;

typedef struct
{
	enum AVPMENU_FONT_ID FontToUse;
	enum TEXTSTRING_ID	MenuTitle;

	AVPMENU_ELEMENT		*MenuElements;
	enum AVPMENU_ID		ParentMenu;
	int					DefaultElement;

} AVPMENU;


typedef struct
{
	enum MENUSSTATE_ID	MenusState;

	enum AVPMENU_ID		CurrentMenu;
	AVPMENU_ELEMENT		*MenuElements;
	int					NumberOfElementsInMenu;
	int					CurrentlySelectedElement;
	int					MenuHeight;

	enum AVPMENU_FONT_ID FontToUse;

	size_t				PositionInTextField;

	int 				WidthLeftForText; //thing for checking whether user input will fit on screen

	unsigned int		UserEnteringText :1;
	unsigned int		UserEnteringNumber :1;
	unsigned int		UserChangingKeyConfig :1;
	unsigned int		ChangingPrimaryConfig :1;
	unsigned int		UserChangingViewKey :1;

} AVP_MENUS;


#define BRIGHTNESS_OF_HIGHLIGHTED_ELEMENT (ONE_FIXED)
#define BRIGHTNESS_OF_DARKENED_ELEMENT (ONE_FIXED/4)
#define BRIGHTNESS_CHANGE_SPEED (RealFrameTime*2)

#define MENU_TOPY		(150)
#define MENU_CENTREY	(280)
#define MENU_HEIGHT		(255)
#define MENU_CENTREX	(320)

#define MENU_ELEMENT_SPACING	(9)
#define MENU_FONT_HEIGHT		(21)
#define MENU_LEFTXEDGE			(30)
#define MENU_RIGHTXEDGE			(610)
#define MENU_BOTTOMYEDGE		(390)
#define MENU_TOPYEDGE			(130)
#define MENU_WIDTH (MENU_RIGHTXEDGE-MENU_LEFTXEDGE)


/* KJL 18:05:51 05/07/98 - multiplayer stuff */
typedef struct
{
	char Name[40];
	char levelIndex; //local level index
	char coopLobby; // kCoopLobbyNone unless this game was started from the co-op campaign page
	uint32_t host;
	uint16_t port;
	GUID Guid;
	bool AllowedToJoin;
	bool throughRelay;                               // a game from the relay server's list, joined through its region's relay (relay_list.h)
	char relayCode[kRelayCodeLetters];               // its code: the 8 letters, with no 0 after them
	char relayRegion[kRelayListRegionLetters + 1];   // its region, which says which relay
	uint8_t relayFingerprint[kNetIdentityFingerprintBytes];   // its host's fingerprint, from the list: who handshake B must find it's talking to
	// What the games page shows (online_screens.h). The host sent these, so they're hostile: the page draws the name only after
	// OnlineScreens_SafeName, and checks the numbers before it uses them.
	char levelName[kRelayListLevelLetters + 1];      // the level's name, as the host said it
	uint16_t gameStyle;                              // NGT_..., which says whether levelIndex is on the multiplayer or the co-op list
	uint8_t players;                                 // how many are playing, as the host said
	uint8_t maxPlayers;                              // and how many fit
} SESSION_DESC;

#define MAX_NO_OF_SESSIONS 10
extern SESSION_DESC SessionData[];
extern uint32_t NumberOfSessionsFound;


typedef struct
{
	unsigned char	SlotUsed;
	unsigned char	SavesLeft;
	unsigned char	Species;
	unsigned char	Episode;
	unsigned char	ElapsedTime_Hours;
	unsigned char	ElapsedTime_Minutes;
	unsigned char	ElapsedTime_Seconds;
	unsigned char	Difficulty;

	SYSTEMTIME		TimeStamp;

} SAVE_SLOT_HEADER;

#define NUMBER_OF_SAVE_SLOTS 8

#endif
