// See hudson_grid.h for what this is. Written 2026-11-05 by Melanikus for the AvP port. This file knows nothing about the game.
#include "hudson_grid.h"

#include <algorithm>
#include <math.h>
#include <new>
#include <stdlib.h>

namespace
{
	// How many columns either side count as being within his square. A column that any part of a polygon passes through might be within 450 of his middle, because
	// one column either side reaches 150 + 300 = 450 from the middle of his own. So at 300 mm this is exactly his square.
	int const kColumnsInHisSquare = 1;

	// A drop lands this many columns out, where his whole square is past the edge
	int const kColumnsOutForADrop = 2;

	// Heights this close together are one surface. A polygon's edge rounded up and its neighbour's rounded down can be this far apart.
	int const kSameHeight = 1;

	// A patch of floor smaller than this, in places, doesn't count as somewhere to go. It's 6 square metres: the top of a lamp, or a crate.
	int const kSmallPatch = 6000000 / (kHudsonGridCell * kHudsonGridCell);

	// How far from a wall "the middle of the room" reaches. Closer than this costs more, up to double right against the wall.
	int const kMiddleReachesInColumns = 1500 / kHudsonGridCell;

	// A rise smaller than this isn't climbing. It's what a slope's height comes to across part of a column, or two polygons not quite level.
	int const kClimbingIsMoreThan = 50;

	// Walking up or down counts for half its height, on top of the distance across. A drop counts as three times its height, so that he takes the stairs if there are any.
	float const kRiseCosts = 0.5f;
	float const kDropCosts = 3.0f;

	int const kMostPlacesKeptForASpot = 64;

	// A spot's floor can be up to this much worse than the best one, and still be tried
	int const kSpotScoreRoom = 800;

	// About how often the way keeps a point
	int const kWayPointEvery = 900;

	struct Corner
	{
		double x, h, z;
	};

	enum Axis { kAxisX, kAxisZ };

	double Along(const Corner &c, Axis axis)
	{
		return axis == kAxisX ? c.x : c.z;
	}

	// Keeps the part of a polygon between two lines. A polygon lying exactly on a line is kept, so that a wall along a column's edge blocks both sides of it. Each line can add one
	// corner, so out needs room for two more than in has. It gives back how many corners there are now.
	int KeepBetween(const Corner *in, int count, Axis axis, double low, double high, Corner *out)
	{
		Corner middle[kHudsonGridMostCorners + 4];
		int middleCount = 0;

		for (int i = 0; i < count; i++)
		{
			Corner const &a = in[i];
			Corner const &b = in[(i + 1) % count];
			double const da = Along(a, axis) - low, db = Along(b, axis) - low;

			if (da >= 0.0)
			{
				middle[middleCount++] = a;
			}

			if ((da > 0.0 && db < 0.0) || (da < 0.0 && db > 0.0))
			{
				double const t = da / (da - db);
				Corner const made = { a.x + (b.x - a.x) * t, a.h + (b.h - a.h) * t, a.z + (b.z - a.z) * t };
				middle[middleCount++] = made;
			}
		}

		int outCount = 0;

		for (int i = 0; i < middleCount; i++)
		{
			Corner const &a = middle[i];
			Corner const &b = middle[(i + 1) % middleCount];
			double const da = high - Along(a, axis), db = high - Along(b, axis);

			if (da >= 0.0)
			{
				out[outCount++] = a;
			}

			if ((da > 0.0 && db < 0.0) || (da < 0.0 && db > 0.0))
			{
				double const t = da / (da - db);
				Corner const made = { a.x + (b.x - a.x) * t, a.h + (b.h - a.h) * t, a.z + (b.z - a.z) * t };
				out[outCount++] = made;
			}
		}

		return outCount;
	}

	int Find(std::vector<int> &parent, int place)
	{
		while (parent[place] != place)
		{
			parent[place] = parent[parent[place]];
			place = parent[place];
		}

		return place;
	}
}

// One polygon crossing one column, before they're joined up
struct HudsonGrid::Bit
{
	uint32_t column;     // row times width, plus how far along the row
	int low, high;       // heights, going up
	uint32_t flags;

	bool operator<(const Bit &other) const
	{
		return column != other.column ? column < other.column : low < other.low;
	}
};

namespace
{
	uint32_t const kIsFloor = 1;       // the top of it can be stood on
	uint32_t const kBlocks = 2;
	uint32_t const kNearlyFlat = 4;
	uint32_t const kCrawl = 8;
}

float HudsonGrid_HowMuchItFacesUp(const HudsonGridPolygon &polygon)
{
	double nx = 0.0, ny = 0.0, nz = 0.0;

	for (int i = 0; i < polygon.corners; i++)
	{
		int const j = (i + 1) % polygon.corners;

		// Heights go up here, so y has its sign turned over
		double const ax = polygon.x[i], ah = -(double)polygon.y[i], az = polygon.z[i];
		double const bx = polygon.x[j], bh = -(double)polygon.y[j], bz = polygon.z[j];

		nx += (ah - bh) * (az + bz);
		ny += (az - bz) * (ax + bx);
		nz += (ax - bx) * (ah + bh);
	}

	double const size = sqrt(nx * nx + ny * ny + nz * nz);

	// The minus sign was measured: on Derelict, flat polygons at the bottom of a room come out 592 facing up and 48 facing down with it
	return size > 0.0 ? (float)(-ny / size) : 0.0f;
}

HudsonGrid::HudsonGrid() : limitPolygons(kHudsonGridMostPolygons), limitBits(kHudsonGridMostBits), limitPlaces(kHudsonGridMostPlaces), limitMoves(kHudsonGridMostMoves)
{
	Clear();
}

void HudsonGrid::LowerLimitsForATest(size_t mostPolygons, size_t mostBits, size_t mostPlaces, size_t mostMoves)
{
	limitPolygons = mostPolygons;
	limitBits = mostBits;
	limitPlaces = mostPlaces;
	limitMoves = mostMoves;
}

void HudsonGrid::Clear()
{
	ready = false;
	minX = minZ = width = depth = 0;
	drops = polygonsLeftOut = polygonsUsed = bitsBeforeJoining = 0;
	searchNumber = 0;
	state = kHudsonGridNoWay;
	nearestPlace = -1;
	nearestDistance = 0.0f;
	goalX = goalZ = goalAskedX = goalAskedZ = 0;
	looks = 0;
	goalWasFound = startWasFound = false;
	goalFloorY = 0;

	// swap, because clear keeps the memory
	std::vector<int>().swap(rowStart);
	std::vector<uint16_t>().swap(columnX);
	std::vector<int>().swap(columnFirstSpan);
	std::vector<Span>().swap(spans);
	std::vector<int>().swap(columnFirstPlace);
	std::vector<uint16_t>().swap(placeX);
	std::vector<uint16_t>().swap(placeZ);
	std::vector<int>().swap(placeHeight);
	std::vector<uint8_t>().swap(placeFlat);
	std::vector<int>().swap(firstMove);
	std::vector<uint32_t>().swap(moves);
	std::vector<uint8_t>().swap(fromWall);
	std::vector<uint8_t>().swap(smallPatch);
	std::vector<int>().swap(seen);
	std::vector<float>().swap(costSoFar);
	std::vector<int>().swap(cameFrom);
	std::vector<float>().swap(heapCost);
	std::vector<int>().swap(heapPlace);
	std::vector<int>().swap(goals);
	std::vector<int>().swap(goalMark);
	std::vector<HudsonGridPoint>().swap(way);
}

size_t HudsonGrid::BytesKept() const
{
	return rowStart.size() * sizeof(int) + columnX.size() * sizeof(uint16_t) + columnFirstPlace.size() * sizeof(int)
		+ placeHeight.size() * (sizeof(int) + 2 * sizeof(uint16_t) + 3 * sizeof(uint8_t) + sizeof(int) * 3 + sizeof(float))
		+ moves.size() * sizeof(uint32_t);
}

// Marks every column one polygon passes through. It says false if there's too much of it all.
bool HudsonGrid::MarkOnePolygon(const HudsonGridPolygon &polygon, std::vector<Bit> *bits)
{
	bool const isFloor = polygon.canStandOn && polygon.up >= kHudsonGridFloorFacing;

	if (!polygon.blocks && !isFloor)
	{
		return true;
	}

	uint32_t const flags = (isFloor ? kIsFloor : 0) | (polygon.blocks ? kBlocks : 0) | (polygon.up >= kHudsonGridNearlyFlatFacing ? kNearlyFlat : 0) | (polygon.crawl ? kCrawl : 0);

	Corner corners[kHudsonGridMostCorners];
	double lowZ = polygon.z[0], highZ = polygon.z[0];

	for (int i = 0; i < polygon.corners; i++)
	{
		corners[i].x = polygon.x[i];
		corners[i].h = -(double)polygon.y[i];
		corners[i].z = polygon.z[i];
		lowZ = (std::min)(lowZ, corners[i].z);
		highZ = (std::max)(highZ, corners[i].z);
	}

	int const firstRow = (int)floor((lowZ - minZ) / kHudsonGridCell), lastRow = (int)floor((highZ - minZ) / kHudsonGridCell);

	for (int row = firstRow; row <= lastRow; row++)
	{
		Corner inRow[kHudsonGridMostCorners + 4];
		int const rowCount = KeepBetween(corners, polygon.corners, kAxisZ, minZ + row * (double)kHudsonGridCell, minZ + (row + 1) * (double)kHudsonGridCell, inRow);

		if (!rowCount)
		{
			continue;
		}

		double lowX = inRow[0].x, highX = inRow[0].x;

		for (int i = 1; i < rowCount; i++)
		{
			lowX = (std::min)(lowX, inRow[i].x);
			highX = (std::max)(highX, inRow[i].x);
		}

		int const firstColumn = (int)floor((lowX - minX) / kHudsonGridCell), lastColumn = (int)floor((highX - minX) / kHudsonGridCell);

		for (int column = firstColumn; column <= lastColumn; column++)
		{
			// a whole polygon's width of nothing but one corner can land just outside. The grid has a column to spare on each side, so this only guards against nonsense
			if (column < 0 || column >= width || row < 0 || row >= depth)
			{
				continue;
			}

			Corner inColumn[kHudsonGridMostCorners + 8];
			int const count = KeepBetween(inRow, rowCount, kAxisX, minX + column * (double)kHudsonGridCell, minX + (column + 1) * (double)kHudsonGridCell, inColumn);

			if (!count)
			{
				continue;
			}

			double low = inColumn[0].h, high = inColumn[0].h;

			for (int i = 1; i < count; i++)
			{
				low = (std::min)(low, inColumn[i].h);
				high = (std::max)(high, inColumn[i].h);
			}

			if (bits->size() >= limitBits)
			{
				return false;
			}

			Bit const bit = { (uint32_t)row * (uint32_t)width + (uint32_t)column, (int)floor(low), (int)ceil(high), flags };

			bits->push_back(bit);
		}
	}

	return true;
}

// Puts each column's bits in height order and joins those that touch. What's left is one list of spans for each column.
void HudsonGrid::JoinBitsIntoSpans(std::vector<Bit> *bits)
{
	std::sort(bits->begin(), bits->end());

	rowStart.assign((size_t)depth + 1, 0);

	size_t i = 0;

	while (i < bits->size())
	{
		uint32_t const column = (*bits)[i].column;
		int const row = (int)(column / (uint32_t)width);

		columnX.push_back((uint16_t)(column % (uint32_t)width));
		columnFirstSpan.push_back((int)spans.size());
		rowStart[(size_t)row + 1]++;

		size_t const firstInColumn = spans.size();

		for (; i < bits->size() && (*bits)[i].column == column; i++)
		{
			Bit const &bit = (*bits)[i];

			if (spans.size() > firstInColumn && bit.low <= spans.back().high + kSameHeight)
			{
				Span &last = spans.back();

				if (bit.high > last.high + kSameHeight)
				{
					// It goes higher, so what's on top of the span is now this polygon. Whether the span blocks is kept from both.
					last.high = bit.high;
					last.flags = (uint8_t)((bit.flags & (kIsFloor | kNearlyFlat | kCrawl)) | ((last.flags | bit.flags) & kBlocks));
				}
				else if (bit.high >= last.high - kSameHeight)
				{
					// The same top: floor if either is
					last.flags = (uint8_t)(last.flags | bit.flags);
				}
				else
				{
					last.flags = (uint8_t)(last.flags | (bit.flags & kBlocks));
				}
			}
			else
			{
				Span const span = { bit.low, bit.high, (uint8_t)bit.flags };

				spans.push_back(span);
			}
		}
	}

	columnFirstSpan.push_back((int)spans.size());

	for (int row = 0; row < depth; row++)
	{
		rowStart[(size_t)row + 1] += rowStart[(size_t)row];
	}
}

// Which entry in the list of columns this one is, or -1 if nothing's in it.
int HudsonGrid::ColumnAt(int ix, int iz) const
{
	if (iz < 0 || iz >= depth || ix < 0 || ix >= width)
	{
		return -1;
	}

	int low = rowStart[(size_t)iz], high = rowStart[(size_t)iz + 1] - 1;

	while (low <= high)
	{
		int const middle = (low + high) / 2;
		int const found = columnX[(size_t)middle];

		if (found == ix)
		{
			return middle;
		}

		if (found < ix)
		{
			low = middle + 1;
		}
		else
		{
			high = middle - 1;
		}
	}

	return -1;
}

// Is anything that blocks him within his square, between these two heights?
bool HudsonGrid::InTheWay(int ix, int iz, int fromHeight, int toHeight) const
{
	for (int dz = -kColumnsInHisSquare; dz <= kColumnsInHisSquare; dz++)
	{
		for (int dx = -kColumnsInHisSquare; dx <= kColumnsInHisSquare; dx++)
		{
			int const column = ColumnAt(ix + dx, iz + dz);

			if (column < 0)
			{
				continue;
			}

			for (int s = columnFirstSpan[(size_t)column]; s < columnFirstSpan[(size_t)column + 1]; s++)
			{
				Span const &span = spans[(size_t)s];

				if ((span.flags & kBlocks) && span.low < toHeight && span.high > fromHeight)
				{
					return true;
				}
			}
		}
	}

	return false;
}

// Is there floor marked as a duct within his square, at about this height?
bool HudsonGrid::CrawlingHere(int ix, int iz, int height) const
{
	for (int dz = -kColumnsInHisSquare; dz <= kColumnsInHisSquare; dz++)
	{
		for (int dx = -kColumnsInHisSquare; dx <= kColumnsInHisSquare; dx++)
		{
			int const column = ColumnAt(ix + dx, iz + dz);

			if (column < 0)
			{
				continue;
			}

			for (int s = columnFirstSpan[(size_t)column]; s < columnFirstSpan[(size_t)column + 1]; s++)
			{
				Span const &span = spans[(size_t)s];

				if ((span.flags & kCrawl) && abs(span.high - height) <= kHudsonGridStepUp)
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool HudsonGrid::MakePlaces()
{
	columnFirstPlace.assign(columnX.size() + 1, 0);

	for (int row = 0; row < depth; row++)
	{
		for (int column = rowStart[(size_t)row]; column < rowStart[(size_t)row + 1]; column++)
		{
			columnFirstPlace[(size_t)column] = (int)placeHeight.size();

			int const ix = columnX[(size_t)column];

			for (int s = columnFirstSpan[(size_t)column]; s < columnFirstSpan[(size_t)column + 1]; s++)
			{
				Span const &span = spans[(size_t)s];

				if (!(span.flags & kIsFloor))
				{
					continue;
				}

				// He crawls as soon as he's in a duct, so the last stretch before its mouth, where his square is already under its roof, has to count as well
				int const tall = CrawlingHere(ix, row, span.high) ? kHudsonGridCrawlHeight : kHudsonGridMarineHeight;

				if (InTheWay(ix, row, span.high + kHudsonGridStepUp, span.high + tall))
				{
					continue;
				}

				if (placeHeight.size() >= limitPlaces)
				{
					return false;
				}

				placeX.push_back((uint16_t)ix);
				placeZ.push_back((uint16_t)row);
				placeHeight.push_back(span.high);
				placeFlat.push_back((span.flags & kNearlyFlat) ? 1 : 0);
			}
		}
	}

	columnFirstPlace[columnX.size()] = (int)placeHeight.size();

	return true;
}

// Is there a place in this column he could walk to from this height?
bool HudsonGrid::WalkableInto(int ix, int iz, int height) const
{
	int const column = ColumnAt(ix, iz);

	if (column < 0)
	{
		return false;
	}

	for (int p = columnFirstPlace[(size_t)column]; p < columnFirstPlace[(size_t)column + 1]; p++)
	{
		if (abs(placeHeight[(size_t)p] - height) <= kHudsonGridStepUp)
		{
			return true;
		}
	}

	return false;
}

bool HudsonGrid::MakeMoves()
{
	static int const kWays[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };

	size_t const count = placeHeight.size();

	firstMove.assign(count + 1, 0);

	for (size_t place = 0; place < count; place++)
	{
		firstMove[place] = (int)moves.size();

		int const ix = placeX[place], iz = placeZ[place], height = placeHeight[place];

		for (int way = 0; way < 8; way++)
		{
			int const dx = kWays[way][0], dz = kWays[way][1];
			int const column = ColumnAt(ix + dx, iz + dz);

			if (column < 0)
			{
				continue;
			}

			// A corner can't be cut: his square would catch it
			if (way >= 4 && !(WalkableInto(ix + dx, iz, height) && WalkableInto(ix, iz + dz, height)))
			{
				continue;
			}

			for (int p = columnFirstPlace[(size_t)column]; p < columnFirstPlace[(size_t)column + 1]; p++)
			{
				int const rise = placeHeight[(size_t)p] - height;

				if (abs(rise) > kHudsonGridStepUp)
				{
					continue;
				}

				// Up onto a steep slope is the one thing he can't do by walking: see hudson_grid.h
				if (rise > kClimbingIsMoreThan && !placeFlat[(size_t)p])
				{
					continue;
				}

				if (moves.size() >= limitMoves)
				{
					return false;
				}

				moves.push_back((uint32_t)p * 2);
			}
		}

		// Drops, straight out, off an edge he has no way to walk over
		for (int way = 0; way < 4; way++)
		{
			int const dx = kWays[way][0], dz = kWays[way][1];

			if (WalkableInto(ix + dx, iz + dz, height))
			{
				continue;
			}

			int const landX = ix + dx * kColumnsOutForADrop, landZ = iz + dz * kColumnsOutForADrop;
			int const column = ColumnAt(landX, landZ);

			if (column < 0)
			{
				continue;
			}

			// He lands on the highest floor under him, and only that one
			int landing = -1;

			for (int p = columnFirstPlace[(size_t)column]; p < columnFirstPlace[(size_t)column + 1]; p++)
			{
				if (placeHeight[(size_t)p] < height - kHudsonGridStepUp && (landing < 0 || placeHeight[(size_t)p] > placeHeight[(size_t)landing]))
				{
					landing = p;
				}
			}

			if (landing < 0 || height - placeHeight[(size_t)landing] >= kHudsonGridMostFall)
			{
				continue;
			}

			// Nothing in his way, all the way down. His square where he lands reaches back to the column next to him, and his square where he stands covers the rest, so that's
			// the walk out to the edge checked as well.
			if (!InTheWay(landX, landZ, placeHeight[(size_t)landing] + kHudsonGridStepUp, height + kHudsonGridMarineHeight))
			{
				if (moves.size() >= limitMoves)
				{
					return false;
				}

				moves.push_back((uint32_t)landing * 2 + 1);
				drops++;
			}
		}
	}

	firstMove[count] = (int)moves.size();

	return true;
}

// How far each place is from a wall, in moves. A place with fewer than eight ways to walk has something next to it, and everything else counts out from those.
void HudsonGrid::WorkOutHowFarFromWalls()
{
	size_t const count = placeHeight.size();

	fromWall.assign(count, 255);

	std::vector<int> wave, next;

	for (size_t place = 0; place < count; place++)
	{
		int walking = 0;

		for (int m = firstMove[place]; m < firstMove[place + 1]; m++)
		{
			if (!(moves[(size_t)m] & 1))
			{
				walking++;
			}
		}

		if (walking < 8)
		{
			fromWall[place] = 0;
			wave.push_back((int)place);
		}
	}

	// Past this reach it doesn't change what anything costs, so there's no need to go further
	for (int distance = 1; distance <= kMiddleReachesInColumns && !wave.empty(); distance++)
	{
		next.clear();

		for (size_t i = 0; i < wave.size(); i++)
		{
			int const place = wave[i];

			for (int m = firstMove[(size_t)place]; m < firstMove[(size_t)place + 1]; m++)
			{
				uint32_t const move = moves[(size_t)m];

				if (!(move & 1) && fromWall[move / 2] > distance)
				{
					fromWall[move / 2] = (uint8_t)distance;
					next.push_back((int)(move / 2));
				}
			}
		}

		wave.swap(next);
	}
}

// Joins up places he can walk between, and marks those on a patch too small to count. A drop doesn't join them, or the top of every lamp would be part of the floor under it.
void HudsonGrid::WorkOutWhichPatchesAreSmall()
{
	size_t const count = placeHeight.size();
	std::vector<int> parent(count), size(count, 1);

	for (size_t place = 0; place < count; place++)
	{
		parent[place] = (int)place;
	}

	for (size_t place = 0; place < count; place++)
	{
		for (int m = firstMove[place]; m < firstMove[place + 1]; m++)
		{
			if (moves[(size_t)m] & 1)
			{
				continue;
			}

			int a = Find(parent, (int)place), b = Find(parent, (int)(moves[(size_t)m] / 2));

			if (a != b)
			{
				if (size[(size_t)a] < size[(size_t)b])
				{
					std::swap(a, b);
				}

				parent[(size_t)b] = a;
				size[(size_t)a] += size[(size_t)b];
			}
		}
	}

	smallPatch.resize(count);

	for (size_t place = 0; place < count; place++)
	{
		smallPatch[place] = size[(size_t)Find(parent, (int)place)] < kSmallPatch ? 1 : 0;
	}
}

const char *HudsonGrid::Build(const std::vector<HudsonGridPolygon> &polygons)
{
	Clear();

	if (polygons.size() > limitPolygons)
	{
		return "the level has more polygons than the grid takes";
	}

	try
	{
		// Which polygons count, and how big the grid has to be for them
		std::vector<const HudsonGridPolygon *> used;

		used.reserve(polygons.size());

		int64_t lowX = 0, highX = 0, lowZ = 0, highZ = 0;

		for (size_t i = 0; i < polygons.size(); i++)
		{
			HudsonGridPolygon const &polygon = polygons[i];
			bool good = polygon.corners >= 3 && polygon.corners <= kHudsonGridMostCorners;
			int64_t lx = 0, hx = 0, lz = 0, hz = 0;

			for (int c = 0; good && c < polygon.corners; c++)
			{
				good = abs(polygon.x[c]) <= kHudsonGridMostCoordinate && abs(polygon.y[c]) <= kHudsonGridMostCoordinate && abs(polygon.z[c]) <= kHudsonGridMostCoordinate;

				lx = c ? (std::min)(lx, (int64_t)polygon.x[c]) : polygon.x[c];
				hx = c ? (std::max)(hx, (int64_t)polygon.x[c]) : polygon.x[c];
				lz = c ? (std::min)(lz, (int64_t)polygon.z[c]) : polygon.z[c];
				hz = c ? (std::max)(hz, (int64_t)polygon.z[c]) : polygon.z[c];
			}

			if (!good || (hx - lx) * (hz - lz) > kHudsonGridMostPolygonArea)
			{
				polygonsLeftOut++;
				continue;
			}

			lowX = used.empty() ? lx : (std::min)(lowX, lx);
			highX = used.empty() ? hx : (std::max)(highX, hx);
			lowZ = used.empty() ? lz : (std::min)(lowZ, lz);
			highZ = used.empty() ? hz : (std::max)(highZ, hz);
			used.push_back(&polygon);
		}

		polygonsUsed = used.size();

		if (used.empty())
		{
			return "the level has no polygons the grid can use";
		}

		// A spare column round the outside, so that nothing has to ask about a column that isn't there
		minX = (int)lowX - 2 * kHudsonGridCell;
		minZ = (int)lowZ - 2 * kHudsonGridCell;

		int64_t const wide = (highX - minX) / kHudsonGridCell + 3, deep = (highZ - minZ) / kHudsonGridCell + 3;

		if (wide > kHudsonGridMostSide || deep > kHudsonGridMostSide)
		{
			Clear();
			return "the level is wider than the grid takes";
		}

		width = (int)wide;
		depth = (int)deep;

		std::vector<Bit> bits;

		for (size_t i = 0; i < used.size(); i++)
		{
			if (!MarkOnePolygon(*used[i], &bits))
			{
				Clear();
				return "the level's polygons cross more columns than the grid takes";
			}
		}

		bitsBeforeJoining = bits.size();

		JoinBitsIntoSpans(&bits);
		std::vector<Bit>().swap(bits);

		if (!MakePlaces())
		{
			Clear();
			return "the level has more places to stand than the grid takes";
		}

		if (!MakeMoves())
		{
			Clear();
			return "the level has more moves than the grid takes";
		}

		WorkOutHowFarFromWalls();
		WorkOutWhichPatchesAreSmall();

		// The spans are only for building. Nothing below asks about them, and they're a lot of memory.
		std::vector<Span>().swap(spans);
		std::vector<int>().swap(columnFirstSpan);

		seen.assign(placeHeight.size(), 0);
		costSoFar.assign(placeHeight.size(), 0.0f);
		cameFrom.assign(placeHeight.size(), -1);
		goalMark.assign(placeHeight.size(), 0);
	}
	catch (const std::bad_alloc &)
	{
		Clear();
		return "there isn't the memory for the grid";
	}

	ready = true;
	state = kHudsonGridNoWay;

	return 0;
}

// The places a spot could mean. A spot can be in the air, so the floor under it comes first, then whatever's nearest. floorY is the one that scored best.
void HudsonGrid::PlacesNearSpot(int x, int y, int z, std::vector<int> *found, int *floorY) const
{
	found->clear();

	int const reach = kHudsonGridSpotReach / kHudsonGridCell + 1;
	int const middleX = (int)floor((double)(x - minX) / kHudsonGridCell), middleZ = (int)floor((double)(z - minZ) / kHudsonGridCell);
	int const height = -y;

	std::vector<std::pair<int, int> > scored;

	for (int dz = -reach; dz <= reach; dz++)
	{
		for (int dx = -reach; dx <= reach; dx++)
		{
			int const column = ColumnAt(middleX + dx, middleZ + dz);

			if (column < 0)
			{
				continue;
			}

			double const acrossX = WorldX(middleX + dx) - (double)x, acrossZ = WorldZ(middleZ + dz) - (double)z;
			int const across = (int)sqrt(acrossX * acrossX + acrossZ * acrossZ);

			if (across > kHudsonGridSpotReach)
			{
				continue;
			}

			for (int p = columnFirstPlace[(size_t)column]; p < columnFirstPlace[(size_t)column + 1]; p++)
			{
				if (smallPatch[(size_t)p])
				{
					continue;
				}

				int const above = height - placeHeight[(size_t)p];

				if (above < -kHudsonGridSpotBelow || above > kHudsonGridSpotAbove)
				{
					continue;
				}

				scored.push_back(std::make_pair(across + (above >= 0 ? above / 4 : -above * 2), p));
			}
		}
	}

	if (scored.empty())
	{
		return;
	}

	std::sort(scored.begin(), scored.end());

	if (floorY)
	{
		*floorY = -placeHeight[(size_t)scored[0].second];
	}

	for (size_t i = 0; i < scored.size() && i < (size_t)kMostPlacesKeptForASpot && scored[i].first <= scored[0].first + kSpotScoreRoom; i++)
	{
		found->push_back(scored[i].second);
	}
}

float HudsonGrid::HowFarToGoal(int place) const
{
	float const dx = (float)(WorldX(placeX[(size_t)place]) - goalX), dz = (float)(WorldZ(placeZ[(size_t)place]) - goalZ);

	return sqrtf(dx * dx + dz * dz);
}

void HudsonGrid::Push(float cost, int place)
{
	heapCost.push_back(cost);
	heapPlace.push_back(place);

	size_t i = heapCost.size() - 1;

	while (i > 0)
	{
		size_t const up = (i - 1) / 2;

		if (heapCost[up] <= cost)
		{
			break;
		}

		heapCost[i] = heapCost[up];
		heapPlace[i] = heapPlace[up];
		i = up;
	}

	heapCost[i] = cost;
	heapPlace[i] = place;
}

int HudsonGrid::Pop()
{
	int const top = heapPlace[0];
	float const lastCost = heapCost.back();
	int const lastPlace = heapPlace.back();

	heapCost.pop_back();
	heapPlace.pop_back();

	size_t const size = heapCost.size();

	if (size)
	{
		size_t i = 0;

		for (;;)
		{
			size_t child = 2 * i + 1;

			if (child >= size)
			{
				break;
			}

			if (child + 1 < size && heapCost[child + 1] < heapCost[child])
			{
				child++;
			}

			if (heapCost[child] >= lastCost)
			{
				break;
			}

			heapCost[i] = heapCost[child];
			heapPlace[i] = heapPlace[child];
			i = child;
		}

		heapCost[i] = lastCost;
		heapPlace[i] = lastPlace;
	}

	return top;
}

void HudsonGrid::StartSearch(int fromX, int fromY, int fromZ, int toX, int toY, int toZ)
{
	way.clear();
	looks = 0;
	heapCost.clear();
	heapPlace.clear();
	goals.clear();
	nearestPlace = -1;
	nearestDistance = 0.0f;
	goalWasFound = startWasFound = false;
	goalFloorY = toY;
	goalAskedX = toX;
	goalAskedZ = toZ;
	goalX = toX;
	goalZ = toZ;
	state = kHudsonGridNoWay;

	if (!ready)
	{
		return;
	}

	searchNumber++;

	std::vector<int> starts;

	PlacesNearSpot(fromX, fromY, fromZ, &starts, 0);
	PlacesNearSpot(toX, toY, toZ, &goals, &goalFloorY);

	startWasFound = !starts.empty();
	goalWasFound = !goals.empty();

	if (starts.empty() || goals.empty())
	{
		return;
	}

	for (size_t i = 0; i < goals.size(); i++)
	{
		goalMark[(size_t)goals[i]] = searchNumber;
	}

	for (size_t i = 0; i < starts.size(); i++)
	{
		int const place = starts[i];

		seen[(size_t)place] = searchNumber;
		costSoFar[(size_t)place] = 0.0f;
		cameFrom[(size_t)place] = -1;
		Push(HowFarToGoal(place) * 0.9f, place);
	}

	state = kHudsonGridSearching;
}

HudsonGridSearch HudsonGrid::Work(int placesAllowed)
{
	if (state != kHudsonGridSearching)
	{
		return state;
	}

	for (int done = 0; done < placesAllowed; done++)
	{
		if (heapCost.empty())
		{
			MakeTheWay(nearestPlace, false);
			state = kHudsonGridNoWay;
			return state;
		}

		int const place = Pop();

		looks++;

		float const distance = HowFarToGoal(place);

		if (nearestPlace < 0 || distance < nearestDistance)
		{
			nearestPlace = place;
			nearestDistance = distance;
		}

		if (goalMark[(size_t)place] == searchNumber)
		{
			MakeTheWay(place, true);
			state = kHudsonGridFound;
			return state;
		}

		int const height = placeHeight[(size_t)place];
		int const ix = placeX[(size_t)place], iz = placeZ[(size_t)place];
		float const soFar = costSoFar[(size_t)place];

		for (int m = firstMove[(size_t)place]; m < firstMove[(size_t)place + 1]; m++)
		{
			uint32_t const move = moves[(size_t)m];
			int const to = (int)(move / 2);
			int const acrossX = (int)placeX[(size_t)to] - ix, acrossZ = (int)placeZ[(size_t)to] - iz;
			int const rise = placeHeight[(size_t)to] - height;
			float step = sqrtf((float)(acrossX * acrossX + acrossZ * acrossZ)) * kHudsonGridCell;

			if (move & 1)
			{
				step += (float)abs(rise) * kDropCosts;
			}
			else
			{
				step += (float)abs(rise) * kRiseCosts;

				// Stay in the middle of the room: closer to a wall costs more, up to double right against it
				int const clear = fromWall[(size_t)to];

				if (clear < kMiddleReachesInColumns)
				{
					step *= 1.0f + (float)(kMiddleReachesInColumns - clear) / (float)kMiddleReachesInColumns;
				}
			}

			float const cost = soFar + step;

			if (seen[(size_t)to] != searchNumber || cost < costSoFar[(size_t)to])
			{
				seen[(size_t)to] = searchNumber;
				costSoFar[(size_t)to] = cost;
				cameFrom[(size_t)to] = place;
				Push(cost + HowFarToGoal(to) * 0.9f, to);
			}
		}
	}

	return state;
}

bool HudsonGrid::MarkWhatCanBeReachedFrom(int x, int y, int z, std::vector<uint8_t> *reached) const
{
	reached->assign(placeHeight.size(), 0);

	if (!ready)
	{
		return false;
	}

	std::vector<int> toLookAt;

	PlacesNearSpot(x, y, z, &toLookAt, 0);

	if (toLookAt.empty())
	{
		return false;
	}

	for (size_t i = 0; i < toLookAt.size(); i++)
	{
		(*reached)[(size_t)toLookAt[i]] = 1;
	}

	// Each place reached goes on the list once, and each of its moves is looked at once, so this ends
	while (!toLookAt.empty())
	{
		int const place = toLookAt.back();

		toLookAt.pop_back();

		for (int m = firstMove[(size_t)place]; m < firstMove[(size_t)place + 1]; m++)
		{
			int const to = (int)(moves[(size_t)m] / 2);

			if (!(*reached)[(size_t)to])
			{
				(*reached)[(size_t)to] = 1;
				toLookAt.push_back(to);
			}
		}
	}

	return true;
}

bool HudsonGrid::IsMarked(const std::vector<uint8_t> &marks, int x, int y, int z) const
{
	if (!ready || marks.size() != placeHeight.size())
	{
		return false;
	}

	std::vector<int> places;

	PlacesNearSpot(x, y, z, &places, 0);

	for (size_t i = 0; i < places.size(); i++)
	{
		if (marks[(size_t)places[i]])
		{
			return true;
		}
	}

	return false;
}

// Walks back along how each place was reached, then keeps about a point a metre, as well as both ends of every drop, because a drop can't be cut across.
void HudsonGrid::MakeTheWay(int lastPlace, bool arrived)
{
	way.clear();

	if (lastPlace < 0)
	{
		return;
	}

	std::vector<int> places;

	for (int place = lastPlace; place >= 0; place = cameFrom[(size_t)place])
	{
		places.push_back(place);
	}

	std::reverse(places.begin(), places.end());

	int acrossSinceKept = 0;

	for (size_t i = 0; i < places.size(); i++)
	{
		int const place = places[i];
		bool keep = i == 0 || i + 1 == places.size();

		if (i > 0)
		{
			int const before = places[i - 1];
			int const dx = abs((int)placeX[(size_t)place] - (int)placeX[(size_t)before]), dz = abs((int)placeZ[(size_t)place] - (int)placeZ[(size_t)before]);

			acrossSinceKept += (dx + dz > 1) ? (kHudsonGridCell * 14) / 10 : kHudsonGridCell;

			// A move longer than one column is a drop, and so is what's just before one
			if (dx > 1 || dz > 1)
			{
				keep = true;
			}
		}

		if (i + 1 < places.size())
		{
			int const after = places[i + 1];

			if (abs((int)placeX[(size_t)after] - (int)placeX[(size_t)place]) > 1 || abs((int)placeZ[(size_t)after] - (int)placeZ[(size_t)place]) > 1)
			{
				keep = true;
			}
		}

		if (acrossSinceKept >= kWayPointEvery)
		{
			keep = true;
		}

		if (keep)
		{
			HudsonGridPoint const point = { WorldX(placeX[(size_t)place]), -placeHeight[(size_t)place], WorldZ(placeZ[(size_t)place]) };

			way.push_back(point);
			acrossSinceKept = 0;
		}
	}

	// The last point is the goal itself, if it's near enough to where the floor is, so that arriving at the end of the way is arriving
	if (arrived && !way.empty())
	{
		HudsonGridPoint &last = way.back();
		double const dx = (double)last.x - goalAskedX, dz = (double)last.z - goalAskedZ;

		if (sqrt(dx * dx + dz * dz) <= kHudsonGridCell * 3)
		{
			last.x = goalAskedX;
			last.z = goalAskedZ;
		}
	}
}

void HudsonGrid::FillCell(int place, HudsonGridCell *cell) const
{
	cell->x = WorldX(placeX[(size_t)place]);
	cell->y = -placeHeight[(size_t)place];
	cell->z = WorldZ(placeZ[(size_t)place]);
	cell->kind = placeFlat[(size_t)place] ? kHudsonGridFlat : kHudsonGridSlope;
	cell->link = -1;
	cell->number = place;

	for (int m = firstMove[(size_t)place]; m < firstMove[(size_t)place + 1]; m++)
	{
		uint32_t const move = moves[(size_t)m];

		if (move & 1)
		{
			cell->kind = kHudsonGridDrop;
			cell->link = (int)(move / 2);
			break;
		}

		// a rise too big to be the same flat floor, and small enough to be a stair
		if (cell->kind == kHudsonGridFlat && abs(placeHeight[move / 2] - placeHeight[(size_t)place]) >= 100)
		{
			cell->kind = kHudsonGridStair;
		}
	}
}

bool HudsonGrid::CellAt(int number, HudsonGridCell *cell) const
{
	if (!ready || number < 0 || (size_t)number >= placeHeight.size())
	{
		return false;
	}

	FillCell(number, cell);
	return true;
}

int HudsonGrid::CellsNear(int x, int y, int z, int radius, HudsonGridCell *cells, int most) const
{
	if (!ready || most <= 0 || radius <= 0)
	{
		return 0;
	}

	int const middleX = (int)floor((double)(x - minX) / kHudsonGridCell), middleZ = (int)floor((double)(z - minZ) / kHudsonGridCell);
	int const reach = radius / kHudsonGridCell + 1;
	int const height = -y;
	int written = 0;

	for (int iz = (std::max)(0, middleZ - reach); iz <= middleZ + reach && iz < depth; iz++)
	{
		for (int ix = (std::max)(0, middleX - reach); ix <= middleX + reach && ix < width; ix++)
		{
			int const column = ColumnAt(ix, iz);

			if (column < 0)
			{
				continue;
			}

			for (int p = columnFirstPlace[(size_t)column]; p < columnFirstPlace[(size_t)column + 1]; p++)
			{
				if (abs(placeHeight[(size_t)p] - height) > radius)
				{
					continue;
				}

				if (written >= most)
				{
					return written;
				}

				FillCell(p, &cells[written++]);
			}
		}
	}

	return written;
}
