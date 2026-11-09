/*KJL***************************************
*    Language Internationalization Code    *
***************************************KJL*/
// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   the language file is read only up to its end, an unknown text id gives empty text, and the error
//   message about a bad language file has a size limit
// Modified 2026-11-05 by Melanikus for the AvP port:
//   the rendering page's text comes from here. A stock language.txt has no such strings, and a player's file stays as it is
//   the name of the marine's shoulder lamp key comes from here too
//   the text of the rendering page's lines for choosing a key
// Modified 2026-11-07 by Melanikus for the AvP port:
//   the rendering page's two lamp lines: whether lamps cast shadows, and whether their beams show in the air
// Modified 2026-11-06 by Melanikus for the AvP port:
//   the names for the new classes, Allow Staff and the sidearm
//   the new guns' words come from here
//   the words for choosing a campaign on the level row
// Modified 2026-11-07 by Melanikus for the AvP port:
//   the words a player sees during a co-op game
//   the words of the single player Custom maps page. Credit: Melanikus and Bobblen (the custom single-player maps follow Bobblen's avp-forever, rewritten)
// Modified 2026-11-08 by Melanikus for the AvP port:
//   the words of the sound pages' Room echo row. Reported by Bobblen; the setting follows the idea in his avp-forever, rewritten
//   the words of the co-op campaign pages. Credit: Melanikus and Bobblen (the co-op campaign system follows Bobblen's avp-forever, rewritten)
// Modified 2026-11-08 by Melanikus for the AvP port:
//   the words of the rendering page's HUD size line (HUD-SCALE-1). Reported by Bobblen; follows his avp-forever (commit a53f482), rewritten
// Modified 2026-11-09 by Melanikus for the AvP port:
//   the Join page's first row, "Find Games", and its help: the search asks this network and the online list of games, not only the
//   local network that the 1999 "Find Local Servers" said
//   the page of games found's Search box and Refresh row, and their help
#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "gamedef.h"
#include "langenum.h"
#include "language.h"
#include "huffman.hpp"

// DHM 12 Nov 97: hooks for C++ string handling code:
#include "string_table.hpp"

#define UseLocalAssert TRUE
#include "ourasert.h"
#include "menus.h"
#include "logString.h"

void ChangeWindowsSize(uint32_t width, uint32_t height);

static char EmptyString[]="";

static char *TextStringPtr[MAX_NO_OF_TEXTSTRINGS]={EmptyString};
static char *TextBufferPtr;

// Text that isn't in anyone's language.txt. That file belongs to the player's copy of the game, and it's packed, so it isn't ours to change, and these are in English whatever the language.
// They come after everything the file holds, in the order of the enum in langenum.h.
static char OurTextStrings[][200] =
{
	"Rendering options",
	"How the world is lit, and whether things cast shadows.",
	"Rendering",
	"How the world is lit. A choice this card can't draw is skipped. Maps are read when a level loads, so changing to a choice that uses them shows them from the next level.",
	"Gouraud",
	"Per-pixel (flat)",
	"Per-pixel + normals",
	"Relief map",
	"Shadows",
	"Whether lights cast shadows.",
	"This does nothing here: this card can't draw shadows.",
	"Auto-generate maps",
	"Works out maps for textures that have none. Turning it on takes effect from the next level.",
	"This does nothing with Gouraud or Per-pixel (flat): only Per-pixel + normals and Relief map read maps.",
	"Shoulder lamp",
	"Rendering key",
	"Changes how the world is lit. Press Enter, then the key you want. Backspace means no key, and Escape leaves it as it is.",
	"Shadows key",
	"Turns shadows on and off. Press Enter, then the key you want. Backspace means no key, and Escape leaves it as it is.",
	"Dev views key",
	"It only works when the game was started with -dev, and never in a network game. Press Enter, then the key you want. Backspace means no key, and Escape leaves it as it is.",
	"That key can't be used: the game keeps it for itself. This line still has its old key.",
	"That key can't be used: another line here has it. This line still has its old key.",
	"That key can't be used: it's one of your controls. This line still has its old key.",
	"One of your controls has this key too. The control wins, so this line does nothing for that species. Choose another key here, or change the control.",
	"Shotgun",
	"Sniper Rifle",
	"Revolver",
	"M79",
	"Sniper Ammo",
	"Revolver Ammo",
	"M79 Grenades",
	"M79 Fire",
	"M79 Frag",
	"Stock open",
	"Stock closed",
	"Reload",
	"Staff",
	"Marine - Shotgun",
	"Marine - Revolver",
	"Marine - M79",
	"Max Shotgun Marines",
	"Max Revolver Marines",
	"Max M79 Marines",
	"Allow Shotgun",
	"Allow Revolver",
	"Allow M79",
	"Allow Staff",
	"Sidearm",
	"For marine specialists",
	"Pistol",
	"Revolver",
	"Campaign",
	"Plays as single player does: everyone is this species, with its start, kit and pick-ups",
	"For a campaign's level. The host chooses, and everyone plays it",
	"Custom maps",
	"Play a map someone has made for single player. The maps in avp_rifs/Custom that are made for single player are listed here.",
	"Map",
	"Which map. A map's own picture shows above, if it comes with one.",
	"None found",
	"Play as",
	"Who you play. A map made for one of them may not suit the others.",
	"How hard the map is.",
	"Start the map. Finishing it unlocks nothing and counts towards no campaign. You can't save on a custom map yet.",
	"Co-op campaign",
	"Play a campaign with friends. Others can join as soon as you open this page, and you start when everyone is here.",
	"Which campaign to play. You can all play one of the three, or a custom map made for single player.",
	"Marine",
	"Predator",
	"Alien",
	"Custom map",
	"Start at",
	"The level to start from, or which custom map.",
	"Who everyone plays. Each campaign has its own, so this only changes for a custom map.",
	"Kit between levels",
	"Keep: each level starts with what you were carrying, or what the level gives, whichever is more. Reset: each level gives its own kit. If you die, you get the level's own kit.",
	"Reset",
	"Keep",
	"Players joined",
	"How many are here, with you.",
	"Without the map",
	"They stay behind when you start. Maps aren't sent over the network, so they need their own copy.",
	"Nobody",
	"and %d more",
	"Start, for everyone who has the map.",
	"The map",
	"Maps aren't sent over the network, so you need your own copy of a custom map, in avp_rifs/Custom.",
	"Waiting for the host",
	"You have this map",
	"Missing",
	"Different version",
	"The game has started without you, because you don't have this map.",
	"The host chooses what's played. You'll go when the host starts.",
	"Remove",
	"Takes this player out of the game. They can't come back into this game from the same address.",
	"The host removed you from this game.",
	"alive",
	"out until the next level",
	"joining...",
	"You're out until the next level. Operate watches someone else.",
	"Level complete:",
	"Waiting for:",
	"The game carries on while you're here.",
	"Players",
	"The host has left the game.",
	"You're the host: leaving ends the game for everyone.",
	"Campaign complete",
	"Map complete",
	"Room echo",
	"The echo each room adds to sounds (EAX reverb). Turn it off for dry sound, if it's too strong on your speakers or headphones.",
	"Lamps cast shadows",
	"Whether marines' shoulder lamps are real lights that cast shadows, or painted on. Real ones cost more to draw.",
	"This does nothing here: a lamp is only a real light with a Per-pixel or Relief map choice of Rendering, and Shadows on.",
	"Lamp beams",
	"Whether other marines' lamps show their beam in the air. Off, a lamp still lights what it points at.",
	"This does nothing here: with Gouraud Rendering a lamp's beam is its only light, so it always shows.",
	"Find Games",
	"Press ENTER to look for games on this network and on the online list of games.",
	"Search",
	"Type part of a game's name to show only the games with it. What you type stays on this PC.",
	"Refresh",
	"Press ENTER to fetch the open sessions again.",
	"HUD size",
	"How big the health, ammo, motion tracker and other on-screen readings are. The middle is the usual size.",
};

static_assert(sizeof(OurTextStrings) / sizeof(OurTextStrings[0]) == MAX_NO_OF_TEXTSTRINGS - TEXTSTRING_RENDERING_TITLE, "one line of text for each of our ids in langenum.h");

// moved here from langplat.c
char *LanguageFilename[] = 
{
	ENGLISH_TEXT_FILENAME,	/* I_English */ 
	ENGLISH_TEXT_FILENAME,	/* I_French  */
	ENGLISH_TEXT_FILENAME,	/* I_Spanish */ 
	ENGLISH_TEXT_FILENAME,	/* I_German  */
	ENGLISH_TEXT_FILENAME,	/* I_Italian */
	ENGLISH_TEXT_FILENAME,	/* I_Swedish */
}; 

void InitTextStrings(void)
{
	char *textPtr;
	char *filenamePtr;

	GLOBALASSERT(AvP.Language >= 0);
	GLOBALASSERT(AvP.Language < I_MAX_NO_OF_LANGUAGES);
	
	#if MARINE_DEMO
	filenamePtr = "menglish.txt";
	#elif ALIEN_DEMO
	filenamePtr = "aenglish.txt";
	#elif PREDATOR_DEMO
	filenamePtr = "english.txt";
	#else
	filenamePtr = "language.txt";
	#endif

	size_t textSize = 0;
	char *textEnd;

	// the buffer ends with an extra 0 byte (LoadTextFileWithSize)
	TextBufferPtr = LoadTextFileWithSize(filenamePtr, &textSize);

	if (TextBufferPtr == NULL) 
	{
		// have to quit if this file isn't available
		char message[100];
		_snprintf(message, sizeof(message) - 1, "Unable to load language file: %s\n", filenamePtr);
		message[sizeof(message) - 1] = '\0';

		ChangeWindowsSize(1, 1);
		avp_MessageBox(message, MB_OK);
		avp_exit(1);
		return;
	}

	if (!strncmp(TextBufferPtr, "REBCRIF1", 8))
	{
		// checked decompression: bounded by the file's size, NULL for a package that cannot be decoded safely
		// (a truncated file)
		int uncompressedSize = (textSize >= sizeof(HuffmanPackage)) ? ((HuffmanPackage*)TextBufferPtr)->UncompressedDataSize : 0;
		textPtr = HuffmanDecompressChecked((HuffmanPackage*)(TextBufferPtr), (textSize > 0x7fffffff) ? 0x7fffffff : (int)textSize);
		DeallocateMem(TextBufferPtr);
		TextBufferPtr = textPtr;

		if (textPtr == NULL)
		{
			char message[100];
			// Write the error text with a size limit. message holds 100 bytes: _snprintf writes at most 99 of them, and
			// the last byte is then set to 0, the marker that ends a text. So however long the file name is, the text
			// is cut short instead of running past the end of message (today the name is always our own short one).
			_snprintf(message, sizeof(message) - 1, "Unable to load language file: %s\n", filenamePtr);
			message[sizeof(message) - 1] = '\0';

			ChangeWindowsSize(1, 1);
			avp_MessageBox(message, MB_OK);
			avp_exit(1);
			return;
		}

		// the decompressed buffer has spare bytes after the data (HuffmanDecompressChecked): end it with a 0
		textEnd = textPtr + uncompressedSize;
		*textEnd = 0;
	}
	else
	{
		textPtr = TextBufferPtr;
		textEnd = textPtr + textSize;
	}

	// check if language file is not from Gold version
	if (strncmp(textPtr, "REBINFF2", 8))
	{
		char message[100];
		_snprintf(message, sizeof(message) - 1, "File %s is not compatible with Gold Edition\n", filenamePtr);
		message[sizeof(message) - 1] = '\0';

		ChangeWindowsSize(1, 1);
		avp_MessageBox(message, MB_OK+MB_SYSTEMMODAL);
		avp_exit(1);
	}

	AddToTable(EmptyString);

	// both scans stop at textEnd (the 0 byte after the data): a truncated file used to run them past the buffer
	// and write the terminator there. Ids for which the file has no (complete) string get EmptyString, like id 0.
	bool truncated = false;
	for (int i = 1; i < TEXTSTRING_RENDERING_TITLE; i++)
	{
		// scan for a quote mark
		while ((textPtr < textEnd) && (*textPtr != '"'))
		{
			textPtr++;
		}

		if (textPtr >= textEnd)
		{
			// no more strings in the file (every later id ends up here too)
			truncated = true;
			TextStringPtr[i] = EmptyString;
			AddToTable(TextStringPtr[i]);
			continue;
		}
		textPtr++;

		// now pointing to a text string after quote mark
		TextStringPtr[i] = textPtr;

		// scan for a quote mark
		while ((textPtr < textEnd) && (*textPtr != '"'))
		{
			textPtr++;
		}

		if (textPtr < textEnd)
		{
			// change quote mark to zero terminator
			*textPtr = 0;
		}
		else
		{
			// no closing quote: the string runs to the end of the data, where the 0 byte ends it
			truncated = true;
		}

		AddToTable(TextStringPtr[i]);
	}

	for (int i = TEXTSTRING_RENDERING_TITLE; i < MAX_NO_OF_TEXTSTRINGS; i++)
	{
		TextStringPtr[i] = OurTextStrings[i - TEXTSTRING_RENDERING_TITLE];
		AddToTable(TextStringPtr[i]);
	}

	if (truncated)
	{
		LogErrorString(std::string("language file ") + filenamePtr + " ends before all the text strings; the missing ones are empty");
	}
}

void KillTextStrings(void)
{
	UnloadTextFile(LanguageFilename[AvP.Language], TextBufferPtr);

	UnloadTable();
}

char *GetTextString(enum TEXTSTRING_ID stringID)
{
	LOCALASSERT(stringID < MAX_NO_OF_TEXTSTRINGS);

	// an id from outside the table (save file, level data, a computed offset) gets the empty string instead of a read
	// past TextStringPtr (LOCALASSERT is empty in Release)
	if ((int)stringID < 0 || (int)stringID >= MAX_NO_OF_TEXTSTRINGS)
	{
		return EmptyString;
	}

	return TextStringPtr[stringID];
}
