// The tester's mark key: a line in the log and a picture of the game's own screen. Written 2026-11-07 by Melanikus for the AvP port.
// mark_files.h says what a mark is for, how its pictures are kept, and why the folder is treated as input.
//
// Modified 2026-11-09 by Melanikus for the AvP port:
//   Mark_ThisPage marks a menu page, with no player (-dev -onlinescreens' pictures). Its number and time start the same way a level's do.
#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "bh_types.h"
#include "user_io.h"
#include "networking.h"
#include "mark_key.h"
#include "mark_files.h"
#include "logString.h"
#include "utilities.h"

#include <d3dx9.h>
#include <time.h>

extern unsigned char DebouncedKeyboardInput[];
extern void NewOnScreenMessage(char *messagePtr);

// The key a tester presses
static const unsigned char kMarkKey = KEY_F2;

// A moment noted by Mark_ReadKey, waiting for the end of its frame to have its picture taken
static bool momentWaiting = false;
static Mark_Moment moment;
static char momentLevel[64];
static char momentNetwork[256];
static unsigned long lastMarkAt = 0;

// The last number given this run, so two marks never share one, even when a picture couldn't be saved and the folder can't show it
static int lastNumber = 0;

// The words on the screen, kept here because the screen keeps what it's given
static char shownWords[100];

static void Show(const char *words)
{
	_snprintf(shownWords, sizeof(shownWords) - 1, "%s", words);
	shownWords[sizeof(shownWords) - 1] = '\0';
	NewOnScreenMessage(shownWords);
}

// Has the player given the mark key to one of their controls, for the species they're playing? Then it does that, and marks nothing.
static bool KeyIsTaken()
{
	PLAYER_INPUT_CONFIGURATION const *primary = &MarineInputPrimaryConfig;
	PLAYER_INPUT_CONFIGURATION const *secondary = &MarineInputSecondaryConfig;

	if (AvP.PlayerType == I_Alien)
	{
		primary = &AlienInputPrimaryConfig;
		secondary = &AlienInputSecondaryConfig;
	}
	else if (AvP.PlayerType == I_Predator)
	{
		primary = &PredatorInputPrimaryConfig;
		secondary = &PredatorInputSecondaryConfig;
	}

	return Mark_KeyIsIn((const unsigned char *)primary, sizeof(*primary), kMarkKey) || Mark_KeyIsIn((const unsigned char *)secondary, sizeof(*secondary), kMarkKey);
}

// The start of any mark: its number and the time of day, in moment, and nothing else of it yet. False, with the reason on the screen, when
// there can't be a mark now: within a second of the last, or with no number left in the folder.
static bool StartAMoment()
{
	unsigned long const now = GetTickCount();

	if (Mark_TooSoon(now, lastMarkAt))
	{
		Show("One mark a second: that one wasn't saved");
		return false;
	}

	std::string const folder = GetSaveFolderPath() ? GetSaveFolderPath() : "";
	int const fromFolder = Mark_NextNumber(folder);
	int const number = (fromFolder != 0 && fromFolder <= lastNumber) ? lastNumber + 1 : fromFolder;

	if (number == 0 || number > kMarkHighestNumber)
	{
		Show("No more marks can be saved in this folder");
		return false;
	}

	lastMarkAt = now ? now : 1;
	lastNumber = number;

	time_t const clock = time(NULL);
	struct tm local;

	if (localtime_s(&local, &clock) != 0)
	{
		memset(&local, 0, sizeof(local));
	}

	memset(&moment, 0, sizeof(moment));
	moment.number = number;
	moment.hours = local.tm_hour;
	moment.minutes = local.tm_min;
	moment.seconds = local.tm_sec;
	return true;
}

void Mark_ReadKey()
{
	if (!DebouncedKeyboardInput[kMarkKey] || momentWaiting || !Player || !Player->ObStrategyBlock || KeyIsTaken())
	{
		return;
	}

	if (!StartAMoment())
	{
		return;
	}

	PLAYER_STATUS const *status = (PLAYER_STATUS const *)Player->ObStrategyBlock->SBdataptr;

	// The level's name: one of the game's own, or a custom one's, which the log has said already
	_snprintf(momentLevel, sizeof(momentLevel) - 1, "%s", (AvP.CurrentEnv >= 0 && AvP.CurrentEnv < I_Num_Environments && Env_List[AvP.CurrentEnv]) ? Env_List[AvP.CurrentEnv]->main : "a level of its own");
	momentLevel[sizeof(momentLevel) - 1] = '\0';

	Net_DescribeConnections(momentNetwork, sizeof(momentNetwork));

	moment.level = momentLevel;
	moment.species = AvP.PlayerType == I_Alien ? "alien" : (AvP.PlayerType == I_Predator ? "predator" : "marine");
	moment.x = Player->ObWorld.vx;
	moment.y = Player->ObWorld.vy;
	moment.z = Player->ObWorld.vz;
	moment.facing = Player->ObEuler.EulerY & 4095;
	moment.health = status ? status->Health >> 16 : 0;
	// The slot is checked before it picks from the list, whatever else keeps it in range: -1 means the mark couldn't say
	int const slot = status ? (int)status->SelectedWeaponSlot : -1;
	moment.weapon = (slot >= 0 && slot < MAX_NO_OF_WEAPON_SLOTS) ? (int)status->WeaponSlot[slot].WeaponIDNumber : -1;
	moment.network = momentNetwork[0] ? momentNetwork : NULL;
	momentWaiting = true;
}

bool Mark_ThisPage(const char *page)
{
	if (!page || momentWaiting || !StartAMoment())
	{
		return false;
	}

	moment.page = page;
	momentWaiting = true;
	return true;
}

// The frame as it is now, as a PNG in memory, from the game's own picture of it: never the desktop. Gives back NULL if it can't be made.
static ID3DXBuffer *PictureOfThisFrame(LPDIRECT3DDEVICE9 device)
{
	IDirect3DSurface9 *backBuffer = 0;
	IDirect3DSurface9 *copy = 0;
	ID3DXBuffer *picture = 0;
	D3DSURFACE_DESC size;

	if (SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer))
		&& SUCCEEDED(backBuffer->GetDesc(&size))
		&& SUCCEEDED(device->CreateOffscreenPlainSurface(size.Width, size.Height, size.Format, D3DPOOL_SYSTEMMEM, &copy, NULL))
		&& SUCCEEDED(device->GetRenderTargetData(backBuffer, copy)))
	{
		// Made in memory and written by Mark_WritePicture, so the file is only ever a new one, in a plain folder (mark_files.h)
		if (FAILED(D3DXSaveSurfaceToFileInMemory(&picture, D3DXIFF_PNG, copy, NULL, NULL)))
		{
			picture = 0;
		}
	}

	if (copy)
	{
		copy->Release();
	}

	if (backBuffer)
	{
		backBuffer->Release();
	}

	return picture;
}

void Mark_EndFrame(LPDIRECT3DDEVICE9 device)
{
	if (!momentWaiting)
	{
		return;
	}

	momentWaiting = false;

	std::string const folder = GetSaveFolderPath() ? GetSaveFolderPath() : "";
	bool saved = false;

	if (device && Mark_FolderIsPlain(folder))
	{
		ID3DXBuffer *const picture = PictureOfThisFrame(device);

		if (picture)
		{
			saved = Mark_WritePicture(Mark_PictureName(folder, moment.number), picture->GetBufferPointer(), picture->GetBufferSize());
			picture->Release();
		}
	}

	if (saved)
	{
		Mark_KeepTheNewest(folder, moment.number);
	}

	// The line goes through the log, which leaves out the player's names, folders and addresses (log_files.h)
	char words[512];
	Mark_DescribeMoment(moment, words, sizeof(words));
	LogString(std::string(words) + (saved ? "" : " (its picture couldn't be saved)"));

	char shown[64];
	_snprintf(shown, sizeof(shown) - 1, saved ? "Mark %d saved" : "Mark %d saved, without its picture", moment.number);
	shown[sizeof(shown) - 1] = '\0';
	Show(shown);
}
