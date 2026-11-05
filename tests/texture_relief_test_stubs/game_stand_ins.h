// What texture_relief.cpp needs from the game, made small enough for a test. Written 2026-11-04 by Melanikus for the AvP port.
//
// The compiler puts this ahead of everything else in texture_relief.cpp (/FI in the .args file). A header finds its neighbours in its own folder before
// any other folder, so a folder of stand-ins can't take the game's place. These guards can: each is the name the real header checks to see if it's
// already been read, so when texture_relief.cpp asks for the real one, the compiler skips it.

#ifndef _TextureManager_h_
#define _TextureManager_h_
#define _included_d3_func_h_
#define _renderer_h_

#include <stddef.h>
#include <stdint.h>
#include <string>

typedef size_t texID_t;

extern texID_t NO_TEXTURE;
extern texID_t MISSING_TEXTURE;

enum TextureUsage { TextureUsage_Normal, TextureUsage_Dynamic };
enum TextureLock  { TextureLock_Normal, TextureLock_Discard };

struct Texture
{
	std::string name;
	TextureUsage usage;
	uint16_t realWidth;
	uint16_t realHeight;
	uint8_t bitsPerPixel;
};

texID_t Tex_Create(const std::string &textureName, uint32_t width, uint32_t height, uint32_t bitsPerPixel, enum TextureUsage usageType);
const Texture &Tex_GetTextureDetails(texID_t textureID);
bool Tex_Lock(texID_t textureID, uint8_t **data, uint32_t *pitch, enum TextureLock lockType = TextureLock_Normal);
bool Tex_Unlock(texID_t textureID);
texID_t Tex_CreateWithCopies(const std::string &textureName, uint32_t side, uint32_t levels);
bool Tex_LockCopy(texID_t textureID, uint32_t level, uint8_t **data, uint32_t *pitch);
bool Tex_UnlockCopy(texID_t textureID, uint32_t level);
void Tex_Release(texID_t textureID);

#endif

#ifndef LOGSTRING_H
#define LOGSTRING_H
#include <string>
void LogString(const std::string &text);
#endif
