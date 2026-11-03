/*
	
	modcmds.hpp

*/
// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   removed the module list command, which nothing could reach

#ifndef _modcmds
#define _modcmds 1

	#ifndef MODULE_INCLUDED
	#include "module.h"
	#endif

/* Version settings *****************************************************/

/* Constants  ***********************************************************/

/* Macros ***************************************************************/

/* Type definitions *****************************************************/

/* Exported globals *****************************************************/

/* Function prototypes **************************************************/
	namespace ModuleCommands
	{
		void TryToTeleport(char* UpperCasePotentialModuleName);

		MODULE* FindModule(char* UpperCasePotentialModuleName);
			// allowed to return NULL if no match

		void TeleportPlayerToModule(MODULE* pModule_Dst);
	};
	


/* End of the header ****************************************************/

#endif
