// AvP port by Barry Duncan.
// Modified 2026-11-04 by Melanikus for the AvP port:
//   a text number from outside the table gets the empty string, where only an assert stood before

/*******************************************************************
 *
 *    DESCRIPTION: 	strtab.cpp
 *
 *    AUTHOR: David Malcolm
 *
 *    HISTORY:  Created 12 Nov 97
 *
 *******************************************************************/

/* Includes ********************************************************/
#include "3dc.h"
#include "string_table.hpp"

	#define UseLocalAssert TRUE
	#include "ourasert.h"

/* Version settings ************************************************/

/* Constants *******************************************************/

/* Macros **********************************************************/

/* Imported function prototypes ************************************/

/* Imported data ***************************************************/

/* Exported globals ************************************************/
#if OnlyOneStringTable
	/*static*/ SCString* StringTable :: pSCString[ MAX_NO_OF_TEXTSTRINGS ];
	/*static*/ unsigned int StringTable :: NumStrings = 0;
#endif

/* Internal type definitions ***************************************/

/* Internal function prototypes ************************************/

/* Internal globals ************************************************/

/* Exported function definitions ***********************************/
// class StringTable
// public:
/*STRINGTABLE_DECL_SPECIFIER*/ SCString& StringTable :: GetSCString
(
	enum TEXTSTRING_ID stringID
)
{
	/* CODE */
	{
		// The number can come from a level's mission data. One from outside the table gets the first entry, which is the
		// empty string, the same as GetTextString does.
		if ( (int)stringID < 0 || (int)stringID >= (int)NumStrings )
		{
			stringID = (enum TEXTSTRING_ID)0;
		}

		pSCString[ stringID ] -> R_AddRef();
		return *( pSCString[ stringID ] );
	}
}


/*STRINGTABLE_DECL_SPECIFIER*/ void StringTable :: Add( ProjChar* pProjChar )
{
	/* PRECONDITION */
	{
		GLOBALASSERT( NumStrings < MAX_NO_OF_TEXTSTRINGS );
		GLOBALASSERT( pSCString[ NumStrings ] == 0 );
	}

	/* CODE */
	{
		pSCString[ NumStrings++ ] = new SCString( pProjChar );
	}
}

#if OnlyOneStringTable
/*static*/ void StringTable :: Unload(void)
{
	/* PRECONDITION */
	{
	}

	/* CODE */
	{
		while ( NumStrings )
		{
			pSCString[ --NumStrings ] -> R_Release();
		}
	}
}
#else
StringTable :: StringTable()
{
	NumStrings = 0;
}

StringTable :: ~StringTable()
{
	while ( NumStrings )
	{
		pSCString[ --NumStrings ] -> R_Release();
	}
}
#endif


// private:

// C-callable functions:
#if OnlyOneStringTable
extern void AddToTable( ProjChar* pProjChar )
{
	/* PRECONDITION */
	{
		GLOBALASSERT( pProjChar );
	}

	/* CODE */
	{
		StringTable :: Add( pProjChar );
	}
}

extern void UnloadTable(void)
{
	/* PRECONDITION */
	{
	}

	/* CODE */
	{
		StringTable :: Unload();
	}
}
#endif



/* Internal function definitions ***********************************/
