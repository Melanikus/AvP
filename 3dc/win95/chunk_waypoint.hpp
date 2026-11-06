// AvP port by Barry Duncan.
// Modified 2026-11-04 by Melanikus for the AvP port:
//   a module link starts with no name, so one a broken list never reached is safe to free
//   a waypoint chunk no longer reads two spare numbers after its list: the code that writes one never wrote them, so it was reading past its end, and nothing used them

#ifndef wpchunk_hpp
#define wpchunk_hpp 1

#include "chunk.hpp"
#include "chunk_type.hpp"
#include "chunk_object.hpp"

struct WaypointLink
{
	int index;
	int flags;
};

struct ModuleLink
{
	// A file's list can stop early, and the links it didn't reach still get freed, so each starts with nothing to free.
	ModuleLink() : module_name(0), flags(0) {}
	~ModuleLink();

	char*  module_name;
	int flags;
};

#define WaypointFlag_CentreDefinedByUser 0x80000000
#define WaypointFlag_FirstGroundWaypoint 0x40000000 
struct ChunkWaypoint
{
	ChunkWaypoint();
	~ChunkWaypoint();
	
	int index;
	ChunkVectorInt min,max; //relative to centre
	ChunkVectorInt centre; //relative to world

	int NumWPLinks;
	WaypointLink* WayLinks;

	int NumModLinks;
	ModuleLink* ModLinks;

	int flags,spare2;

};

class Module_Waypoint_Chunk : public Chunk
{
	public :
	Module_Waypoint_Chunk(Chunk_With_Children*,const char *,size_t);
	Module_Waypoint_Chunk(Chunk_With_Children*);
	~Module_Waypoint_Chunk();

	virtual size_t size_chunk();
	virtual void fill_data_block(char* data_start);
	
	//Copies waypoint data and deletes the old waypoint_chunk
	void TransferWaypointData(Module_Waypoint_Chunk*);

	int NumWaypoints;
	ChunkWaypoint* Waypoints;

	ChunkWaypoint* AlienWaypoints;
	ChunkWaypoint* GroundWaypoints;
	
	short NumAlienWaypoints;
	short NumGroundWaypoints;

};

class AI_Module_Master_Chunk : public Chunk
{
	public :
	AI_Module_Master_Chunk(Chunk_With_Children*,const char*,size_t);
	AI_Module_Master_Chunk(Object_Module_Data_Chunk*);

	virtual size_t size_chunk();
	virtual void fill_data_block(char* data_start);

	void AddModule(Object_Chunk*);
	Object_Chunk* get_my_object_chunk();

	List<Object_Chunk*> ModuleList;
};

class AI_Module_Slave_Chunk : public Chunk
{
	public :
	AI_Module_Slave_Chunk(Chunk_With_Children*,const char*,size_t);
	AI_Module_Slave_Chunk(Object_Module_Data_Chunk*,Object_Chunk*);
	~AI_Module_Slave_Chunk();

	virtual size_t size_chunk();
	virtual void fill_data_block(char* data_start);
	virtual void post_input_processing();

	Object_Chunk* get_my_object_chunk();

	Object_Chunk* MasterModule;
	int MasterModuleIndex;
};


#endif


