// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   text is copied using its exact measured length, so it always fits
//   a module's master and slave chunks are only made inside a module's data
//   counted lists are measured against their chunks before they're made
//   a waypoint's links have to lead to places in its own list
//   a name is only read as far as the end of its chunk
//   a chunk too small for what its kind always reads is kept as plain bytes, and the level is refused
//   what comes after a name or a list is only read while it's still inside the chunk
//   a waypoint chunk no longer reads two spare numbers after its list: the code that writes one never wrote them, so it was reading past its end, and nothing used them
//   a waypoint's box and centre have to be inside the level, as CheckPlace in chunk.hpp says (2026-11-14)
// Modified 2026-11-14 by Melanikus for the AvP port:
//   a slave module has to name a master module that isn't a slave itself, or the level is refused
//   a waypoint's box has to give room on every axis at the level's scale, or the level is refused (waypoint_room.h)

#include "chunk_waypoint.hpp"
#include "environment_chunk.hpp"
#include "waypoint_room.h"

//macro for helping to force inclusion of chunks when using libraries
FORCE_CHUNK_INCLUDE_IMPLEMENT(chunk_waypoint)

ChunkWaypoint::ChunkWaypoint()
{
	index=-1;
	NumWPLinks=0;
	WayLinks=0;
	NumModLinks=0;
	ModLinks=0;
	flags=0;
	spare2=0;
}
ChunkWaypoint::~ChunkWaypoint()
{
	delete[] WayLinks;
	delete[] ModLinks;
}
ModuleLink::~ModuleLink()
{
	delete[] module_name;
}

RIF_IMPLEMENT_DYNCREATE("WAYPOINT",Module_Waypoint_Chunk)

Module_Waypoint_Chunk::Module_Waypoint_Chunk(Chunk_With_Children* parent,const char* data,size_t size)
:Chunk(parent,"WAYPOINT")
{
	char const * const start = data;
	// three corners, two numbers, and two counts: the least a waypoint can take
	size_t const smallest_waypoint=3*sizeof(ChunkVectorInt)+4*sizeof(int);

	NumWaypoints=ReadListCount(start,data,size,smallest_waypoint);

	NumGroundWaypoints=0;
	NumAlienWaypoints=0;
	AlienWaypoints=0;
	GroundWaypoints=0;

	if(NumWaypoints)
	   	Waypoints=new ChunkWaypoint[NumWaypoints];
	else
		Waypoints=0;

	if(NumWaypoints)
	{
		int first_ground_waypoint=-1;
		for(int i=0;i<NumWaypoints;i++)
		{
			// a waypoint's own lists can use up the room that the count was measured against
			if((size_t)(data - start) + smallest_waypoint > size)
			{
				Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
				break;
			}

			ChunkWaypoint* cw=&Waypoints[i];
			cw->index=i;

			cw->min=*(ChunkVectorInt*)data;
			data+=sizeof(ChunkVectorInt);	
			cw->max=*(ChunkVectorInt*)data;
			data+=sizeof(ChunkVectorInt);	
			cw->centre=*(ChunkVectorInt*)data;
			CheckPlace(cw->min);
			CheckPlace(cw->max);
			CheckPlace(cw->centre);
			data+=sizeof(ChunkVectorInt);	
			
			cw->flags=*(int*)data;
			data+=4;	
			cw->spare2=*(int*)data;
			data+=4;	
			
			cw->NumWPLinks=ReadListCount(start,data,size,sizeof(WaypointLink));

			if(cw->NumWPLinks)
				cw->WayLinks=new WaypointLink[cw->NumWPLinks];
			else
				cw->WayLinks=0;
			
			for(int j=0;j<cw->NumWPLinks;j++)
			{
				cw->WayLinks[j]=*(WaypointLink*)data;
				data+=sizeof(WaypointLink);
			}
				
			// a module's name takes at least 4 bytes, and its flags 4
			size_t const smallest_link=8;

			cw->NumModLinks=ReadListCount(start,data,size,smallest_link);

			if(cw->NumModLinks)
				cw->ModLinks=new ModuleLink[cw->NumModLinks];
			else
				cw->ModLinks=0;

			// Long names can use up the room that the count was measured against. Each read here looks for itself, so a list that runs out is refused, and the count stops it going on for long.
			for(int j=0;j<cw->NumModLinks;j++)
			{
				ModuleLink* ml=&cw->ModLinks[j];
				size_t name_length=0;
				ml->module_name=CopyNameFromChunk(start,size,data,&name_length);
				data+=(name_length+4)&~3;
				ml->flags=ReadFromChunk<int>(start,size,data);
			}

			if(cw->flags & WaypointFlag_FirstGroundWaypoint)
			{
				first_ground_waypoint=i;
				cw->flags &=~WaypointFlag_FirstGroundWaypoint;
			}
		}

		if(first_ground_waypoint>=0)
		{
			GroundWaypoints=&Waypoints[first_ground_waypoint];
			NumGroundWaypoints=NumWaypoints-first_ground_waypoint;
		}
		NumAlienWaypoints=NumWaypoints-NumGroundWaypoints;
		if(NumAlienWaypoints)
		{
			AlienWaypoints=&Waypoints[0];
		}

		// A link says which waypoint it leads to, counted from the start of its own list, and an alien or a marine following it goes straight to that place
		// in the list. So it has to be a place in the list.
		for(int i=0;i<NumWaypoints;i++)
		{
			ChunkWaypoint const* cw=&Waypoints[i];
			int const list_size=(i<NumAlienWaypoints) ? NumAlienWaypoints : NumGroundWaypoints;

			for(int j=0;j<cw->NumWPLinks;j++)
			{
				if(cw->WayLinks[j].index<0 || cw->WayLinks[j].index>=list_size)
				{
					Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
				}
			}
		}
	}
}

// The scale the game sets this file's level up at (set_local_scale, projload.cpp): GlobalScale, which is 1 (equates.h), times the scale
// the file's level environment gives (ENVSDSCL), if it gives one. So it's that scale, or 1. False if the environment or its scale is kept
// only as plain bytes, as then there's no scale to read. waypoint_room_test reads projload.cpp and equates.h to check this is still so.
static bool LevelScale(Chunk_With_Children * file, double * scale)
{
	*scale = 1.0;

	Chunk * const environment = file->lookup_single_child("REBENVDT");

	if (!environment)
	{
		return true;
	}

	if (environment->r_u_miscellaneous())
	{
		return false;
	}

	Chunk * const levelScale = ((Chunk_With_Children *) environment)->lookup_single_child("ENVSDSCL");

	if (!levelScale)
	{
		return true;
	}

	if (levelScale->r_u_miscellaneous())
	{
		return false;
	}

	*scale = ((Environment_Scale_Chunk *) levelScale)->scale;
	return true;
}

// Each waypoint's box has to give an alien room to aim for on every axis, worked out at the level's scale as the game works it out
// (waypoint_room.h). A room of 0 refuses the level, and so does a scale that isn't a finite number, or a corner too big for the game's
// whole numbers at that scale. This waits until the whole file has been read, because the scale is in the level's environment, which can
// come after the waypoints. All 5,528 waypoints in the game's own levels have room, at scale 1 and at each level's own scale: 9 of them, in
// three level files, have a room of -1 on an axis, which is allowed. Added 2026-11-14 by Melanikus for the AvP port.
void Module_Waypoint_Chunk::post_input_processing()
{
	double scale;
	bool const scaleKnown = LevelScale(GetRootChunk(), &scale);

	for(int i=0;i<NumWaypoints;i++)
	{
		ChunkWaypoint const& waypoint=Waypoints[i];
		int32_t roomX;
		int32_t roomY;
		int32_t roomZ;

		bool const roomOnEveryAxis = scaleKnown
			&& WaypointRoom(waypoint.min.x, waypoint.max.x, waypoint.centre.x, scale, &roomX) && roomX != 0
			&& WaypointRoom(waypoint.min.y, waypoint.max.y, waypoint.centre.y, scale, &roomY) && roomY != 0
			&& WaypointRoom(waypoint.min.z, waypoint.max.z, waypoint.centre.z, scale, &roomZ) && roomZ != 0;

		if(!roomOnEveryAxis)
		{
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			return;
		}
	}
}

Module_Waypoint_Chunk::Module_Waypoint_Chunk(Chunk_With_Children* parent)
:Chunk(parent,"WAYPOINT")
{
	NumWaypoints=0;
	Waypoints=0;
	
	NumAlienWaypoints=0;
	AlienWaypoints=0;
	
	NumGroundWaypoints=0;
	GroundWaypoints=0;
}

Module_Waypoint_Chunk::~Module_Waypoint_Chunk()
{
	delete[] Waypoints;
	Waypoints = 0;
}

size_t Module_Waypoint_Chunk::size_chunk()
{
	chunk_size=16;
	for(int i=0;i<NumWaypoints;i++)
	{
		chunk_size+=16+3*sizeof(ChunkVectorInt);
		chunk_size+=sizeof(WaypointLink)*Waypoints[i].NumWPLinks;
		for(int j=0;j<Waypoints[i].NumModLinks;j++)
		{
			chunk_size+=4;
			chunk_size+=(strlen(Waypoints[i].ModLinks[j].module_name)+4)&~3;
		}
	}	
	return chunk_size;
}

void Module_Waypoint_Chunk::fill_data_block(char* data_start)
{
	strncpy (data_start, identifier, 8);
	data_start += 8;
	*((int *) data_start) = chunk_size;
	data_start += 4;
	
	*(int*)data_start=NumWaypoints;
	data_start+=4;

	for(int i=0;i<NumWaypoints;i++)
	{
		ChunkWaypoint* cw=&Waypoints[i];

		*(ChunkVectorInt*)data_start=cw->min;
		data_start+=sizeof(ChunkVectorInt);
		*(ChunkVectorInt*)data_start=cw->max;
		data_start+=sizeof(ChunkVectorInt);
		*(ChunkVectorInt*)data_start=cw->centre;
		data_start+=sizeof(ChunkVectorInt);
	
		if(i==NumAlienWaypoints)
		{
			//mark start of marine waypoints
			*(int*)data_start=cw->flags | WaypointFlag_FirstGroundWaypoint;
		}
		else
		{
			*(int*)data_start=cw->flags;
		}
		data_start+=4;

		*(int*)data_start=cw->spare2;
		data_start+=4;

		*(int*)data_start=cw->NumWPLinks;
		data_start+=4;

		for(int j=0;j<cw->NumWPLinks;j++)
		{
			*(WaypointLink*)data_start=cw->WayLinks[j];
			data_start+=sizeof(WaypointLink);
		}

		*(int*)data_start=cw->NumModLinks;
		data_start+=4;
		
		for(int j=0;j<cw->NumModLinks;j++)
		{
			ModuleLink* ml=&cw->ModLinks[j];
			memcpy(data_start,ml->module_name,strlen(ml->module_name)+1);
			data_start+=(strlen(ml->module_name)+4)&~3;
			*(int*)data_start=ml->flags;
			data_start+=4;
		}
	}
}

void Module_Waypoint_Chunk::TransferWaypointData(Module_Waypoint_Chunk* mwc_from)
{
	if(!mwc_from)return;	
	if(!mwc_from->NumWaypoints)return;
	if(mwc_from==this)return;

	if(mwc_from->NumWaypoints)
	{
		ChunkWaypoint* new_wp=new ChunkWaypoint[NumWaypoints+mwc_from->NumWaypoints];
		//first take alien waypoints from this chunk
		for(int i=0;i<NumAlienWaypoints;i++)
		{
			new_wp[i]=AlienWaypoints[i];
			//set pointers to zero so the memory doesn't get deallocated when the old
			//waypoint array is deleted
			AlienWaypoints[i].WayLinks=0;
			AlienWaypoints[i].ModLinks=0;
		}

		//copy alien waypoints from other chunk

		for(int i=0;i<mwc_from->NumAlienWaypoints;i++)
		{
			ChunkWaypoint* cw=&new_wp[i+NumAlienWaypoints];
			*cw=mwc_from->AlienWaypoints[i];
			//set pointers to zero so the memory doesn't get deallocated when the old
			//waypoint chunk is deleted
			mwc_from->AlienWaypoints[i].WayLinks=0;
			mwc_from->AlienWaypoints[i].ModLinks=0;
			
			//adjust the indeces
			cw->index+=NumAlienWaypoints;
			for(int j=0;j<cw->NumWPLinks;j++)
			{
				cw->WayLinks[j].index+=NumAlienWaypoints;
			}
 		}
		NumAlienWaypoints+=mwc_from->NumAlienWaypoints;

		//now take ground waypoints from this chunk
		for(int i=0;i<NumGroundWaypoints;i++)
		{
			new_wp[NumAlienWaypoints+i]=GroundWaypoints[i];
			//set pointers to zero so the memory doesn't get deallocated when the old
			//waypoint array is deleted
			GroundWaypoints[i].WayLinks=0;
			GroundWaypoints[i].ModLinks=0;
		}

		//copy ground waypoints from other chunk

		for(int i=0;i<mwc_from->NumGroundWaypoints;i++)
		{
			ChunkWaypoint* cw=&new_wp[i+NumAlienWaypoints+NumGroundWaypoints];
			*cw=mwc_from->GroundWaypoints[i];
			//set pointers to zero so the memory doesn't get deallocated when the old
			//waypoint chunk is deleted
			mwc_from->GroundWaypoints[i].WayLinks=0;
			mwc_from->GroundWaypoints[i].ModLinks=0;
			
			//adjust the indeces
			cw->index+=NumGroundWaypoints;
			for(int j=0;j<cw->NumWPLinks;j++)
			{
				cw->WayLinks[j].index+=NumGroundWaypoints;
			}
		}
		NumGroundWaypoints+=mwc_from->NumGroundWaypoints;

		NumWaypoints+=mwc_from->NumWaypoints;
		//replace pointer to waypoints
		delete[] Waypoints;
		Waypoints=new_wp;
	}

	if(NumAlienWaypoints)
		AlienWaypoints=&Waypoints[0];
	else
		AlienWaypoints=0;
	
	if(NumGroundWaypoints)
		GroundWaypoints=&Waypoints[NumAlienWaypoints];
	else
		GroundWaypoints=0;

   	delete mwc_from;
}
///////////////////////////////////////////////////////////////////////////////

// Both of these treat their parent as a module's data, and its parent as an object, so they can only be made there. All 285 and all
// 1711 in the game's own levels are.
RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT("AIMODMAS",AI_Module_Master_Chunk,"MODULEDT",Object_Module_Data_Chunk)

AI_Module_Master_Chunk::AI_Module_Master_Chunk(Chunk_With_Children* parent,const char* data,size_t)
:Chunk(parent,"AIMODMAS")
{
}

AI_Module_Master_Chunk::AI_Module_Master_Chunk(Object_Module_Data_Chunk* parent)
:Chunk(parent,"AIMODMAS")
{
}


size_t AI_Module_Master_Chunk::size_chunk()
{
	chunk_size=12;
	return chunk_size;
}

void AI_Module_Master_Chunk::fill_data_block(char* data_start)
{
	strncpy (data_start, identifier, 8);
	data_start += 8;
	*((int *) data_start) = chunk_size;
	data_start += 4;
}

AI_Module_Slave_Chunk* AddModuleSlaveChunk(Object_Chunk* oc,Object_Chunk* master)
{
	Object_Module_Data_Chunk* omdc=0;
	omdc=(Object_Module_Data_Chunk*)oc->lookup_single_child("MODULEDT");
	if (!omdc)
		omdc = new Object_Module_Data_Chunk(oc);

	Chunk* child_chunk=omdc->lookup_single_child("AIMODSLA");
	
	if (child_chunk)
		delete child_chunk;

	return new AI_Module_Slave_Chunk(omdc,master);
}

void AI_Module_Master_Chunk::AddModule(Object_Chunk* oc)
{
	if(ModuleList.contains(oc)) return;
	if(oc==get_my_object_chunk()) return;
	
	Object_Module_Data_Chunk* omdc=0;
	omdc=(Object_Module_Data_Chunk*)oc->lookup_single_child("MODULEDT");
	
	if(omdc)
	{
		List<Chunk*> chlist;
		omdc->lookup_child("AIMODMAS",chlist);
		if(chlist.size())
		{
			//if the module being added is a master , add all its slaves as well
			AI_Module_Master_Chunk* ammc=(AI_Module_Master_Chunk*)chlist.first_entry();
			for(LIF<Object_Chunk*> oblif(&ammc->ModuleList);!oblif.done();oblif.next())
			{
				if(!ModuleList.contains(oblif()) && oblif()!=get_my_object_chunk())
				{
					ModuleList.add_entry(oblif());
					//create new slave chunks for the modules being added
					AddModuleSlaveChunk(oblif(),get_my_object_chunk());
				}
			}
			delete ammc;
		}
	}

	//add this module
	ModuleList.add_entry(oc);
	AddModuleSlaveChunk(oc,get_my_object_chunk());
	
	//see if there are any waypoints to copy
	if(omdc)
	{
		Module_Waypoint_Chunk* mwc_from=(Module_Waypoint_Chunk*)omdc->lookup_single_child("WAYPOINT");
		if(mwc_from)
		{
			Module_Waypoint_Chunk* mwc_to=0;
			
			mwc_to=(Module_Waypoint_Chunk*)parent->lookup_single_child("WAYPOINT");
			if(!mwc_to)
				mwc_to=new Module_Waypoint_Chunk(parent);

			mwc_to->TransferWaypointData(mwc_from);
		}
	}
}

Object_Chunk* AI_Module_Master_Chunk::get_my_object_chunk()
{
	return (Object_Chunk*)((Object_Module_Data_Chunk*)parent)->parent;	
}

///////////////////////////////////////////////////////////////////////////////

RIF_IMPLEMENT_DYNCREATE_DECLARE_PARENT_HOLDING_AT_LEAST("AIMODSLA",AI_Module_Slave_Chunk,"MODULEDT",Object_Module_Data_Chunk,4)

AI_Module_Slave_Chunk::AI_Module_Slave_Chunk(Chunk_With_Children* parent,const char* data,size_t)
:Chunk(parent,"AIMODSLA")
{
	MasterModule=0;
	MasterModuleIndex=*(int*)data;
}

AI_Module_Slave_Chunk::AI_Module_Slave_Chunk(Object_Module_Data_Chunk* parent,Object_Chunk* _MasterModule)
:Chunk(parent,"AIMODSLA")
{
	MasterModule=_MasterModule;
	MasterModuleIndex=MasterModule->object_data.index_num;
}

AI_Module_Slave_Chunk::~AI_Module_Slave_Chunk()
{
}

size_t AI_Module_Slave_Chunk::size_chunk()
{
	chunk_size=16;
	return chunk_size;
}

void AI_Module_Slave_Chunk::fill_data_block(char* data_start)
{
	strncpy (data_start, identifier, 8);
	data_start += 8;
	*((int *) data_start) = chunk_size;
	data_start += 4;
	*((int *) data_start) = MasterModuleIndex;
	data_start += 4;
}

// A slave module shares its master's room in the AI. The game gives a slave its place there only through its master's list, and a
// module left with no place is still linked to by the modules beside it (the AI setup in projload.cpp). So a slave has to name a module
// that is a master, with a master chunk of its own, and isn't a slave itself. A level where one doesn't is refused. All 1,711 slaves in
// the game's own levels name such a master, and none of their 285 masters is a slave. A link that leads to a slave is fine, and stays so:
// 5,331 in the game's own levels do. Modified 2026-11-14 by Melanikus for the AvP port.
void AI_Module_Slave_Chunk::post_input_processing()
{
	File_Chunk* fc=(File_Chunk*)GetRootChunk();
	if(!strcmp(fc->identifier,"REBINFF2"))
	{
		// No such object, an object that isn't a module, a module with no master chunk, and a master that is a slave itself all leave
		// the list empty or the module wrong, and each refuses the level
		Object_Chunk* oc=fc->get_object_by_index(MasterModuleIndex);
		Object_Module_Data_Chunk* omdc=oc ? (Object_Module_Data_Chunk*)oc->lookup_single_child("MODULEDT") : 0;

		List<Chunk*> chlist;
		if(omdc)
		{
			omdc->lookup_child("AIMODMAS",chlist);
		}

		if(!chlist.size() || omdc->lookup_single_child("AIMODSLA"))
		{
			Parent_File->error_code = CHUNK_FAILED_ON_LOAD_NOT_RECOGNISED;
			return;
		}

		AI_Module_Master_Chunk* ammc=(AI_Module_Master_Chunk*)chlist.first_entry();
		ammc->ModuleList.add_entry(get_my_object_chunk());
		MasterModule=oc;
	}
}

Object_Chunk* AI_Module_Slave_Chunk::get_my_object_chunk()
{
	return (Object_Chunk*)((Object_Module_Data_Chunk*)parent)->parent;	
}
