/*******************************************************************
 *
 *    DESCRIPTION: 	iofocus.cpp
 *
 *    AUTHOR: David Malcolm
 *
 *    HISTORY:  Created 21/11/97
 *
 *******************************************************************/

/* Includes ********************************************************/
#include "3dc.h"
#include "iofocus.h"
#include "gadget.h"
#include "menus.h"
#include "psnd.h"
#define UseLocalAssert TRUE
#include "ourasert.h"

/* Version settings ************************************************/

/* Constants *******************************************************/

/* Macros **********************************************************/

/* Imported function prototypes ************************************/
extern int InGameMenusAreRunning(void);

/* Imported data ***************************************************/


/* Exported globals ************************************************/

/* Internal type definitions ***************************************/

/* Internal function prototypes ************************************/

/* Internal globals ************************************************/
static bool iofocus_AcceptTyping = false;
static int ioFocus = IOFOCUS_GAME;

/* Exported function definitions ***********************************/
bool IOFOCUS_AcceptControls(void)
{
	return !iofocus_AcceptTyping;
}

bool IOFOCUS_AcceptTyping(void)
{
	return iofocus_AcceptTyping;
}

int IOFOCUS_Get()
{
	return ioFocus;
}

void IOFOCUS_Set(int focus)
{
	ioFocus = focus;
}

void IOFOCUS_Toggle(void)
{
	#if CONSOLE_DEBUGGING_COMMANDS_ACTIVATED||!(PREDATOR_DEMO||MARINE_DEMO||ALIEN_DEMO)
	if (InGameMenusAreRunning()) return;;

	iofocus_AcceptTyping = !iofocus_AcceptTyping;
	if (iofocus_AcceptTyping)
	{
		Sound_Play(SID_CONSOLE_ACTIVATES,NULL);
	}
	else
	{
		Sound_Play(SID_CONSOLE_DEACTIVATES,NULL);
		RemoveTheConsolePlease();
	}
	#endif
}


/* Internal function definitions ***********************************/