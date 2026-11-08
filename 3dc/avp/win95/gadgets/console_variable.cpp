// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   temporary text objects are let go automatically when the code using them ends, instead of by hand
// Modified 2026-11-07 by Melanikus for the AvP port:
//   a typed number is kept in range before it's worked out, so no number anyone types can go wrong in the sum (console_value.h)
//   a variable can be refused in some games, such as TIMESCALE in a network game
/*******************************************************************
 *
 *    DESCRIPTION: 	consvar.cpp
 *
 *    AUTHOR: David Malcolm
 *
 *    HISTORY:  Created 17/12/97
 *
 *******************************************************************/

/* Includes ********************************************************/
#include "3dc.h"
#include "console_variable.hpp"
#include "console_value.h"

#include <stdlib.h>
#include "strutil.h"
#define UseLocalAssert TRUE
#include "ourasert.h"

/* Version settings ************************************************/

/* Constants *******************************************************/

/* Macros **********************************************************/

/* Imported function prototypes ************************************/

/* Imported data ***************************************************/

/* Exported globals ************************************************/
	/*static*/ List <ConsoleVariable*> ConsoleVariable :: List_pConsoleVar;

/* Internal type definitions ***************************************/
	class ConsoleVariable_Simple_Int : public ConsoleVariable
	{
	public:
		ConsoleVariable_Simple_Int
		(
			int& Value_ToUse,
			ProjChar* pProjCh_ToUse,
			ProjChar* pProjCh_Description_ToUse,
			int MinVal_New,
			int MaxVal_New,
			bool Cheat = false
		);

		int GetValue(void) const;
		void SetValue(int Val_New);
		void SetValue(float Val_New);

	private:
		SCString* MakeRangeString(void);
		SCString* MakeValueString(int Val);

	private:
		int& theValue;
	};

	class ConsoleVariable_Simple_FixP : public ConsoleVariable
	{
	public:
		ConsoleVariable_Simple_FixP
		(
			int& Value_ToUse,
			ProjChar* pProjCh_ToUse,
			ProjChar* pProjCh_Description_ToUse,
			int MinVal_New,
			int MaxVal_New,
			bool Cheat = false
		);

		int GetValue(void) const;
		void SetValue(int Val_New);
		void SetValue(float Val_New);

	private:
		SCString* MakeRangeString(void);
		SCString* MakeValueString(int Val);

		static float FixP2Float(int FixP)
		{
			return
			(
				((float) FixP) / (ONE_FIXED)
			);
		}

	private:
		int& theValue;
	};

/* Internal function prototypes ************************************/

/* Internal globals ************************************************/

/* Exported function definitions ***********************************/
// class ConsoleVariable
// Factory method:
/*static*/ ConsoleVariable* ConsoleVariable :: MakeSimpleConsoleVariable_Int
(
	int& Value_ToUse,
	ProjChar* pProjCh_Symbol_ToUse,
	ProjChar* pProjCh_Description_ToUse,
	int MinVal_New,
	int MaxVal_New,
	bool Cheat
)
{
	/* PRECONDITION */
	GLOBALASSERT( pProjCh_Symbol_ToUse );
	GLOBALASSERT( pProjCh_Description_ToUse );

	/* CODE */
	return new ConsoleVariable_Simple_Int
	(
		Value_ToUse,
		pProjCh_Symbol_ToUse,
		pProjCh_Description_ToUse,
		MinVal_New,
		MaxVal_New,
		Cheat
	);
}

/*static*/ ConsoleVariable* ConsoleVariable::MakeSimpleConsoleVariable_FixP
(
	int& Value_ToUse,
	ProjChar* pProjCh_Symbol_ToUse,
	ProjChar* pProjCh_Description_ToUse,
	int MinVal_New,
	int MaxVal_New,
	bool Cheat
)
{
	/* PRECONDITION */
	GLOBALASSERT( pProjCh_Symbol_ToUse );
	GLOBALASSERT( pProjCh_Description_ToUse );

	/* CODE */
	return new ConsoleVariable_Simple_FixP
	(
		Value_ToUse,
		pProjCh_Symbol_ToUse,
		pProjCh_Description_ToUse,
		MinVal_New,
		MaxVal_New,
		Cheat
	);
}

ConsoleVariable::~ConsoleVariable()
{
	pSCString_Description->R_Release();

	// remove from the list
	List_pConsoleVar.delete_entry(this);
}

/*static*/ bool ConsoleVariable :: Process( ProjChar* pProjCh_In )
{
	// used for proccesing input text.  Could decide that the user
	// was requesting the value of a variable, or was setting a new
	// value etc; if so, acts accordingly.
	// return value = was any processing performed?

	// Check to see if there's a match between the entire
	// input string and each console command
	{
		for
		(
			LIF<ConsoleVariable*> oi(&List_pConsoleVar);
			!oi.done();
			oi.next()
		)
		{
			if
			(
				oi()->ThisIsACheat
				?
				STRUTIL_SC_Strequal //case sensitive comparisons for cheats
				(
					pProjCh_In,
					oi()->pSCString_Symbol->pProjCh()
				)
				:
				STRUTIL_SC_Strequal_Insensitive
				(
					pProjCh_In,
					oi()->pSCString_Symbol->pProjCh()
				)
			)
			{
				oi()->Display();
				return TRUE;
			}
		}
	}

	// Otherwise check to see if there's a match between the front
	// of the console command up to the first space (if any)
	// with the rest being treated as a number
	{
		// Find the point in the input text where the first word
		// ends (if there is one...):
		ProjChar* pProjCh_Search = pProjCh_In;
		int NumChars = 0;

		while
		(
			(*pProjCh_Search != '\0')
			&&
			(*pProjCh_Search != ' ')
		)
		{
			pProjCh_Search++;
			NumChars++;
		}

		if ( *pProjCh_Search == '\0' )
		{
			// then there were no word breaks; stop
			return FALSE;
		}

		if ( NumChars < 1 )
		{
			return FALSE;
		}

		for
		(
			LIF<ConsoleVariable*> oi(&List_pConsoleVar);
			!oi.done();
			oi.next()
		)
		{
			// LOCALISEME():
			if
			(
				0 == strncmp
				(
					pProjCh_In,
					oi()->pSCString_Symbol->pProjCh(),
					NumChars
				)
			)
			{
				if (strchr(pProjCh_Search,'.'))
				{
					// interpret as fraction
					float NewValue = atof(pProjCh_Search);

					oi()->ProcessSetValue( NewValue );

					return TRUE;
				}
				else
				{
					// interpret as int. strtoll stops at the largest or smallest number it can hold, where atoi's answer for digits that go on
					// too long isn't defined
					int NewValue = ConsoleValue_ToInt(strtoll(pProjCh_Search, NULL, 10));

					oi()->ProcessSetValue( NewValue );

					return TRUE;
				}
			}
		}
	}

	// If you get here, no processing has been performed:
	return FALSE;
}

/*static*/ void ConsoleVariable :: ListAllVariables(void)
{
	RefHolder<SCString> pSCString_Temp(new SCString("LIST OF ALL CONSOLE VARIABLES:"));

	pSCString_Temp -> SendToScreen();


	for
	(
		LIF<ConsoleVariable*> oi(&List_pConsoleVar);
		!oi.done();
		oi.next()
	)
	{
		oi()->Display();
	}
}
// protected:
ConsoleVariable :: ConsoleVariable
(
	ProjChar* pProjCh_ToUse,
	ProjChar* pProjCh_Description_ToUse,
	int MinVal_New,
	int MaxVal_New,
	bool Cheat

) : ConsoleSymbol
	(
		pProjCh_ToUse
	),
	pSCString_Description
	(
		new SCString( pProjCh_Description_ToUse )
			// constructor for the SCString adds the required reference
	),
	MinVal(MinVal_New),
	MaxVal(MaxVal_New),
	pRefusedNow(0),
	pProjCh_RefusedWhy(0)
{
	/* PRECONDITION */
	{
	}

	/* CODE */
	{
		ThisIsACheat = Cheat;
	
		// add to list of all console variables
		List_pConsoleVar.add_entry(this);
	}
}

// private:
void ConsoleVariable :: Display(void)
{
	// used by the list display and to interrogate an individual variable
	SCString* pSCString_Temp1 = new SCString
	(
		" = "
	);

	SCString* pSCString_Temp2 = MakeValueString( GetValue() );

	SCString* pSCString_Temp3 = MakeRangeString();

	RefHolder<SCString> pSCString_Out(new SCString
	(
		pSCString_Symbol,
		pSCString_Temp1,
		pSCString_Temp2,
		pSCString_Temp3,
		pSCString_Description
	));

	pSCString_Temp3 ->R_Release();
	pSCString_Temp2 ->R_Release();
	pSCString_Temp1 ->R_Release();

	pSCString_Out -> SendToScreen();

}


void ConsoleVariable :: ProcessSetValue
(
	int Val_New
)
{
	// used by command processor; sets the value and outputs
	// a message

	if (IsRefusedNow())
	{
		return;
	}

	int OldValue = GetValue();

	SetValue( Val_New );

	OutputResultOfSetValue( OldValue );
}

void ConsoleVariable :: ProcessSetValue
(
	float Val_New
)
{
	// used by command processor; sets the value and outputs
	// a message

	if (IsRefusedNow())
	{
		return;
	}

	int OldValue = GetValue();

	SetValue( Val_New );

	OutputResultOfSetValue( OldValue );
}

void ConsoleVariable :: RefuseWhen(bool (*refusedNow)(void), ProjChar* pProjCh_Why)
{
	pRefusedNow = refusedNow;
	pProjCh_RefusedWhy = pProjCh_Why;
}

bool ConsoleVariable :: IsRefusedNow(void)
{
	if (pRefusedNow == 0 || !pRefusedNow())
	{
		return false;
	}

	RefHolder<SCString> pSCString_Out(new SCString(pProjCh_RefusedWhy ? pProjCh_RefusedWhy : (ProjChar*)"NOT NOW"));
	pSCString_Out -> SendToScreen();
	return true;
}

void ConsoleVariable :: OutputResultOfSetValue( int OldVal )
{
	int NewValue = GetValue();

	// Output result
	{
		SCString* pSCString_Temp1 = new SCString
		(
			" "
		);

		SCString* pSCString_Temp2 = MakeValueString
		(
			OldVal
		);

		SCString* pSCString_Temp3 = new SCString
		(
			" -> "
		);

		SCString* pSCString_Temp4 = MakeValueString
		(
			NewValue
		);

		RefHolder<SCString> pSCString_Out(new SCString
		(
			pSCString_Symbol,
			pSCString_Temp1,
			pSCString_Temp2,
			pSCString_Temp3,
			pSCString_Temp4
		));

		pSCString_Temp4 ->R_Release();
		pSCString_Temp3 ->R_Release();
		pSCString_Temp2 ->R_Release();
		pSCString_Temp1 ->R_Release();

		pSCString_Out -> SendToScreen();
		
	}
	
}


/* Internal function definitions ***********************************/
ConsoleVariable_Simple_Int :: ConsoleVariable_Simple_Int
(
	int& Value_ToUse,
	ProjChar* pProjCh_ToUse,
	ProjChar* pProjCh_Description_ToUse,
	int MinVal_New,
	int MaxVal_New,
	bool Cheat
) : theValue( Value_ToUse ),
	ConsoleVariable
	(
		pProjCh_ToUse,
		pProjCh_Description_ToUse,
		MinVal_New,
		MaxVal_New,
		Cheat
	)
{
	/* PRECONDITION */
	{
		GLOBALASSERT( pProjCh_ToUse );
		GLOBALASSERT( pProjCh_Description_ToUse );
	}

	/* CODE */
	{
	}
}

int ConsoleVariable_Simple_Int :: GetValue(void) const
{
	return theValue;
}

void ConsoleVariable_Simple_Int :: SetValue(int Val_New)
{
	// Ensure bounded:
	if ( Val_New > MaxVal )
	{
		Val_New = MaxVal;
	}

	if ( Val_New < MinVal )
	{
		Val_New = MinVal;
	}

	theValue = Val_New;
}

void ConsoleVariable_Simple_Int :: SetValue(float Val_New_F )
{
	// Kept in range while it's still a decimal, then made whole. "Not a number" leaves the value as it was.
	int Val_New;

	if (ConsoleValue_FromDecimal(Val_New_F, 1.0, MinVal, MaxVal, &Val_New))
	{
		theValue = Val_New;
	}
}

// private:
SCString* ConsoleVariable_Simple_Int :: MakeRangeString(void)
{
	SCString* pSCString_Temp2_1 = new SCString
	(
		" INT:("
	);

	SCString* pSCString_Temp2_2 = MakeValueString
	(
		MinVal
	);

	SCString* pSCString_Temp2_3 = new SCString
	(
		","
	);

	SCString* pSCString_Temp2_4 = MakeValueString
	(
		MaxVal
	);

	SCString* pSCString_Temp2_5 = new SCString
	(
		") "
	);

	SCString* pSCString_Return = new SCString
	(
		pSCString_Temp2_1,
		pSCString_Temp2_2,
		pSCString_Temp2_3,
		pSCString_Temp2_4,
		pSCString_Temp2_5
	);

	pSCString_Temp2_5 ->R_Release();
	pSCString_Temp2_4 ->R_Release();
	pSCString_Temp2_3 ->R_Release();
	pSCString_Temp2_2 ->R_Release();
	pSCString_Temp2_1 ->R_Release();

	return pSCString_Return;
}

SCString* ConsoleVariable_Simple_Int :: MakeValueString(int Val)
{
	return new SCString
	(
		Val
	);
}


ConsoleVariable_Simple_FixP :: ConsoleVariable_Simple_FixP
(
	int& Value_ToUse,
	ProjChar* pProjCh_ToUse,
	ProjChar* pProjCh_Description_ToUse,
	int MinVal_New,
	int MaxVal_New,
	bool Cheat
) : theValue( Value_ToUse ),
	ConsoleVariable
	(
		pProjCh_ToUse,
		pProjCh_Description_ToUse,
		MinVal_New,
		MaxVal_New,
		Cheat
	)
{
	/* PRECONDITION */
	{
		GLOBALASSERT( pProjCh_ToUse );
		GLOBALASSERT( pProjCh_Description_ToUse );
	}

	/* CODE */
	{
	}
}

int ConsoleVariable_Simple_FixP :: GetValue(void) const
{
	return theValue;
}

void ConsoleVariable_Simple_FixP :: SetValue(int Val_New)
{
	// A whole number of units times ONE_FIXED, worked out in numbers big enough for any int, then kept in range
	theValue = ConsoleValue_WholeAsFixed(Val_New, MinVal, MaxVal);
}

void ConsoleVariable_Simple_FixP :: SetValue(float Val_New_F )
{
	// Times ONE_FIXED and kept in range while it's still a decimal, then made whole. "Not a number" leaves the value as it was.
	int Val_New;

	if (ConsoleValue_FromDecimal(Val_New_F, ONE_FIXED, MinVal, MaxVal, &Val_New))
	{
		theValue = Val_New;
	}
}

// private:
SCString* ConsoleVariable_Simple_FixP :: MakeRangeString(void)
{
	SCString* pSCString_Temp2_1 = new SCString
	(
		" FRAC:("
	);

	SCString* pSCString_Temp2_2 = MakeValueString
	(
		MinVal
	);

	SCString* pSCString_Temp2_3 = new SCString
	(
		","
	);

	SCString* pSCString_Temp2_4 = MakeValueString
	(
		MaxVal
	);

	SCString* pSCString_Temp2_5 = new SCString
	(
		") "
	);

	SCString* pSCString_Return = new SCString
	(
		pSCString_Temp2_1,
		pSCString_Temp2_2,
		pSCString_Temp2_3,
		pSCString_Temp2_4,
		pSCString_Temp2_5
	);

	pSCString_Temp2_5 ->R_Release();
	pSCString_Temp2_4 ->R_Release();
	pSCString_Temp2_3 ->R_Release();
	pSCString_Temp2_2 ->R_Release();
	pSCString_Temp2_1 ->R_Release();

	return pSCString_Return;
}

SCString* ConsoleVariable_Simple_FixP::MakeValueString(int Val)
{
	return new SCString(FixP2Float(Val));
}
