// The test of the game's fixed-size lists. Written 2026-11-04 by Melanikus for the AvP port.
//
// WHAT IT TESTS
//
// The game keeps many things in lists with a fixed size or a fixed number of entries, and a level file or a model can ask
// for more than a list holds. Those limits used to be guarded only by asserts, which are left out of the game players run,
// so asking for too much wrote or read past the end of the list. Each is now a real check. This test overfills each list
// and asks for entries that aren't there, and checks that the game says no and stays inside the list.
//
// HOW IT RUNS
//
// It uses the game's real code. dynamics.cpp is included below, because the functions that fill the collision list can only
// be reached from inside that file. kshape.cpp, plat_shp.cpp, shpanim.cpp, string_table.cpp and text_input.cpp are compiled
// alongside this file, with the settings players' copies have, so the asserts are gone just as they are for players.
//
// Those files call most of the rest of the game, which isn't here. tests/limits_test.args tells the linker to carry on
// anyway (/FORCE:UNRESOLVED). A call to something that isn't here would crash this test, which would fail it, and none of
// the code tested here makes one. The few things it does need are the stand-ins below.
//
// If a check is taken out, the test usually notices by a count being wrong. Some of them notice by crashing or never
// finishing, because that's what the game would have done. Each line is printed straight away, so the last line shows how
// far it got. A crash gives a non-zero result too.
//
// WHAT IT CAN'T TEST
//
// Checks in code that can't be reached from here are shown only by the game building, and by reading them: the weapon slots
// in weapons.cpp, the animation lookup in hmodel.cpp, a model's frame lengths in projload.cpp and a module's lights in
// object_setup.cpp. tests/far_locations_test.cpp has the far locations.

#include "dynamics.cpp"

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "game_statistics.h"
#include "kshape.h"
#include "shpanim.h"
#include "string_table.hpp"
#include "text_input.hpp"

// Parts of the game that no header declares
extern int CurrentNumberOfTranslucentPolygons;
void ClearTranslucentPolyList(void);
void AddToTranslucentPolyList(POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr);
int *GetPolygonVertexIndices(void);
void CreateTxAnimUVArray(int *txa_data, int *uv_array, int *shapeitemptr);
void ControlTextureAnimation(DISPLAYBLOCK *dptr);
void DoShapeAnimation(DISPLAYBLOCK *dptr);

static int checksFailed = 0;

static void LimitCheck(bool passed, char const *what)
{
	printf("  %s  %s\n", passed ? "ok    " : "FAILED", what);

	if (!passed)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for the rest of the game --------------------------------------------------------------------------------

static std::vector<std::string> logLines;

void LogString(const std::string &logString)
{
	logLines.push_back(logString);
}

static int LogLinesSaying(char const *words)
{
	int count = 0;

	for (size_t i = 0; i < logLines.size(); i++)
	{
		if (logLines[i].find(words) != std::string::npos)
		{
			count++;
		}
	}

	return count;
}

static SHAPEHEADER *testShape = 0;

SHAPEHEADER *GetShapeData(int)
{
	return testShape;
}

DISPLAYBLOCK *Player = 0;
int NormalFrameTime = 65536;
PLAYER_STATUS *PlayerStatusPtr = NULL;
int NumActiveBlocks = 0;
DISPLAYBLOCK *ActiveBlockList[1];

void TransformToViewspace(VECTORCHF *)
{
}

void D3D_ZBufferedGouraudTexturedPolygon_Output(POLYHEADER *, RENDERVERTEX *)
{
}

// ---- A shape to work on ------------------------------------------------------------------------------------------------

// A polygon is a header followed by its corner numbers, ending with a marker, so it's built in a block of whole numbers
struct TestPolygon
{
	int numbers[32];

	POLYHEADER *Header()
	{
		return reinterpret_cast<POLYHEADER *>(numbers);
	}
};

static int shapePoints[3 * 8];
static int *shapePointsList[1] = { shapePoints };
static int shapeNormals[3] = { 0, 65536, 0 };
static int *shapeNormalsList[1] = { shapeNormals };

static std::vector<TestPolygon> polygons;
static std::vector<int *> polygonList;
static SHAPEHEADER shape;

static void MakeShape(int howManyPolygons, int polygonFlags)
{
	polygons.assign(howManyPolygons, TestPolygon());
	polygonList.assign(howManyPolygons, static_cast<int *>(0));

	for (int i = 0; i < howManyPolygons; i++)
	{
		POLYHEADER *polygon = polygons[i].Header();
		polygon->PolyFlags = polygonFlags;

		int *corners = &polygon->Poly1stPt;
		corners[0] = 0;
		corners[1] = 1;
		corners[2] = 2;
		corners[3] = kTerminationMarker;

		polygonList[i] = polygons[i].numbers;
	}

	memset(&shape, 0, sizeof(shape));
	shape.numitems = howManyPolygons;
	shape.items = polygonList.data();
	shape.points = shapePointsList;
	shape.sh_normals = shapeNormalsList;
	testShape = &shape;
}

// ---- The collision list --------------------------------------------------------------------------------------------------

static void StartCollisionList()
{
	CollisionPolysPtr = &CollisionPolysArray[0];
	NumberOfCollisionPolys = 0;

	DBBMinX = DBBMinY = DBBMinZ = SBBMinX = SBBMinY = SBBMinZ = -1000;
	DBBMaxX = DBBMaxY = DBBMaxZ = SBBMaxX = SBBMaxY = SBBMaxZ = 1000;
}

// Inside the list, and as full as it can get. It stops with one place free if the next polygon might need two. A list that
// never filled up wouldn't show anything, so that fails too.
static bool CollisionListIsInOrder()
{
	return NumberOfCollisionPolys >= MAXIMUM_NUMBER_OF_COLLISIONPOLYS - 1
		&& NumberOfCollisionPolys <= MAXIMUM_NUMBER_OF_COLLISIONPOLYS
		&& CollisionPolysPtr == &CollisionPolysArray[NumberOfCollisionPolys];
}

static void CheckCollisionList()
{
	printf("The collision list:\n");

	DISPLAYBLOCK object;
	memset(&object, 0, sizeof(object));

	DYNAMICSBLOCK dynamics;
	memset(&dynamics, 0, sizeof(dynamics));

	logLines.clear();

	// A few polygons all go in, one place for each
	MakeShape(10, 0);
	StartCollisionList();
	TestShapeWithDynamicBoundingBox(&object, &dynamics);
	LimitCheck(NumberOfCollisionPolys == 10, "10 polygons take 10 places");

	// One that can be hit from both sides takes two
	MakeShape(10, iflag_no_bfc);
	StartCollisionList();
	TestShapeWithDynamicBoundingBox(&object, &dynamics);
	LimitCheck(NumberOfCollisionPolys == 20, "10 polygons that can be hit from both sides take 20 places");
	LimitCheck(logLines.empty(), "and neither of those says anything in the log");

	// Too many of each kind, through each of the functions that fill the list
	MakeShape(MAXIMUM_NUMBER_OF_COLLISIONPOLYS + 500, 0);
	StartCollisionList();
	TestShapeWithDynamicBoundingBox(&object, &dynamics);
	LimitCheck(CollisionListIsInOrder(), "too many polygons for a moving object stay inside the list");
	LimitCheck(NumberOfCollisionPolys == MAXIMUM_NUMBER_OF_COLLISIONPOLYS - 1, "and it keeps a place free in case the next one takes two");
	LimitCheck(LogLinesSaying("polygons to test for collisions is full") == 1, "and the log says why");

	MakeShape(MAXIMUM_NUMBER_OF_COLLISIONPOLYS, iflag_no_bfc);
	StartCollisionList();
	TestShapeWithDynamicBoundingBox(&object, &dynamics);
	LimitCheck(CollisionListIsInOrder(), "too many that can be hit from both sides stay inside the list");
	LimitCheck(NumberOfCollisionPolys == MAXIMUM_NUMBER_OF_COLLISIONPOLYS, "and they fill it exactly");

	StartCollisionList();
	TestShapeWithParticlesDynamicBoundingBox(&object);
	LimitCheck(CollisionListIsInOrder(), "too many for a particle stay inside the list");

	MakeShape(MAXIMUM_NUMBER_OF_COLLISIONPOLYS + 500, 0);
	StartCollisionList();
	TestShapeWithParticlesDynamicBoundingBox(&object);
	LimitCheck(CollisionListIsInOrder(), "and so do too many that can only be hit from one side");

	StartCollisionList();
	TestShapeWithStaticBoundingBox(&object);
	LimitCheck(CollisionListIsInOrder(), "too many for an object standing still stay inside the list");

	MakeShape(MAXIMUM_NUMBER_OF_COLLISIONPOLYS, iflag_no_bfc);
	StartCollisionList();
	TestShapeWithStaticBoundingBox(&object);
	LimitCheck(CollisionListIsInOrder(), "and so do too many that can be hit from both sides");

	// A box has six sides, and each one takes a place
	STRATEGYBLOCK box;
	memset(&box, 0, sizeof(box));
	DISPLAYBLOCK boxDisplay;
	memset(&boxDisplay, 0, sizeof(boxDisplay));
	DYNAMICSBLOCK boxDynamics;
	memset(&boxDynamics, 0, sizeof(boxDynamics));
	box.DynPtr = &boxDynamics;
	box.SBdptr = &boxDisplay;
	boxDisplay.ObStrategyBlock = &box;

	StartCollisionList();
	int const boxesThatFit = (MAXIMUM_NUMBER_OF_COLLISIONPOLYS / 6) + 10;

	for (int i = 0; i < boxesThatFit; i++)
	{
		TestObjectWithStaticBoundingBox(&boxDisplay);
	}

	LimitCheck(CollisionListIsInOrder(), "too many boxes near an object standing still stay inside the list");

	// The same for a moving object, which looks through the list of moving things
	STRATEGYBLOCK mover;
	memset(&mover, 0, sizeof(mover));
	DISPLAYBLOCK moverDisplay;
	memset(&moverDisplay, 0, sizeof(moverDisplay));
	DYNAMICSBLOCK moverDynamics;
	memset(&moverDynamics, 0, sizeof(moverDynamics));
	mover.DynPtr = &moverDynamics;
	mover.SBdptr = &moverDisplay;
	moverDisplay.ObStrategyBlock = &mover;

	NumberOfDynamicObjects = 0;

	while (NumberOfDynamicObjects < MAX_NO_OF_DYNAMICS_BLOCKS)
	{
		DynamicObjectsList[NumberOfDynamicObjects++] = &box;
	}

	StartCollisionList();
	FindObjectPolygonsInObjectsPath(&mover);
	LimitCheck(NumberOfDynamicObjects * 6 > MAXIMUM_NUMBER_OF_COLLISIONPOLYS, "(the list of moving things can hold enough boxes to overfill it)");
	LimitCheck(CollisionListIsInOrder(), "too many boxes near a moving object stay inside the list");

	LimitCheck(LogLinesSaying("polygons to test for collisions is full") == 1, "and all of that says so only once in the log");
}

// ---- The list of see-through polygons --------------------------------------------------------------------------------------

static void CheckSeeThroughList()
{
	printf("The list of see-through polygons:\n");

	logLines.clear();
	ClearTranslucentPolyList();

	POLYHEADER header;
	memset(&header, 0, sizeof(header));
	RENDERVERTEX corners[3];
	memset(corners, 0, sizeof(corners));
	RenderPolygon.NumberOfVertices = 3;

	for (int i = 0; i < 999; i++)
	{
		AddToTranslucentPolyList(&header, corners);
	}

	LimitCheck(CurrentNumberOfTranslucentPolygons == 999, "999 go in");
	LimitCheck(logLines.empty(), "and the log says nothing");

	AddToTranslucentPolyList(&header, corners);
	LimitCheck(CurrentNumberOfTranslucentPolygons == 1000, "the 1000th goes in, which fills the list");
	LimitCheck(logLines.empty(), "and the log still says nothing");

	// Not a round number of lists' worth. Writing past the end overwrites the count, which starts it again from 0, so with
	// the check taken out, 2000 more would land back on exactly 1000 and look right.
	for (int i = 0; i < 1500; i++)
	{
		AddToTranslucentPolyList(&header, corners);
	}

	LimitCheck(CurrentNumberOfTranslucentPolygons == 1000, "1500 more are left out");
	LimitCheck(LogLinesSaying("see-through polygons is full") == 1, "and the log says so once");

	ClearTranslucentPolyList();
	AddToTranslucentPolyList(&header, corners);
	LimitCheck(CurrentNumberOfTranslucentPolygons == 1, "the next frame has room again");
	ClearTranslucentPolyList();
}

// ---- The corners of a polygon -----------------------------------------------------------------------------------------------

static void CheckPolygonCorners()
{
	printf("A polygon with too many corners:\n");

	// Far more corners than the list holds, so that writing them all would run off the end of the game's memory
	int const corners = 4000000;
	std::vector<int> numbers(corners + 64, 7);
	POLYHEADER *polygon = reinterpret_cast<POLYHEADER *>(numbers.data());
	(&polygon->Poly1stPt)[corners] = kTerminationMarker;

	std::vector<int *> list(1, numbers.data());
	MakeShape(1, 0);
	shape.items = list.data();

	DISPLAYBLOCK object;
	memset(&object, 0, sizeof(object));
	SetupPolygonAccess(&object);

	int const *indices = GetPolygonVertexIndices();
	LimitCheck(indices[0] == 7 && indices[3] == 7, "it gets the first four");
}

// ---- Texture animation -------------------------------------------------------------------------------------------------------

static void CheckTextureAnimation()
{
	printf("Texture animation sequences:\n");

	TXANIMHEADER first;
	memset(&first, 0, sizeof(first));
	first.txa_flags = 1;
	first.txa_maxframe = 5 << 16;
	first.txa_numframes = 6;

	TXANIMHEADER second = first;
	second.txa_flags = 2;

	// The first place holds the number of the sequence being shown. After the empty entry that ends the list comes a number
	// that isn't a sequence at all, so anything that looks past the end gets it and the checks notice.
	TXANIMHEADER * sequences[6] = { 0, &first, &second, 0, reinterpret_cast<TXANIMHEADER *>(0x0BADF00D), reinterpret_cast<TXANIMHEADER *>(0x0BADF00D) };
	int *textures[1] = { reinterpret_cast<int *>(sequences) };

	MakeShape(1, iflag_txanim);
	shape.sh_textures = textures;

	LimitCheck(GetTxAnimDataZ(0, 0, 0) == &first, "the first sequence is found");
	LimitCheck(GetTxAnimDataZ(0, 0, 1) == &second, "the second sequence is found");
	LimitCheck(GetTxAnimDataZ(0, 0, 2) == 0, "the end of the list gives nothing");
	LimitCheck(GetTxAnimDataZ(0, 0, 3) == 0, "one past the end of the list gives nothing");
	LimitCheck(GetTxAnimDataZ(0, 0, 4) == 0, "two past the end gives nothing");

	// Just before the list is the number of the sequence being shown. It has to hold something here, or looking there by
	// mistake would give back nothing, which is the right answer for the wrong reason.
	sequences[0] = reinterpret_cast<TXANIMHEADER *>(1);
	LimitCheck(GetTxAnimDataZ(0, 0, -1) == 0, "a negative number gives nothing");
	sequences[0] = 0;

	LimitCheck(GetTxAnimDataZ(0, 0, 100000) == 0, "a huge number gives nothing");

	TXACTRLBLK control;
	memset(&control, 0, sizeof(control));
	control.tac_sequence = 1;
	control.tac_txah_s = GetTxAnimHeaderFromShape(&control, 0);
	LimitCheck(control.tac_txah_s == &second && control.tac_txah.txa_flags == 2, "a real sequence is copied across");

	control.tac_sequence = 3;
	control.tac_txah_s = GetTxAnimHeaderFromShape(&control, 0);
	LimitCheck(control.tac_txah_s == 0, "a sequence that isn't there gives nothing");
	LimitCheck(control.tac_txah.txa_flags == 0, "and nothing is left playing");

	// Each frame, the control block tells the shape which sequence it's on. A sequence that isn't there must not be passed on.
	DISPLAYBLOCK object;
	memset(&object, 0, sizeof(object));
	object.ObTxAnimCtrlBlks = &control;
	control.tac_txarray = reinterpret_cast<int *>(sequences);
	sequences[0] = reinterpret_cast<TXANIMHEADER *>(1);

	ControlTextureAnimation(&object);
	LimitCheck(sequences[0] == reinterpret_cast<TXANIMHEADER *>(1), "the shape stays on the sequence it had");

	control.tac_sequence = 0;
	control.tac_txah_s = &first;
	control.tac_txah.txa_currentframe = 3 << 16;
	ControlTextureAnimation(&object);
	LimitCheck(sequences[0] == 0, "and a real one is passed on");
	LimitCheck(first.txa_currentframe == (3 << 16), "with its place in the animation");

	// Working out a polygon's place in the image, for a sequence that isn't there. If this ran on, it would crash.
	int places[maxpolypts * 2];
	memset(places, 0x55, sizeof(places));
	sequences[0] = reinterpret_cast<TXANIMHEADER *>(2);
	CreateTxAnimUVArray(reinterpret_cast<int *>(sequences), places, shape.items[0]);
	LimitCheck(places[0] == 0 && places[maxpolypts * 2 - 1] == 0, "a polygon on a sequence that isn't there gets places of zero");

	sequences[0] = reinterpret_cast<TXANIMHEADER *>(5);
	memset(places, 0x55, sizeof(places));
	CreateTxAnimUVArray(reinterpret_cast<int *>(sequences), places, shape.items[0]);
	LimitCheck(places[0] == 0, "and so does one further past the end");

	// An animation with one frame has a last frame of 0. Its place in the animation has to stay at 0, not go to -1, which
	// would read the frame before the first.
	int frameCorners[8] = { 0 };
	TXANIMFRAME frames[1];
	memset(frames, 0, sizeof(frames));
	frames[0].txf_uvdata = frameCorners;
	frames[0].txf_numuvs = 4;

	first.txa_maxframe = 0;
	first.txa_numframes = 1;
	first.txa_currentframe = 0;
	first.txa_framedata = frames;

	sequences[0] = 0;
	memset(places, 0x55, sizeof(places));
	CreateTxAnimUVArray(reinterpret_cast<int *>(sequences), places, shape.items[0]);
	LimitCheck(first.txa_currentframe == 0, "an animation with one frame stays on that frame");

	// An animation with no frames has no list of them. This one's list is a number that isn't a list, so reading it would crash.
	first.txa_numframes = 0;
	first.txa_framedata = reinterpret_cast<TXANIMFRAME *>(0x0BADF00D);
	sequences[0] = 0;
	memset(places, 0x55, sizeof(places));
	CreateTxAnimUVArray(reinterpret_cast<int *>(sequences), places, shape.items[0]);
	LimitCheck(places[0] == 0, "an animation with no frames gets places of zero");
}

// ---- Animated shapes ---------------------------------------------------------------------------------------------------------

static void CheckShapeAnimation()
{
	printf("Animated shapes:\n");

	static int framePoints[4][3];
	static int frameNormals[4][3];

	SHAPEANIMATIONFRAME frames[3];
	memset(frames, 0, sizeof(frames));

	for (int i = 0; i < 3; i++)
	{
		frames[i].vertices = framePoints[i];
		frames[i].item_normals = frameNormals[i];
	}

	SHAPEANIMATIONSEQUENCE sequences[2];
	memset(sequences, 0, sizeof(sequences));
	sequences[0].num_frames = 3;
	sequences[0].anim_frames = frames;
	sequences[1].num_frames = 3;
	sequences[1].anim_frames = frames;

	SHAPEANIMATIONHEADER header;
	memset(&header, 0, sizeof(header));
	header.num_sequences = 2;
	header.anim_sequences = sequences;

	SHAPEANIMATIONCONTROLLER controller;
	memset(&controller, 0, sizeof(controller));
	controller.anim_header = &header;
	controller.current.empty = 1;
	controller.next.empty = 1;

	MakeShape(0, 0);

	DISPLAYBLOCK object;
	memset(&object, 0, sizeof(object));
	object.ShapeAnimControlBlock = &controller;
	object.ObShapeData = &shape;

	SHAPEANIMATIONCONTROLDATA data;
	InitShapeAnimationControlData(&data);
	data.default_start_and_end_frames = 1;
	data.seconds_per_frame = 65536;

	data.sequence_no = 1;
	LimitCheck(SetShapeAnimationSequence(&object, &data) == 1, "a sequence the shape has can be set");
	LimitCheck(controller.current.sequence == &sequences[1], "and is the one being played");

	data.sequence_no = 2;
	LimitCheck(SetShapeAnimationSequence(&object, &data) == 0, "one past the last sequence is refused");
	LimitCheck(controller.current.sequence == &sequences[1], "and what was playing carries on");

	data.sequence_no = static_cast<unsigned long>(-1);
	LimitCheck(SetShapeAnimationSequence(&object, &data) == 0, "a negative sequence is refused");

	data.sequence_no = 0;
	LimitCheck(SetNextShapeAnimationSequence(&object, &data) == 1, "the next sequence can be set");
	LimitCheck(controller.next.sequence == &sequences[0], "to the one asked for");

	controller.next.sequence = 0;
	controller.next.empty = 1;
	data.sequence_no = 2;
	LimitCheck(SetNextShapeAnimationSequence(&object, &data) == 0, "the next sequence can't be one the shape doesn't have");
	LimitCheck(controller.next.empty == 1, "and nothing is waiting to play");

	data.sequence_no = 2;
	LimitCheck(SetOrphanedShapeAnimationSequence(&controller, &data) == 0, "nor can a part of a model's");
	LimitCheck(controller.current.sequence == &sequences[1], "and that doesn't change what's playing");

	data.sequence_no = 0;
	LimitCheck(SetOrphanedShapeAnimationSequence(&controller, &data) == 1, "a part of a model can have one that's there");

	// The last frame of the sequence playing
	data.sequence_no = 1;
	SetShapeAnimationSequence(&object, &data);
	unsigned long const lastFrame = controller.current.end_frame;

	SetCurrentShapeAnimationToStop(&object, 0, 3);
	LimitCheck(controller.current.end_frame == lastFrame, "stopping at a frame past the end is ignored");

	SetCurrentShapeAnimationToStop(&object, 0, -5);
	LimitCheck(controller.current.end_frame == lastFrame, "and so is stopping at a negative frame");

	SetCurrentShapeAnimationToStop(&object, 0, 1);
	LimitCheck(controller.current.end_frame == 1, "stopping at a real frame works");

	PauseCurrentShapeAnimation(&object, 0, 3);
	LimitCheck(controller.current.end_frame == 1, "pausing at a frame past the end is ignored");

	PauseCurrentShapeAnimation(&object, 0, 2);
	LimitCheck(controller.current.end_frame == 2, "pausing at a real frame works");

	// Copying a frame to the shape
	static int original[3];
	static int *points[1];
	static int *normals[1];
	points[0] = original;
	normals[0] = original;
	shape.points = points;
	shape.sh_normals = normals;

	controller.current.current_frame = 1;
	CopyAnimationFrameToShape(&controller.current, &object);
	LimitCheck(shape.points[0] == framePoints[1], "a real frame is copied to the shape");

	points[0] = original;
	controller.current.current_frame = 3;
	CopyAnimationFrameToShape(&controller.current, &object);
	LimitCheck(shape.points[0] == original, "a frame past the end isn't");

	controller.current.current_frame = -1;
	CopyAnimationFrameToShape(&controller.current, &object);
	LimitCheck(shape.points[0] == original, "and neither is a negative one");

	// Frames that take no time. If the check for this is missing, the test never finishes, as the game wouldn't.
	data.sequence_no = 0;
	data.seconds_per_frame = 0;
	SetShapeAnimationSequence(&object, &data);
	controller.current.time_to_next_frame = 0;
	DoShapeAnimation(&object);
	LimitCheck(controller.current.done_a_frame == 0, "frames that take no time don't hang the game, and it stays on its frame");

	// A negative time. Without the check this takes a long time but it does finish, so what shows it is that frames went by.
	data.seconds_per_frame = static_cast<unsigned long>(-1);
	SetShapeAnimationSequence(&object, &data);
	controller.current.time_to_next_frame = 0;
	DoShapeAnimation(&object);
	LimitCheck(controller.current.done_a_frame == 0, "nor do frames with a negative time");
}

// ---- The text table ------------------------------------------------------------------------------------------------------------

// The real text table, holding stand-ins for real strings, which need fonts that this test doesn't have. The table only
// keeps them and counts who's using them.
SCString::SCString(const ProjChar *)
{
}

SCString::~SCString()
{
}

static void CheckTextTable()
{
	printf("The text table:\n");

	static char first[] = "";
	static char second[] = "second";
	static char third[] = "third";

	AddToTable(first);
	AddToTable(second);
	AddToTable(third);

	SCString &empty = StringTable::GetSCString(static_cast<enum TEXTSTRING_ID>(0));
	SCString &real = StringTable::GetSCString(static_cast<enum TEXTSTRING_ID>(1));

	LimitCheck(&empty != &real, "different numbers give different strings");
	LimitCheck(&StringTable::GetSCString(static_cast<enum TEXTSTRING_ID>(2)) != &empty, "the last string in the table is found");
	LimitCheck(&StringTable::GetSCString(static_cast<enum TEXTSTRING_ID>(3)) == &empty, "one past the last gives the empty string");
	LimitCheck(&StringTable::GetSCString(static_cast<enum TEXTSTRING_ID>(-1)) == &empty, "a negative number gives the empty string");
	LimitCheck(&StringTable::GetSCString(static_cast<enum TEXTSTRING_ID>(MAX_NO_OF_TEXTSTRINGS)) == &empty, "the size of the table gives the empty string");
	LimitCheck(&StringTable::GetSCString(static_cast<enum TEXTSTRING_ID>(100000000)) == &empty, "a huge number gives the empty string");
}

// ---- Typing a line --------------------------------------------------------------------------------------------------------------

class TestTextInput : public TextInputState
{
public:
	// The longest line the game takes, not counting the zero that ends it. The game's own number is MAX_LENGTH_INPUT in
	// text_input.hpp, which only the text input itself can see, so it's written again here. If the two ever differ,
	// CheckTyping fails.
	enum { LongestLine = 250 };

	TestTextInput(char *start) : TextInputState(false, start)
	{
	}

	char const *Line() const
	{
		return GetProjChar();
	}

	size_t Length() const
	{
		return GetNumChars();
	}

	int Insert(char letter, size_t place)
	{
		return bInsertAt(letter, place);
	}

	void ProcessCarriageReturn()
	{
	}

	void TextEntryError()
	{
	}
};

// A slot number comes from a saved game, so it could be anything. The three lists sit next to each other, so one past the end of
// each is the first number of the next. PLAYER_STATUS is big, so this one's kept out of the way.
static PLAYER_STATUS WeaponStatisticsPlayer;

static bool AllTheStatisticsAreZero()
{
	AvP_GameStats zero;
	memset(&zero, 0, sizeof(zero));
	return memcmp(&CurrentGameStatistics, &zero, sizeof(zero)) == 0;
}

static void CheckWeaponStatistics()
{
	puts("the weapon statistics");

	PLAYER_STATUS *const playerBefore = PlayerStatusPtr;
	PlayerStatusPtr = &WeaponStatisticsPlayer;
	WeaponStatisticsPlayer.IsAlive = 1;

	const WEAPON_SLOT last = static_cast<WEAPON_SLOT>(MAX_NO_OF_WEAPON_SLOTS - 1);
	const WEAPON_SLOT pastTheEnd = MAX_NO_OF_WEAPON_SLOTS;
	const WEAPON_SLOT belowTheStart = static_cast<WEAPON_SLOT>(-1);

	LimitCheck(sizeof(CurrentGameStatistics.ShotsFired) / sizeof(CurrentGameStatistics.ShotsFired[0]) == MAX_NO_OF_WEAPON_SLOTS, "each list has a place for every slot");

	memset(&CurrentGameStatistics, 0, sizeof(CurrentGameStatistics));
	CurrentGameStats_WeaponFired(WEAPON_SLOT_1, 5);
	CurrentGameStats_WeaponFired(last, 3);
	CurrentGameStats_WeaponHit(WEAPON_SLOT_1, 6);
	CurrentGameStats_WeaponHit(last, 4);
	CurrentGameStats_UsingWeapon(WEAPON_SLOT_1);
	CurrentGameStats_UsingWeapon(last);
	LimitCheck(CurrentGameStatistics.ShotsFired[WEAPON_SLOT_1] == 5 && CurrentGameStatistics.ShotsFired[last] == 3, "shots fired are counted in the first slot and the last");
	LimitCheck(CurrentGameStatistics.ShotsHit[WEAPON_SLOT_1] == 6 && CurrentGameStatistics.ShotsHit[last] == 4, "and shots that hit");
	LimitCheck(CurrentGameStatistics.WeaponTimes[WEAPON_SLOT_1] == (unsigned int)NormalFrameTime && CurrentGameStatistics.WeaponTimes[last] == (unsigned int)NormalFrameTime, "and the time spent holding it");

	memset(&CurrentGameStatistics, 0, sizeof(CurrentGameStatistics));
	CurrentGameStats_UsingWeapon(pastTheEnd);
	LimitCheck(AllTheStatisticsAreZero(), "a slot past the end adds no time to the first shot count");
	CurrentGameStats_WeaponFired(pastTheEnd, 1);
	LimitCheck(AllTheStatisticsAreZero(), "no shots to the first hit count");
	CurrentGameStats_WeaponHit(pastTheEnd, 1);
	LimitCheck(AllTheStatisticsAreZero(), "and no hits to the first vision mode's time");

	CurrentGameStats_UsingWeapon(belowTheStart);
	CurrentGameStats_WeaponFired(belowTheStart, 1);
	CurrentGameStats_WeaponHit(belowTheStart, 1);
	LimitCheck(AllTheStatisticsAreZero(), "a slot below the start counts nowhere");

	WeaponStatisticsPlayer.IsAlive = 0;
	CurrentGameStats_UsingWeapon(WEAPON_SLOT_1);
	CurrentGameStats_WeaponFired(WEAPON_SLOT_1, 1);
	CurrentGameStats_WeaponHit(WEAPON_SLOT_1, 1);
	LimitCheck(AllTheStatisticsAreZero(), "and a dead player counts nothing, as before");

	PlayerStatusPtr = playerBefore;
}

static void CheckTyping()
{
	printf("Typing into a line:\n");

	static char start[] = "abc";
	TestTextInput input(start);

	LimitCheck(input.Insert('x', 1) != 0, "a letter can go in the middle of a line");
	LimitCheck(strcmp(input.Line(), "axbc") == 0, "and it goes where it should");

	// This used to run backwards through memory for as long as it could. With the check taken out, the test crashes here.
	LimitCheck(input.Insert('y', 0) != 0, "a letter can go at the very start of a line");
	LimitCheck(strcmp(input.Line(), "yaxbc") == 0, "and it goes where it should");
	LimitCheck(input.Length() == 5, "and the line is one longer");

	// Fill the line up. This stops early if a letter is refused, so the test can't go round for ever if the game's longest
	// line is ever shorter than the number above.
	while (input.Length() < TestTextInput::LongestLine && input.Insert('z', 0) != 0)
	{
	}

	LimitCheck(input.Length() == TestTextInput::LongestLine, "a line takes letters up to the longest the game allows");

	// Only looked at when the line really is that long, or this would read past the end of a shorter one
	if (input.Length() == TestTextInput::LongestLine)
	{
		LimitCheck(input.Line()[TestTextInput::LongestLine] == 0, "a full line still ends where it should");
	}

	LimitCheck(input.Insert('z', 0) == 0, "and a letter that doesn't fit is refused");
	LimitCheck(input.Length() == TestTextInput::LongestLine, "and doesn't change it");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	CheckCollisionList();
	CheckSeeThroughList();
	CheckPolygonCorners();
	CheckTextureAnimation();
	CheckShapeAnimation();
	CheckTextTable();
	CheckTyping();
	CheckWeaponStatistics();

	if (checksFailed)
	{
		printf("The limits test FAILED: %d checks\n", checksFailed);
		return 1;
	}

	puts("The limits test passed");
	return 0;
}
