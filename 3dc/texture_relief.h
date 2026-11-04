// Gives a level's flat pictures a feeling of depth. Written 2026-11-04 by Melanikus for the AvP port.
// See texture_relief.cpp for how it works.
//
// THE RELIEF PICTURE: anyone making one by hand has to match this. It's the same size as its texture and holds four numbers for each pixel, 0 to 255.
//   red    how far the surface leans towards the right of the picture
//   green  how far it leans towards the bottom of the picture (the way tools call "DirectX", or "Y-")
//   blue   how much it faces straight out of the picture
//   alpha  how high the surface is: 255 is the highest point, 0 the deepest
// Red, green and blue are each 128 + 127 * a number from -1 to 1, and those three make an arrow 1 long. So a flat pixel is exactly 128, 128, 255.
//
// LATER: players will be able to bring their own. If somerimname_Normal.rim exists it will be used in place of working one out, and it goes in
// TextureRelief::ReliefPictureFor, the one place a texture gets its relief picture. A file like that comes from strangers, so it has to pass the same
// size checks as everything here: SizeIsFine, before anything is allocated or added up.

#ifndef TEXTURE_RELIEF_H
#define TEXTURE_RELIEF_H

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "TextureManager.h"

class TextureRelief
{
	public:
		// mostBytes is the most its relief pictures may add up to. A picture that would go past it goes without.
		explicit TextureRelief(size_t mostBytes = 64 * 1024 * 1024);

		// With -reliefoff on the command line. Nothing is built after this, and the game draws exactly as if none of this existed.
		void TurnOff();
		bool IsOn() const;

		// While a level loads, for each of its textures. A texture that can't have a relief picture simply goes without.
		void Build(texID_t picture);

		// Whether the surface pass can use relief pictures at all. If not, it draws the way it did without them.
		bool IsReady() const;

		// The relief picture to use with a texture. One that has none gets a flat picture, which changes nothing, so anything can be asked for.
		texID_t PictureFor(texID_t picture) const;

		// Gives every relief picture back, when the level's textures go.
		void ReleaseAll();

		size_t PicturesHeld() const;
		size_t BytesHeld() const;

		// Whether a picture of this size may be worked on. Every size comes from a file, so this comes before anything is allocated or added up.
		static bool SizeIsFine(uint32_t width, uint32_t height);

		// The calculation itself, on memory: no graphics card and nothing of the game, so a test can run it. Both pictures are four bytes a pixel, in the
		// order blue, green, red, alpha, and a pitch is how many bytes a row takes up. Gives back false, with relief untouched, if it can't be done.
		static bool Calculate(uint8_t const *picture, uint32_t picturePitch, uint32_t width, uint32_t height, uint8_t *relief, uint32_t reliefPitch);

	private:
		texID_t ReliefPictureFor(texID_t picture);
		texID_t CalculatedPictureFor(texID_t picture);
		bool MakeFlatPicture();

		size_t mostBytesHeld;
		bool on;
		bool flatPictureMade;
		texID_t flatPicture;
		std::vector<texID_t> reliefOf;      // by texture number. NO_TEXTURE means none
		size_t picturesHeld;
		size_t bytesHeld;
};

extern TextureRelief textureRelief;

#endif
