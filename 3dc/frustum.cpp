/*
 * KJL 15:13:43 7/17/97 - frustum.c
 *
 * Contains all the functions connected
 * to the view frustum and clipping
 *
 */

#if 1

// #define USE_FRUSTUM_CULL

#include "3dc.h"
#include "module.h"
#include "inline.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "kshape.h"
#include "kzsort.h"
#include "frustum.h"
#include "particle.h"
#define UseLocalAssert Yes
#include "ourasert.h"

/*KJL****************************************************************************************
*                                       G L O B A L S                                       *
****************************************************************************************KJL*/
extern VECTORCH RotatedPts[];
extern MORPHDISPLAY MorphDisplay;
extern DISPLAYBLOCK *Global_ODB_Ptr;
extern SHAPEHEADER *Global_ShapeHeaderPtr;
extern int *Global_ShapePoints;
extern int *Global_ShapeNormals;

extern VECTORCH LocalView;

#define FAR_Z_CLIP 0
#define FAR_Z_CLIP_RANGE 49000

/* clipping code macros - these are used as building blocks to assemble
clipping fns for different polygon types with the minimum of fuss */
#define ZCLIPPINGVALUE 4
//64
#define Clip_Z_Test(v) (ZCLIPPINGVALUE <= (v)->Z)
#define Clip_NX_Test(v) (-(v)->X <= (v)->Z)
#define Clip_PX_Test(v) ((v)->X <= (v)->Z)
#define Clip_NY_Test(v) (-(v)->Y <= (v)->Z)
#define Clip_PY_Test(v) ((v)->Y <= (v)->Z)

/*KJL****************************************************************************************
*                                    P R O T O T Y P E S                                    *
****************************************************************************************KJL*/
/* GOURAUD POLYGON CLIPPING */
void (*GouraudPolygon_ClipWithNegativeX)(void);
void (*GouraudPolygon_ClipWithPositiveY)(void);
void (*GouraudPolygon_ClipWithNegativeY)(void);
void (*GouraudPolygon_ClipWithPositiveX)(void);

/* TEXTURED POLYGON CLIPPING */
void (*TexturedPolygon_ClipWithNegativeX)(void);
void (*TexturedPolygon_ClipWithPositiveY)(void);
void (*TexturedPolygon_ClipWithNegativeY)(void);
void (*TexturedPolygon_ClipWithPositiveX)(void);

/* GOURAUD TEXTURED POLYGON CLIPPING */
void (*GouraudTexturedPolygon_ClipWithNegativeX)(void);
void (*GouraudTexturedPolygon_ClipWithPositiveY)(void);
void (*GouraudTexturedPolygon_ClipWithNegativeY)(void);
void (*GouraudTexturedPolygon_ClipWithPositiveX)(void);

/* FRUSTUM TESTS */
int (*ObjectWithinFrustum)(DISPLAYBLOCK *dbPtr);
int (*ObjectCompletelyWithinFrustum)(DISPLAYBLOCK *dbPtr);
int (*VertexWithinFrustum)(RENDERVERTEX *vertexPtr);
void (*TestVerticesWithFrustum)(void);


static void GouraudPolygon_Norm_ClipWithNegativeX(void);
static void GouraudPolygon_Wide_ClipWithNegativeX(void);
static void GouraudPolygon_Norm_ClipWithPositiveY(void);
static void GouraudPolygon_Wide_ClipWithPositiveY(void);
static void GouraudPolygon_Norm_ClipWithNegativeY(void);
static void GouraudPolygon_Wide_ClipWithNegativeY(void);
static void GouraudPolygon_Norm_ClipWithPositiveX(void);
static void GouraudPolygon_Wide_ClipWithPositiveX(void);

static void TexturedPolygon_Norm_ClipWithNegativeX(void);
static void TexturedPolygon_Wide_ClipWithNegativeX(void);
static void TexturedPolygon_Norm_ClipWithPositiveY(void);
static void TexturedPolygon_Wide_ClipWithPositiveY(void);
static void TexturedPolygon_Norm_ClipWithNegativeY(void);
static void TexturedPolygon_Wide_ClipWithNegativeY(void);
static void TexturedPolygon_Norm_ClipWithPositiveX(void);
static void TexturedPolygon_Wide_ClipWithPositiveX(void);

static void GouraudTexturedPolygon_Norm_ClipWithNegativeX(void);
static void GouraudTexturedPolygon_Wide_ClipWithNegativeX(void);
static void GouraudTexturedPolygon_Norm_ClipWithPositiveY(void);
static void GouraudTexturedPolygon_Wide_ClipWithPositiveY(void);
static void GouraudTexturedPolygon_Norm_ClipWithNegativeY(void);
static void GouraudTexturedPolygon_Wide_ClipWithNegativeY(void);
static void GouraudTexturedPolygon_Norm_ClipWithPositiveX(void);
static void GouraudTexturedPolygon_Wide_ClipWithPositiveX(void);

static int VertexWithin_Norm_Frustum(RENDERVERTEX *vertexPtr);
static int VertexWithin_Wide_Frustum(RENDERVERTEX *vertexPtr);
static int ObjectWithin_Norm_Frustum(DISPLAYBLOCK *dbPtr);
static int ObjectWithin_Wide_Frustum(DISPLAYBLOCK *dbPtr);
static int ObjectCompletelyWithin_Norm_Frustum(DISPLAYBLOCK *dbPtr);
static int ObjectCompletelyWithin_Wide_Frustum(DISPLAYBLOCK *dbPtr);
static void TestVerticesWith_Norm_Frustum(void);
static void TestVerticesWith_Wide_Frustum(void);

/*KJL****************************************************************************************
*                                     F U N C T I O N S                                     *
****************************************************************************************KJL*/
void SetFrustumType(enum FrustumType frustumType)
{
	switch (frustumType)
	{
		default:
  		case FRUSTUM_TYPE_NORMAL:
		{
			/* GOURAUD POLYGON CLIPPING */
			GouraudPolygon_ClipWithNegativeX = GouraudPolygon_Norm_ClipWithNegativeX;
			GouraudPolygon_ClipWithPositiveY = GouraudPolygon_Norm_ClipWithPositiveY;
			GouraudPolygon_ClipWithNegativeY = GouraudPolygon_Norm_ClipWithNegativeY;
			GouraudPolygon_ClipWithPositiveX = GouraudPolygon_Norm_ClipWithPositiveX;

			/* TEXTURED POLYGON CLIPPING */
			TexturedPolygon_ClipWithNegativeX = TexturedPolygon_Norm_ClipWithNegativeX;
			TexturedPolygon_ClipWithPositiveY = TexturedPolygon_Norm_ClipWithPositiveY;
			TexturedPolygon_ClipWithNegativeY = TexturedPolygon_Norm_ClipWithNegativeY;
			TexturedPolygon_ClipWithPositiveX = TexturedPolygon_Norm_ClipWithPositiveX;

			/* GOURAUD TEXTURED POLYGON CLIPPING */
			GouraudTexturedPolygon_ClipWithNegativeX = GouraudTexturedPolygon_Norm_ClipWithNegativeX;
			GouraudTexturedPolygon_ClipWithPositiveY = GouraudTexturedPolygon_Norm_ClipWithPositiveY;
			GouraudTexturedPolygon_ClipWithNegativeY = GouraudTexturedPolygon_Norm_ClipWithNegativeY;
			GouraudTexturedPolygon_ClipWithPositiveX = GouraudTexturedPolygon_Norm_ClipWithPositiveX;

			/* FRUSTUM TESTS */
			TestVerticesWithFrustum = TestVerticesWith_Norm_Frustum;
			ObjectWithinFrustum = ObjectWithin_Norm_Frustum;
			ObjectCompletelyWithinFrustum = ObjectCompletelyWithin_Norm_Frustum;
			VertexWithinFrustum = VertexWithin_Norm_Frustum;

			break;
		}

		case FRUSTUM_TYPE_WIDE:
		{
			/* GOURAUD POLYGON CLIPPING */
			GouraudPolygon_ClipWithNegativeX = GouraudPolygon_Wide_ClipWithNegativeX;
			GouraudPolygon_ClipWithPositiveY = GouraudPolygon_Wide_ClipWithPositiveY;
			GouraudPolygon_ClipWithNegativeY = GouraudPolygon_Wide_ClipWithNegativeY;
			GouraudPolygon_ClipWithPositiveX = GouraudPolygon_Wide_ClipWithPositiveX;

			/* TEXTURED POLYGON CLIPPING */
			TexturedPolygon_ClipWithNegativeX = TexturedPolygon_Wide_ClipWithNegativeX;
			TexturedPolygon_ClipWithPositiveY = TexturedPolygon_Wide_ClipWithPositiveY;
			TexturedPolygon_ClipWithNegativeY = TexturedPolygon_Wide_ClipWithNegativeY;
			TexturedPolygon_ClipWithPositiveX = TexturedPolygon_Wide_ClipWithPositiveX;

			/* GOURAUD TEXTURED POLYGON CLIPPING */
			GouraudTexturedPolygon_ClipWithNegativeX = GouraudTexturedPolygon_Wide_ClipWithNegativeX;
			GouraudTexturedPolygon_ClipWithPositiveY = GouraudTexturedPolygon_Wide_ClipWithPositiveY;
			GouraudTexturedPolygon_ClipWithNegativeY = GouraudTexturedPolygon_Wide_ClipWithNegativeY;
			GouraudTexturedPolygon_ClipWithPositiveX = GouraudTexturedPolygon_Wide_ClipWithPositiveX;

			/* FRUSTUM TESTS */
			TestVerticesWithFrustum = TestVerticesWith_Wide_Frustum;
			ObjectWithinFrustum = ObjectWithin_Wide_Frustum;
			ObjectCompletelyWithinFrustum = ObjectCompletelyWithin_Wide_Frustum;
			VertexWithinFrustum = VertexWithin_Wide_Frustum;

			break;
		}
	}
}

#define Clip_LoopStart(b) \
	do \
	{ \
		RENDERVERTEX *nextVertexPtr; \
		int nextVertexInside; \
 \
		/* setup pointer to next vertex, wrapping round if necessary */ \
		if (!--verticesLeft) \
		{ \
			nextVertexPtr = (b); \
		} \
		else  \
		{ \
			nextVertexPtr = curVertexPtr+1; \
		} \
 \
		/* if current vertex is inside the plane, output it */ \
		if (curVertexInside) \
		{ \
			numberOfPointsOutputted++; \
			*outputVerticesPtr++ = *curVertexPtr; \
		} \
 \
		/* test if next vertex is inside the plane */ \
			nextVertexInside =

#define Clip_Z_OutputXYZ \
		/* if one is in, and the other is out, output a clipped vertex */ \
		if (nextVertexInside != curVertexInside) \
		{ \
			int lambda; \
 \
			lambda = DIV_FIXED \
			( \
				(ZCLIPPINGVALUE - curVertexPtr->Z), \
				(nextVertexPtr->Z - curVertexPtr->Z) \
			); \
 \
			outputVerticesPtr->X = curVertexPtr->X + MUL_FIXED(lambda,nextVertexPtr->X-curVertexPtr->X); \
			outputVerticesPtr->Y = curVertexPtr->Y + MUL_FIXED(lambda,nextVertexPtr->Y-curVertexPtr->Y); \
			outputVerticesPtr->Z = ZCLIPPINGVALUE;

#define Clip_NX_OutputXYZ \
		/* if one is in, and the other is out, output a clipped vertex */ \
		if (nextVertexInside != curVertexInside) \
		{ \
			int lambda; \
 \
			lambda = DIV_FIXED \
			( \
				(curVertexPtr->Z + curVertexPtr->X), \
				-(nextVertexPtr->X-curVertexPtr->X) - (nextVertexPtr->Z-curVertexPtr->Z) \
			); \
 \
			outputVerticesPtr->X = curVertexPtr->X + MUL_FIXED(lambda,nextVertexPtr->X-curVertexPtr->X); \
			outputVerticesPtr->Z = -outputVerticesPtr->X; \
			outputVerticesPtr->Y = curVertexPtr->Y + MUL_FIXED(lambda,nextVertexPtr->Y-curVertexPtr->Y);

#define Clip_PX_OutputXYZ \
		/* if one is in, and the other is out, output a clipped vertex */ \
		if (nextVertexInside != curVertexInside) \
		{ \
			int lambda; \
 \
			lambda = DIV_FIXED \
			( \
				(curVertexPtr->Z - curVertexPtr->X), \
				(nextVertexPtr->X-curVertexPtr->X) - (nextVertexPtr->Z-curVertexPtr->Z) \
			); \
 \
			outputVerticesPtr->X = curVertexPtr->X + MUL_FIXED(lambda,nextVertexPtr->X-curVertexPtr->X); \
			outputVerticesPtr->Z = outputVerticesPtr->X; \
			outputVerticesPtr->Y = curVertexPtr->Y + MUL_FIXED(lambda,nextVertexPtr->Y-curVertexPtr->Y);

#define Clip_NY_OutputXYZ \
		/* if one is in, and the other is out, output a clipped vertex */ \
		if (nextVertexInside != curVertexInside) \
		{ \
			int lambda; \
 \
			lambda = DIV_FIXED \
			( \
				(curVertexPtr->Z + curVertexPtr->Y), \
				-(nextVertexPtr->Y-curVertexPtr->Y) - (nextVertexPtr->Z-curVertexPtr->Z) \
			); \
 \
			outputVerticesPtr->Z = curVertexPtr->Z + MUL_FIXED(lambda,nextVertexPtr->Z-curVertexPtr->Z); \
			outputVerticesPtr->Y = -(outputVerticesPtr->Z); \
			outputVerticesPtr->X = curVertexPtr->X + MUL_FIXED(lambda,nextVertexPtr->X-curVertexPtr->X);

#define Clip_PY_OutputXYZ \
		/* if one is in, and the other is out, output a clipped vertex */ \
		if (nextVertexInside != curVertexInside) \
		{ \
			int lambda; \
 \
			lambda = DIV_FIXED \
			( \
				(curVertexPtr->Z - curVertexPtr->Y), \
				(nextVertexPtr->Y-curVertexPtr->Y) - (nextVertexPtr->Z-curVertexPtr->Z) \
			); \
 \
			outputVerticesPtr->Z = curVertexPtr->Z + MUL_FIXED(lambda,nextVertexPtr->Z-curVertexPtr->Z); \
			outputVerticesPtr->Y = (outputVerticesPtr->Z); \
			outputVerticesPtr->X = curVertexPtr->X + MUL_FIXED(lambda,nextVertexPtr->X-curVertexPtr->X);

#define Clip_OutputUV \
			outputVerticesPtr->U = curVertexPtr->U + MUL_FIXED(lambda,nextVertexPtr->U-curVertexPtr->U); \
			outputVerticesPtr->V = curVertexPtr->V + MUL_FIXED(lambda,nextVertexPtr->V-curVertexPtr->V);

#define Clip_OutputI \
			outputVerticesPtr->A = curVertexPtr->A + MUL_FIXED(lambda,nextVertexPtr->A-curVertexPtr->A); \
			outputVerticesPtr->R = curVertexPtr->R + MUL_FIXED(lambda,nextVertexPtr->R-curVertexPtr->R); \
			outputVerticesPtr->G = curVertexPtr->G + MUL_FIXED(lambda,nextVertexPtr->G-curVertexPtr->G); \
			outputVerticesPtr->B = curVertexPtr->B + MUL_FIXED(lambda,nextVertexPtr->B-curVertexPtr->B); \
			outputVerticesPtr->SpecularR = curVertexPtr->SpecularR + MUL_FIXED(lambda,nextVertexPtr->SpecularR-curVertexPtr->SpecularR); \
			outputVerticesPtr->SpecularG = curVertexPtr->SpecularG + MUL_FIXED(lambda,nextVertexPtr->SpecularG-curVertexPtr->SpecularG); \
			outputVerticesPtr->SpecularB = curVertexPtr->SpecularB + MUL_FIXED(lambda,nextVertexPtr->SpecularB-curVertexPtr->SpecularB);

#define Clip_LoopEnd(b) \
			numberOfPointsOutputted++; \
			outputVerticesPtr++; \
		} \
 \
		/* okay, now the current vertex becomes what was the next vertex */ \
		curVertexPtr = nextVertexPtr; \
		curVertexInside = nextVertexInside; \
	} \
	while(verticesLeft); \
 \
	RenderPolygon.NumberOfVertices = numberOfPointsOutputted;

/* Wide screen versions of clip macros */

#define Clip_Wide_NX_Test(v) (-(v)->X <= (v)->Z*2)
#define Clip_Wide_PX_Test(v) ((v)->X <= (v)->Z*2)
#define Clip_Wide_NY_Test(v) (-(v)->Y <= (v)->Z*2)
#define Clip_Wide_PY_Test(v) ((v)->Y <= (v)->Z*2)

#define Clip_Wide_NX_OutputXYZ \
		/* if one is in, and the other is out, output a clipped vertex */ \
		if (nextVertexInside != curVertexInside) \
		{ \
			int lambda; \
 \
			lambda = DIV_FIXED \
			( \
				(-2*curVertexPtr->Z - curVertexPtr->X), \
				(nextVertexPtr->X-curVertexPtr->X) - (-2)*(nextVertexPtr->Z-curVertexPtr->Z) \
			); \
 \
			outputVerticesPtr->X = curVertexPtr->X + MUL_FIXED(lambda,nextVertexPtr->X-curVertexPtr->X); \
			outputVerticesPtr->Z = -outputVerticesPtr->X/2; \
			outputVerticesPtr->Y = curVertexPtr->Y + MUL_FIXED(lambda,nextVertexPtr->Y-curVertexPtr->Y);

#define Clip_Wide_PX_OutputXYZ \
		/* if one is in, and the other is out, output a clipped vertex */ \
		if (nextVertexInside != curVertexInside) \
		{ \
			int lambda; \
 \
			lambda = DIV_FIXED \
			( \
				(2*curVertexPtr->Z - curVertexPtr->X), \
				(nextVertexPtr->X-curVertexPtr->X) - 2*(nextVertexPtr->Z-curVertexPtr->Z) \
			); \
 \
			outputVerticesPtr->X = curVertexPtr->X + MUL_FIXED(lambda,nextVertexPtr->X-curVertexPtr->X); \
			outputVerticesPtr->Z = outputVerticesPtr->X/2; \
			outputVerticesPtr->Y = curVertexPtr->Y + MUL_FIXED(lambda,nextVertexPtr->Y-curVertexPtr->Y);

#define Clip_Wide_NY_OutputXYZ \
		/* if one is in, and the other is out, output a clipped vertex */ \
		if (nextVertexInside != curVertexInside) \
		{ \
			int lambda; \
 \
			lambda = DIV_FIXED \
			( \
				(2*curVertexPtr->Z + curVertexPtr->Y), \
				-(nextVertexPtr->Y-curVertexPtr->Y) - 2*(nextVertexPtr->Z-curVertexPtr->Z) \
			); \
 \
			outputVerticesPtr->Z = curVertexPtr->Z + MUL_FIXED(lambda,nextVertexPtr->Z-curVertexPtr->Z); \
			outputVerticesPtr->Y = -(outputVerticesPtr->Z*2); \
			outputVerticesPtr->X = curVertexPtr->X + MUL_FIXED(lambda,nextVertexPtr->X-curVertexPtr->X);

#define Clip_Wide_PY_OutputXYZ \
		/* if one is in, and the other is out, output a clipped vertex */ \
		if (nextVertexInside != curVertexInside) \
		{ \
			int lambda; \
 \
			lambda = DIV_FIXED \
			( \
				(2*curVertexPtr->Z - curVertexPtr->Y), \
				(nextVertexPtr->Y-curVertexPtr->Y) - 2*(nextVertexPtr->Z-curVertexPtr->Z) \
			); \
 \
			outputVerticesPtr->Z = curVertexPtr->Z + MUL_FIXED(lambda,nextVertexPtr->Z-curVertexPtr->Z); \
			outputVerticesPtr->Y = (outputVerticesPtr->Z*2); \
			outputVerticesPtr->X = curVertexPtr->X + MUL_FIXED(lambda,nextVertexPtr->X-curVertexPtr->X);


/*KJL**************
* GOURAUD POLYGON *
**************KJL*/

/* Clip against Z plane */
void GouraudPolygon_ClipWithZ(void)
{
	RENDERVERTEX *curVertexPtr = VerticesBuffer;
	RENDERVERTEX *outputVerticesPtr = (RenderPolygon.Vertices);
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_Z_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart(VerticesBuffer)
	Clip_Z_Test(nextVertexPtr);
	Clip_Z_OutputXYZ
	Clip_OutputI
	Clip_LoopEnd((RenderPolygon.Vertices))
}


/* Clip against negative X plane */
static void GouraudPolygon_Norm_ClipWithNegativeX(void)
{
	RENDERVERTEX *curVertexPtr = (RenderPolygon.Vertices);
	RENDERVERTEX *outputVerticesPtr = VerticesBuffer;
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_NX_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart((RenderPolygon.Vertices))
	Clip_NX_Test(nextVertexPtr);
	Clip_NX_OutputXYZ
	Clip_OutputI
	Clip_LoopEnd(VerticesBuffer)
}

static void GouraudPolygon_Wide_ClipWithNegativeX(void)
{
	RENDERVERTEX *curVertexPtr = (RenderPolygon.Vertices);
	RENDERVERTEX *outputVerticesPtr = VerticesBuffer;
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_Wide_NX_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart((RenderPolygon.Vertices))
	Clip_Wide_NX_Test(nextVertexPtr);
	Clip_Wide_NX_OutputXYZ
	Clip_OutputI
	Clip_LoopEnd(VerticesBuffer)
}

/* Clip against positive Y plane*/
static void GouraudPolygon_Norm_ClipWithPositiveY(void)
{
	RENDERVERTEX *curVertexPtr = VerticesBuffer;
	RENDERVERTEX *outputVerticesPtr = (RenderPolygon.Vertices);
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_PY_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart(VerticesBuffer)
	Clip_PY_Test(nextVertexPtr);
	Clip_PY_OutputXYZ
	Clip_OutputI
	Clip_LoopEnd((RenderPolygon.Vertices))
}

static void GouraudPolygon_Wide_ClipWithPositiveY(void)
{
	RENDERVERTEX *curVertexPtr = VerticesBuffer;
	RENDERVERTEX *outputVerticesPtr = (RenderPolygon.Vertices);
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_Wide_PY_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart(VerticesBuffer)
	Clip_Wide_PY_Test(nextVertexPtr);
	Clip_Wide_PY_OutputXYZ
	Clip_OutputI
	Clip_LoopEnd((RenderPolygon.Vertices))
}

/* Clip against negative Y plane*/
static void GouraudPolygon_Norm_ClipWithNegativeY(void)
{
	RENDERVERTEX *curVertexPtr = (RenderPolygon.Vertices);
	RENDERVERTEX *outputVerticesPtr = VerticesBuffer;
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_NY_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart((RenderPolygon.Vertices))
	Clip_NY_Test(nextVertexPtr);
	Clip_NY_OutputXYZ
	Clip_OutputI
	Clip_LoopEnd(VerticesBuffer)
}

static void GouraudPolygon_Wide_ClipWithNegativeY(void)
{
	RENDERVERTEX *curVertexPtr = (RenderPolygon.Vertices);
	RENDERVERTEX *outputVerticesPtr = VerticesBuffer;
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_Wide_NY_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart((RenderPolygon.Vertices))
	Clip_Wide_NY_Test(nextVertexPtr);
	Clip_Wide_NY_OutputXYZ
	Clip_OutputI
	Clip_LoopEnd(VerticesBuffer)
}

/* Clip against positive X plane */
static void GouraudPolygon_Norm_ClipWithPositiveX(void)
{
	RENDERVERTEX *curVertexPtr = VerticesBuffer;
	RENDERVERTEX *outputVerticesPtr = (RenderPolygon.Vertices);
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_PX_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart(VerticesBuffer)
	Clip_PX_Test(nextVertexPtr);
	Clip_PX_OutputXYZ
	Clip_OutputI
	Clip_LoopEnd((RenderPolygon.Vertices))
}

static void GouraudPolygon_Wide_ClipWithPositiveX(void)
{
	RENDERVERTEX *curVertexPtr = VerticesBuffer;
	RENDERVERTEX *outputVerticesPtr = (RenderPolygon.Vertices);
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_Wide_PX_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart(VerticesBuffer)
	Clip_Wide_PX_Test(nextVertexPtr);
	Clip_Wide_PX_OutputXYZ
	Clip_OutputI
	Clip_LoopEnd((RenderPolygon.Vertices))
}

/*KJL***************
* TEXTURED POLYGON *
***************KJL*/

/* Clip against Z plane */
void TexturedPolygon_ClipWithZ(void)
{
	RENDERVERTEX *curVertexPtr = VerticesBuffer;
	RENDERVERTEX *outputVerticesPtr = (RenderPolygon.Vertices);
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_Z_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart(VerticesBuffer)
	Clip_Z_Test(nextVertexPtr);
	Clip_Z_OutputXYZ
	Clip_OutputUV
	Clip_OutputI
	Clip_LoopEnd((RenderPolygon.Vertices))
}

/* Clip against negative X plane */
static void TexturedPolygon_Norm_ClipWithNegativeX(void)
{
	RENDERVERTEX *curVertexPtr = (RenderPolygon.Vertices);
	RENDERVERTEX *outputVerticesPtr = VerticesBuffer;
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_NX_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart((RenderPolygon.Vertices))
	Clip_NX_Test(nextVertexPtr);
	Clip_NX_OutputXYZ
	Clip_OutputUV
	Clip_OutputI
	Clip_LoopEnd(VerticesBuffer)
}

static void TexturedPolygon_Wide_ClipWithNegativeX(void)
{
	RENDERVERTEX *curVertexPtr = (RenderPolygon.Vertices);
	RENDERVERTEX *outputVerticesPtr = VerticesBuffer;
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_Wide_NX_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart((RenderPolygon.Vertices))
	Clip_Wide_NX_Test(nextVertexPtr);
	Clip_Wide_NX_OutputXYZ
	Clip_OutputUV
	Clip_OutputI
	Clip_LoopEnd(VerticesBuffer)
}

/* Clip against positive Y plane*/
static void TexturedPolygon_Norm_ClipWithPositiveY(void)
{
	RENDERVERTEX *curVertexPtr = VerticesBuffer;
	RENDERVERTEX *outputVerticesPtr = (RenderPolygon.Vertices);
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_PY_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart(VerticesBuffer)
	Clip_PY_Test(nextVertexPtr);
	Clip_PY_OutputXYZ
	Clip_OutputUV
	Clip_OutputI
	Clip_LoopEnd((RenderPolygon.Vertices))
}

static void TexturedPolygon_Wide_ClipWithPositiveY(void)
{
	RENDERVERTEX *curVertexPtr = VerticesBuffer;
	RENDERVERTEX *outputVerticesPtr = (RenderPolygon.Vertices);
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_Wide_PY_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart(VerticesBuffer)
	Clip_Wide_PY_Test(nextVertexPtr);
	Clip_Wide_PY_OutputXYZ
	Clip_OutputUV
	Clip_OutputI
	Clip_LoopEnd((RenderPolygon.Vertices))
}

/* Clip against negative Y plane*/
static void TexturedPolygon_Norm_ClipWithNegativeY(void)
{
	RENDERVERTEX *curVertexPtr = (RenderPolygon.Vertices);
	RENDERVERTEX *outputVerticesPtr = VerticesBuffer;
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_NY_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart((RenderPolygon.Vertices))
	Clip_NY_Test(nextVertexPtr);
	Clip_NY_OutputXYZ
	Clip_OutputUV
	Clip_OutputI
	Clip_LoopEnd(VerticesBuffer)
}

static void TexturedPolygon_Wide_ClipWithNegativeY(void)
{
	RENDERVERTEX *curVertexPtr = (RenderPolygon.Vertices);
	RENDERVERTEX *outputVerticesPtr = VerticesBuffer;
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_Wide_NY_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart((RenderPolygon.Vertices))
	Clip_Wide_NY_Test(nextVertexPtr);
	Clip_Wide_NY_OutputXYZ
	Clip_OutputUV
	Clip_OutputI
	Clip_LoopEnd(VerticesBuffer)
}

/* Clip against positive X plane */
static void TexturedPolygon_Norm_ClipWithPositiveX(void)
{
	RENDERVERTEX *curVertexPtr = VerticesBuffer;
	RENDERVERTEX *outputVerticesPtr = (RenderPolygon.Vertices);
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_PX_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart(VerticesBuffer)
	Clip_PX_Test(nextVertexPtr);
	Clip_PX_OutputXYZ
	Clip_OutputUV
	Clip_OutputI
	Clip_LoopEnd((RenderPolygon.Vertices))
}

static void TexturedPolygon_Wide_ClipWithPositiveX(void)
{
	RENDERVERTEX *curVertexPtr = VerticesBuffer;
	RENDERVERTEX *outputVerticesPtr = (RenderPolygon.Vertices);
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_Wide_PX_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart(VerticesBuffer)
	Clip_Wide_PX_Test(nextVertexPtr);
	Clip_Wide_PX_OutputXYZ
	Clip_OutputUV
	Clip_OutputI
	Clip_LoopEnd((RenderPolygon.Vertices))
}


/*KJL************************
* GOURAUD TEXTURED POLYGONS *
************************KJL*/

/* Clip against Z plane */
void GouraudTexturedPolygon_ClipWithZ(void)
{
	RENDERVERTEX *curVertexPtr = VerticesBuffer;
	RENDERVERTEX *outputVerticesPtr = (RenderPolygon.Vertices);
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_Z_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart(VerticesBuffer)
	Clip_Z_Test(nextVertexPtr);
	Clip_Z_OutputXYZ
	Clip_OutputUV
	Clip_OutputI
	Clip_LoopEnd((RenderPolygon.Vertices))
}

/* Clip against negative X plane */
static void GouraudTexturedPolygon_Norm_ClipWithNegativeX(void)
{
	RENDERVERTEX *curVertexPtr = (RenderPolygon.Vertices);
	RENDERVERTEX *outputVerticesPtr = VerticesBuffer;
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_NX_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart((RenderPolygon.Vertices))
	Clip_NX_Test(nextVertexPtr);
	Clip_NX_OutputXYZ
	Clip_OutputUV
	Clip_OutputI
	Clip_LoopEnd(VerticesBuffer)
}

static void GouraudTexturedPolygon_Wide_ClipWithNegativeX(void)
{
	RENDERVERTEX *curVertexPtr = (RenderPolygon.Vertices);
	RENDERVERTEX *outputVerticesPtr = VerticesBuffer;
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_Wide_NX_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart((RenderPolygon.Vertices))
	Clip_Wide_NX_Test(nextVertexPtr);
	Clip_Wide_NX_OutputXYZ
	Clip_OutputUV
	Clip_OutputI
	Clip_LoopEnd(VerticesBuffer)
}

/* Clip against positive Y plane*/
static void GouraudTexturedPolygon_Norm_ClipWithPositiveY(void)
{
	RENDERVERTEX *curVertexPtr = VerticesBuffer;
	RENDERVERTEX *outputVerticesPtr = (RenderPolygon.Vertices);
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_PY_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart(VerticesBuffer)
	Clip_PY_Test(nextVertexPtr);
	Clip_PY_OutputXYZ
	Clip_OutputUV
	Clip_OutputI
	Clip_LoopEnd((RenderPolygon.Vertices))
}

static void GouraudTexturedPolygon_Wide_ClipWithPositiveY(void)
{
	RENDERVERTEX *curVertexPtr = VerticesBuffer;
	RENDERVERTEX *outputVerticesPtr = (RenderPolygon.Vertices);
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_Wide_PY_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart(VerticesBuffer)
	Clip_Wide_PY_Test(nextVertexPtr);
	Clip_Wide_PY_OutputXYZ
	Clip_OutputUV
	Clip_OutputI
	Clip_LoopEnd((RenderPolygon.Vertices))
}

/* Clip against negative Y plane*/
static void GouraudTexturedPolygon_Norm_ClipWithNegativeY(void)
{
	RENDERVERTEX *curVertexPtr = (RenderPolygon.Vertices);
	RENDERVERTEX *outputVerticesPtr = VerticesBuffer;
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_NY_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart((RenderPolygon.Vertices))
	Clip_NY_Test(nextVertexPtr);
	Clip_NY_OutputXYZ
	Clip_OutputUV
	Clip_OutputI
	Clip_LoopEnd(VerticesBuffer)
}

static void GouraudTexturedPolygon_Wide_ClipWithNegativeY(void)
{
	RENDERVERTEX *curVertexPtr = (RenderPolygon.Vertices);
	RENDERVERTEX *outputVerticesPtr = VerticesBuffer;
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_Wide_NY_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart((RenderPolygon.Vertices))
	Clip_Wide_NY_Test(nextVertexPtr);
	Clip_Wide_NY_OutputXYZ
	Clip_OutputUV
	Clip_OutputI
	Clip_LoopEnd(VerticesBuffer)
}

/* Clip against positive X plane */
static void GouraudTexturedPolygon_Norm_ClipWithPositiveX(void)
{
	RENDERVERTEX *curVertexPtr = VerticesBuffer;
	RENDERVERTEX *outputVerticesPtr = (RenderPolygon.Vertices);
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_PX_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart(VerticesBuffer)
	Clip_PX_Test(nextVertexPtr);
	Clip_PX_OutputXYZ
	Clip_OutputUV
	Clip_OutputI
	Clip_LoopEnd((RenderPolygon.Vertices))
}

static void GouraudTexturedPolygon_Wide_ClipWithPositiveX(void)
{
	RENDERVERTEX *curVertexPtr = VerticesBuffer;
	RENDERVERTEX *outputVerticesPtr = (RenderPolygon.Vertices);
	int verticesLeft = RenderPolygon.NumberOfVertices;
	int numberOfPointsOutputted=0;

#ifdef USE_FRUSTUM_CULL
	int curVertexInside = Clip_Wide_PX_Test(curVertexPtr);
#else
	int curVertexInside = 1;
#endif

	Clip_LoopStart(VerticesBuffer)
	Clip_Wide_PX_Test(nextVertexPtr);
	Clip_Wide_PX_OutputXYZ
	Clip_OutputUV
	Clip_OutputI
	Clip_LoopEnd((RenderPolygon.Vertices))
}

int PolygonWithinFrustum(POLYHEADER *polyPtr)
{
    char inFrustumFlag = 0;
   	char noClippingFlag = INSIDE_FRUSTUM;
	int *vertexNumberPtr = &polyPtr->Poly1stPt;

	if (polyPtr->PolyFlags & iflag_notvis)
		return 0;

	RenderPolygon.NumberOfVertices = 0;

	while (*vertexNumberPtr != kTerminationMarker)
	{
		int vertexNumber = *vertexNumberPtr++;

		inFrustumFlag |= FrustumFlagForVertex[vertexNumber];
		noClippingFlag &= FrustumFlagForVertex[vertexNumber];

		/* count the number of points in the polygon; this is used for all the loops that follow */
	   	RenderPolygon.NumberOfVertices++;
	}

#ifdef USE_FRUSTUM_CULL
	if (inFrustumFlag != INSIDE_FRUSTUM)
		return 0;
#endif

	/* at this point we know that the poly is inside the view frustum */

	/* if not a sprite, test direction of poly */
	if (!( (Global_ShapeHeaderPtr->shapeflags&ShapeFlag_Sprite) || (polyPtr->PolyFlags & iflag_no_bfc) ))
	{
		VECTORCH pop;
		VECTORCH *normalPtr = (VECTORCH*)(Global_ShapeNormals + polyPtr->PolyNormalIndex);

		if (Global_ODB_Ptr->ObMorphCtrl)
		{
			SHAPEHEADER *shape1Ptr;
			VECTORCH *shape1PointsPtr;
			VECTORCH *shape2PointsPtr;

			/* Set up the morph data */
			GetMorphDisplay(&MorphDisplay, Global_ODB_Ptr);

			shape1Ptr = MorphDisplay.md_sptr1;

			if (MorphDisplay.md_lerp == 0x0000)
			{
				shape1PointsPtr = (VECTORCH *)*shape1Ptr->points;
				pop = shape1PointsPtr[polyPtr->Poly1stPt];
			}
			else if (MorphDisplay.md_lerp == 0xffff)
			{
				SHAPEHEADER *shape2Ptr = MorphDisplay.md_sptr2;

				shape2PointsPtr = (VECTORCH *)*shape2Ptr->points;
				pop = shape2PointsPtr[polyPtr->Poly1stPt];
			}
			else
			{
				SHAPEHEADER *shape2Ptr = MorphDisplay.md_sptr2;

				shape1PointsPtr = (VECTORCH *)(*shape1Ptr->points);
				shape2PointsPtr = (VECTORCH *)(*shape2Ptr->points);

				VECTORCH vertex1 = shape1PointsPtr[polyPtr->Poly1stPt];
				VECTORCH vertex2 = shape2PointsPtr[polyPtr->Poly1stPt];

				if ((vertex1.vx == vertex2.vx && vertex1.vy == vertex2.vy && vertex1.vz == vertex2.vz))
				{
					pop = vertex1;
				}
				else
				{
					/* KJL 15:27:20 05/22/97 - I've changed this to speed things up, If a vertex
					component has a magnitude greater than 32768 things will go wrong. */
					pop.vx = vertex1.vx + (((vertex2.vx-vertex1.vx)*MorphDisplay.md_lerp)>>16);
					pop.vy = vertex1.vy + (((vertex2.vy-vertex1.vy)*MorphDisplay.md_lerp)>>16);
					pop.vz = vertex1.vz + (((vertex2.vz-vertex1.vz)*MorphDisplay.md_lerp)>>16);
				}
			}
		}

		/* Get the 1st polygon point as the POP */
		VECTORCH *pointsArray = (VECTORCH*)(Global_ShapePoints);
		pop = pointsArray[polyPtr->Poly1stPt];
	
		pop.vx -= LocalView.vx;
		pop.vy -= LocalView.vy;
		pop.vz -= LocalView.vz;

		if (Dot(&pop, normalPtr) > 0) {
			return 0;
		}
	}

	if (noClippingFlag == INSIDE_FRUSTUM) {
		return 2;
	}

	/* yes, we need to draw poly */
	return 1;
}

#if 0
int PolygonShouldBeDrawn(POLYHEADER *polyPtr)
{

	/* at this point we know that the poly is inside the view frustum */
	if (polyPtr->PolyFlags & iflag_notvis)
		return 0;

	/* if not a sprite, test direction of poly */
	if (!( (Global_ShapeHeaderPtr->shapeflags&ShapeFlag_Sprite) || (polyPtr->PolyFlags & iflag_no_bfc) ))
	{
		/* KJL 16:49:14 7/10/97 -

		***** MORPHED NORMALS SUPPORT NOT YET ADDED *****

		*/
		VECTORCH pop;
		VECTORCH *normalPtr = (VECTORCH*)(Global_ShapeNormals + polyPtr->PolyNormalIndex);
		VECTORCH *pointsArray = (VECTORCH*)(Global_ShapePoints);
		/* Get the 1st polygon point as the POP */

		pop.vx = pointsArray[polyPtr->Poly1stPt].vx - LocalView.vx;
		pop.vy = pointsArray[polyPtr->Poly1stPt].vy - LocalView.vy;
		pop.vz = pointsArray[polyPtr->Poly1stPt].vz - LocalView.vz;

		if (Dot(&pop, normalPtr)>0)
			return 0;
	}
	{
		int *vertexNumberPtr = &polyPtr->Poly1stPt;
		if (vertexNumberPtr[3] == Term)
		{
			RenderPolygon.NumberOfVertices = 3;
		}
		else
		{
			RenderPolygon.NumberOfVertices = 4;
		}
	}

	return 2;
}
#endif

/* KJL 16:18:59 7/7/97 - simple vertex test to be used in first
pass of subdividing code */
static int VertexWithin_Norm_Frustum(RENDERVERTEX *vertexPtr)
{
	int vertexFlag = 0;

#ifdef USE_FRUSTUM_CULL

	if (Clip_Z_Test(vertexPtr))
		vertexFlag |= INSIDE_FRUSTUM_Z_PLANE;

	if (Clip_PX_Test(vertexPtr))
		vertexFlag |= INSIDE_FRUSTUM_PX_PLANE;

	if (Clip_NX_Test(vertexPtr))
		vertexFlag |= INSIDE_FRUSTUM_NX_PLANE;

	if (Clip_PY_Test(vertexPtr))
		vertexFlag |= INSIDE_FRUSTUM_PY_PLANE;

	if (Clip_NY_Test(vertexPtr))
		vertexFlag |= INSIDE_FRUSTUM_NY_PLANE;

#else

//	if (Clip_Z_Test(vertexPtr))
		vertexFlag |= INSIDE_FRUSTUM_Z_PLANE;

//	if (Clip_PX_Test(vertexPtr))
		vertexFlag |= INSIDE_FRUSTUM_PX_PLANE;

//	if (Clip_NX_Test(vertexPtr))
		vertexFlag |= INSIDE_FRUSTUM_NX_PLANE;

//	if (Clip_PY_Test(vertexPtr))
		vertexFlag |= INSIDE_FRUSTUM_PY_PLANE;

//	if (Clip_NY_Test(vertexPtr))
		vertexFlag |= INSIDE_FRUSTUM_NY_PLANE;
#endif



	return vertexFlag;
}

static int VertexWithin_Wide_Frustum(RENDERVERTEX *vertexPtr)
{
	int vertexFlag = 0;

#ifdef USE_FRUSTUM_CULL

	if (Clip_Z_Test(vertexPtr))
		vertexFlag |= INSIDE_FRUSTUM_Z_PLANE;

	if (Clip_Wide_PX_Test(vertexPtr))
		vertexFlag |= INSIDE_FRUSTUM_PX_PLANE;

	if (Clip_Wide_NX_Test(vertexPtr))
		vertexFlag |= INSIDE_FRUSTUM_NX_PLANE;

	if (Clip_Wide_PY_Test(vertexPtr))
		vertexFlag |= INSIDE_FRUSTUM_PY_PLANE;

	if (Clip_Wide_NY_Test(vertexPtr))
		vertexFlag |= INSIDE_FRUSTUM_NY_PLANE;

#else

//	if (Clip_Z_Test(vertexPtr))
		vertexFlag |= INSIDE_FRUSTUM_Z_PLANE;

//	if (Clip_Wide_PX_Test(vertexPtr))
		vertexFlag |= INSIDE_FRUSTUM_PX_PLANE;

//	if (Clip_Wide_NX_Test(vertexPtr))
		vertexFlag |= INSIDE_FRUSTUM_NX_PLANE;

//	if (Clip_Wide_PY_Test(vertexPtr))
		vertexFlag |= INSIDE_FRUSTUM_PY_PLANE;

//	if (Clip_Wide_NY_Test(vertexPtr))
		vertexFlag |= INSIDE_FRUSTUM_NY_PLANE;
#endif

	return vertexFlag;
}


/* KJL 15:32:52 7/17/97 - Test to see if an object is in the view frustum */
static int ObjectWithin_Norm_Frustum(DISPLAYBLOCK *dbPtr)
{

#ifndef USE_FRUSTUM_CULL
	return 1; // bjd
#endif

#if FAR_Z_CLIP
	if (dbPtr->ObView.vz - dbPtr->ObShapeData->shaperadius <= FAR_Z_CLIP_RANGE)
#endif
	if (dbPtr->ObView.vz + dbPtr->ObShapeData->shaperadius >= ZCLIPPINGVALUE)
	{
		/* scale radius by square root of 2 */
		int radius = MUL_FIXED(92682, dbPtr->ObShapeData->shaperadius);

		if ((dbPtr->ObView.vx-dbPtr->ObView.vz)<=radius)
			if ((-dbPtr->ObView.vx-dbPtr->ObView.vz)<=radius)
				if ((dbPtr->ObView.vy-dbPtr->ObView.vz)<=radius)
					if ((-dbPtr->ObView.vy-dbPtr->ObView.vz)<=radius)
						return 1;
	}
	return 0;
}

static int ObjectCompletelyWithin_Norm_Frustum(DISPLAYBLOCK *dbPtr)
{

#ifndef USE_FRUSTUM_CULL
	return 1; // bjd
#endif

 //	LOCALASSERT(dbPtr->ObShapeData->shaperadius);
	if (dbPtr->ObView.vz-dbPtr->ObShapeData->shaperadius>=ZCLIPPINGVALUE)
	{
		/* scale radius by square root of 2 */
		int radius = MUL_FIXED(92682,dbPtr->ObShapeData->shaperadius);

		if ((dbPtr->ObView.vz-dbPtr->ObView.vx)>=radius)
			if ((dbPtr->ObView.vz+dbPtr->ObView.vx)>=radius)
				if ((dbPtr->ObView.vz-dbPtr->ObView.vy)>=radius)
					if ((dbPtr->ObView.vz+dbPtr->ObView.vy)>=radius)
						return 1;
	}
	return 0;
}

static int ObjectCompletelyWithin_Wide_Frustum(DISPLAYBLOCK *dbPtr)
{
	return 0;
}

static int ObjectWithin_Wide_Frustum(DISPLAYBLOCK *dbPtr)
{

#ifndef USE_FRUSTUM_CULL
	return 1; // bjd
#endif

	if (dbPtr->ObView.vz+dbPtr->ObShapeData->shaperadius>=ZCLIPPINGVALUE)
	{
		/* scale radius by square root of 5 */
		int radius = MUL_FIXED(146543,dbPtr->ObShapeData->shaperadius);

		if ((dbPtr->ObView.vx-2*dbPtr->ObView.vz)<=radius)
			if ((-dbPtr->ObView.vx-2*dbPtr->ObView.vz)<=radius)
				if ((dbPtr->ObView.vy-2*dbPtr->ObView.vz)<=radius)
					if ((-dbPtr->ObView.vy-2*dbPtr->ObView.vz)<=radius)
						return 1;
	}
	return 0;
}


char FrustumFlagForVertex[maxrotpts];

void TestVerticesWith_Norm_Frustum(void)
{
	int v = Global_ShapeHeaderPtr->numpoints;

	GLOBALASSERT(v>0);

	while (v--)
	{
		char vertexFlag = 0;

#if FAR_Z_CLIP
		if(ZCLIPPINGVALUE <= RotatedPts[v].vz && RotatedPts[v].vz<=FAR_Z_CLIP_RANGE)
#else

	#ifdef USE_FRUSTUM_CULL
		if(ZCLIPPINGVALUE <= RotatedPts[v].vz)
	#endif

#endif

#ifdef USE_FRUSTUM_CULL

		vertexFlag |= INSIDE_FRUSTUM_Z_PLANE;

		if(-RotatedPts[v].vx <= RotatedPts[v].vz)
			vertexFlag |= INSIDE_FRUSTUM_PX_PLANE;

		if(RotatedPts[v].vx <= RotatedPts[v].vz)
			vertexFlag |= INSIDE_FRUSTUM_NX_PLANE;

		if(-RotatedPts[v].vy <= RotatedPts[v].vz)
			vertexFlag |= INSIDE_FRUSTUM_PY_PLANE;

		if(RotatedPts[v].vy <= RotatedPts[v].vz)
			vertexFlag |= INSIDE_FRUSTUM_NY_PLANE;

#else
		vertexFlag |= INSIDE_FRUSTUM_Z_PLANE;

//		if(-RotatedPts[v].vx <= RotatedPts[v].vz)
			vertexFlag |= INSIDE_FRUSTUM_PX_PLANE;

//		if(RotatedPts[v].vx <= RotatedPts[v].vz)
			vertexFlag |= INSIDE_FRUSTUM_NX_PLANE;

//		if(-RotatedPts[v].vy <= RotatedPts[v].vz)
			vertexFlag |= INSIDE_FRUSTUM_PY_PLANE;

//		if(RotatedPts[v].vy <= RotatedPts[v].vz)
			vertexFlag |= INSIDE_FRUSTUM_NY_PLANE;

#endif
		FrustumFlagForVertex[v] = vertexFlag;
	}
}

void TestVerticesWith_Wide_Frustum(void)
{
	int v = Global_ShapeHeaderPtr->numpoints;

	GLOBALASSERT(v>0);

	while (v--)
	{
		char vertexFlag = 0;

#ifdef USE_FRUSTUM_CULL

		if (ZCLIPPINGVALUE <= RotatedPts[v].vz)
			vertexFlag |= INSIDE_FRUSTUM_Z_PLANE;

		if (-RotatedPts[v].vx <= RotatedPts[v].vz*2)
			vertexFlag |= INSIDE_FRUSTUM_PX_PLANE;

		if (RotatedPts[v].vx <= RotatedPts[v].vz*2)
			vertexFlag |= INSIDE_FRUSTUM_NX_PLANE;

		if (-RotatedPts[v].vy <= RotatedPts[v].vz*2)
			vertexFlag |= INSIDE_FRUSTUM_PY_PLANE;

		if (RotatedPts[v].vy <= RotatedPts[v].vz*2)
			vertexFlag |= INSIDE_FRUSTUM_NY_PLANE;

#else

//		if(ZCLIPPINGVALUE <= RotatedPts[v].vz)
			vertexFlag |= INSIDE_FRUSTUM_Z_PLANE;

//		if(-RotatedPts[v].vx <= RotatedPts[v].vz*2)
			vertexFlag |= INSIDE_FRUSTUM_PX_PLANE;

//		if(RotatedPts[v].vx <= RotatedPts[v].vz*2)
			vertexFlag |= INSIDE_FRUSTUM_NX_PLANE;

//		if(-RotatedPts[v].vy <= RotatedPts[v].vz*2)
			vertexFlag |= INSIDE_FRUSTUM_PY_PLANE;

//		if(RotatedPts[v].vy <= RotatedPts[v].vz*2)
			vertexFlag |= INSIDE_FRUSTUM_NY_PLANE;

#endif

		FrustumFlagForVertex[v] = vertexFlag;
	}
}



int DecalWithinFrustum(DECAL *decalPtr)
{
	char inFrustumFlag;
	char noClippingFlag;

#ifndef USE_FRUSTUM_CULL
	return 2; // bjd
#endif

	if (ModuleCurrVisArray[decalPtr->ModuleIndex] != 2)
		return 0;

    inFrustumFlag=0;
   	noClippingFlag=INSIDE_FRUSTUM;

	{
		int vertexFlag = VertexWithinFrustum(&VerticesBuffer[0]);
		inFrustumFlag |= vertexFlag;
		noClippingFlag &= vertexFlag;
	}
	{
		int vertexFlag = VertexWithinFrustum(&VerticesBuffer[1]);
		inFrustumFlag |= vertexFlag;
		noClippingFlag &= vertexFlag;
	}
	{
		int vertexFlag = VertexWithinFrustum(&VerticesBuffer[2]);
		inFrustumFlag |= vertexFlag;
		noClippingFlag &= vertexFlag;
	}
	{
		int vertexFlag = VertexWithinFrustum(&VerticesBuffer[3]);
		inFrustumFlag |= vertexFlag;
		noClippingFlag &= vertexFlag;
	}

	if (inFrustumFlag != INSIDE_FRUSTUM) return 0;
	if (noClippingFlag == INSIDE_FRUSTUM) return 2;

	/* yes, we need to draw poly */
	return 1;
}

int QuadWithinFrustum(void)
{
	char inFrustumFlag;
	char noClippingFlag;

#ifndef USE_FRUSTUM_CULL
	return 2; // bjd
#endif

    inFrustumFlag=0;
   	noClippingFlag=INSIDE_FRUSTUM;

	{
		int vertexFlag = VertexWithinFrustum(&VerticesBuffer[0]);
		inFrustumFlag |= vertexFlag;
		noClippingFlag &= vertexFlag;
	}
	{
		int vertexFlag = VertexWithinFrustum(&VerticesBuffer[1]);
		inFrustumFlag |= vertexFlag;
		noClippingFlag &= vertexFlag;
	}
	{
		int vertexFlag = VertexWithinFrustum(&VerticesBuffer[2]);
		inFrustumFlag |= vertexFlag;
		noClippingFlag &= vertexFlag;
	}
	{
		int vertexFlag = VertexWithinFrustum(&VerticesBuffer[3]);
		inFrustumFlag |= vertexFlag;
		noClippingFlag &= vertexFlag;
	}

	if (inFrustumFlag != INSIDE_FRUSTUM) return 0;
	if (noClippingFlag == INSIDE_FRUSTUM) return 2;

	/* yes, we need to draw poly */
	return 1;
}

int TriangleWithinFrustum(void)
{
	char inFrustumFlag;
	char noClippingFlag;

#ifndef USE_FRUSTUM_CULL
	return 2; // bjd
#endif

    inFrustumFlag=0;
   	noClippingFlag=INSIDE_FRUSTUM;

	{
		int vertexFlag = VertexWithinFrustum(&VerticesBuffer[0]);
		inFrustumFlag |= vertexFlag;
		noClippingFlag &= vertexFlag;
	}
	{
		int vertexFlag = VertexWithinFrustum(&VerticesBuffer[1]);
		inFrustumFlag |= vertexFlag;
		noClippingFlag &= vertexFlag;
	}
	{
		int vertexFlag = VertexWithinFrustum(&VerticesBuffer[2]);
		inFrustumFlag |= vertexFlag;
		noClippingFlag &= vertexFlag;
	}

	if (inFrustumFlag != INSIDE_FRUSTUM) return 0;
	if (noClippingFlag == INSIDE_FRUSTUM) return 2;

	/* yes, we need to draw poly */
	return 1;
}

#endif