// The picture a scope draws over the screen. See weapon_scope.h. Written 2026-11-06 by Melanikus for the AvP port.
// The game takes this from MelPyth M5 by Mupp. Only how it behaves was read from that mod, and this is written new.

#include "3dc.h"
#include "renderer.h"
#include "TextureManager.h"
#include "renderStates.h"
#include "weapon_scope.h"

extern SCREENDESCRIPTORBLOCK ScreenDescriptorBlock;

static char const *const kScopePicture = "graphics/MelPyth/HUDs/Marine/RevolverScope.rim";

// The picture is black with a white hole. This way of drawing multiplies what's on the screen by the picture, so white leaves the view as it is and black covers it.
void WeaponScope_Draw(void)
{
	// It's only looked for once. A file that isn't there shouldn't be searched for on every frame.
	static bool lookedForPicture = false;
	static texID_t picture = MISSING_TEXTURE;

	if (!lookedForPicture)
	{
		lookedForPicture = true;
		picture = Tex_CreateFromRIM(kScopePicture);
	}

	if (picture == MISSING_TEXTURE)
	{
		return;
	}

	int const width = ScreenDescriptorBlock.SDB_Width;
	int const height = ScreenDescriptorBlock.SDB_Height;
	WeaponScopeLayout const layout = WeaponScope_LayoutFor(width, height);
	uint32_t const white = RCOLOR_ARGB(255, 255, 255, 255);
	uint32_t const black = RCOLOR_XRGB(0, 0, 0);

	DrawQuad(layout.pictureX, layout.pictureY, layout.pictureWidth, layout.pictureHeight, picture, white, TRANSLUCENCY_COLOUR);

	// Black bars wherever the picture doesn't reach. Placing it by its cross can leave a few pixels even on a screen the same shape as the view.
	int const rightOfPicture = layout.pictureX + layout.viewWidth;
	int const belowPicture = layout.pictureY + layout.pictureHeight;

	if (layout.pictureX > 0)
	{
		DrawQuad(0, 0, layout.pictureX, height, NO_TEXTURE, black, TRANSLUCENCY_OFF);
	}

	if (rightOfPicture < width)
	{
		DrawQuad(rightOfPicture, 0, width - rightOfPicture, height, NO_TEXTURE, black, TRANSLUCENCY_OFF);
	}

	if (layout.pictureY > 0)
	{
		DrawQuad(0, 0, width, layout.pictureY, NO_TEXTURE, black, TRANSLUCENCY_OFF);
	}

	if (belowPicture < height)
	{
		DrawQuad(0, belowPicture, width, height - belowPicture, NO_TEXTURE, black, TRANSLUCENCY_OFF);
	}
}
