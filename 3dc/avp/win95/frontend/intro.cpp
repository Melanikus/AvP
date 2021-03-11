
#include "FmvCutscenes.h"
#include "TextureManager.h"
#include "io.h"
#include "Input.h"
#include "3dc.h"
#include "inline.h"
#include "menus.h"
#include "intro.h"

extern void DirectReadKeyboard(void);
extern void DrawFadeQuad(uint32_t topX, uint32_t topY, uint32_t alpha);
extern void ThisFramesRenderingHasBegun(void);
extern void ThisFramesRenderingHasFinished(void);
void Show_CopyrightInfo(void);
void Show_Presents(void);
void Show_ARebellionGame(void);
void Show_AvPLogo(void);
extern void ShowSplashScreens(void);
extern void Show_WinnerScreen(void);
extern void DrawMainMenusBackdrop(void);
extern void FadedScreen(int alpha);

static bool IntroHasAlreadyBeenPlayed = true;
extern bool bRunning;

void WeWantAnIntro(void)
{
	IntroHasAlreadyBeenPlayed = false;
}

extern void PlayIntroSequence(void)
{
	if (IntroHasAlreadyBeenPlayed)
	{
		StartMenuMusic();
		return;
	}

	IntroHasAlreadyBeenPlayed = true;

	ResetFrameCounter();
	Show_CopyrightInfo();

	PlayFMV("FMVs/logos.bik");

	StartMenuMusic();
	ResetFrameCounter();

	Show_Presents();
	#if ALLOW_SKIP_INTRO
	if (!GotAnyKey) Show_ARebellionGame();
	if (!GotAnyKey) Show_AvPLogo();
	#else
	Show_ARebellionGame();
	Show_AvPLogo();
	#endif
}

extern void ShowSplashScreens(void)
{
	texID_t graphic[] =
	{
		AVPMENUGFX_SPLASH_SCREEN1, AVPMENUGFX_SPLASH_SCREEN2, AVPMENUGFX_SPLASH_SCREEN3,
		AVPMENUGFX_SPLASH_SCREEN4, AVPMENUGFX_SPLASH_SCREEN5
	};

	for (int i = 0; i < 5; i++)
	{
		int timeRemaining = 5*ONE_FIXED;
		do
		{
			CheckForWindowsMessages();

			ThisFramesRenderingHasBegun();

			int a = timeRemaining*2;
			if (a > ONE_FIXED) 
				a = ONE_FIXED;

			if (i != 4)
			{
				timeRemaining -= NormalFrameTime;
			}
			else
			{
				if (a == ONE_FIXED)
				{
				  	DrawAvPMenuGfx(graphic[i], 0, 0, ONE_FIXED+1, AVPMENUFORMAT_LEFTJUSTIFIED);
				}
				else
				{
				  	DrawAvPMenuGfx_Faded(graphic[i], 0, 0, a, AVPMENUFORMAT_LEFTJUSTIFIED);
					DrawFadeQuad(0, 0, a);
				}
				timeRemaining -= NormalFrameTime / 2;
			}

			ThisFramesRenderingHasFinished();
			FlipBuffers();

		  	DirectReadKeyboard();
			FrameCounterHandler();
		}
		while (timeRemaining >= 0 && !DebouncedGotAnyKey && bRunning);
	}
}

extern void Show_WinnerScreen(void)
{
	int timeRemaining = 10*ONE_FIXED;
	do
	{
		CheckForWindowsMessages();

		ThisFramesRenderingHasBegun();

		int a = timeRemaining*2;
		if (a>ONE_FIXED)
		{
		  	DrawAvPMenuGfx(AVPMENUGFX_WINNER_SCREEN, 0, 0, ONE_FIXED+1,AVPMENUFORMAT_LEFTJUSTIFIED);
		}
		else
		{
		  	DrawAvPMenuGfx_Faded(AVPMENUGFX_WINNER_SCREEN, 0, 0, a,AVPMENUFORMAT_LEFTJUSTIFIED);
		}

		ThisFramesRenderingHasFinished();
		FlipBuffers();

	  	DirectReadKeyboard();
		FrameCounterHandler();
		timeRemaining -= NormalFrameTime;
	}
	while (timeRemaining >= 0 && !DebouncedGotAnyKey && bRunning);
}

void Show_CopyrightInfo(void)
{
	int timeRemaining = ONE_FIXED/2;
	do
	{
		CheckForWindowsMessages();

		ThisFramesRenderingHasBegun();
		DrawAvPMenuGfx_Faded(AVPMENUGFX_COPYRIGHT_SCREEN, 0, 0, ONE_FIXED-timeRemaining*2, AVPMENUFORMAT_LEFTJUSTIFIED);
		ThisFramesRenderingHasFinished();
		FlipBuffers();

		FrameCounterHandler();
		timeRemaining -= NormalFrameTime;
	}
	while (timeRemaining > 0 && bRunning);

	timeRemaining = ONE_FIXED*2;

	do
	{
		CheckForWindowsMessages();

		ThisFramesRenderingHasBegun();
		DrawAvPMenuGfx_Faded(AVPMENUGFX_COPYRIGHT_SCREEN, 0, 0, ONE_FIXED,AVPMENUFORMAT_LEFTJUSTIFIED);
		ThisFramesRenderingHasFinished();
		FlipBuffers();

		FrameCounterHandler();
		timeRemaining-=NormalFrameTime;
	}
	while (timeRemaining > 0 && bRunning);

	timeRemaining = ONE_FIXED/2;
	do
	{
		CheckForWindowsMessages();

		ThisFramesRenderingHasBegun();
		DrawAvPMenuGfx_Faded(AVPMENUGFX_COPYRIGHT_SCREEN, 0, 0, timeRemaining*2,AVPMENUFORMAT_LEFTJUSTIFIED);
		ThisFramesRenderingHasFinished();
		FlipBuffers();

		FrameCounterHandler();
		timeRemaining-=NormalFrameTime;
	}
	while (timeRemaining > 0 && bRunning);
}

void Show_Presents(void)
{
	int timeRemaining = 8*ONE_FIXED-ONE_FIXED/2;

	do
	{
		CheckForWindowsMessages();

		ThisFramesRenderingHasBegun();
		{
			char *textPtr = GetTextString(TEXTSTRING_FOXINTERACTIVE);
			//int y = (480 - AvPMenuGfxStorage[AVPMENUGFX_PRESENTS].Height) / 2;
			uint32_t height, width;
			Tex_GetDimensions(AVPMENUGFX_PRESENTS, width, height);
			int y = (480 - height) / 2;

			PlayMenuMusic();
			DrawMainMenusBackdrop();

			if (timeRemaining > 6*ONE_FIXED)
			{
				DrawFadeQuad(0, 0, (15*ONE_FIXED-timeRemaining*2)/3);
			}
			else if (timeRemaining > 5*ONE_FIXED)
			{
				RenderMenuText(textPtr, MENU_CENTREX, y, 6*ONE_FIXED-timeRemaining, AVPMENUFORMAT_CENTREJUSTIFIED);
			}
			else if (timeRemaining > 4*ONE_FIXED)
			{
				RenderMenuText(textPtr, MENU_CENTREX, y, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
			}
			else if (timeRemaining > 3*ONE_FIXED)
			{
				RenderMenuText(textPtr, MENU_CENTREX, y, timeRemaining-3*ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
			}

			ThisFramesRenderingHasFinished();
			FlipBuffers();
		}
		#if ALLOW_SKIP_INTRO
		DirectReadKeyboard();
		#endif
		FrameCounterHandler();
		timeRemaining-=NormalFrameTime;
	}
	#if ALLOW_SKIP_INTRO

	while ((timeRemaining > 0) && !GotAnyKey && bRunning);

	#else
	while (timeRemaining > 0 && bRunning);// && !GotAnyKey);
	#endif
}

void Show_ARebellionGame(void)
{
	int timeRemaining = 7*ONE_FIXED;
	do
	{
		CheckForWindowsMessages();

		ThisFramesRenderingHasBegun();
		{
			char *textPtr = GetTextString(TEXTSTRING_PRESENTS);
			//int y = (480-AvPMenuGfxStorage[AVPMENUGFX_AREBELLIONGAME].Height)/2;
			uint32_t width, height;
			Tex_GetDimensions(AVPMENUGFX_AREBELLIONGAME, width, height);
			int y = (480-height)/2;

			DrawMainMenusBackdrop();
//			DrawAvPMenuGfx(AVPMENUGFX_BACKDROP, 0, 0, ONE_FIXED+1,AVPMENUFORMAT_LEFTJUSTIFIED);
			PlayMenuMusic();

			if (timeRemaining > 13*ONE_FIXED/2)
			{
//				DrawAvPMenuGfx(AVPMENUGFX_AREBELLIONGAME, MENU_CENTREX, y, 14*ONE_FIXED-timeRemaining*2,AVPMENUFORMAT_CENTREJUSTIFIED);
				RenderMenuText(textPtr, MENU_CENTREX, y, 14*ONE_FIXED-timeRemaining*2, AVPMENUFORMAT_CENTREJUSTIFIED);
 			}
			else if (timeRemaining > 5*ONE_FIXED)
			{
//				DrawAvPMenuGfx(AVPMENUGFX_AREBELLIONGAME, MENU_CENTREX, y, ONE_FIXED,AVPMENUFORMAT_CENTREJUSTIFIED);
				RenderMenuText(textPtr, MENU_CENTREX, y, ONE_FIXED, AVPMENUFORMAT_CENTREJUSTIFIED);
			}
			else if (timeRemaining > 3*ONE_FIXED)
			{
//				DrawAvPMenuGfx(AVPMENUGFX_AREBELLIONGAME, MENU_CENTREX, y, (timeRemaining-3*ONE_FIXED)/2,AVPMENUFORMAT_CENTREJUSTIFIED);
				RenderMenuText(textPtr, MENU_CENTREX, y, (timeRemaining-3*ONE_FIXED)/2, AVPMENUFORMAT_CENTREJUSTIFIED);
			}

			ThisFramesRenderingHasFinished();
			FlipBuffers();
		}
		DirectReadKeyboard();
		FrameCounterHandler();
		timeRemaining-=NormalFrameTime;
	}
	#if ALLOW_SKIP_INTRO
	while((timeRemaining>0) && !GotAnyKey && bRunning);
	#else
	while(timeRemaining>0);// && !GotAnyKey);
	#endif
}

void Show_AvPLogo(void)
{
	int timeRemaining = 5*ONE_FIXED;
	do
	{
		CheckForWindowsMessages();

		ThisFramesRenderingHasBegun();
		{
			uint32_t width, height;
			Tex_GetDimensions(AVPMENUGFX_ALIENSVPREDATOR, width, height);

			//int y = (480-AvPMenuGfxStorage[AVPMENUGFX_ALIENSVPREDATOR].Height)/2;
			int y = (480-height)/2;

			DrawMainMenusBackdrop();
//			DrawAvPMenuGfx(AVPMENUGFX_BACKDROP, 0, 0, ONE_FIXED+1,AVPMENUFORMAT_LEFTJUSTIFIED);
			PlayMenuMusic();

			if (timeRemaining > 9*ONE_FIXED/2)
			{
				DrawAvPMenuGfx(AVPMENUGFX_ALIENSVPREDATOR, MENU_CENTREX, y, -timeRemaining*2+10*ONE_FIXED,AVPMENUFORMAT_CENTREJUSTIFIED);
 			}
			else if (timeRemaining > 4*ONE_FIXED)
			{
				DrawAvPMenuGfx(AVPMENUGFX_ALIENSVPREDATOR, MENU_CENTREX, y, ONE_FIXED,AVPMENUFORMAT_CENTREJUSTIFIED);
			}
			else
			{
				DrawAvPMenuGfx(AVPMENUGFX_ALIENSVPREDATOR, MENU_CENTREX, y, timeRemaining/4,AVPMENUFORMAT_CENTREJUSTIFIED);
				timeRemaining-=NormalFrameTime/4;
			}

			ThisFramesRenderingHasFinished();
			FlipBuffers();
		}
		DirectReadKeyboard();
		FrameCounterHandler();
		timeRemaining-=NormalFrameTime;
	}
	#if ALLOW_SKIP_INTRO
	while((timeRemaining>0) && !GotAnyKey && bRunning);
	#else
	while(timeRemaining>0);// && !GotAnyKey);
	#endif
}
