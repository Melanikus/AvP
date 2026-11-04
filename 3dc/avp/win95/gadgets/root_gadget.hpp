/*
	
	rootgadg.hpp

*/
// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   added Make, a small function that makes the root gadget and hands it back

#ifndef _rootgadg
#define _rootgadg 1

	#ifndef _gadget
	#include "gadget.h"
	#endif

/* Version settings *****************************************************/

/* Constants  ***********************************************************/

/* Macros ***************************************************************/

/* Type definitions *****************************************************/

	#if UseGadgets
	class HUDGadget; // fully declared in HUDGADG.HPP

	class RootGadget : public Gadget
	{
		friend void GADGET_Init(void);
		friend void GADGET_UnInit(void);
			// friend functions: these get permission in order to allow
			// construction/destruction

	public:
		void Render
		(
			const struct r2pos& R2Pos,
			const struct r2rect& R2Rect_Clip,
			int FixP_Alpha
		);

		static RootGadget* GetRoot(void);

		HUDGadget* GetHUD(void);

		void RefreshHUD(void);

	private:
		RootGadget();
		~RootGadget();

		// Makes the one and only RootGadget. Its constructor records it in pSingleton and GADGET_UnInit deletes it at
		// shutdown, so a caller has no need to keep the pointer this hands back.
		static RootGadget* Make(void)
		{
			return new RootGadget;
		}

	private:
		static RootGadget* pSingleton;

		HUDGadget* pHUDGadg;
			// allowed to be NULL if no head-up-display e.g. when not in a game

	};
	
	// Inline methods:
		inline /*static*/ RootGadget* RootGadget::GetRoot(void)
		{
			return pSingleton;
		}
		inline HUDGadget* RootGadget::GetHUD(void)
		{
			return pHUDGadg;
		}
	#endif // UseGadgets

/* Exported globals *****************************************************/

/* Function prototypes **************************************************/



/* End of the header ****************************************************/

#endif
