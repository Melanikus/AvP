// Gives a level's flat pictures a feeling of depth. Written 2026-11-04 by Melanikus for the AvP port.
// See texture_relief.cpp for how it works.
//
// THE RELIEF PICTURE: anyone making one by hand has to match this. It holds three numbers for each pixel, 0 to 255.
//   red    how far the surface leans towards the right of the picture
//   green  how far it leans towards the bottom of the picture (the way tools call "DirectX", or "Y-")
//   blue   how much it faces straight out of the picture
// Red, green and blue are each 128 + 127 * a number from -1 to 1, and those three make an arrow 1 long. So a flat pixel is exactly 128, 128, 255.
//
// WHERE ONE COMES FROM, in this order. TextureRelief::ReliefPictureFor is the one place that decides.
//   1. A hand-made map: a file beside the texture, with _Normal on the end of its name, so graphics/Envrnmts/Derelict/Hive01_Normal.rim goes with graphics/Envrnmts/Derelict/Hive01.rim.
//      It's a file from a stranger: relief_map_file.h says what it has to be, and that's checked before anything is allocated. If it fails, the log says why, and the texture
//      carries on as if the file weren't there. It can be larger than its texture, because it's read by position on the picture, not by pixel.
//   2. One worked out from the texture, if SetAutoGenerate has been asked for.
//   3. Otherwise a flat picture, which changes nothing.
//
// THE DEPTH MAP is a second picture, read the same way from name_Depth.rim, with the same rules and from the same allowance. A texture can have either, both or neither, because each is read
// on its own. White is the level of the polygon, and black is as deep as the surface goes. reliefMarch.fxh walks through it. A texture with no depth map gets the flat picture instead, and
// its alpha of 0 says there's no depth, so nothing moves. A film gets neither, because it's changed while it plays. name_Depth.dds is noticed and logged, and isn't read yet. A picture this
// code works out keeps a height in its alpha, which nothing reads.

#ifndef TEXTURE_RELIEF_H
#define TEXTURE_RELIEF_H

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "TextureManager.h"

class TextureRelief
{
	public:
		// mostBytes is the most its relief pictures may add up to, over the whole time the game is running, because nothing is given back between levels:
		// see ReleaseAll. A picture that would go past it goes without relief, and the log says so once. It can't crash anything: the texture just looks flat.
		// The stock levels and characters can load 50.8 MB of pictures, so a stock game shouldn't meet this limit. Every picture the game has comes to 72.9 MB,
		// so if that count ever proves short, the last few textures of a very long session would lose their relief.
		// The limit's other job is to hold back a hostile level: it fits 16 of the largest pictures allowed, about 0.6 seconds of work.
		// mostHandMadeBytes is the same for hand-made maps, which are larger, and can be many. A map past it goes without, and the log says so.
		//
		// A map costs time to read and check whether it's used or not, and a hostile level can name thousands of them, each one only wrong at its very end. So every file that's tried
		// is charged to what one level may spend: mostHandMadeTries files, and three times mostHandMadeBytes in bytes read and unpacked. Once either is spent, or the maps kept have reached
		// mostHandMadeBytes, no more files are opened until the next level, and the log says so once. A texture that wasn't looked at this way can still get its map on a later level.
		//
		// These leave room for real maps. The game has 1,028 pictures in all and its largest level names 38, so a map for every one of them is well under the tries. And a map that's kept
		// takes at most about 2.1 times what it holds to read and unpack, so maps that fill mostHandMadeBytes never run out of bytes first.
		static size_t const kMostHandMadeTriesPerLevel = 2048;
		explicit TextureRelief(size_t mostBytes = 64 * 1024 * 1024, size_t mostHandMadeBytes = 256 * 1024 * 1024, size_t mostHandMadeTries = kMostHandMadeTriesPerLevel);

		// With -reliefoff on the command line. Nothing is built after this, and the game draws exactly as if none of this existed.
		void TurnOff();
		bool IsOn() const;

		// Whether a texture with no hand-made map gets one worked out. The Video menu's "Auto-generate maps", and off to start with. Turned off, it takes effect at once. Turned on, it takes
		// effect for each texture when a level next asks for it. A hand-made map stays either way.
		void SetAutoGenerate(bool wanted);
		bool AutoGenerates() const;

		// While a level loads, for each of its textures. A texture that can't have a relief picture simply goes without.
		void Build(texID_t picture);

		// Whether the surface pass can use relief pictures at all. If not, it draws the way it did without them.
		bool IsReady() const;

		// The relief picture to use with a texture. One that has none gets a flat picture, which changes nothing, so anything can be asked for.
		texID_t PictureFor(texID_t picture) const;

		// The depth map to use with a texture, for reliefMarch.fxh. One that has none gets the same flat picture, whose alpha of 0 means there's no depth, so no pixel of it moves.
		texID_t HeightFor(texID_t picture) const;

		// Writes what's held to the log. Called when a level has finished loading.
		void LogWhatIsHeld();

		// Gives every relief picture back. Only release_rif_bitmaps asks for this, and nothing calls that today: the game keeps a level's textures until it
		// exits, so that a picture used again later doesn't have to load again, and these stay with them. A picture keeps its number all that time, so a
		// relief picture can't end up beside the wrong texture.
		void ReleaseAll();

		size_t PicturesHeld() const;
		size_t BytesHeld() const;
		size_t HandMadeBytesHeld() const;
		size_t HandMadeTriesThisLevel() const;
		uint64_t HandMadeBytesTriedThisLevel() const;
		size_t MapsNotThereThisLevel() const;

		// Whether a picture of this size may be worked on. Every size comes from a file, so this comes before anything is allocated or added up.
		static bool SizeIsFine(uint32_t width, uint32_t height);

		// The calculation itself, on memory: no graphics card and nothing of the game, so a test can run it. Both pictures are four bytes a pixel, in the
		// order blue, green, red, alpha, and a pitch is how many bytes a row takes up. Gives back false, with relief untouched, if it can't be done.
		static bool Calculate(uint8_t const *picture, uint32_t picturePitch, uint32_t width, uint32_t height, uint8_t *relief, uint32_t reliefPitch);

	private:
		// What's known about one texture
		struct Entry
		{
			texID_t relief;              // NO_TEXTURE means none
			bool workedOut;              // it came from this code, not from a file
			bool lookedForFile;          // a hand-made map is only looked for once, so that the disk isn't asked again and the log says things once
			size_t levelCounted;         // the last level that counted this texture
			texID_t height;              // its depth map. NO_TEXTURE means none
			bool lookedForDepth;         // on its own, because a texture can have its map on one level and its depth map on a later one
		};

		void ReliefPictureFor(texID_t picture, Entry &entry);
		enum MapKind { kNormalMap, kDepthMap };

		bool MayTryAnotherMap();
		void DepthPictureFor(texID_t picture, Entry &entry);
		texID_t HandMadePictureFor(texID_t picture, MapKind kind);
		texID_t CalculatedPictureFor(texID_t picture);
		void NoteDepthFile(std::string const &pictureName);
		bool MakeFlatPicture();

		size_t mostBytesHeld;
		size_t mostHandMadeBytesHeld;
		bool limitWasLogged;
		bool on;
		bool autoGenerate;
		bool flatPictureMade;
		texID_t flatPicture;
		std::vector<Entry> reliefOf;        // by texture number
		size_t picturesHeld;
		size_t bytesHeld;
		size_t handMadeBytesHeld;
		size_t mostHandMadeTries;
		size_t handMadeTries;               // map files opened on this level, used or not
		uint64_t handMadeBytesTried;        // bytes of them read and unpacked on this level, used or not
		bool stoppedTryingWasLogged;        // on this level
		size_t mapsNotThere;                // map files looked for on this level that aren't there. Most textures have none, so this isn't an error, and it's only a count
		size_t levelNumber;                 // goes up each time LogWhatIsHeld says what a level had
};

extern TextureRelief textureRelief;

#endif
