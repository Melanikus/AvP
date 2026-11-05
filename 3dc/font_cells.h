// Whether a font's letters can be found in its picture. Written 2026-11-05 by Melanikus for the AvP port.

#ifndef FONT_CELLS_H
#define FONT_CELLS_H

#include <stdint.h>

// A font's picture is a grid of cells, one letter in each, and the font's .dat file says how large a cell is. Finding a letter divides the picture's size by the cell's, and then divides by what
// that gives. So a cell of no size, or one larger than the picture, would divide by zero. These numbers come from a file, so they're checked when it's read.
inline bool Font_CellsFit(uint32_t mapWidth, uint32_t mapHeight, uint32_t cellWidth, uint32_t cellHeight)
{
	return cellWidth >= 1 && cellHeight >= 1 && cellWidth <= mapWidth && cellHeight <= mapHeight;
}

#endif
