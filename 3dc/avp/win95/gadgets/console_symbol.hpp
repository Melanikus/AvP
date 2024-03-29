/*
	
	conssym.hpp

	"Console symbols" - a base class for managing auto-completion of typing.

	Console commands and variables are derived from this class

*/

#ifndef _conssym_hpp
#define _conssym_hpp 1

	#if ( defined( __WATCOMC__ ) || defined( _MSC_VER ) )
		#pragma once
	#endif

	#ifndef _scstring
	#include "scstring.hpp"
	#endif	

/* Version settings *****************************************************/

/* Constants  ***********************************************************/

/* Macros ***************************************************************/

/* Type definitions *****************************************************/
	class ConsoleSymbol
	{
		friend class TextInputState;
			/*	WARNING!

				TextInputState objects can refer to the list of ConsoleSymbols
				in order to iterate through possible completion strings.  For
				this reason, don't destroy ConsoleSymbol objects if TextInputState
				objects exist...
				(I believe I've asserted against all such possible failures)
			*/

	public:
	
	bool ThisIsACheat;
	protected:
		ConsoleSymbol
		(
			ProjChar* pProjCh_ToUse
		);
		virtual ~ConsoleSymbol();

		SCString* pSCString_Symbol;


	public:
		SCString* GetpSCString(void) const
		{
			return pSCString_Symbol; 
		}


	private:
		static List <ConsoleSymbol*> List_pConsoleSym;
	};	// suggested naming: "ConsoleSym"

/* Exported globals *****************************************************/

/* Function prototypes **************************************************/



/* End of the header ****************************************************/

#endif
