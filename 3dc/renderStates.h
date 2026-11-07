// Modified 2026-11-05 by Melanikus for the AvP port:
//   a way of blending that brightens what's already there, for the shoulder lamp
#ifndef _RenderStates_h_
#define _RenderStates_h_

/*
 * moved these from d3_func.h to here as an effort to keep the d3d stuff out of .c files. These aren't
 * DirectX specific so didn't need to be there anyway, and any files such as decal.c and particl.c will
 * be fine just including this file.
 */
enum TRANSLUCENCY_TYPE
{
	TRANSLUCENCY_OFF,
	TRANSLUCENCY_NORMAL,
	TRANSLUCENCY_INVCOLOUR,
	TRANSLUCENCY_COLOUR,
	TRANSLUCENCY_GLOWING,
	TRANSLUCENCY_DARKENINGCOLOUR,
	TRANSLUCENCY_BRIGHTEN,   // what's there times one and a bit: dark stays dark and black stays black
//	TRANSLUCENCY_JUSTSETZ,
	TRANSLUCENCY_NOT_SET,
	NUM_TRANSLUCENCY_TYPES
};

enum FILTERING_MODE_ID
{
	FILTERING_BILINEAR_OFF,
	FILTERING_BILINEAR_ON,
	FILTERING_NOT_SET,
	NUM_FILTERING_TYPES
};

enum TEXTURE_ADDRESS_MODE
{
	TEXTURE_CLAMP,
	TEXTURE_WRAP,
	NUM_TEXTURE_ADDRESS_MODES
};

enum ZWRITE_ENABLE
{
	ZWRITE_DISABLED,
	ZWRITE_ENABLED
};

typedef struct
{
	enum TRANSLUCENCY_TYPE		TranslucencyMode;
	enum FILTERING_MODE_ID		FilteringMode[8];
	enum TEXTURE_ADDRESS_MODE	TextureAddressMode[8];
	enum ZWRITE_ENABLE			ZWriteEnable;
	int FogDistance;
	unsigned int FogIsOn :1;
	unsigned int WireFrameModeIsOn :1;

} RENDERSTATES;

#endif
