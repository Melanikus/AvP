// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   declares the text-file loader that also returns the file's size (see platsup.cpp)
#ifndef _language_h_
#define _language_h_ 1
/* KJL 11:54:21 05/02/97 - language.h */
#include "langenum.h"

#define ENGLISH_TEXT_FILENAME "ENGLISH.TXT"
extern char *LanguageFilename[];

extern void InitTextStrings(void);
/*KJL***************************************************************
* Initialization to be called ONCE right at the start of the game. *
***************************************************************KJL*/

extern void KillTextStrings(void);
/*KJL********************************************************
* Free memory etc, to be called ONCE on exit from the game. *
********************************************************KJL*/

extern char *GetTextString(enum TEXTSTRING_ID stringID);
/*KJL**********************************************************
* This function returns a pointer to a null terminated string *
* which is described by its enumeration value, as defined in  *
* the header file langenum.h.                                 *
**********************************************************KJL*/



extern char *LoadTextFile(const char *filename);
/*KJL*****************************************************
* Platform specific function, which loads the named file *
* and returns a pointer to the start of the file's data. *
*****************************************************KJL*/
/* The buffer holds the file's bytes followed by one extra 0 byte, so a scan of it can stop at the end. */

extern char *LoadTextFileWithSize(const char *filename, size_t *sizePtr);
/* The same, also returning the file's size in bytes (the 0 byte after it not counted). */

extern void UnloadTextFile(const char *filename, char *bufferPtr);
/*KJL**********************************************************
* Platform specific function, which unloads the named file    *
* and frees the memory pointed to by the bufferPtr. You may   *
* not need the filename, but I'm passing it for completeness. *
**********************************************************KJL*/


#endif
