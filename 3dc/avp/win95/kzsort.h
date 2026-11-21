/* If this is set to a non-zero value then kzsort.c hijacks the pipeline */
#define KZSORT_ON 1

/* maximum number of modules likely to appear on-screen */
#define MAX_NUMBER_OF_VISIBLE_MODULES 400

struct KItem
{
	POLYHEADER *PolyPtr;

	int SortKey;
};

struct KObject
{
	DISPLAYBLOCK *DispPtr;

	int SortKey;

	int DrawBeforeEnvironment;
};

/* The level editor shows the whole level at once, so it asks for room for more visible modules than the game's 400 (kzsort.cpp). The game never calls this. */
extern void KRender_AllowManyModules(bool on);

/* render with new z-sort */
extern void KRenderItems(VIEWDESCRIPTORBLOCK *VDBPtr);

/* generic item shape function */
extern void KShapeItemsInstr(SHAPEINSTR *shapeinstrptr);
extern void OutputKItem(int *shapeitemptr);

extern void RenderThisDisplayblock(DISPLAYBLOCK *dbPtr);