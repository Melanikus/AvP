// See alien_ways_level.h. Written 2026-11-08 by Melanikus for the AvP port.
// Modified 2026-11-08 by Melanikus for the AvP port:
//   lists the level's doors, marks their polygons with the door's number, and reads each door's state for the map (#118)
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
		std::vector<AlienWaysLevelDoor> *doors;
		AlienWaysLevelCounts *counts;
		bool tooMany;
	};

	const char *const kWhyTooMany = "the level has more polygons than the map takes";
	const char *const kWhyNoMemory = "there isn't the memory to list the level's polygons";

	// A loaded polygon's record has room for 5 corners: the loader writes its corners and fills the rest of the 5 with -1
	// (3dc/win95/chunk_load.cpp, where item_list is filled; POLYHEADER's Poly1stPt is the first, in 3dc/include/shape.h). The chunk reader
	// takes at most 4, so a real polygon always has its -1 within the 5. One without is broken, and nothing past its 5 is ever read.
	int const kAlienWaysRecordCorners = 5;
}

// Every polygon of one shape, placed at place, and turned by turn if it has one
static void AddShape(const SHAPEHEADER *shape, const VECTORCH &place, const MATRIXCH *turn, Listing *listing)
{
	if (!shape || !shape->items || !shape->points || !*shape->points || (shape->shapeflags & (ShapeFlag_Sprite | ShapeFlag_SpriteR)))
	{
		return;
	}

	VECTORCH const *const points = (const VECTORCH *)*shape->points;
	AlienWaysXYZ const where = { place.vx, place.vy, place.vz };
	AlienWaysTurn turned = {};

	if (turn)
	{
		turned = AlienWaysTurn{ turn->mat11, turn->mat12, turn->mat13, turn->mat21, turn->mat22, turn->mat23, turn->mat31, turn->mat32, turn->mat33 };
	}

	AlienWaysTurn const *const turning = turn ? &turned : 0;

	for (int number = 0; number < shape->numitems && !listing->tooMany; number++)
	{
		POLYHEADER const *const polygon = (const POLYHEADER *)shape->items[number];

		if (!polygon)
		{
			continue;
		}

		// Read the corners, up to the negative number that ends them, and never past the record's own 5 places
		int x[kAlienWaysRecordCorners], y[kAlienWaysRecordCorners], z[kAlienWaysRecordCorners];
		int const *const corners = &polygon->Poly1stPt;
		int count = 0;
		bool trusted = true, nearEnough = true;

		while (count < kAlienWaysRecordCorners && corners[count] >= 0)
		{
			if (corners[count] >= shape->numpoints)
			{
				trusted = false;
				break;
			}

			// Turned and placed in 64 bits, so no level's numbers can overflow it
			VECTORCH const &point = points[corners[count]];
			AlienWaysXYZ const corner = { point.vx, point.vy, point.vz };
			AlienWaysXYZ placed;

			if (!AlienWays_PlaceCorner(corner, turning, where, &placed))
			{
				nearEnough = false;
				break;
			}

			x[count] = placed.x;
			y[count] = placed.y;
			z[count] = placed.z;
			count++;
		}

		if (!trusted)
		{
			listing->counts->badCorner++;
			continue;
		}

		if (!nearEnough)
		{
			listing->counts->farCorner++;
			continue;
		}

		if (count == kAlienWaysRecordCorners)
		{
			listing->counts->noEnd++;
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
				// Turned with the object, in 64 bits, and kept from -1 to 1
				up = AlienWays_TurnedUp(AlienWaysXYZ{ stored.vx, stored.vy, stored.vz }, turning);

				// A polygon drawn from both sides: an alien can stand on its top
				if ((polygon->PolyFlags & iflag_no_bfc) && up < 0.0f)
				{
					up = -up;
				}

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

		// A door: the same three kinds the game's own ModuleIsADoor knows (pfarlocs.cpp). Its polygons carry its number, its place in the list of
		// doors plus 1, so a search can keep off it while it won't let an alien through (AlienWaysLevel_ReadDoors). A level with more doors than
		// the map takes is refused by the map, which checks every number it's given.
		bool isADoor = false;
		AlienWaysDoorKind kind = kAlienWaysProximityDoor;

		if (module->m_sbptr)
		{
			switch (module->m_sbptr->I_SBtype)
			{
				case I_BehaviourProximityDoor: isADoor = true; kind = kAlienWaysProximityDoor; break;
				case I_BehaviourLiftDoor:      isADoor = true; kind = kAlienWaysLiftDoor;      break;
				case I_BehaviourSwitchDoor:    isADoor = true; kind = kAlienWaysSwitchDoor;    break;
				default: break;
			}
		}

		if (isADoor)
		{
			listing->doors->push_back(AlienWaysLevelDoor{ i, kind });
			listing->counts->doors++;
			int const door = static_cast<int>(listing->doors->size());

			for (size_t p = first; p < listing->polygons->size(); p++)
			{
				(*listing->polygons)[p].door = door;
			}
		}

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

const char *AlienWaysLevel_List(std::vector<AlienWaysPolygon> *polygons, std::vector<AlienWaysRange> *modules, std::vector<AlienWaysLevelDoor> *doors,
	AlienWaysLevelCounts *counts)
{
	AlienWaysLevelCounts const none = {};
	*counts = none;
	polygons->clear();
	modules->clear();
	doors->clear();
	Listing listing = { polygons, modules, doors, counts, false };

	try
	{
		AddModules(&listing);
		AddStillObjects(&listing);
	}
	catch (const std::bad_alloc &)
	{
		std::vector<AlienWaysPolygon>().swap(*polygons);
		std::vector<AlienWaysRange>().swap(*modules);
		std::vector<AlienWaysLevelDoor>().swap(*doors);
		return kWhyNoMemory;
	}

	if (listing.tooMany)
	{
		std::vector<AlienWaysPolygon>().swap(*polygons);
		std::vector<AlienWaysRange>().swap(*modules);
		std::vector<AlienWaysLevelDoor>().swap(*doors);
		return kWhyTooMany;
	}

	return 0;
}

void AlienWaysLevel_ReadDoors(const std::vector<AlienWaysLevelDoor> &doors, AlienWaysMap *map)
{
	if (!map)
	{
		return;
	}

	SCENEMODULE const *const scene = Global_ModulePtr ? Global_ModulePtr[Global_Scene] : 0;

	for (size_t d = 0; d < doors.size(); d++)
	{
		int const place = doors[d].module;
		MODULE const *const module = (scene && scene->sm_marray && place >= 0 && place < ModuleArraySize) ? scene->sm_marray[place] : 0;
		STRATEGYBLOCK *const door = module ? module->m_sbptr : 0;
		bool through = false;

		// Each kind's block is read only once it's checked to be that kind, the way GetState reads them (bh_types.cpp), and only if it's this
		// module's own: every door's setup points its block and its module at each other (bh_types.cpp, bh_lift_door.cpp, bh_switch_door.cpp),
		// so a block that points at another module is a stale one, now another door's, and counts as shut rather than as the other door
		// (Secondary's read of b786f33b)
		if (door && door->SBdataptr && door->SBmoptr == module)
		{
			switch (doors[d].kind)
			{
				case kAlienWaysProximityDoor:
					if (door->I_SBtype == I_BehaviourProximityDoor)
					{
						PROXDOOR_BEHAV_BLOCK const *const block = (PROXDOOR_BEHAV_BLOCK const *)door->SBdataptr;
						through = AlienWays_DoorLetsAnAlienThrough(kAlienWaysProximityDoor, block->door_locked != 0, GetState(door) != 0);
					}
					break;

				case kAlienWaysLiftDoor:
					if (door->I_SBtype == I_BehaviourLiftDoor)
					{
						through = AlienWays_DoorLetsAnAlienThrough(kAlienWaysLiftDoor, false, GetState(door) != 0);
					}
					break;

				case kAlienWaysSwitchDoor:
					if (door->I_SBtype == I_BehaviourSwitchDoor)
					{
						through = AlienWays_DoorLetsAnAlienThrough(kAlienWaysSwitchDoor, false, GetState(door) != 0);
					}
					break;
			}
		}

		map->SetDoorShut(static_cast<int>(d) + 1, !through);
	}
}
