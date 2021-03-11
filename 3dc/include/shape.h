#ifndef SHAPE_INCLUDED
#define SHAPE_INCLUDED

// Header File for Shape Data

#include "aw.h"
#include "shpanim.h"
#include <stdint.h>

// Shape Item Function Array Indices

typedef enum 
{
	I_Pixel,
	I_Line,

	I_Polygon,
	I_GouraudPolygon,
	I_PhongPolygon,
	I_2dTexturedPolygon,
	I_Gouraud2dTexturedPolygon,
	I_3dTexturedPolygon,

	I_UnscaledSprite,
	I_ScaledSprite,
	I_SimpleShadedSphere,
	I_ShadedSphere,

	I_CloakedPolygon,
	I_Pad2,

	I_Polyline,
	I_FilledPolyline,
	I_Wireframe,

	I_Pad3,

	/* Z-Buffered */

	I_ZB_Polygon,
	I_ZB_GouraudPolygon,
	I_ZB_PhongPolygon,
	I_ZB_2dTexturedPolygon,
	I_ZB_Gouraud2dTexturedPolygon,
	I_ZB_3dTexturedPolygon,

	/* Others */

	I_Gouraud3dTexturedPolygon,
	I_ZB_Gouraud3dTexturedPolygon,

	I_Last

} ShapeItems;

// Shape Instruction Block
typedef struct shapeinstr 
{
	int sh_instr;								/* int data */
	int sh_numitems;
	int **sh_instr_data;						/* ptr to int data */

} SHAPEINSTR;

// ZSP Header Block
typedef struct zspheader 
{
	int zsp_x;						/* ZSP Array dimensions */
	int zsp_y;
	int zsp_z;

	int zsp_edge;					/* Cube edge extent */
	int zsp_diagonal;				/* Cube diagonal extent */

	struct zspzone *zsp_zone_array;

} ZSPHEADER;

// ZSP Zone Structure
typedef struct zspzone 
{
	int zsp_numitems;
	int zsp_numpoints;

	int **zsp_item_array_ptr;
	int *zsp_vertex_array_ptr;

} ZSPZONE;

// RSP plane outcode flags
#define rsp_oc_x0 0x00000001
#define rsp_oc_x1 0x00000002
#define rsp_oc_y0 0x00000004
#define rsp_oc_y1 0x00000008
#define rsp_oc_z0 0x00000010
#define rsp_oc_z1 0x00000020





/*

 Shape Header Block

*/


#if StandardShapeLanguage

/*

 Extra Item Data.

 As well as item extensions, items can have extra data in a parallel array.
 This data can be accessed using the normal index.

*/

typedef struct extraitemdata 
{
	int EID_VertexI;				/* Prelighting Intensity for each Vertex */

} EXTRAITEMDATA;


/* it might be a good idea to put in an instruction field here
   so that each fragment can have an instruction telling it what
   to do e.g. a remain around instruction */

typedef struct shapefragment
{

	int ShapeIndex;
	int NumFrags;

	int x_offset;
	int y_offset;
	int z_offset;

} SHAPEFRAGMENT;

struct loaded_sound;

typedef struct shapefragmentsound
{
	unsigned long inner_range;
	unsigned long outer_range;
	int max_volume;
	int	pitch;
	struct loaded_sound const * sound_loaded;

} SHAPEFRAGMENTSOUND;

typedef struct shapefragmentdesc
{
  /* array of shape fragment indices terminated with
     ShapeIndex = NumFrags = -1 */
	SHAPEFRAGMENT* sh_frags;
	SHAPEFRAGMENTSOUND* sh_fragsound;
} SHAPEFRAGMENTDESC;

typedef struct	Adaptive_Degradation_Desc
{
	struct shapeheader* shape;
	int distance;/*The shape should be used if the distance is greater than or equal to this distance*/

	/* KJL - some models are extremely low poly, and *only* work if they are drawn small on screen. */
	int shapeCanBeUsedCloseUp;

}ADAPTIVE_DEGRADATION_DESC;

typedef struct shapeheader 
{
	int numpoints;							/* Total #points in shape */
	int numitems;							/* Total #items in shape */

	int shapeflags;							/* Various Display Options */

	int **points;
	int **items;

	int **sh_normals;
	int **sh_vnormals;

	int **sh_textures;						/* Polygon u,v definitions */
	char **sh_localtextures;				/* Array of ptrs to filenames */

	SHAPEFRAGMENTDESC * sh_fragdesc;

	EXTRAITEMDATA *sh_extraitemdata;

	int sh_num_subshapes;					/* General use - NEVER use as test for the data being present */
	int shaperadius;						/* max(sqr(x^2+y^2+z^2)) */
	int shapemaxx;
	int shapeminx;
	int shapemaxy;
	int shapeminy;
	int shapemaxz;
	int shapeminz;

	SHAPEINSTR *sh_instruction;				/* ptr to shape instr struct */

	char *sh_name;

	ZSPHEADER *sh_zsp_header;				/* ptr to zsp header structure */

	SHAPEANIMATIONHEADER *animation_header;

	/*if shape_degradation_array is not null then it is terminated with an entry whose distance is 0
	 and whose shape is this shapeheader*/
	 /*the shapes are listed in ascending order of complexity*/
	ADAPTIVE_DEGRADATION_DESC *shape_degradation_array;

} SHAPEHEADER;


/* Shape Flags */

#define ShapeFlag_3DS_AxisFlip		0x00000001
#define ShapeFlag_RSP				0x00000002		/* Run time creation */
#define ShapeFlag_Detail			0x00000004		/* Run time creation */


#define ShapeFlag_AugZ				0x00000010		/* For the Preprocessor */
#define ShapeFlag_AugZ_Lite			0x00000020		/* No points array */

#define ShapeFlag_Free1				0x00000040

#define ShapeFlag_SizeSortItems		0x00000080		/* For PP, AugZ only */
#define ShapeFlag_VSC_tx3d			0x00000100		/* Test for VSC usage */
#define ShapeFlag_ZSP				0x00000200		/* Run time creation */
#define ShapeFlag_Sprite			0x00000400		/* Object is a sprite */
#define ShapeFlag_SpriteR			0x00000800		/* It's a rotated sprite */
#define ShapeFlag_PreLit			0x00001000		/* Use EID prelighting data */
#define ShapeFlag_Cylinder			0x00002000		/* For binary loaders */


#define ShapeFlag_SpriteResizing	0x00008000		/* Resize polygon */

#define ShapeFlag_MultiViewSprite	0x00010000		/* See "c7.doc" */

#define ShapeFlag_UnrotatedPoints	0x00020000		/* Ignores "ObMat" */
#define ShapeFlag_HasTextureAnimation 0x00040000	/*at least one of the polygons has texture animation*/



#else		/* StandardShapeLanguage */


	/*

	If not using the standard shape language, place your own version of the
	shape header in the following include file.

	*/

	#include "sheader.h"


#endif	/* StandardShapeLanguage */

/*

 Polygon Header Block

 Polygon Data is as generic as any item. However it might be useful to cast
 the initial fixed part of its data to the following struct.

*/


#if StandardShapeLanguage


#define IHdrSize 4
#define ITrmSize 1

typedef struct polyheader 
{
	int PolyItemType;
	int PolyNormalIndex;
	int PolyFlags;
	int PolyColour;
	int Poly1stPt;

} POLYHEADER;


/*

 Item Flags

 Some Item Flags can be shared, others are unique to a group of one or
 more items.

*/

#define iflag_notvis			0x00000001	/* Don't draw this item */
#define iflag_nolight			0x00000002	/* Take colour as is */
#define iflag_ignore0			0x00000004	/* Don't draw colour 0 - textures */

#if (SupportViewports && SupportViewportClipping && 0)
#define iflag_noviewportclip	0x00000008	/* See object level option too */
#endif

#define iflag_nosubdiv			0x00000008	// polygon too small to need sub dividing
#define iflag_transparent		0x00000010	/* Function depends on Video Mode */
#define iflag_no_bfc			0x00000020	/* No Back Face Cull */
#define iflag_hazing			0x00000040	/* Haze / Depth Cue colour */
#define iflag_zbuffer_w			0x00000080	/* Z-Buffer, Write-Only */
#define iflag_shadingtable		0x00000100	/* Hue is a table index */
#define iflag_tab_gour_8		0x00000200	/* Gour. for 8-bit modes uses tab. */
#define iflag_extended			0x00000400	/* N. Index ptr to item ext. blk */
#define iflag_verticaledges	0x00000800		/* A collision option whereby the item is treated as if it is a prism of infinite extent formed by extrusion of its world xz projection in the y-axis */
#define iflag_mirror			0x00001000	/* polygon is a mirror polygon. Now there's a suprise*/
#define iflag_viewdotpos		0x00002000	/* Used by BFCRO */
#define iflag_hue_per_vertex	0x00004000	/* INTERNAL USE ONLY! */
#define iflag_no_mip			0x00008000	/* Use Index #0 */
#define iflag_zbuffer_r			0x00010000	/* Z-Buffer, Read-Only */
#define iflag_linear			0x00020000	/* Linear Interpolation */
#define iflag_sortnearz			0x00040000	/* Use minz for depth value */
#define iflag_detail			0x00080000	/* Item can be range outcoded */
#define iflag_dtest_not_done	0x00100000	/* Ensure just one range test */
#define iflag_augz_planetest	0x00200000	/* Plane Test to help build tree */
#define iflag_tx2dor3d			0x00400000	/* Decide each frame which it is */
#define iflag_linear_s			0x00800000	/* Subdivided linear scans for 3d textured polygons */
#define iflag_gsort_ptest		0x01000000	/* Global sort, use plane test */
#define iflag_drawtx3das2d		0x02000000	/* 3d until SC, draw as 2d */
#define iflag_sortfarz			0x04000000	/* Use maxz for depth value */
#define iflag_light_corona		0x20000000 /* For use by the placed light strategy */
#define iflag_txanim			0x40000000	/* UV array has animation data */

// Taken this flag
#if SupportViewports && 0
#define iflag_viewport			0x80000000
#endif

#define iflag_cwise				0x80000000	/* Polygon is clockwise */

#if SupportViewports


/*

 Poly Header for Viewport Polygons

 Viewport polygons have "iflag_viewport" set. They can only be accessed
 through the structure below when they have had their Clip Window structure
 allocated.

*/

typedef struct polyheader_vp {

	int PolyItemType;
	int PolyNormalIndex;
	int PolyFlags;
	struct viewportclipwindow *PolyViewportClipWindow;
	int Poly1stPt;

} POLYHEADER_VP;


#endif	/* SupportViewports */





#else		/* StandardShapeLanguage */


	/*

	If not using the standard shape language, place your own version of the
	item/polygon header in the following include file.

	*/

	#include "pheader.h"


#endif	/* StandardShapeLanguage */

/*

 Texels

*/

typedef struct texel 
{
	int uuu;
	int vee;

} TEXEL;

typedef struct texelf 
{
	float uuuf;
	float veef;

} TEXELF;


/*

 Internal Image Structure

*/


#if StandardShapeLanguage

/* Image Header Flags */

#define ih_flag_mip         0x00000001 /* MIP map data is available */
#define ih_flag_nochromakey 0x00000002 /* internal load flag indicating that d3_func should NOT set chroma keying for this image */
#define ih_flag_tlt         0x00000004 /* image pixels must be remapped through the tlt to get the screen palette entry */
#define ih_flag_16bit       0x00000008 /* in conjunction with ih_flag_tlt, the image is 16bit and the tlt has more entries to correspond */


#else		/* StandardShapeLanguage */


	/*

	If not using the standard shape language, place your own version of the
	image header in the following include file.

	*/

	#include "iheader.h"


#endif	/* StandardShapeLanguage */


typedef struct imageextents 
{
	int u_low;
	int v_low;

	int u_high;
	int v_high;

} IMAGEEXTENTS;


typedef struct imagepolyextents 
{
	int x_low;
	int y_low;

	int x_high;
	int y_high;

} IMAGEPOLYEXTENTS;

/*

 Texture and Sprite Animation

*/


#if StandardShapeLanguage


/*

 Texture Animation Header Structure

 The data is stored as an array of ints and so the pointer must be recast
 to this structure for access, just as item pointers must be.

*/

typedef struct txanimheader 
{
	int txa_flags;
	int txa_state;
	int txa_numframes;
	int txa_currentframe;
	int txa_maxframe;
	int txa_speed;
	int txa_anim_id;			//this will be the same for all sequences on a given polygon

	struct txanimframe *txa_framedata;

} TXANIMHEADER;

#define txa_flag_play					0x00000001
#define txa_flag_reverse				0x00000002
#define txa_flag_noloop	 				0x00000004
#define txa_flag_interpolate_uvs		0x00000008
#define txa_flag_quantiseframetime	0x00000010


/*

 Texture Animation Frame Structure

 The header has a pointer to an array of these structures
 UV data for each frame is held in a separate int array

*/

typedef struct txanimframe 
{
	int txf_flags;
	int txf_scale;
	int txf_scalex;
	int txf_scaley;
	int txf_orient;
	int txf_orientx;
	int txf_orienty;
	int txf_numuvs;
	int txf_image;
	int* txf_uvdata;

} TXANIMFRAME;

/*

 Display Block Texture Animation Control Block

 An arbitrary number of these can be attached through a linked list to the
 display block.

*/

typedef struct txactrlblk 
{
	int tac_flags;
	int tac_item;
	int tac_sequence;
	int tac_node;
	int *tac_txarray;
	TXANIMHEADER tac_txah;
	TXANIMHEADER *tac_txah_s;
	struct txactrlblk *tac_next;
	int tac_anim_id;

} TXACTRLBLK;




/*

 Shape Instruction Function Array Indices

*/

typedef enum 
{
	I_ShapePoints,
	I_ShapeProject,
	I_ShapeNormals,
	I_ShapeVNormals,
	I_ShapeItems,

	I_ShapeFree5,
	I_ShapeFree6,

	I_ShapeEnd,

	I_ShapeAugZItems,

	I_ShapeFree1,
	I_ShapeFree2,
	I_ShapeFree3,
	I_ShapeFree4,

	I_ShapeSpritePoints,
	I_ShapeSpriteRPoints,

	I_Shape_ZSP_Points,
	I_Shape_ZSP_Project,
	I_Shape_ZSP_VNormals,
	I_Shape_ZSP_Items,

	I_ShapeCylinder,

	I_ShapeTransformLightRender,

	I_ShapeViewFacingPolys,

	I_ShapeUnrotatedPoints,

    I_ShapeBackdropPoints,

	I_Shape_LastInstr

} SHAPEFUNCTION;


#else		/* StandardShapeLanguage */


	/*

	If not using the standard shape language, place your own version of the
	texture animation control block in the following include file.

	*/

	#include "theader.h"


#endif	/* StandardShapeLanguage */

#define SHAPE_INCLUDED

#endif

