// Gives each see-through texel of a picture the colour of the solid texels beside it, so that smooth filtering never mixes in a colour
// nobody was meant to see. Written 2026-11-08 by Melanikus for the AvP port (#148). Only the copy in memory changes, never the files.
//
// WHY: a RIM picture marks its see-through texels with one colour, its "transparent colour", often black or a bright pink. RimLoader
// gives those texels an alpha of 0 but leaves that colour in them, since nothing was ever meant to see it. Smooth (bilinear) filtering
// reads four texels and mixes them. At the edge of a hole, half of what it mixes is that hidden colour, at half the alpha, and the game's
// alpha test draws anything above 0. So every hole, and the edge of every cut-out shape, had a thin line of the hidden colour round it:
// the outline the user saw on the marines' faces.
//
// WHAT THIS DOES: each texel with an alpha of 0 that has a solid texel among its eight neighbours takes the average colour of those solid
// neighbours. Its alpha stays 0, so it's still a hole, and nothing new shows anywhere: only the mixed edge now mixes the solid colour with
// itself. A hole with no solid neighbour keeps its colour, because filtering never mixes it with anything solid.
//
// Neighbours wrap round the picture's edges, as the graphics card reads a picture drawn with wrap, which is how the level's are drawn.
// Only solid texels are read and only holes are written, so the answer doesn't depend on which texel is done first.
//
// pixels: 4 bytes a texel, blue, green, red, alpha, as RimLoader writes them; each row starts pitch bytes after the last. Nothing past
// width texels of a row is read or written. The answer is false, with nothing changed, if the sizes can't be a picture.

#ifndef RIM_EDGES_H
#define RIM_EDGES_H

#include <stddef.h>
#include <stdint.h>

inline bool RimEdges_ColourTheHoles(uint8_t *pixels, uint32_t width, uint32_t height, uint32_t pitch)
{
	if (!pixels || width == 0 || height == 0 || width > pitch / 4)
	{
		return false;
	}

	for (uint32_t y = 0; y < height; y++)
	{
		for (uint32_t x = 0; x < width; x++)
		{
			uint8_t *const texel = pixels + (size_t)y * pitch + (size_t)x * 4;

			if (texel[3] != 0)
			{
				continue;
			}

			uint32_t blue = 0, green = 0, red = 0, solid = 0;

			for (int dy = -1; dy <= 1; dy++)
			{
				for (int dx = -1; dx <= 1; dx++)
				{
					if (dx == 0 && dy == 0)
					{
						continue;
					}

					// One step left of the first column is the last column, as the card reads a picture that wraps
					uint32_t const nx = (dx < 0) ? (x == 0 ? width - 1 : x - 1) : (dx > 0) ? (x == width - 1 ? 0 : x + 1) : x;
					uint32_t const ny = (dy < 0) ? (y == 0 ? height - 1 : y - 1) : (dy > 0) ? (y == height - 1 ? 0 : y + 1) : y;
					uint8_t const *const neighbour = pixels + (size_t)ny * pitch + (size_t)nx * 4;

					// Only a solid texel's colour counts. A hole's is never read, so it doesn't matter which hole was given a colour first.
					if (neighbour[3] == 0)
					{
						continue;
					}

					blue += neighbour[0];
					green += neighbour[1];
					red += neighbour[2];
					solid++;
				}
			}

			if (solid > 0)
			{
				texel[0] = (uint8_t)(blue / solid);
				texel[1] = (uint8_t)(green / solid);
				texel[2] = (uint8_t)(red / solid);
			}
		}
	}

	return true;
}

#endif
