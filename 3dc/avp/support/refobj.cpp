/*******************************************************************
 *
 *    DESCRIPTION: 	refobj.cpp
 *
 *    AUTHOR: David Malcolm
 *
 *    HISTORY:  Created 15/9/97
 *
 *******************************************************************/
// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   deleted the reference-tracking debug code, which was switched off and never compiled

/* Includes ********************************************************/
#include "3dc.h"
#include "refobj.hpp"


	#define UseLocalAssert TRUE
	#include "ourasert.h"

/* Version settings ************************************************/

/* Constants *******************************************************/

/* Macros **********************************************************/

/* Imported function prototypes ************************************/

/* Imported data ***************************************************/

/* Exported globals ************************************************/
	char const* refobj_fail_addref = "Failure in R_AddRef()\n";
	char const* refobj_fail_release = "Failure in R_Release()\n";
	char const* refobj_fail_destructor = "Failure in Destructor()\n";

/* Internal type definitions ***************************************/
/* Internal function prototypes ************************************/

/* Internal globals ************************************************/

/* Exported function definitions ***********************************/

// class RefCountObject
/* Internal function definitions ***********************************/