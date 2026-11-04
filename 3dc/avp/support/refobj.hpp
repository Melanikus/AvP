/*
	
	refobj.hpp

	Created by DHM for Headhunter; copied into AVP

	Reference-counted base class to be used by e.g. strings

*/
// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   added RefHolder, a helper that lets go of a shared object automatically when the code using it ends;
//   deleted the reference-tracking debug code, which was switched off and never compiled

#ifndef _refobj
#define _refobj 1


	#ifndef __fail_h
	#include "fail.h"
	#endif

/* Version settings *****************************************************/

/* Macros ***************************************************************/

	
/* Imported globals *****************************************************/

	extern char const* refobj_fail_addref;
	extern char const* refobj_fail_release;
	extern char const* refobj_fail_destructor;


/* Type definitions *****************************************************/
	class R_DumpContext; // fully declared in DCONTEXT.HPP

	class RefCountObject
	{
	// {{{ Private data
	private:
		int RefCount;


	// }}}


	// {{{ Private functions
	private:
	// }}}

	public:
		RefCountObject() :
			RefCount(1)
		{
		}

		void R_AddRef(void)
		{
			#ifndef NDEBUG
			if ( RefCount <= 0)
			{
				fail(refobj_fail_addref);
			}
			#endif
			RefCount++;
		}

		void R_Release(void)
		{
			#ifndef NDEBUG
			if ( RefCount <= 0)
			{
				fail(refobj_fail_release);
			}
			#endif

			if ( (--RefCount) == 0 )
			{
				delete this;
			}
		}

		#if debug
		int CheckRef() const
		{
			return RefCount;
		}
			// Handy way to examine reference count for debugging only
		#endif


	protected:
		// Destructors must be protected; only derived classes may use the
		// destructor and (we hope) only in the R_Release() method:
		virtual ~RefCountObject()
		{
			#ifndef NDEBUG
			if ( RefCount != 0 )
			{
				fail(refobj_fail_destructor);
			}
			#endif

		}
	};

	// A shared object counts who is using it, and deletes itself when the count reaches zero. A new object starts with a
	// count of one. RefHolder looks after that one count: it gives it back by itself when the code holding it ends,
	// on every way out. For example:
	//     RefHolder<SCString> pSCString(new SCString("TEXT")); pSCString -> SendToScreen();
	template <class RC> class RefHolder
	{
	public:
		explicit RefHolder(RC* pRC_New) : pRC(pRC_New) {}
		~RefHolder() { if (pRC) pRC -> R_Release(); }

		RC* operator->() const { return pRC; }
		RC& operator*() const { return *pRC; }
		operator RC*() const { return pRC; }

	private:
		RC* pRC;

		// a holder cannot be copied: two copies would each give the same count back, once too often
		RefHolder(const RefHolder&);
		RefHolder& operator=(const RefHolder&);
	};



/* Exported globals *****************************************************/

/* Function prototypes **************************************************/



/* End of the header ****************************************************/

#endif
