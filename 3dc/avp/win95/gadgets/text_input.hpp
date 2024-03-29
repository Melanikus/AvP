/*
	
	textin.hpp

	Created by DHM.

	DHM 23/1/98:
	------------

	Defines a abstract project-independent TextInputState class that can react to typing,
	special keys like "backspace" etc. in a sensible fashion.

	The idea is that each project can derive project-specific concrete classes and
	embed them in objects for that project's user interface system

	e.g.
		-	into a TextEntryGadget for AvP
		-	into a TELObject for HeadHunter

	etc.

	The pure virtual functions are to process carriage returns, and to provide feedback
	on errors (e.g. typing backspace when there's nothing left).
	
	There's a version setting: "LimitedLineLength":

		-	If this is set to true, the implementation involves a fixed-size
		array of ProjChars.

		-	If it's set to false, there's an unfinished implementation involving a
		linked list of ProjChars; don't use this option.

*/

#ifndef _textin_hpp
#define _textin_hpp 1

	#ifndef _textexp
	#include "text_expansion.hpp"
	#endif

/* Version settings *****************************************************/
	#define LimitedLineLength	TRUE

	#define SupportHistory		TRUE

	#define SupportAutomation	TRUE

	#define SupportCompletion	TRUE

	#if !LimitedLineLength

		#ifndef list_template_hpp
		#include "list_tem.hpp"
		#endif

	#endif


/* Constants  ***********************************************************/

/* Macros ***************************************************************/

/* Type definitions *****************************************************/
	#if SupportCompletion
	class ConsoleSymbol; // fully declared in CONSSYM.HPP
	#endif

	class TextInputState
	{
	// Friends:
		friend void TextExpansion :: TestForExpansions
		(
			TextInputState* pTextInputState_In
		);

	// Constants:
	private:
		#if LimitedLineLength
		enum
		{
			MAX_LENGTH_INPUT	= 250,
				// doesn't include a terminator character

			MAX_SIZE_INPUT		= (MAX_LENGTH_INPUT + 1)
				// does include a terminator character
		};
		#endif // Limited line length

		#if SupportHistory
		enum
		{
			MAX_LINES_HISTORY	= 32
		};
		#endif

	// Private data:
	private:		
		#if LimitedLineLength
		ProjChar ProjCh[ MAX_SIZE_INPUT ];
			// Null-terminated string "in-place"

		size_t NumChars;
			// number of chars befire the null terminator
		#else
		List<ProjChar> List_ProjChar;
		#endif

		#if SupportHistory
		List <SCString*> List_pSCString_History;
		SCString* pSCString_CurrentHistory;
			// Can be NULL; indicates no cycling through the history has yet occurred.
			// This does NOT own a reference to the string.
		#endif

		#if SupportAutomation
		size_t ManualPos;
			// all characters with index less than this were
			// typed "by hand", as opposed to being automatically
			// generated by completion-guessing or by histories    
		#endif

		#if SupportCompletion
		ConsoleSymbol* pConsoleSym_CurrentCompletion;
			// Can be NULL; indicates no cycling through the history has yet occurred.
		#endif


		size_t CursorPos;
			// cursor is to the left of the character with this index

		bool bForceUpperCase_Val;
		static bool bOverwrite_Val;
			// otherise it's insert mode

	public:
		virtual ~TextInputState();

		SCString& GetCurrentState(void);
			// returns a ref to a copy of the "string under construction" in its current state

		void CharTyped
		(
			char Ch
				// note that this _is _ a char
		);
		void Key_Backspace(void);
		void Key_End(void);
		void Key_Home(void);
		void Key_Left(void);
		void Key_Right(void);
		void Key_Delete(void);

		static bool bOverwrite(void)
		{
			return bOverwrite_Val;
		}
		static void ToggleTypingMode(void);
			// toggles overwrite/insert mode


	public:
		virtual void ProcessCarriageReturn(void) = 0;
	
	protected:
		virtual void TextEntryError(void) = 0;
			// called e.g. when no more text can be typed, or no more deleted
			// may make a beep noise

	// Protected methods:
	protected:
		// Constructor is protected since it's an abstract base class
		TextInputState
		(
			bool bForceUpperCase,
			char* pProjCh_Init // could be const
		);

		void TryToInsertAt
		(
			SCString* pSCString_ToInsert,
			size_t Pos_Where
		);

		int bOvertypeAt
		(
			ProjChar ProjCh_In,
			size_t Pos_Where
		);
			// return value: was overtype succesful?
		int bInsertAt
		(
			ProjChar ProjCh_In,
			size_t Pos_Where
		);
			// return value: was insertion succesful?

		void DeleteAt( size_t Pos );

		void Clear(void);


		#if SupportHistory
		void History_SelectNxt(void);
		void History_SelectPrv(void);

		void AddToHistory
		(
			SCString& SCString_ToAdd
		);
		#endif

		#if SupportAutomation
		void FullyManual(void);
			/*
				Flags the string as if all its content has been manually
				typed (call when the user types something that signifies he/she
				accepts something that might have been generated by completion/history
				code).
			*/
		bool bManualMatch
		(
			ProjChar* pProjCh
		) const;
		bool bManualMatchInsensitive
		(
			ProjChar* pProjCh
		) const;
			/* Returns true iff there's a match with the manually-typed prefix of
			the current state string and the input comparison string
			*/
		#endif

		#if SupportCompletion
		void Completion_SelectNxt(void);
		void Completion_SelectPrv(void);
		#endif


	// I'm not sure what interface I should supply; this is a stopgap so that I can write
	// rendering functions for AvP
	public:
		void SetString
		(
			SCString& SCString_ToUse
		);
		#if LimitedLineLength
		const ProjChar* GetProjChar(void) const
		{
			return &ProjCh[0];
		}
		size_t GetCursorPos(void) const
		{
			return CursorPos;
		}
		size_t GetNumChars(void) const
		{
			return NumChars;
		}
		#endif

	#if SupportHistory
	private:
		SCString* GetNxtMatchingHistory(void) const;
		SCString* GetPrvMatchingHistory(void) const;
			// will return NULL if it wraps around internally i.e. no matches
	#endif

	#if SupportCompletion
	private:
		ConsoleSymbol* GetNxtMatchingCompletion(void) const;
		ConsoleSymbol* GetPrvMatchingCompletion(void) const;
			// will return NULL if it wraps around internally i.e. no matches
	#endif


	}; // end of class TextInputState

/* Exported globals *****************************************************/

/* Function prototypes **************************************************/



/* End of the header ****************************************************/

#endif
