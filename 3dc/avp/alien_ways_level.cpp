// See alien_ways_level.h. Written 2026-11-08 by Melanikus for the AvP port.
#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "bh_types.h"
#include "dynblock.h"
#include "pvisible.h"

#include "alien_ways_level.h"

#include <new>

// Which scene of the level is loaded. No header says so: the game's own files say it where they use it, as pfarlocs.cpp does.
extern SCENE Global_Scene;

// The list being made, each module's part of it, the counts, and whether it's grown past kAlienWaysMostListed
namespace
{
	struct Listing
	{
		std::vector<AlienWaysPolygon> *polygons;
		std::vector<AlienWaysRange> *modules;
		AlienWaysLevelCounts *counts;
		bool tooMany;
	};

	const char *const kWhyTooMany = "the level has more polygons than the map takes";
	const char *const kWhyNoMemory = "there isn't the memory to list the level's polygons";
}

// Every polygon of one shape, placed at place, and turned by turn if it has one
static void AddShape(const SHAPEHEADER *shape, const VECTORCH &place, const MATRIXCH *turn, Listing *listing)
{
	if (!shape || !shape->items || !shape->points || !*shape->points || (shape->shapeflags & (ShapeFlag_Sprite | ShapeFlag_SpriteR)))
	{
		return;
	}

	VECTORCH const *const points = (const VECTORCH *)*shape->points;

	for (int number = 0; number < shape->numitems && !listing->tooMany; number++)
	{
		POLYHEADER const *const polygon = (const POLYHEADER *)shape->items[number];

		if (!polygon)
		{
			continue;
		}

		// Read the corners, up to the negative number that ends them. One more than the map takes is enough to know it's too many, so no more
		// than that is ever read: a level file's polygons have at most 4.
		int x[kAlienWaysMostCorners + 1], y[kAlienWaysMostCorners + 1], z[kAlienWaysMostCorners + 1];
		int const *const corners = &polygon->Poly1stPt;
		int count = 0;
		bool trusted = true;

		while (count <= kAlienWaysMostCorners && corners[count] >= 0)
		{
			if (corners[count] >= shape->numpoints)
			{
				trusted = false;
				break;
			}

			VECTORCH const &point = points[corners[count]];
			int px = point.vx, py = point.vy, pz = point.vz;

			if (turn)
			{
				px = MUL_FIXED(turn->mat11, point.vx) + MUL_FIXED(turn->mat21, point.vy) + MUL_FIXED(turn->mat31, point.vz);
				py = MUL_FIXED(turn->mat12, point.vx) + MUL_FIXED(turn->mat22, point.vy) + MUL_FIXED(turn->mat32, point.vz);
				pz = MUL_FIXED(turn->mat13, point.vx) + MUL_FIXED(turn->mat23, point.vy) + MUL_FIXED(turn->mat33, point.vz);
			}

			x[count] = px + place.vx;
			y[count] = py + place.vy;
			z[count] = pz + place.vz;
			count++;
		}

		if (!trusted)
		{
			listing->counts->badCorner++;
			continue;
		}

		// Which way it faces: the normal stored with the shape, turned with the object, where there's one
		float up = 0.0f;
		bool haveFacing = false;
		int const normalPlace = polygon->PolyNormalIndex;

		if (shape->sh_normals && *shape->sh_normals && normalPlace >= 0 && static_cast<long long>(normalPlace) + 3 <= static_cast<long long>(shape->numitems) * 3)
		{
			VECTORCH const &stored = *(const VECTORCH *)(*shape->sh_normals + normalPlace);

			if (stored.vx || stored.vy || stored.vz)
			{
				int const normalY = turn ? MUL_FIXED(turn->mat12, stored.vx) + MUL_FIXED(turn->mat22, stored.vy) + MUL_FIXED(turn->mat32, stored.vz) : stored.vy;

				// y goes down, so facing up is a negative y
				up = static_cast<float>(-normalY) / 65536.0f;

				// A polygon drawn from both sides: an alien can stand on its top
				if ((polygon->PolyFlags & iflag_no_bfc) && up < 0.0f)
				{
					up = -up;
				}

				// A stored normal is a little over or under 1 long at times, so it's kept in the range the map takes
				up = up > 1.0f ? 1.0f : (up < -1.0f ? -1.0f : up);
				haveFacing = true;
			}
		}

		if (!haveFacing && count >= 3 && count <= kAlienWaysMostCorners)
		{
			AlienWaysPolygon worked;
			worked.corners = count;

			for (int c = 0; c < kAlienWaysMostCorners; c++)
			{
				worked.x[c] = c < count ? x[c] : 0;
				worked.y[c] = c < count ? y[c] : 0;
				worked.z[c] = c < count ? z[c] : 0;
			}

			up = AlienWays_HowMuchItFacesUp(worked);
		}

		if (!haveFacing)
		{
			listing->counts->noStoredFacing++;
		}

		size_t const before = listing->polygons->size();

		switch (AlienWays_AddPolygon(x, y, z, count, up, listing->polygons))
		{
			case kAlienWaysAddedAsItIs:
				break;
			case kAlienWaysLeftOutTooFewCorners:
				listing->counts->tooFewCorners++;
				break;
			case kAlienWaysLeftOutTooManyCorners:
				listing->counts->tooManyCorners++;
				break;
		}

		listing->counts->polygons += static_cast<int>(listing->polygons->size() - before);

		if (listing->polygons->size() > kAlienWaysMostListed)
		{
			listing->tooMany = true;
		}
	}
}

static void AddModules(Listing *listing)
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

	// One range for each module, by its own number, all empty to start with
	listing->modules->assign(ModuleArraySize > 0 ? static_cast<size_t>(ModuleArraySize) : 0, AlienWaysRange{ 0, 0 });

	for (int i = 0; i < ModuleArraySize && !listing->tooMany; i++)
	{
		MODULE const *const module = scene->sm_marray[i];

		if (!module || !module->m_mapptr)
		{
			continue;
		}

		// A lift's car, a platform and anything on a track won't be where the level has them
		if (module->m_sbptr)
		{
			AVP_BEHAVIOUR_TYPE const type = module->m_sbptr->I_SBtype;

			if (type == I_BehaviourLift || type == I_BehaviourPlatform || type == I_BehaviourTrackObject)
			{
				continue;
			}
		}

		size_t const first = listing->polygons->size();
		AddShape(GetShapeData(module->m_mapptr->MapShape), module->m_mapptr->MapWorld, 0, listing);
		listing->counts->modules++;

		// A module whose number is outside the game's own list of modules gets no range, so no alien in it is guided
		if (module->m_index >= 0 && static_cast<size_t>(module->m_index) < listing->modules->size())
		{
			(*listing->modules)[module->m_index] = AlienWaysRange{ first, listing->polygons->size() };
		}
	}
}

static void AddStillObjects(Listing *listing)
{
	for (int i = 0; i < NumActiveStBlocks && !listing->tooMany; i++)
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

		AddShape(GetShapeData(object->shapeIndex), object->DynPtr->Position, &object->DynPtr->OrientMat, listing);
		listing->counts->objects++;
	}
}

const char *AlienWaysLevel_List(std::vector<AlienWaysPolygon> *polygons, std::vector<AlienWaysRange> *modules, AlienWaysLevelCounts *counts)
{
	AlienWaysLevelCounts const none = { 0, 0, 0, 0, 0, 0, 0 };
	*counts = none;
	polygons->clear();
	modules->clear();
	Listing listing = { polygons, modules, counts, false };

	try
	{
		AddModules(&listing);
		AddStillObjects(&listing);
	}
	catch (const std::bad_alloc &)
	{
		std::vector<AlienWaysPolygon>().swap(*polygons);
		std::vector<AlienWaysRange>().swap(*modules);
		return kWhyNoMemory;
	}

	if (listing.tooMany)
	{
		std::vector<AlienWaysPolygon>().swap(*polygons);
		std::vector<AlienWaysRange>().swap(*modules);
		return kWhyTooMany;
	}

	return 0;
}
