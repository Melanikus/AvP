// See hudson_grid_level.h. Written 2026-11-05 by Melanikus for the AvP port.
//
// The grid itself knows nothing about the game. This is the one place that does. It goes through every module of the level, and every object that stands still, and makes
// a plain list of their polygons in the world's numbers.
//
// WHAT COUNTS AS WHAT:
// - A room's polygons block him, and can be floors.
// - A door counts the same. A door's shape in the level is its open shape, and what's round the opening is a real wall. So a door doesn't close a way on this map: he waits at it when he gets there.
// - A lift's car or anything on a track won't be where the file has it, so it isn't counted at all.
// - An object placed in the level, like a crate or a lamp, counts where it stands, turned the way it's turned, unless the game says nothing collides with it. Things to pick up don't count.
//
// WHICH WAY A POLYGON FACES comes from the normal stored with its shape, which is what the game's own collisions test (FLOOR_THRESHOLD, in dynamics.cpp). Only a polygon with
// none stored has it worked out from its corners. A polygon that shows both sides counts as facing up whichever way it's stored, because he can stand on either.
#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "logString.h"
#include "bh_types.h"
#include "dynblock.h"
#include "pvisible.h"
#include "hudson_grid.h"
#include "hudson_grid_level.h"

#include <new>
#include <stdio.h>
#include <string>
#include <vector>

extern SCENE Global_Scene;

namespace
{
	struct HowItCounts
	{
		bool blocks;
		bool canStandOn;
		bool crawl;
	};

	struct Counts
	{
		size_t modules, objects, polygons, badPolygons, noNormal;
	};
}

// Adds one shape's polygons. turn is how it's turned, or nothing if it isn't, and the sums are the game's own (_RotateVector, in maths.cpp).
static void AddShape(const SHAPEHEADER *shape, const VECTORCH &place, const MATRIXCH *turn, HowItCounts const &how, std::vector<HudsonGridPolygon> *polygons, Counts *counts)
{
	if (!shape || !shape->items || !shape->points || !*shape->points || (shape->shapeflags & (ShapeFlag_Sprite | ShapeFlag_SpriteR)))
	{
		return;
	}

	VECTORCH const *const points = (const VECTORCH *)*shape->points;

	for (int number = 0; number < shape->numitems; number++)
	{
		POLYHEADER const *const polygon = (const POLYHEADER *)shape->items[number];

		if (!polygon)
		{
			continue;
		}

		HudsonGridPolygon out;
		int const *const corners = &polygon->Poly1stPt;
		int count = 0;
		bool good = true;

		// A list of corners ends at a negative number. A corner that isn't one of the shape's points means the whole polygon can't be trusted.
		while (good && count <= kHudsonGridMostCorners && corners[count] >= 0)
		{
			if (count == kHudsonGridMostCorners || corners[count] >= shape->numpoints)
			{
				good = false;
				break;
			}

			VECTORCH const &point = points[corners[count]];
			int x = point.vx, y = point.vy, z = point.vz;

			if (turn)
			{
				x = MUL_FIXED(turn->mat11, point.vx) + MUL_FIXED(turn->mat21, point.vy) + MUL_FIXED(turn->mat31, point.vz);
				y = MUL_FIXED(turn->mat12, point.vx) + MUL_FIXED(turn->mat22, point.vy) + MUL_FIXED(turn->mat32, point.vz);
				z = MUL_FIXED(turn->mat13, point.vx) + MUL_FIXED(turn->mat23, point.vy) + MUL_FIXED(turn->mat33, point.vz);
			}

			out.x[count] = x + place.vx;
			out.y[count] = y + place.vy;
			out.z[count] = z + place.vz;
			count++;
		}

		if (!good || count < 3)
		{
			counts->badPolygons++;
			continue;
		}

		out.corners = count;
		out.blocks = how.blocks;
		out.canStandOn = how.canStandOn;
		out.crawl = how.crawl;

		int const normalPlace = polygon->PolyNormalIndex;
		bool haveNormal = false;

		if (shape->sh_normals && *shape->sh_normals && normalPlace >= 0 && (int64_t)normalPlace + 3 <= (int64_t)shape->numitems * 3)
		{
			VECTORCH const &stored = *(const VECTORCH *)(*shape->sh_normals + normalPlace);

			if (stored.vx || stored.vy || stored.vz)
			{
				int const normalY = turn ? MUL_FIXED(turn->mat12, stored.vx) + MUL_FIXED(turn->mat22, stored.vy) + MUL_FIXED(turn->mat32, stored.vz) : stored.vy;

				// y goes down, so facing up is a negative y
				out.up = (float)-normalY / 65536.0f;

				if ((polygon->PolyFlags & iflag_no_bfc) && out.up < 0.0f)
				{
					out.up = -out.up;
				}

				haveNormal = true;
			}
		}

		if (!haveNormal)
		{
			counts->noNormal++;
			out.up = HudsonGrid_HowMuchItFacesUp(out);
		}

		polygons->push_back(out);
		counts->polygons++;
	}
}

static void AddModules(std::vector<HudsonGridPolygon> *polygons, Counts *counts)
{
	if (!Global_ModulePtr)
	{
		return;
	}

	SCENEMODULE const *const scene = Global_ModulePtr[Global_Scene];

	if (!scene || !scene->sm_marray)
	{
		return;
	}

	for (int i = 0; i < ModuleArraySize; i++)
	{
		MODULE const *const module = scene->sm_marray[i];

		if (!module || !module->m_mapptr)
		{
			continue;
		}

		HowItCounts how = { true, true, (module->m_flags & MODULEFLAG_AIRDUCT) != 0 };

		if (module->m_sbptr)
		{
			AVP_BEHAVIOUR_TYPE const type = module->m_sbptr->I_SBtype;

			if (type == I_BehaviourLift || type == I_BehaviourPlatform || type == I_BehaviourTrackObject)
			{
				continue;
			}
		}

		AddShape(GetShapeData(module->m_mapptr->MapShape), module->m_mapptr->MapWorld, 0, how, polygons, counts);
		counts->modules++;
	}
}

static void AddStillObjects(std::vector<HudsonGridPolygon> *polygons, Counts *counts)
{
	for (int i = 0; i < NumActiveStBlocks; i++)
	{
		STRATEGYBLOCK const *const object = ActiveStBlockList[i];

		if (!object || !object->DynPtr || object->containingModule == 0 || object->shapeIndex < 0 || object->DynPtr->DynamicsType == DYN_TYPE_NO_COLLISIONS)
		{
			continue;
		}

		bool stays = false;

		switch (object->I_SBtype)
		{
			case I_BehaviourInanimateObject:
			{
				INANIMATEOBJECT_STATUSBLOCK const *const status = (const INANIMATEOBJECT_STATUSBLOCK *)object->SBdataptr;

				stays = status && (status->typeId == IOT_Static || status->typeId == IOT_Furniture);
				break;
			}

			case I_BehaviourPlacedLight:
			case I_BehaviourBinarySwitch:
			case I_BehaviourLinkSwitch:
			case I_BehaviourVideoScreen:
				stays = true;
				break;

			default:
				break;
		}

		if (!stays)
		{
			continue;
		}

		HowItCounts const how = { true, true, false };

		AddShape(GetShapeData(object->shapeIndex), object->DynPtr->Position, &object->DynPtr->OrientMat, how, polygons, counts);
		counts->objects++;
	}
}

const char *HudsonGridLevel_ListPolygons(std::vector<HudsonGridPolygon> *polygons)
{
	Counts counts = { 0, 0, 0, 0, 0 };

	try
	{
		AddModules(polygons, &counts);
		AddStillObjects(polygons, &counts);
	}
	catch (const std::bad_alloc &)
	{
		polygons->clear();
		return "there isn't the memory to list the level's polygons";
	}

	char line[200] = { 0 };

	_snprintf(line, sizeof(line) - 1, "grid: listed %d modules and %d objects: %d polygons, %d with bad corners, %d with no normal", (int)counts.modules, (int)counts.objects, (int)counts.polygons, (int)counts.badPolygons, (int)counts.noNormal);
	LogString(line);

	return 0;
}

const char *HudsonGridLevel_Build(HudsonGrid *grid, const std::vector<HudsonGridPolygon> &polygons)
{
	DWORD const started = GetTickCount();
	const char *const why = grid->Build(polygons);
	DWORD const took = GetTickCount() - started;
	char line[300] = { 0 };

	_snprintf(line, sizeof(line) - 1, "grid: built in %lu ms, on its own thread: %d polygons used, %d left out as scenery or nonsense. %d places, %d moves, %d drops, %d crossings before joining, %d KB kept",
		(unsigned long)took, (int)grid->PolygonsUsed(), (int)grid->PolygonsLeftOut(), (int)grid->Places(), (int)grid->Moves(), (int)grid->Drops(), (int)grid->BitsBeforeJoining(), (int)(grid->BytesKept() / 1024));
	LogString(line);

	if (why)
	{
		LogErrorString(std::string("grid: ") + why);
	}

	return why;
}
