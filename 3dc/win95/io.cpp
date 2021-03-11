#include "3dc.h"
#include <sys\stat.h>
#include <string.h>
#include "io.h"
#include "inline.h"
#include "module.h"
#include "chunk_textures.h"
#include "d3d_hud.h"
#define UseLocalAssert TRUE
#include "ourasert.h"
#include "hud_layout.h"
#include "psnd.H"

#undef textprint

#if SupportTLTFiles
#define Output_TLT_File				FALSE
#endif

#define textprintOn TRUE

#define DHMtextprint TRUE /* define to use Dave Malcolm's replacement textprint routines */


/*
	externs for commonly used global variables and arrays
*/

extern SHAPEHEADER **mainshapelist;
extern SHAPEHEADER *testpaletteshapelist[];
extern SCREENDESCRIPTORBLOCK ScreenDescriptorBlock;
extern int *Global_ShapeNormals;
extern int *Global_ShapePoints;
extern int *ItemPointers[];
extern int ItemData[];
extern char projectsubdirectory[];

extern int WinLeftX;
extern int WinRightX;
extern int WinTopY;
extern int WinBotY;

extern int NumAvailableVideoModes;

void InitialiseRawInput();

extern void ConstructOneOverSinTable(void);

/*

 Global Variables for PC Watcom Functions
 and Windows 95!

*/
	
/* Timer */
uint32_t lastTickCount;

int VideoMode;
int VideoModeType;
int VideoModeTypeScreen;
int FrameRate;

int WindowMode;

int NormalFrameTime;
int PrevNormalFrameTime;
int CloakingPhase;

unsigned char *TextureLightingTable = 0;

/* KJL 11:48:45 28/01/98 - used to scale NormalFrameTime, so the game can be slowed down */
int TimeScale = 65536;

/* KJL 16:00:11 28/01/98 - unscaled frame time */
int RealFrameTime;

/* Keyboard */
unsigned char KeyboardInput[MAX_NUMBER_OF_INPUT_KEYS];

/* KJL 15:08:43 29/03/98 - added to give extra flexibility to debugging text */
int PrintDebuggingText(const char* t, ...);
int ReleasePrintDebuggingText(const char* t, ...);

int GlobalFrameCounter;
int RouteFinder_CallsThisFrame;

static VIEWDESCRIPTORBLOCK* vdb_tmp;
static SCREENDESCRIPTORBLOCK* sdb_tmp;

bool GotAnyKey = false;

/* Input communication with Windows Procedure */
/* Print system */

#if !DHMtextprint
    PRINTQUEUEITEM PrintQueue[MaxMessages];
	int MessagesStoredThisFrame;
#endif

int textprintPosX;
int textprintPosY;

/* Added 28/11/97 by DHM: boolean for run-time switching on/off of textprint */
int bEnableTextprint = FALSE;

/* Added 28/1/98 by DHM: as above, but applies specifically to textprintXY */
int bEnableTextprintXY = TRUE;

/*

 Get Shape Data

 Function returns a pointer to the Shape Header Block

*/

SHAPEHEADER* GetShapeData(int shapenum)
{
	if (shapenum >= 0 && shapenum < maxshapes)
	{
		SHAPEHEADER *sptr = mainshapelist[shapenum];
		return sptr;
	}

	return NULL;
}


/*

 Platform specific VDB functions for Initialisation and ShowView()

*/

void PlatformSpecificVDBInit(VIEWDESCRIPTORBLOCK *vdb)
{
	vdb_tmp = vdb;
}

void PlatformSpecificShowViewEntry(VIEWDESCRIPTORBLOCK *vdb, SCREENDESCRIPTORBLOCK *sdb)
{
	vdb_tmp = vdb;
	sdb_tmp = sdb;
}

void PlatformSpecificShowViewExit(VIEWDESCRIPTORBLOCK *vdb, SCREENDESCRIPTORBLOCK *sdb)
{
	vdb_tmp = vdb;
	sdb_tmp = sdb;
}

/*

 Initialise System and System Variables

*/

void InitialiseSystem()
{
    /* Initialise input interface */
	memset((void*)KeyboardInput, FALSE, MAX_NUMBER_OF_INPUT_KEYS);
	GotAnyKey = false;

#ifdef _WIN32
    /* init raw input for mouse */
	InitialiseRawInput();
#endif

    /* launch Direct Input */
	InitialiseDirectInput();
	InitialiseDirectKeyboard();
	InitialiseDirectMouse();
	InitJoysticks();

    /* Initialise textprint system */
	textprintPosX = 0;
	textprintPosY = 0;
	#if debug
	InitPrintQueue();
	#endif

	ConstructOneOverSinTable();
}


/*

 Exit the system

*/

void ExitSystem(void)
{
	/* Game specific exit functions */
	ExitGame();

	// close sound system
	SoundSys_End();

	/* should be taken care of with WM_DESTROY ?*/
	ReleaseDirect3D();

	/* Kill windows procedures */
	ExitWindowsSystem();
}

/*
	Timer functions are based on Windows timer
	giving number of millisecond ticks since Windows
	was last booted.  Note this will wrap round after
   Windows has been up continuously for  approximately
	49.7 days.  This is not considered to be too
	significant a limitation...
*/



void ResetFrameCounter(void)
{
	lastTickCount = timeGetTime();
	
	/* KJL 15:03:33 12/16/96 - I'm setting NormalFrameTime too, rather than checking that it's
	non-zero everytime I have to divide by it, since it usually is zero on the first frame. */
	NormalFrameTime = 65536 >> 4;
	PrevNormalFrameTime = NormalFrameTime;

	RealFrameTime = NormalFrameTime;
	FrameRate = 16;
	GlobalFrameCounter = 0;
	CloakingPhase = 0;
	RouteFinder_CallsThisFrame = 0;
}

void FrameCounterHandler(void)
{
	uint32_t newTickCount = timeGetTime();

	// fcnt = time passed since last check
	uint32_t fcnt = newTickCount - lastTickCount;

	lastTickCount = newTickCount;

    if (fcnt == 0)
		fcnt = 1; /* for safety */

	FrameRate = TimerFrame / fcnt;

	PrevNormalFrameTime = NormalFrameTime;
	NormalFrameTime = DIV_FIXED(fcnt,TimerFrame);

//	NormalFrameTime = (fcnt / TimerFrame) << 16;

	// RealFrameTime. unscaled frame time
	RealFrameTime = NormalFrameTime;

	// do the scaling on NormalFrameTime if reqired
	if (TimeScale != ONE_FIXED)
	{
		NormalFrameTime = MUL_FIXED(NormalFrameTime, TimeScale);
	}

	/* cap NormalFrameTime if frame rate is really low */
	if (NormalFrameTime > 16384) // 0.25. quarter of a second?
		NormalFrameTime = 16384;

	GlobalFrameCounter++;
	CloakingPhase += NormalFrameTime>>5;

	RouteFinder_CallsThisFrame = 0;
}

/*

 Wait for Return Key

 Such a function may not be defined on some platforms

 On Windows 95 the description of this function has 
 been changed, so that it calls FlushTextprintBuffer
 and FlipBuffers before going into the actual
 WaitForReturn code. This is necessary if it is to
 behave in the same way after a textprint call as it
 does on the DOS platform.

*/

void WaitForReturn(void)
{
	/* Crude but probably serviceable for now */
	long SavedTickCount;
	SavedTickCount  = lastTickCount;

	/* Display any lingering text */
    FlushTextprintBuffer();
	FlipBuffers();

	while (!(KeyboardInput[KEY_CR]))
	{
		DirectReadKeyboard();
	}

	lastTickCount = SavedTickCount;
}


/*
	By copying the globals here we guarantee
	that game functions will receive a set of
	input values updated at a defined time
*/


void ReadUserInput(void)
{
	DirectReadMouse();
/* bjd - called at top of DirectReadKeyboard() */ //ReadJoysticks();
	DirectReadKeyboard();
}

/*
	Not NECESSARILY the standard functionality,
	but it seems good enough to me...
*/

void CursorHome(void)
{
/* Reset positions for textprint system */
	textprintPosX = 0;
	textprintPosY = 0;
}


void GetProjectFilename(char *fname, char *image)
{
	char *src;
	char *dst;

	src = projectsubdirectory;
	dst = fname;

	while (*src) {
		*dst++ = *src++;
	}

	src = image;

	while (*src) {
		*dst++ = *src++;
	}

	*dst = 0;
}

/*

 Platform specific version of "printf()"

 Not all platforms support, or indeed are ABLE to support printf() in its
 general form. For this reasons calls to textprint() are made through this
 function.

*/

/*
	If debug or textprintOn are not defined, these 
	function defintions are collapsed to a simple
	return value, which should collapse to no object
	code under optimisation.  
	The whole issue of turning on or off textprint
	beyond this point is hereby left to Kevin and 
	Chris H to fight to the death about...
*/

#if DHMtextprint


/*
	Dave Malcolm 21/11/96:

	I have rewritten the Win95 textprint routines below.  
	
	It should now support:
		- carriage returns are no longer automatic at the end of lines; there is a #define if you want this behaviour back
		- carriage return characters cause a carriage return
		- wraparound at the right-hand edge of the screen, with textprint() wrapping to the left-hand edge,
		and textprintXY() wrapping back to the X coordinate
		- clipping at the bottom edge of the screen
		- a warning message if text has been lost due to clipping or buffer overflows etc.
		- a y-offset that can be used to scroll up and down the text overlay output from textprint
*/

	/* VERSION SETTINGS: */	
		#define AutomaticNewLines	FALSE
			/* set this to TRUE and you will get a \n inserted automatically at the end of each line */
			#if AutomaticNewLines
				#error Not yet written...
			#endif

	/* LOW LEVEL ASSERTION SUPPORT */
		/*
			We cannot use standard assertions in this routine because this routine is called by the standard
			assertion routine, and so would run the risk of infinite loops and excitingly obscure bugs.

			For this reason we define a special assert macro.
		*/

#if 1
	#define LOWLEVELASSERT(ignore)
#else
		#if debug

			#define LOWLEVELASSERT(x) \
			     (void)									\
			     (										\
			     	(x) 								\
			     	? 1 : (ReleaseDirect3D(),exit(GlobalAssertCode),0)	\
			     )										

		#else
			/* Assertions are disabled at compile-time: */
			#define LOWLEVELASSERT(ignore)
		
		#endif
#endif


	/* 
		We extract arguments into a buffer, with a dodgy hack to increase it in size to give more defence
		against buffer overflows; there seems to be no easy & robust way to give vsprintf() a buffer size...

		This buffer is reset once per string per frame
	*/
	#define PARANOIA_BYTES	(1024)
	#define TEXTPRINT_BUFFER_SIZE	(MaxMsgChars+PARANOIA_BYTES+1)
	static char TextprintBuffer[TEXTPRINT_BUFFER_SIZE]="";

	/*

	The PRINTQUEUEITEM structure from PLATFORM.H is not used by my system; instead of queueing strings to be
	displayed we do it on a character by character basis, with a limit on the total number of chars per frame.

	This limit is set to be (MaxMsgChars*MaxMessages), which gives the same power and more flexibility than the 
	old system.

	When the queue is full, additional characters get ignored.

	This is queue is reset once per frame.

	*/

	typedef struct daveprintchar {
		char CharToPrint;
		int x,y;
	} DAVEPRINTCHAR;

	#define DHM_PRINT_QUEUE_SIZE (MaxMsgChars*MaxMessages)

	static DAVEPRINTCHAR DHM_PrintQueue[DHM_PRINT_QUEUE_SIZE];
	static int DHM_NumCharsInQueue=0;

	static int fTextLost=FALSE;
	static char TextLostMessage[]="textprint warning:TEXT LOST";
	#define TEXT_LOST_X	(50)
	#define TEXT_LOST_Y	(20)

	volatile int textprint_Y_offset=0;


/* Dave's version of initialising the print queue */
void InitPrintQueue(void)
{
	DHM_NumCharsInQueue=0;
	fTextLost=FALSE;
}

/*

	Old systems comment:
		Write all messages in buffer to screen
		(to be called at end of frame, after surface
		/ execute buffer unlock in DrawItemListContents,
		so that text appears at the front of the back 
		buffer immediately before the flip).

	This is Dave's version of the same:
*/

void FlushTextprintBuffer(void)
{
	/* PRECONDITION: */
	{
		LOWLEVELASSERT(DHM_NumCharsInQueue<DHM_PRINT_QUEUE_SIZE);
	}

	/* CODE: */
	{
		DAVEPRINTCHAR* pDPR=&DHM_PrintQueue[0];

		for (int i=0; i<DHM_NumCharsInQueue; i++)
		{
		#if 0
			BlitWin95Char
			(
				pDPR->x, 
				pDPR->y,
				pDPR->CharToPrint
			);
		#else 
			D3D_BlitWhiteChar
			(
				pDPR->x, 
				pDPR->y,
				pDPR->CharToPrint
			);
		#endif
			pDPR++;
		}

		if (fTextLost)
		{
			/* Display error message in case test has been lost due to clipping of Y edge, or buffer overflow */
			size_t NumChars = strlen(TextLostMessage);

			for (size_t i = 0; i < NumChars; i++)
			{
	//			   	BlitWin95Char(TEXT_LOST_X+(i*CharWidth),TEXT_LOST_Y,TextLostMessage[i]);
			}

			fTextLost=FALSE;
		}
	}
	DHM_NumCharsInQueue=0;
}

static int LastDisplayableXForChars(void)
{
	return ScreenDescriptorBlock.SDB_Width-CharWidth;
}

static int LastDisplayableYForChars(void)
{
	return ScreenDescriptorBlock.SDB_Height-CharHeight;
}


static void DHM_AddToQueue(int x,int y, char Ch)
{

	if
	(
		(y>=0)
		&&
		(y<=LastDisplayableYForChars())
	)
	{
		if (DHM_NumCharsInQueue<DHM_PRINT_QUEUE_SIZE)
		{
			DAVEPRINTCHAR* pDPR=&DHM_PrintQueue[DHM_NumCharsInQueue++];
			/* We insert into the queue at this position, updating the length of the queue */

			pDPR->x=x;
			pDPR->y=y;
			pDPR->CharToPrint=Ch;
		}
		else
		{
			/* Otherwise the queue if full, we will have to ignore this char; set an error flag so we get a message*/
			fTextLost=TRUE;
		}
	}
	else
	{
		/* Otherwise the text is off the top or bottom of the screen; set an error flag to get a message up*/
		fTextLost=TRUE;
	}
}

static int DHM_MoveBufferToQueue(int* pPosX,int* pPosY,int fZeroLeftMargin)
{
	/* 
	Function takes two integers by reference (using pointers), and outputs whatever is in
	the string buffer into the character queue, so that code can be shared by textprint() and textprintXY()

	Returns "number of lines": any carriage returns or word wraps
	*/

	/* PRECONDITION */
	{
		LOWLEVELASSERT(pPosX);
		LOWLEVELASSERT(pPosY);
	}

	/* CODE */
	{
		int NumLines=0;

		int LeftMarginX;

		if (fZeroLeftMargin)
		{
			LeftMarginX=0;
		}
		else
		{
			LeftMarginX=*pPosX;
		}



		/* Iterate through the string in the buffer, adding the individual characters to the queue */
		{
			char* pCh=&TextprintBuffer[0];
			int SafetyCount=0;

			while
			(
				((*pCh)!='\0')
				&&
				((SafetyCount++)<MaxMsgChars)
			)
			{
				switch (*pCh)
				{
					case '\n':
						{
							/* Wrap around to next line.,. */
							(*pPosY)+=HUD_FONT_HEIGHT;
							(*pPosX)=LeftMarginX;
							NumLines++;
						
						}
						break;
					default:
						{
							/* It is a standard character or a space */
							DHM_AddToQueue(*pPosX,(*pPosY)+textprint_Y_offset, *pCh);

							(*pPosX)+=AAFontWidths[(unsigned char)*pCh];//CharWidthInPixels(*pCh);

							if ((*pPosX)>LastDisplayableXForChars())
							{
								/* Wrap around to next line.,. */
								(*pPosY)+=HUD_FONT_HEIGHT;
								(*pPosX)=LeftMarginX;
								NumLines++;
							}
						}
				}

				/* ...and on to the next character*/
				pCh++;
			}
		}
		
		/* Clear the string buffer */
		{
			TextprintBuffer[0]='\0';
		}

		return NumLines;
	}
}


int textprint(const char* t, ...)
{
	#if (debug && textprintOn)
	if
	(
		bEnableTextprint
	)	
	{
		/*
		Get message string from arguments into buffer...
		*/
		{
			va_list ap;
		
			va_start(ap, t);
			vsprintf(&TextprintBuffer[0], t, ap);
			va_end(ap);
		}

		/* 
		Attempt to trap buffer overflows...
		*/
		{
			LOWLEVELASSERT(strlen(TextprintBuffer)<TextprintBuffer);
		}

		return DHM_MoveBufferToQueue(&textprintPosX,&textprintPosY,TRUE);
	}
	else
	{
		// Run-time disabling of textprint()
		return 0;
	}
	#else
		/* Do nothing; hope the function call gets optimised away */
		return 0;
	#endif
}

int PrintDebuggingText(const char* t, ...)
{
	/*
	Get message string from arguments into buffer...
	*/
	{
		va_list ap;
	
		va_start(ap, t);
		vsprintf(&TextprintBuffer[0], t, ap);
		va_end(ap);
	}

	/* 
	Attempt to trap buffer overflows...
	*/
	{
		LOWLEVELASSERT(strlen(TextprintBuffer)<TextprintBuffer);
	}

	return DHM_MoveBufferToQueue(&textprintPosX,&textprintPosY,TRUE);
}

int ReleasePrintDebuggingText(const char* t, ...)
{
	/*
	Get message string from arguments into buffer...
	*/
	{
		va_list ap;
	
		va_start(ap, t);
		vsprintf(&TextprintBuffer[0], t, ap);
		va_end(ap);
	}

	/* 
	Attempt to trap buffer overflows...
	*/
	{
		LOWLEVELASSERT(strlen(TextprintBuffer)<TextprintBuffer);
	}

	return DHM_MoveBufferToQueue(&textprintPosX,&textprintPosY,TRUE);
}


int textprintXY(int x, int y, const char* t, ...)
{
	#if (debug && textprintOn)
	if
	(
		bEnableTextprintXY
	)	
	{
		/*
		Get message string from arguments into buffer...
		*/
		{
			va_list ap;
		
			va_start(ap, t);
			vsprintf(&TextprintBuffer[0], t, ap);
			va_end(ap);
		}

		/* 
		Attempt to trap buffer overflows...
		*/
		{
			LOWLEVELASSERT(strlen(TextprintBuffer)<TextprintBuffer);
		}

		{
			int localX=x;
			int localY=y;

			return DHM_MoveBufferToQueue(&localX,&localY,FALSE);
		}

		
	}
	else
	{
		// Run-time disabling of textprint()
		return 0;
	}
	#else
	{
		/* Do nothing; hope the function call gets optimised away */

		return 0;
	}
	#endif
}



	/* 
	 *
	 * 
	
	End of Dave Malcolm's text routines; old version is below 
	
	 *
	 *
	 */
#else

/*
	NOTE!!!! All this software is intended for debugging
	only.  It emulates the print interface in any video
	mode supported by the engine, but there are limits -
	messages will pile up at the bottom of the screen
	and overwrite each other, all messages will appear at the
	fron in the main screen (NOT clipped to the VDB), messages
	will not wrap round if they are longer than a screen line
	unless \n is inserted in the print string, and the text colour
	is not guaranteed to be white in paletted modes.
	So there.
*/


/*
	IMPORTANT!!!!
	Messages longer than MaxMsgChars are liable
	to CRASH this routine.  I haven't bothered 
	to do anything about this on the grounds that
	we can't tell how long the message is until after
	the vsprintf call, and the crash is likely to
	occur in vsprintf itself as it overflows the
	buffer.
*/

/*
	!!!!! FIXME??
	textprints don't seem to appear 
	in SubWindow mode --- possibly
	because the colours in the font
	are going to system font colours which
	are invisible???
*/

#if (debug && textprintOn)

int textprint(const char* t, ...)

{
	int i,j;
	va_list ap;
	char message[MaxMsgChars];
	char outmsg[MaxMsgChars];
	int numlines;
	int CharCount;
	int XPos=0;

	va_start(ap, t);

	vsprintf(&message[0], t, ap);

	va_end(ap);

    i = 0;
	j = 0;
	numlines = 0;
	CharCount = strlen(&message[0]);

    /* Read through message buffer until we reach the terminator */
    while ((i < CharCount) && (message[i] != '\0'))
  	{
    	outmsg[j++] = message[i];
		XPos+=CharWidth;
        /* newline within string */
        if ((message[i] == '\n')||(XPos>ScreenDescriptorBlock.SDB_Width))
	      {
	       /* Display string and reset to start of next line */
	       WriteStringToTextBuffer(textprintPosX, textprintPosY, 
	              &outmsg[0]);
		   textprintPosX = 0;
		   textprintPosY += HUD_FONT_HEIGHT;
		   XPos=0;
		   /* Messages can pile up at bottom of screen */
		   if (textprintPosY > ScreenDescriptorBlock.SDB_Height)
		     textprintPosY = ScreenDescriptorBlock.SDB_Height;
		   /* Clear output string and reset variables */
		   {
			int k;
		    for (k=0; k<(j+1); k++)
			  outmsg[k] = 0;
		   }
		   j = 0;
		   /* Record number of lines output */
		   numlines++;
		  }
		 i++;
       }

	/* Flush any remaining characters */
	WriteStringToTextBuffer(textprintPosX, textprintPosY, 
	     &outmsg[0]);
    textprintPosX = 0;
	textprintPosY += HUD_FONT_HEIGHT;
	/* Messages can pile up at bottom of screen */
	if (textprintPosY > ScreenDescriptorBlock.SDB_Height)
	  textprintPosY = ScreenDescriptorBlock.SDB_Height;
	numlines++;

	return numlines;
}

/*
	Textprint to defined location on screen
	(in screen coordinates for current video
	mode).
	NOTE!!! Newlines within strings sent to this
	function will be IGNORED.
*/

int textprintXY(int x, int y, const char* t, ...)

{
	va_list ap;
	char message[MaxMsgChars];

	va_start(ap, t);

	vsprintf(&message[0], t, ap);

	va_end(ap);

	WriteStringToTextBuffer(x, y, &message[0]);

	return 1; /* for one line */
}

#else

int textprint(const char* t, ...)

{
	return 0;
}

int textprintXY(int x, int y, const char* t, ...)

{
	return 0;
}

#endif


/*
	Add string to text buffer 
*/

void WriteStringToTextBuffer(int x, int y, unsigned char *buffer)

{
	if (MessagesStoredThisFrame < MaxMessages)
	  {
	   strcpy(PrintQueue[MessagesStoredThisFrame].text, buffer);

       PrintQueue[MessagesStoredThisFrame].text_length = strlen(buffer);
	   PrintQueue[MessagesStoredThisFrame].x = x;
	   PrintQueue[MessagesStoredThisFrame].y = y;

	   MessagesStoredThisFrame++;
	  }
}


/*
	Display string of chracters, starting at passed pointer,
	at location on screen starting with x and y.

	Patched by Dave Malcolm 20/11/96 so that text wraps around when it reaches the right hand edge of the screen, in
	this routine, at least...
*/


void DisplayWin95String(int x, int y, unsigned char *buffer)

{
	int InitialX=x;
	int stlen;
	unsigned char ch;

	stlen = strlen(buffer);

    do
	  {
       ch = (unsigned char) *buffer;
	   BlitWin95Char(x, y, ch);
	   x += CharWidth;
	   if (x > (ScreenDescriptorBlock.SDB_Width 
	       - CharWidth))
		{
			#if 1
				/* Wrap to new line, based on coordinates for display...*/
				x=InitialX;
				y+=HUD_FONT_HEIGHT;
			#else
			   	/* Characters will pile up at screen edge */
				x = (ScreenDescriptorBlock.SDB_Width - CharWidth);
			#endif
		}
	   
	   buffer++;
	   stlen--;
	  }
	while ((ch != '\n') && (ch != '\0') &&
	      (stlen > 0));
}

/*
	Write all messages in buffer to screen
	(to be called at end of frame, after surface
	/ execute buffer unlock in DrawItemListContents,
	so that text appears at the front of the back 
	buffer immediately before the flip).
*/

void FlushTextprintBuffer(void)

{
	int i;

    for (i=0; i<MessagesStoredThisFrame; i++)
	  {
	   if (PrintQueue[i].text_length)
	     DisplayWin95String(PrintQueue[i].x, 
	        PrintQueue[i].y, PrintQueue[i].text);

       /* 
         More mystery code from Roxby --- an extra safety
		 check for printing?? Or a hangover from a linked
		 list version of the data structure???
	   */
       PrintQueue[i].text_length = 0;
	  }

    MessagesStoredThisFrame = 0;
}

/* Initialise print queue */

void InitPrintQueue(void)

{
	int i;

    /* Mystery code from Roxby here... */
    for (i=0; i < MaxMessages; i++)
	   PrintQueue[i].text_length = 0;

    MessagesStoredThisFrame = 0;
}

#endif