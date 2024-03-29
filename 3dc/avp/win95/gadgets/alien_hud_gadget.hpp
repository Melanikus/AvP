/*
	
	ahudgadg.hpp

	Alien HUD gadget; a concrete derived class of abstract class "HUDGadget"

*/

#ifndef _ahudgadg
#define _ahudgadg

	#ifndef _hudgadg
	#include "hud_gadget.hpp"
	#endif

/* Version settings *****************************************************/

/* Constants  ***********************************************************/

/* Macros ***************************************************************/

/* Type definitions *****************************************************/
	#if UseGadgets
	class TextEntryGadget; // fully declared in TEXTIN.HPP

	class AlienHUDGadget : public HUDGadget
	{
	public:
		void Render
		(
			const struct r2pos& R2Pos,
			const struct r2rect& R2Rect_Clip,
			int FixP_Alpha
		);

		AlienHUDGadget();
		~AlienHUDGadget();

		void AddTextReport
		(
			SCString* pSCString_ToAdd
				// ultimately turn into an MCString
		);
		void ClearTheTextReportQueue(void);

		#if EnableStatusPanels
		void RequestStatusPanel
		(
			enum StatusPanelIndex I_StatusPanel
		);

		void NoRequestedPanel(void);
		#endif

		void CharTyped
		(
			char Ch
				// note that this _is _ a char
		);
		void Key_Backspace(void);
		void Key_End(void);
		void Key_Home(void);
		void Key_Left(void);
		void Key_Up(void);
		void Key_Right(void);
		void Key_Down(void);
		void Key_Delete(void);
		void Key_Tab(void);

		void SetString(const char* text);

		void Jitter(int FixP_Magnitude);

		TextReportGadget* pTextReportGadg;
	private:
			// not allowed to be NULL

		TextEntryGadget* pTextEntryGadg;
			// not allowed to be NULL
	};
	#endif // UseGadgets

/* Exported globals *****************************************************/

/* Function prototypes **************************************************/
void BringDownConsoleWithSayTypedIn();
void BringDownConsoleWithSaySpeciesTypedIn();


/* End of the header ****************************************************/

#endif
