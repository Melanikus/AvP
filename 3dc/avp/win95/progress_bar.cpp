#include "3dc.h"
#include "module.h"
#include "platform.h"
#include "kshape.h"
#include "progress_bar.h"
#include "chunk_textures.h"
#include "inline.h"
#include "gamedef.h"
#include "psnd.h"
#include "TextureManager.h"
#include "d3d_render.h"
#include "io.h"
#include "Input.h"
#include "language.h"
#include "d3d_hud.h"

extern SCREENDESCRIPTORBLOCK ScreenDescriptorBlock;
extern int FadingGameInAfterLoading;

extern void MinimalNetCollectMessages(void);
extern void NetSendMessages(void);
extern void RenderGrabbedScreen(void);
extern void ThisFramesRenderingHasBegun(void);
extern void ThisFramesRenderingHasFinished(void);
extern void RenderBriefingText(int centreY, int brightness);
extern void RenderStringCentred(char *stringPtr, int centreX, int y, int colour);

static uint32_t	CurrentPosition = 0;
static uint32_t BarLeft;
static uint32_t BarRight;
static uint32_t BarTop;
static uint32_t BarBottom;

static const char* Loading_Image_Name = "Menus/Loading.RIM";
static const char* Loading_Bar_Empty_Image_Name = "Menus/LoadingBar_Empty.RIM";
static const char* Loading_Bar_Full_Image_Name = "Menus/LoadingBar_Full.RIM";

RECT LoadingBarEmpty_DestRect;
RECT LoadingBarEmpty_SrcRect;
RECT LoadingBarFull_DestRect;
RECT LoadingBarFull_SrcRect;

extern void DrawProgressBar(const RECT &srcRect, const RECT &destRect, texID_t textureID);
extern bool IsDemoVersion();

texID_t fullTextureID  = NO_TEXTURE;
texID_t emptyTextureID = NO_TEXTURE;
texID_t dbTextureID    = NO_TEXTURE;

void Start_Progress_Bar()
{
//	AAFontImageNumber = Tex_CreateFromRIM("graphics/Common/aa_font.RIM");
	
	// load other graphics
	if (!IsDemoVersion())
	{
		emptyTextureID = Tex_CreateFromRIM("graphics/Menus/LoadingBar_Empty.RIM");
		fullTextureID = Tex_CreateFromRIM("graphics/Menus/LoadingBar_Full.RIM");
	}
	else 
	{
		// load background image for bar
		dbTextureID = Tex_CreateFromRIM("graphics/Menus/Loading.RIM");
	}

	// draw initial progress bar
	LoadingBarEmpty_SrcRect.left   = 0;
	LoadingBarEmpty_SrcRect.right  = 639;
	LoadingBarEmpty_SrcRect.top    = 0;
	LoadingBarEmpty_SrcRect.bottom = 39;
	LoadingBarEmpty_DestRect.left  = 0;
	LoadingBarEmpty_DestRect.right = ScreenDescriptorBlock.SDB_Width-1;
	LoadingBarEmpty_DestRect.top    = ((ScreenDescriptorBlock.SDB_Height - ScreenDescriptorBlock.SDB_SafeZoneHeightOffset)*11)/12;
	LoadingBarEmpty_DestRect.bottom =  (ScreenDescriptorBlock.SDB_Height - ScreenDescriptorBlock.SDB_SafeZoneHeightOffset)-1;

	{
		ThisFramesRenderingHasBegun();

		if (!IsDemoVersion())
		{
			DrawProgressBar(LoadingBarEmpty_SrcRect, LoadingBarEmpty_DestRect, emptyTextureID);
			RenderBriefingText(ScreenDescriptorBlock.SDB_Height/2, ONE_FIXED);
		}
		else
		{
			// user is using demo files, therefor no progress bar graphics? draw demo style..

			// background image
			DrawQuad(0, 0, 640, 480, dbTextureID, RCOLOR_XRGB(255, 255, 255), TRANSLUCENCY_OFF);

			// white outline
			DrawQuad(105, 413, 429, 46, NO_TEXTURE, RCOLOR_XRGB(255, 255, 255), TRANSLUCENCY_OFF);

			// blue background
			DrawQuad(106, 414, 427, 44, NO_TEXTURE, RCOLOR_XRGB(0, 0, 248), TRANSLUCENCY_OFF);
		}

		ThisFramesRenderingHasFinished();

		FlipBuffers();
	}

	CurrentPosition = 0;
}

void Set_Progress_Bar_Position(int pos)
{
	uint32_t NewPosition = DIV_FIXED(pos, PBAR_LENGTH);

	if (NewPosition > CurrentPosition)
	{
		CurrentPosition = NewPosition;
		LoadingBarFull_SrcRect.left   = 0;
		LoadingBarFull_SrcRect.right  = MUL_FIXED(639, NewPosition);
		LoadingBarFull_SrcRect.top    = 0;
		LoadingBarFull_SrcRect.bottom = 39;
		LoadingBarFull_DestRect.left  = 0;
		LoadingBarFull_DestRect.right = MUL_FIXED(ScreenDescriptorBlock.SDB_Width-1, NewPosition);
		LoadingBarFull_DestRect.top    = ((ScreenDescriptorBlock.SDB_Height - ScreenDescriptorBlock.SDB_SafeZoneHeightOffset)*11)/12;
		LoadingBarFull_DestRect.bottom =  (ScreenDescriptorBlock.SDB_Height - ScreenDescriptorBlock.SDB_SafeZoneHeightOffset)-1;
		
		ThisFramesRenderingHasBegun();

		if (!IsDemoVersion())
		{
			// need to render the empty bar here again. As we're not blitting anymore, 
			// the empty bar will only be rendered for one frame.
			DrawProgressBar(LoadingBarEmpty_SrcRect, LoadingBarEmpty_DestRect, emptyTextureID);

			// also need this here again, or else the text disappears!
			RenderBriefingText(ScreenDescriptorBlock.SDB_Height/2, ONE_FIXED); // no briefing text on demo this this call is in this block

			// now render the green percent loaded overlay
			DrawProgressBar(LoadingBarFull_SrcRect, LoadingBarFull_DestRect, fullTextureID);
		}
		else // if we're using demo assets, draw the demo style progress bar
		{
			// background image
			DrawQuad(0, 0, 640, 480, dbTextureID, RCOLOR_XRGB(255, 255, 255), TRANSLUCENCY_OFF);

			// white outline
			DrawQuad(105, 413, 429, 46, NO_TEXTURE, RCOLOR_XRGB(255, 255, 255), TRANSLUCENCY_OFF);

			// blue background
			DrawQuad(106, 414, 427, 44, NO_TEXTURE, RCOLOR_XRGB(0, 0, 248), TRANSLUCENCY_OFF);

			// red progress bar
			DrawQuad(106, 414, MUL_FIXED(427, NewPosition), 44, NO_TEXTURE, RCOLOR_XRGB(248, 0, 0), TRANSLUCENCY_OFF);
		}

		ThisFramesRenderingHasFinished();
		
		FlipBuffers();
		/*
		If this is a network game , then check the received network messages from 
		time to time (~every second).
		Has nothing to do with the progress bar , but this is a convenient place to
		do the check.
		*/
	
#if 0
		if (AvP.Network != I_No_Network)
		{
			static uint32_t LastSendTime;
			uint32_t time = timeGetTime();
			if (time - LastSendTime > 1000 || time < LastSendTime)
			{
				//time to check our messages 
				LastSendTime = time;
				MinimalNetCollectMessages();
				//send messages , mainly  needed so that the host will send the game description
				//allowing people to join while the host is loading
				NetSendMessages();
			}
		}
#endif
	}
}

void Game_Has_Loaded()
{
	SoundSys_StopAll();
	SoundSys_Management();

	int32_t f = 65536;
	ResetFrameCounter();

	do
	{
		CheckForWindowsMessages();
		ReadUserInput();
		ThisFramesRenderingHasBegun();

		if (f)
		{
			LoadingBarFull_SrcRect.left   = 0;
			LoadingBarFull_SrcRect.right  = 639;
			LoadingBarFull_SrcRect.top    = 0;
			LoadingBarFull_SrcRect.bottom = 39;
			LoadingBarFull_DestRect.left  = MUL_FIXED(ScreenDescriptorBlock.SDB_Width-1,(ONE_FIXED-f)/2);
			LoadingBarFull_DestRect.right = MUL_FIXED(ScreenDescriptorBlock.SDB_Width-1,f)+LoadingBarFull_DestRect.left;

			int h = MUL_FIXED((ScreenDescriptorBlock.SDB_Height)/24,ONE_FIXED-f);
			LoadingBarFull_DestRect.top   = ((ScreenDescriptorBlock.SDB_Height - ScreenDescriptorBlock.SDB_SafeZoneHeightOffset) *11)/12+h;
			LoadingBarFull_DestRect.bottom = (ScreenDescriptorBlock.SDB_Height - ScreenDescriptorBlock.SDB_SafeZoneHeightOffset)-1-h;
			
			// also need this here again, or else the text disappears!
			RenderBriefingText(ScreenDescriptorBlock.SDB_Height/2, ONE_FIXED);
	
			if (!IsDemoVersion())
			{
				DrawProgressBar(LoadingBarFull_SrcRect, LoadingBarFull_DestRect, fullTextureID);
			}
			else
			{
				// background image
				DrawQuad(0, 0, 640, 480, dbTextureID, RCOLOR_XRGB(255, 255, 255), TRANSLUCENCY_OFF);

				// white outline
				DrawQuad(105, 413, 429, 46, NO_TEXTURE, RCOLOR_XRGB(255, 255, 255), TRANSLUCENCY_OFF);

				// red background (no more blue as bar is now full with red)
				DrawQuad(106, 414, 427, 44, NO_TEXTURE, RCOLOR_XRGB(248, 0, 0), TRANSLUCENCY_OFF);
			}

			f -= NormalFrameTime;
			if (f < 0) {
				f = 0;
			}
		}

		if (!IsDemoVersion()) { // demo doesn't show this
			RenderStringCentred(GetTextString(TEXTSTRING_INGAME_PRESSANYKEYTOCONTINUE), ScreenDescriptorBlock.SDB_Width/2, ((ScreenDescriptorBlock.SDB_Height - ScreenDescriptorBlock.SDB_SafeZoneHeightOffset)*23)/24-9, 0xffffffff);
		}

		ThisFramesRenderingHasFinished();
		FlipBuffers();
		FrameCounterHandler();

		/* If in a network game then we may as well check the network messages while waiting*/
		if (AvP.Network != I_No_Network)
		{
			MinimalNetCollectMessages();
			// send messages , mainly  needed so that the host will send the game description
			// allowing people to join while the host is loading
			NetSendMessages();
		}
	}

	while (!DebouncedGotAnyKey && AvP.Network != I_Host /*&& LoadingBarFull != NULL*/); // TODO - check me

	FadingGameInAfterLoading = ONE_FIXED;

	Tex_Release(emptyTextureID);
	Tex_Release(fullTextureID);
	Tex_Release(dbTextureID);
}
