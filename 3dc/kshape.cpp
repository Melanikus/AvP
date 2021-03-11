/*KJL************************************************************************************
* kshape.c - replacement for all the pipeline stuff previously done in shape.c & clip.c *
************************************************************************************KJL*/
#include "3dc.h"
#include <math.h>
#include "module.h"
#include "inline.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "chunk_textures.h"
#include "kshape.h"
#include "kzsort.h"
#include "frustum.h"
#include "io.h"
#define UseLocalAssert TRUE
#include "ourasert.h"
#include "equipment.h"
#include "bh_predator.h"
#include "bh_marine.h"
#include "bh_corpse.h"
#include "bh_debris.h"
#include "bh_weapon.h"
#include "bh_types.h"
#include "pldghost.h"
#include "particle.h"
#include "vision.h"
#include "sfx.h"
#include "view.h"
#include "sphere.h"
#include "detaillevels.h"
#include "user_profile.h"
#include "renderStates.h"
#include "d3d_hud.h"
#include "tables.h"
#include "renderer.h"
#include "renderlist.h"
#include "d3d_render.h"

const RCOLOR ALIENS_LIFEFORCE_GLOW_COLOUR    = 0x20ff8080;
const RCOLOR MARINES_LIFEFORCE_GLOW_COLOUR   = 0x208080ff;
const RCOLOR PREDATORS_LIFEFORCE_GLOW_COLOUR = 0x2080ff80;

extern VIEWDESCRIPTORBLOCK *Global_VDB_Ptr;
extern SCREENDESCRIPTORBLOCK ScreenDescriptorBlock;
extern DISPLAYBLOCK *Global_ODB_Ptr;
extern EXTRAITEMDATA *Global_EID_Ptr;
extern int *Global_EID_IPtr;
extern fixed_t SmartTargetSightX, SmartTargetSightY;
extern SCREENDESCRIPTORBLOCK ScreenDescriptorBlock;

extern SHAPEHEADER *Global_ShapeHeaderPtr;
extern int *Global_ShapePoints;
extern int **Global_ShapeItems;
extern int *Global_ShapeNormals;
extern int *Global_ShapeVNormals;
extern int **Global_ShapeTextures;

extern MATRIXCH LToVMat;
extern EULER LToVMat_Euler;
extern MATRIXCH WToLMat;
extern VECTORCH LocalView;

extern int NumLightSourcesForObject;
extern LIGHTBLOCK *LightSourcesForObject[];

extern int PredatorVisionChangeCounter;

#if SupportMorphing
extern MORPHDISPLAY MorphDisplay;
#endif

extern int VideoModeType;
extern int GlobalAmbience;
extern int NumActiveBlocks;

extern DISPLAYBLOCK *ActiveBlockList[];

bool MirroringActive = false;

int MirroringAxis = -149 * 2;
float CameraZoomScale = 1.0f;

VECTORCHF FogPosition;
float FogMagnitude;
#define VOLUMETRIC_FOG 0
#define UNDERWATER 0
#define SPATIAL_SHOCKWAVE 0

int DrawFullBright;

int TripTasticPhase;

void SetupShapePipeline(void);
void ShapePipeline(SHAPEHEADER *shapePtr);

static void GouraudPolygon_Construct(POLYHEADER *polyPtr);
static void GouraudPolygon_Output(POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr);
static void GouraudTexturedPolygon_Construct(POLYHEADER *polyPtr);

static void TexturedPolygon_Construct(POLYHEADER *polyPtr);
static void TexturedPolygon_Output(POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr);

static void (*VertexIntensity)(RENDERVERTEX *renderVertexPtr);
static void VertexIntensity_Hierarchical(RENDERVERTEX *renderVertexPtr);
static void VertexIntensity_PreLit(RENDERVERTEX *renderVertexPtr);
static void VertexIntensity_Pred_Thermal(RENDERVERTEX *renderVertexPtr);
static void VertexIntensity_Pred_SeeAliens(RENDERVERTEX *renderVertexPtr);
static void VertexIntensity_Pred_SeePredatorTech(RENDERVERTEX *renderVertexPtr);
static void VertexIntensity_ImageIntensifier(RENDERVERTEX *renderVertexPtr);
static void VertexIntensity_Standard(RENDERVERTEX *renderVertexPtr);
static void VertexIntensity_Alien_Sense(RENDERVERTEX *renderVertexPtr);
static void VertexIntensity_Standard_Opt(RENDERVERTEX *renderVertexPtr);
static void VertexIntensity_FullBright(RENDERVERTEX *renderVertexPtr);
static void VertexIntensity_DiscoInferno(RENDERVERTEX *renderVertexPtr);
static void VertexIntensity_Underwater(RENDERVERTEX *renderVertexPtr);


void AddParticle(PARTICLE *particlePtr, RENDERVERTEX *renderVerticesPtr);
extern void CreateTxAnimUVArray(int *txa_data, int *uv_array, int *shapeitemptr);
extern void RotateVertex(VECTOR2D *vertexPtr, int theta);

void PredatorThermalVision_ShapePipeline(SHAPEHEADER *shapePtr);
void PredatorSeeAliensVision_ShapePipeline(SHAPEHEADER *shapePtr);
static void CloakedPolygon_Construct(POLYHEADER *polyPtr);
static void PredatorThermalVisionPolygon_Construct(POLYHEADER *polyPtr);
static void PredatorSeeAliensVisionPolygon_Construct(POLYHEADER *polyPtr);
void DoAlienEnergyView(DISPLAYBLOCK *dispPtr);
static void FindAlienEnergySource_Recursion(HMODELCONTROLLER *controllerPtr, SECTION_DATA *sectionDataPtr, unsigned int colour);
void SquishPoints(SHAPEINSTR *shapeinstrptr);
void MorphPoints(SHAPEINSTR *shapeinstrptr);
void TranslateShapeVertices(SHAPEINSTR *shapeinstrptr);
static void ParticlePolygon_Construct(PARTICLE *particlePtr);
void RenderMirroredDecal(DECAL *decalPtr);
static void DecalPolygon_Construct(DECAL *decalPtr);
void FindIntersectionWithYPlane(VECTORCH *startPtr, VECTORCH *directionPtr, VECTORCH *intersectionPtr);
void FindZFromXYIntersection(VECTORCH *startPtr, VECTORCH *directionPtr, VECTORCH *intersectionPtr);
void AddToTranslucentPolyList(POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr);
void DrawWaterFallPoly(VECTORCH *v);
void RenderAllParticlesFurtherAwayThan(int zThreshold);

extern void UpdateViewMatrix(float *viewMat);
extern void UpdateProjectionMatrix();
extern void BuildFrustum();

extern void TransformToViewspace(VECTORCHF *vector);
extern void AddCorona(PARTICLE *particlePtr, VECTORCHF *coronaPoint);
void MakeMatrixFromDirection(VECTORCH *directionPtr, MATRIXCH *matrixPtr);

VECTORCH Global_LightVector;

/*
 Global variables and arrays
*/

VECTORCH RotatedPts[maxrotpts];

#if SupportMorphing

#if (LazyEvaluationForMorphing == FALSE)
VECTORCH MorphedPts[maxmorphPts];
#endif


#endif  /* SupportMorphing */

static COLOURINTENSITIES ColourIntensityArray[maxrotpts];



RENDERPOLYGON RenderPolygon;
RENDERVERTEX VerticesBuffer[9];
static RENDERVERTEX TriangleVerticesBuffer[3];

static int *VertexNumberPtr = 0;

extern struct KItem KItemList[maxpolyptrs];
extern int *MorphedObjectPointsPtr;

#define MAX_NO_OF_TRANSLUCENT_POLYGONS 1000
RENDERPOLYGON TranslucentPolygons[MAX_NO_OF_TRANSLUCENT_POLYGONS];
POLYHEADER TranslucentPolygonHeaders[MAX_NO_OF_TRANSLUCENT_POLYGONS];
int CurrentNumberOfTranslucentPolygons;

static VECTORCH ObjectCentre;
static int HierarchicalObjectsLowestYValue;

HEATSOURCE HeatSourceList[MAX_NUMBER_OF_HEAT_SOURCES];
int NumberOfHeatSources;
int CloakingMode;
char CloakedPredatorIsMoving;
static VECTORCH LocalCameraZAxis;

static int ObjectCounter;

const int kNumStars = 500;

// starfield rendering
typedef struct {
	VECTORCH Position;
	int Colour;
} STARDESC;
static STARDESC StarArray[kNumStars];

extern void LoadStars();
extern RenderList *mainList;

extern void InitialiseLightIntensityStamps(void)
{
	int i = maxrotpts;

	do {
		i--;
		ColourIntensityArray[i].Stamp = 0;
	} while (i);

	ObjectCounter = 0;
}


void SetupShapePipeline(void)
{
	/* Set up these global pointers */
	Global_ShapePoints    = *(Global_ShapeHeaderPtr->points);
	Global_ShapeTextures  = Global_ShapeHeaderPtr->sh_textures;

	if (Global_ODB_Ptr->ObEIDPtr) {
		Global_EID_Ptr  = Global_ODB_Ptr->ObEIDPtr;
		Global_EID_IPtr = (int *) Global_ODB_Ptr->ObEIDPtr;
	} else {
		Global_EID_Ptr  = Global_ShapeHeaderPtr->sh_extraitemdata;
		Global_EID_IPtr = (int *) Global_ShapeHeaderPtr->sh_extraitemdata;
	}

	if (Global_ShapeHeaderPtr->sh_normals) {
		Global_ShapeNormals = *(Global_ShapeHeaderPtr->sh_normals);
	} else {
		Global_ShapeNormals = 0;
	}

	if (Global_ShapeHeaderPtr->sh_vnormals) {
		Global_ShapeVNormals = *(Global_ShapeHeaderPtr->sh_vnormals);
	} else {
		Global_ShapeVNormals = 0;
	}

	// if((Global_ODB_Ptr->ObStrategyBlock)&&(Global_ODB_Ptr->ObStrategyBlock->I_SBtype == I_BehaviourQueenAlien))
	//      Global_ODB_Ptr->ObFlags3 &= ObFlag3_NoLightDot;
	ObjectCounter++;
}

void ChooseLightingModel(DISPLAYBLOCK *dispPtr)
{
	LOCALASSERT(dispPtr);
	LOCALASSERT(dispPtr->ObShapeData);

	if (DrawFullBright) {
		VertexIntensity = VertexIntensity_FullBright;
	} else if (DISCOINFERNO_CHEATMODE || TRIPTASTIC_CHEATMODE) {
		VertexIntensity = VertexIntensity_DiscoInferno;
	} else if (UNDERWATER_CHEATMODE) {
		VertexIntensity = VertexIntensity_Underwater;
	} else {
		switch (CurrentVisionMode) {
			default:
			case VISION_MODE_NORMAL: {
				VertexIntensity = VertexIntensity_Standard_Opt;
				break;
			}

			case VISION_MODE_ALIEN_SENSE: {
				VertexIntensity = VertexIntensity_Alien_Sense;
				break;
			}

			case VISION_MODE_IMAGEINTENSIFIER: {
				VertexIntensity = VertexIntensity_ImageIntensifier;
				break;
			}

			case VISION_MODE_PRED_THERMAL: {
				VertexIntensity = VertexIntensity_Pred_Thermal;
				break;
			}

			case VISION_MODE_PRED_SEEALIENS: {
				VertexIntensity = VertexIntensity_Pred_SeeAliens;
				break;
			}

			case VISION_MODE_PRED_SEEPREDTECH: {
				VertexIntensity = VertexIntensity_Pred_SeePredatorTech;
				break;
			}
		}
	}
}



/*KJL**********************************************************************************
* ShapePipeline() - this function processes a shape for rendering by considering each *
* polygon (item) in turn.                                                             *
**********************************************************************************KJL*/
void ShapePipeline(SHAPEHEADER *shapePtr)
{
	int numitems = shapePtr->numitems;
	int **itemArrayPtr = shapePtr->items;

	LOCALASSERT(numitems);
	bool isCulled = false;

	switch (CurrentVisionMode) {
		case VISION_MODE_PRED_THERMAL: {
			/* if we have an object with heat sources, draw it as such */
			if (NumberOfHeatSources) { //||((Global_ODB_Ptr->ObStrategyBlock)&&(Global_ODB_Ptr->ObStrategyBlock->I_SBtype == I_BehaviourAlien)))
				PredatorThermalVision_ShapePipeline(shapePtr);
				return;
			}
			break;
		}

		case VISION_MODE_PRED_SEEALIENS: {
			STRATEGYBLOCK *sbPtr = Global_ODB_Ptr->ObStrategyBlock;

			if (sbPtr) {
				BOOL useVision = FALSE;

				switch (sbPtr->I_SBtype) {
					case I_BehaviourAutoGun:
					case I_BehaviourAlien:
					case I_BehaviourQueenAlien:
					case I_BehaviourFaceHugger:
					case I_BehaviourPredatorAlien:
					case I_BehaviourXenoborg: {
						useVision = TRUE;
						break;
					}

					case I_BehaviourMarine: {
						MARINE_STATUS_BLOCK *marineStatusPointer = (MARINE_STATUS_BLOCK *)(sbPtr->SBdataptr);
						GLOBALASSERT(marineStatusPointer);

						if (marineStatusPointer->Android) {
							useVision = TRUE;
						}
						break;
					}

					case I_BehaviourNetGhost: {
						NETGHOSTDATABLOCK *ghostDataPtr = (NETGHOSTDATABLOCK *)Global_ODB_Ptr->ObStrategyBlock->SBdataptr;

						if (ghostDataPtr->type == I_BehaviourAlienPlayer || ghostDataPtr->type == I_BehaviourAlien
						    || (ghostDataPtr->type == I_BehaviourNetCorpse && ghostDataPtr->subtype == I_BehaviourAlienPlayer)) {
							useVision = TRUE;
						}
						break;
					}

					case I_BehaviourNetCorpse: {
						NETCORPSEDATABLOCK *corpseDataPtr = (NETCORPSEDATABLOCK *)sbPtr->SBdataptr;

						if (corpseDataPtr->Android || corpseDataPtr->Type == I_BehaviourAlienPlayer || corpseDataPtr->Type == I_BehaviourAlien) {
							useVision = TRUE;
						}
						break;
					}

					case I_BehaviourHierarchicalFragment: {
						HDEBRIS_BEHAV_BLOCK *debrisDataPtr  = (HDEBRIS_BEHAV_BLOCK *)sbPtr->SBdataptr;

						if (debrisDataPtr->Type == I_BehaviourAlien
						    || debrisDataPtr->Type == I_BehaviourQueenAlien
						    || debrisDataPtr->Type == I_BehaviourPredatorAlien
						    || debrisDataPtr->Type == I_BehaviourAutoGun
						    || debrisDataPtr->Android) {
							useVision = TRUE;
						}
						break;
					}

					case I_BehaviourSpeargunBolt: {
						SPEAR_BEHAV_BLOCK *spearDataPtr  = (SPEAR_BEHAV_BLOCK *)sbPtr->SBdataptr;

						if (spearDataPtr->SpearThroughFragment) // more flags required!
							if (spearDataPtr->Type == I_BehaviourAlien
							    || spearDataPtr->Type == I_BehaviourPredatorAlien
							    || spearDataPtr->Type == I_BehaviourAutoGun) 
							{
								useVision = TRUE;
							}
						break;
					}

					default:
						break;
				}

				if (useVision) {
					PredatorSeeAliensVision_ShapePipeline(shapePtr);
					return;
				}
			}
			break;
		}

		case VISION_MODE_PRED_SEEPREDTECH: {
			STRATEGYBLOCK *sbPtr = Global_ODB_Ptr->ObStrategyBlock;

			if (sbPtr) {
				bool useVision = false;

				switch (sbPtr->I_SBtype) {
					case I_BehaviourPredator: {
						PREDATOR_STATUS_BLOCK *predData = (PREDATOR_STATUS_BLOCK *)Global_ODB_Ptr->ObStrategyBlock->SBdataptr;

						if (!predData->CloakingEffectiveness) {
							useVision = true;
						}
						break;
					}

					case I_BehaviourNPCPredatorDisc:
					case I_BehaviourPredatorDisc_SeekTrack: {
						useVision = true;
						break;
					}

					case I_BehaviourNetGhost: {
						NETGHOSTDATABLOCK *ghostDataPtr = (NETGHOSTDATABLOCK *)Global_ODB_Ptr->ObStrategyBlock->SBdataptr;

						if ((ghostDataPtr->CloakingEffectiveness == 0)
						    && (ghostDataPtr->type == I_BehaviourPredatorPlayer || ghostDataPtr->type == I_BehaviourPredator
						        || (ghostDataPtr->type == I_BehaviourInanimateObject && ghostDataPtr->IOType == IOT_Ammo && ghostDataPtr->subtype == AMMO_PRED_DISC)
						        || (ghostDataPtr->type == I_BehaviourPredatorDisc_SeekTrack)
						        || (ghostDataPtr->type == I_BehaviourNetCorpse && ghostDataPtr->subtype == I_BehaviourPredatorPlayer))) {
							useVision = true;
						}
						break;
					}

					case I_BehaviourNetCorpse: {
						NETCORPSEDATABLOCK *corpseDataPtr = (NETCORPSEDATABLOCK *)sbPtr->SBdataptr;

						if (corpseDataPtr->Type == I_BehaviourPredatorPlayer || corpseDataPtr->Type == I_BehaviourPredator) {
							useVision = true;
						}
						break;
					}

					case I_BehaviourHierarchicalFragment: {
						HDEBRIS_BEHAV_BLOCK *debrisDataPtr = (HDEBRIS_BEHAV_BLOCK *)sbPtr->SBdataptr;

						if (debrisDataPtr->Type == I_BehaviourPredator) {
							useVision = true;
						}
						break;
					}

					case I_BehaviourInanimateObject: {
						INANIMATEOBJECT_STATUSBLOCK *objStatPtr = (INANIMATEOBJECT_STATUSBLOCK *) sbPtr->SBdataptr;

						switch (objStatPtr->typeId) {
							case IOT_FieldCharge: {
								useVision = true;
								break;
							}

							case IOT_Ammo: {
								if (objStatPtr->subType == AMMO_PRED_RIFLE || objStatPtr->subType == AMMO_PRED_DISC) {
									useVision = true;
								}

								break;
							}

							default:
								break;
						}

						break;
					}

					default:
						break;
				}

				if (useVision) {
					PredatorSeeAliensVision_ShapePipeline(shapePtr);
					return;
				}
			} else if (!Global_ODB_Ptr->ObMyModule) {
				PLAYER_STATUS *playerStatusPtr = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);

				if (!(playerStatusPtr->cloakOn || playerStatusPtr->CloakingEffectiveness != 0)) {
					PredatorSeeAliensVision_ShapePipeline(shapePtr);
					return;
				}
			}

			break;
		}

		default:
			break;
	}

	TestVerticesWithFrustum();

	/* interesting hack for predator cloaking */
	if (Global_ODB_Ptr->ObStrategyBlock) {
		PRED_CLOAKSTATE cloakingStatus = PCLOAK_Off;

		if (Global_ODB_Ptr->ObStrategyBlock->I_SBtype == I_BehaviourNetGhost) {
			NETGHOSTDATABLOCK *ghostData = (NETGHOSTDATABLOCK *)Global_ODB_Ptr->ObStrategyBlock->SBdataptr;

			if (ghostData->CloakingEffectiveness) {
				cloakingStatus = PCLOAK_On;
				CloakingMode = ONE_FIXED * 5 / 4 - ghostData->CloakingEffectiveness;
			}
		}

		if (Global_ODB_Ptr->ObStrategyBlock->I_SBtype == I_BehaviourPredator) {
			PREDATOR_STATUS_BLOCK *predData = (PREDATOR_STATUS_BLOCK *)Global_ODB_Ptr->ObStrategyBlock->SBdataptr;

			if (predData->CloakingEffectiveness) {
				cloakingStatus = PCLOAK_On;
				CloakingMode = ONE_FIXED * 5 / 4 - predData->CloakingEffectiveness; //32768;
			}
		}

		if (cloakingStatus == PCLOAK_On) {
			do {
				POLYHEADER *polyPtr = (POLYHEADER *)(*itemArrayPtr++);
				int pif = PolygonWithinFrustum(polyPtr);

				if (pif)
				{
					switch (polyPtr->PolyItemType) {
						case I_ZB_Gouraud3dTexturedPolygon:
						case I_ZB_Gouraud2dTexturedPolygon:
							CloakedPolygon_Construct(polyPtr);
							/*
							                        if (pif!=2)
							                        {
							                            GouraudTexturedPolygon_ClipWithZ();
							                            if(RenderPolygon.NumberOfVertices<3) continue;
							                            GouraudTexturedPolygon_ClipWithNegativeX();
							                            if(RenderPolygon.NumberOfVertices<3) continue;
							                            GouraudTexturedPolygon_ClipWithPositiveY();
							                            if(RenderPolygon.NumberOfVertices<3) continue;
							                            GouraudTexturedPolygon_ClipWithNegativeY();
							                            if(RenderPolygon.NumberOfVertices<3) continue;
							                            GouraudTexturedPolygon_ClipWithPositiveX();
							                            if(RenderPolygon.NumberOfVertices<3) continue;
							                            D3D_ZBufferedCloakedPolygon_Output(polyPtr,RenderPolygon.Vertices);
							                        }
							                        else*/ D3D_ZBufferedCloakedPolygon_Output(polyPtr, VerticesBuffer);
							break;

						default:
							// textprint("found polygon of type %d\n",polyPtr->PolyItemType);
							break;
					}
				}
			} while (--numitems);

			return;
		}
	} else if (!Global_ODB_Ptr->ObMyModule) {
		PLAYER_STATUS *playerStatusPtr = (PLAYER_STATUS *)(Player->ObStrategyBlock->SBdataptr);

		if (playerStatusPtr->cloakOn || playerStatusPtr->CloakingEffectiveness != 0) {
			CloakingMode = ONE_FIXED * 5 / 4 - playerStatusPtr->CloakingEffectiveness; //32768;

			do {
				POLYHEADER *polyPtr = (POLYHEADER *)(*itemArrayPtr++);
				int pif = PolygonWithinFrustum(polyPtr);

				if (pif)
				{
					switch (polyPtr->PolyItemType) {
						case I_ZB_Gouraud3dTexturedPolygon:
						case I_ZB_Gouraud2dTexturedPolygon:
							CloakedPolygon_Construct(polyPtr);
							/*
							                        if (pif!=2)
							                        {
							                            GouraudTexturedPolygon_ClipWithZ();
							                            if (RenderPolygon.NumberOfVertices<3) continue;
							                            GouraudTexturedPolygon_ClipWithNegativeX();
							                            if (RenderPolygon.NumberOfVertices<3) continue;
							                            GouraudTexturedPolygon_ClipWithPositiveY();
							                            if (RenderPolygon.NumberOfVertices<3) continue;
							                            GouraudTexturedPolygon_ClipWithNegativeY();
							                            if (RenderPolygon.NumberOfVertices<3) continue;
							                            GouraudTexturedPolygon_ClipWithPositiveX();
							                            if (RenderPolygon.NumberOfVertices<3) continue;
							                            D3D_ZBufferedCloakedPolygon_Output(polyPtr,RenderPolygon.Vertices);
							                        }
							                        else*/ D3D_ZBufferedCloakedPolygon_Output(polyPtr, VerticesBuffer);
							break;

						default:
							//                          textprint("found polygon of type %d\n",polyPtr->PolyItemType);
							break;
					}
				}
			} while (--numitems);

			return;
		}
	}

	do {
		POLYHEADER *polyPtr = (POLYHEADER *)(*itemArrayPtr++);
		int pif = PolygonWithinFrustum(polyPtr);

		if (pif) {
			switch (polyPtr->PolyItemType) {
#if debug

				case I_Polyline:
				case I_FilledPolyline:
				case I_Wireframe:
				/* NB This is intended to fall through to the GouraudPolygon case */
#endif
				case I_GouraudPolygon:
				case I_Gouraud2dTexturedPolygon:
				case I_Gouraud3dTexturedPolygon:
				case I_2dTexturedPolygon:
				case I_3dTexturedPolygon:
				case I_ZB_2dTexturedPolygon:
				case I_ZB_3dTexturedPolygon: {
					//  LOCALASSERT(0);
					break;
				}

				case I_ZB_GouraudPolygon: {
					//                  break;
					//                LOCALASSERT(0);
					GouraudPolygon_Construct(polyPtr);

					if (pif != 2) {
						GouraudPolygon_ClipWithZ();

						if (RenderPolygon.NumberOfVertices < 3) {
							continue;
						}

						GouraudPolygon_ClipWithNegativeX();

						if (RenderPolygon.NumberOfVertices < 3) {
							continue;
						}

						GouraudPolygon_ClipWithPositiveY();

						if (RenderPolygon.NumberOfVertices < 3) {
							continue;
						}

						GouraudPolygon_ClipWithNegativeY();

						if (RenderPolygon.NumberOfVertices < 3) {
							continue;
						}

						GouraudPolygon_ClipWithPositiveX();

						if (RenderPolygon.NumberOfVertices < 3) {
							continue;
						}

						D3D_ZBufferedGouraudPolygon_Output(polyPtr, RenderPolygon.Vertices);
					} else {
						D3D_ZBufferedGouraudPolygon_Output(polyPtr, VerticesBuffer);
					}

					break;
				}

				case I_ZB_Gouraud3dTexturedPolygon:
				case I_ZB_Gouraud2dTexturedPolygon: {

					GouraudTexturedPolygon_Construct(polyPtr);

					if (pif != 2) {
						// if this polygon is a quad, split it into two
						if (RenderPolygon.NumberOfVertices == 4) {
							RenderPolygon.NumberOfVertices = 3;
							TriangleVerticesBuffer[0] = VerticesBuffer[0];
							TriangleVerticesBuffer[1] = VerticesBuffer[2];
							TriangleVerticesBuffer[2] = VerticesBuffer[3];
							GouraudTexturedPolygon_ClipWithZ();

							if (RenderPolygon.NumberOfVertices < 3) {
								goto SecondTriangle;
							}

							GouraudTexturedPolygon_ClipWithNegativeX();

							if (RenderPolygon.NumberOfVertices < 3) {
								goto SecondTriangle;
							}

							GouraudTexturedPolygon_ClipWithPositiveY();

							if (RenderPolygon.NumberOfVertices < 3) {
								goto SecondTriangle;
							}

							GouraudTexturedPolygon_ClipWithNegativeY();

							if (RenderPolygon.NumberOfVertices < 3) {
								goto SecondTriangle;
							}

							GouraudTexturedPolygon_ClipWithPositiveX();

							if (RenderPolygon.NumberOfVertices < 3) {
								goto SecondTriangle;
							}

							if (polyPtr->PolyFlags & iflag_transparent) {
								AddToTranslucentPolyList(polyPtr, RenderPolygon.Vertices);
							} else {
								D3D_ZBufferedGouraudTexturedPolygon_Output(polyPtr, RenderPolygon.Vertices);
							}

						SecondTriangle:
							RenderPolygon.NumberOfVertices = 3;
							VerticesBuffer[0] = TriangleVerticesBuffer[0];
							VerticesBuffer[1] = TriangleVerticesBuffer[1];
							VerticesBuffer[2] = TriangleVerticesBuffer[2];
						}

						GouraudTexturedPolygon_ClipWithZ();

						if (RenderPolygon.NumberOfVertices < 3) {
							continue;
						}

						GouraudTexturedPolygon_ClipWithNegativeX();

						if (RenderPolygon.NumberOfVertices < 3) {
							continue;
						}

						GouraudTexturedPolygon_ClipWithPositiveY();

						if (RenderPolygon.NumberOfVertices < 3) {
							continue;
						}

						GouraudTexturedPolygon_ClipWithNegativeY();

						if (RenderPolygon.NumberOfVertices < 3) {
							continue;
						}

						GouraudTexturedPolygon_ClipWithPositiveX();

						if (RenderPolygon.NumberOfVertices < 3) {
							continue;
						}

						if (polyPtr->PolyFlags & iflag_transparent) {
							AddToTranslucentPolyList(polyPtr, RenderPolygon.Vertices);
						} else {
							D3D_ZBufferedGouraudTexturedPolygon_Output(polyPtr, RenderPolygon.Vertices);
						}
					} else {
						if (polyPtr->PolyFlags & iflag_transparent) {
							// bjd - glass rendering goes through here
							AddToTranslucentPolyList(polyPtr, VerticesBuffer);
						} else {
							D3D_ZBufferedGouraudTexturedPolygon_Output(polyPtr, VerticesBuffer);
						}
					}

					break;
				}

				default:
					break;
			}
		}
	} while (--numitems);
}

void PredatorThermalVision_ShapePipeline(SHAPEHEADER *shapePtr)
{
	int numitems = shapePtr->numitems;
	int **itemArrayPtr = shapePtr->items;
	LOCALASSERT(numitems);

	//  TestVerticesWithFrustum();
	do {
		POLYHEADER *polyPtr = (POLYHEADER *)(*itemArrayPtr++);
		int pif = PolygonWithinFrustum(polyPtr);

		if (pif)
		{
			PredatorThermalVisionPolygon_Construct(polyPtr);

			//if (pif!=2)
			if (0) {
				GouraudPolygon_ClipWithZ();

				if (RenderPolygon.NumberOfVertices < 3) {
					continue;
				}

				GouraudPolygon_ClipWithNegativeX();

				if (RenderPolygon.NumberOfVertices < 3) {
					continue;
				}

				GouraudPolygon_ClipWithPositiveY();

				if (RenderPolygon.NumberOfVertices < 3) {
					continue;
				}

				GouraudPolygon_ClipWithNegativeY();

				if (RenderPolygon.NumberOfVertices < 3) {
					continue;
				}

				GouraudPolygon_ClipWithPositiveX();

				if (RenderPolygon.NumberOfVertices < 3) {
					continue;
				}

				D3D_PredatorThermalVisionPolygon_Output(polyPtr, RenderPolygon.Vertices);
			} else {
				D3D_PredatorThermalVisionPolygon_Output(polyPtr, VerticesBuffer);
			}
		}
	} while (--numitems);
}

void PredatorSeeAliensVision_ShapePipeline(SHAPEHEADER *shapePtr)
{
	int numitems = shapePtr->numitems;
	int **itemArrayPtr = shapePtr->items;
	LOCALASSERT(numitems);

	//  TestVerticesWithFrustum();

	do {
		POLYHEADER *polyPtr = (POLYHEADER *)(*itemArrayPtr++);

		switch (polyPtr->PolyItemType) {
			case I_ZB_Gouraud3dTexturedPolygon:
			case I_ZB_Gouraud2dTexturedPolygon: {
				// bjd - we have to call this to correctly set RenderPolygon.NumberOfVertices
				int pif = PolygonWithinFrustum(polyPtr);

				if (pif)
				{
					PredatorSeeAliensVisionPolygon_Construct(polyPtr);
					//                  if (pif!=2)
					//if (0)
					//                  {
					/*
					                        GouraudTexturedPolygon_ClipWithZ();
					                        if (RenderPolygon.NumberOfVertices<3) continue;
					                        GouraudTexturedPolygon_ClipWithNegativeX();
					                        if (RenderPolygon.NumberOfVertices<3) continue;
					                        GouraudTexturedPolygon_ClipWithPositiveY();
					                        if (RenderPolygon.NumberOfVertices<3) continue;
					                        GouraudTexturedPolygon_ClipWithNegativeY();
					                        if (RenderPolygon.NumberOfVertices<3) continue;
					                        GouraudTexturedPolygon_ClipWithPositiveX();
					                        if (RenderPolygon.NumberOfVertices<3) continue;
					                        D3D_ZBufferedGouraudTexturedPolygon_Output(polyPtr, RenderPolygon.Vertices);
					                    }
					                    else*/ D3D_ZBufferedGouraudTexturedPolygon_Output(polyPtr, VerticesBuffer);
				}

				break;
			}

			default:
				break;
		}
	} while (--numitems);
}


/* CLOAKED POLYGONS */
static void CloakedPolygon_Construct(POLYHEADER *polyPtr)
{
	int *texture_defn_ptr;
	RENDERVERTEX *renderVerticesPtr = VerticesBuffer;
	int i = RenderPolygon.NumberOfVertices;

	/* get ptr to uv coords for this polygon */
	int texture_defn_index = (polyPtr->PolyColour >> TxDefn);
	texture_defn_ptr = Global_ShapeTextures[texture_defn_index];
	int uv_array[maxpolypts * 2];
	VertexNumberPtr = &polyPtr->Poly1stPt;

	/* If this texture is animated the UV array must be calculated */
	if (polyPtr->PolyFlags & iflag_txanim) {
		/* Create the UV array */
		CreateTxAnimUVArray(texture_defn_ptr, uv_array, (int *)polyPtr);
		texture_defn_ptr = uv_array;

		do {
			VECTORCH *vertexPtr = &(RotatedPts[*VertexNumberPtr]);
			renderVerticesPtr->X = vertexPtr->vx;
			renderVerticesPtr->Y = vertexPtr->vy;
			renderVerticesPtr->Z = vertexPtr->vz;
			renderVerticesPtr->U = texture_defn_ptr[0];
			renderVerticesPtr->V = texture_defn_ptr[1];
			VertexIntensity(renderVerticesPtr);
			{
				VECTORCH mag;
				int alpha;
				mag.vx = vertexPtr->vx - Global_ODB_Ptr->ObView.vx;
				mag.vy = vertexPtr->vy - Global_ODB_Ptr->ObView.vy;
				mag.vz = vertexPtr->vz - Global_ODB_Ptr->ObView.vz;

				if (mag.vx < 0) {
					mag.vx = -mag.vx;
				}

				if (mag.vy < 0) {
					mag.vy = -mag.vy;
				}

				if (mag.vz < 0) {
					mag.vz = -mag.vz;
				}

				alpha = GetSin(((mag.vx + mag.vy + mag.vz) * 3 + CloakingPhase) & 4095);
				renderVerticesPtr->A = MUL_FIXED(alpha, alpha) >> 10;

				if (renderVerticesPtr->A == 255) {
					renderVerticesPtr->R = 255;
					renderVerticesPtr->G = 255;
					renderVerticesPtr->B = 255;
				}
			}
			renderVerticesPtr++;
			VertexNumberPtr++;
			texture_defn_ptr += 2;
		} while (--i);
	} else {
		do {
			VECTORCH *vertexPtr = &(RotatedPts[*VertexNumberPtr]);
			renderVerticesPtr->X = vertexPtr->vx;
			renderVerticesPtr->Y = vertexPtr->vy;
			renderVerticesPtr->Z = vertexPtr->vz;
			renderVerticesPtr->U = texture_defn_ptr[0];// << 16;
			renderVerticesPtr->V = texture_defn_ptr[1];// << 16;
			VertexIntensity(renderVerticesPtr);
			{
				VECTORCH mag;
				int alpha;
				mag.vx = vertexPtr->vx - ObjectCentre.vx;
				//mag.vy = vertexPtr->vy - MUL_FIXED(ObjectCentre.vy,87381);
				mag.vy = vertexPtr->vy - ObjectCentre.vy;
				mag.vz = vertexPtr->vz - ObjectCentre.vz;

				if (mag.vx < 0) {
					mag.vx = -mag.vx;
				}

				if (mag.vy < 0) {
					mag.vy = -mag.vy;
				}

				if (mag.vz < 0) {
					mag.vz = -mag.vz;
				}

				alpha = GetSin(((mag.vx + mag.vy + mag.vz) * 8 + CloakingPhase) & 4095);
				alpha = MUL_FIXED(alpha, alpha);

				if (alpha > CloakingMode) {
					alpha = CloakingMode;
				}

				alpha /= 256;

				if (alpha > 255) {
					alpha = 255;
				}

				renderVerticesPtr->A = alpha;

				if (CloakingMode > ONE_FIXED) {
					alpha = GetSin(((mag.vx + mag.vy + mag.vz) + CloakingPhase) & 4095);
					alpha = MUL_FIXED(alpha, alpha) >> 8;

					if (alpha == 255) {
						renderVerticesPtr->A = 255;
						renderVerticesPtr->G = 128;
						renderVerticesPtr->B = 255;
					}
				}
			}
			renderVerticesPtr++;
			VertexNumberPtr++;
			texture_defn_ptr += 2;
		} while (--i);
	}
}

static void PredatorThermalVisionPolygon_Construct(POLYHEADER *polyPtr)
{
	RENDERVERTEX *renderVerticesPtr = VerticesBuffer;
	int i = RenderPolygon.NumberOfVertices;
	VertexNumberPtr = &polyPtr->Poly1stPt;

	do {
		VECTORCH *vertexPtr = &(RotatedPts[*VertexNumberPtr]);
		renderVerticesPtr->X = vertexPtr->vx;
		renderVerticesPtr->Y = vertexPtr->vy;
		renderVerticesPtr->Z = vertexPtr->vz;
		{
			int alpha;

			if (Global_ODB_Ptr->SpecialFXFlags & SFXFLAG_ISAFFECTEDBYHEAT) {
				int distanceFromHeatSource = 100000;
				int sourceNumber = NumberOfHeatSources;

				while (sourceNumber--) {
					VECTORCH mag;
					mag.vx = vertexPtr->vx - HeatSourceList[sourceNumber].Position.vx;
					mag.vy = vertexPtr->vy - HeatSourceList[sourceNumber].Position.vy;
					mag.vz = vertexPtr->vz - HeatSourceList[sourceNumber].Position.vz;
					int m = Approximate3dMagnitude(&mag) * 64;

					if (m < distanceFromHeatSource) {
						distanceFromHeatSource = m;
					}
				}

				alpha = distanceFromHeatSource + (GetSin(CloakingPhase & 4095) >> 3);

				if (alpha > 65536) {
					alpha = 65536;
				}
			} else {
				alpha = 65536;
			}

			{
				int brightness = MUL_FIXED(MUL_FIXED(alpha, alpha), 1275);

				if (brightness < 256) {
					renderVerticesPtr->R = 255;
					renderVerticesPtr->G = brightness;
					renderVerticesPtr->B = 0;
				} else if (brightness < 255 + 256) {
					int b = brightness - 255;
					renderVerticesPtr->R = (255 - b);
					renderVerticesPtr->G = 255;
					renderVerticesPtr->B = 0;
				} else if (brightness < 255 * 2 + 256) {
					int b = brightness - 255 * 2;
					renderVerticesPtr->R = 0;
					renderVerticesPtr->G = 255;
					renderVerticesPtr->B = b;
				} else if (brightness < 255 * 3 + 256) {
					int b = brightness - 255 * 3;
					renderVerticesPtr->R = 0;
					renderVerticesPtr->G = 255 - b;
					renderVerticesPtr->B = 255;
				} else {
					int b = brightness - 255 * 4;
					renderVerticesPtr->R = 0;
					renderVerticesPtr->G = 0;
					renderVerticesPtr->B = 255 - b / 2;
				}
			}
		}
		renderVerticesPtr++;
		VertexNumberPtr++;
	} while (--i);
}

static void PredatorSeeAliensVisionPolygon_Construct(POLYHEADER *polyPtr)
{
	int *texture_defn_ptr;
	RENDERVERTEX *renderVerticesPtr = VerticesBuffer;
	int i = RenderPolygon.NumberOfVertices;
	int alpha;
	VertexNumberPtr = &polyPtr->Poly1stPt;
	int texture_defn_index = (polyPtr->PolyColour >> TxDefn);
	texture_defn_ptr = Global_ShapeTextures[texture_defn_index];
	int uv_array[maxpolypts * 2];

	/* get ptr to uv coords for this polygon */
	if (polyPtr->PolyFlags & iflag_txanim) {
		/* Create the UV array */
		CreateTxAnimUVArray(texture_defn_ptr, uv_array, (int *)polyPtr);
		texture_defn_ptr = uv_array;
	}

	if ((Global_ODB_Ptr->SpecialFXFlags & SFXFLAG_MELTINGINTOGROUND)
	    && (Global_ODB_Ptr->ObFlags2 < ONE_FIXED)) {
		alpha = Global_ODB_Ptr->ObFlags2 >> 8;
		RenderPolygon.TranslucencyMode = TRANSLUCENCY_NORMAL;
	} else {
		alpha = 255;
		RenderPolygon.TranslucencyMode = TRANSLUCENCY_OFF;
	}

	do {
		VECTORCH *vertexPtr = &(RotatedPts[*VertexNumberPtr]);

		if (polyPtr->PolyFlags & iflag_txanim) {
			renderVerticesPtr->U = texture_defn_ptr[0];
			renderVerticesPtr->V = texture_defn_ptr[1];
		} else {
			renderVerticesPtr->U = texture_defn_ptr[0];// << 16;
			renderVerticesPtr->V = texture_defn_ptr[1];// << 16;
		}

		renderVerticesPtr->X = vertexPtr->vx;
		renderVerticesPtr->Y = vertexPtr->vy;
		renderVerticesPtr->Z = vertexPtr->vz;
		{
			VECTORCH mag = RotatedPts[*VertexNumberPtr];//*(((VECTORCH *)Global_ShapeVNormals) + *VertexNumberPtr);
			int colour;
			mag.vx = vertexPtr->vx - Global_ODB_Ptr->ObView.vx;
			mag.vy = vertexPtr->vy - Global_ODB_Ptr->ObView.vy;
			mag.vz = vertexPtr->vz - Global_ODB_Ptr->ObView.vz;
			colour = GetSin(((mag.vx + mag.vy + mag.vz) * 8 + CloakingPhase) & 4095);
			colour = MUL_FIXED(colour, colour);
			renderVerticesPtr->B = MUL_FIXED(colour, 255);
			renderVerticesPtr->R = renderVerticesPtr->B / 2;
			renderVerticesPtr->G = renderVerticesPtr->B / 2;
			colour = MUL_FIXED(colour, colour);
			colour = MUL_FIXED(colour, colour);
			renderVerticesPtr->SpecularR = colour / 1024;
			renderVerticesPtr->SpecularG = colour / 1024;
			renderVerticesPtr->SpecularB = colour / 1024;
			renderVerticesPtr->A = alpha;
		}
		texture_defn_ptr += 2;
		renderVerticesPtr++;
		VertexNumberPtr++;
	} while (--i);
}

/* GOURAUD POLYGONS */
static void GouraudPolygon_Construct(POLYHEADER *polyPtr)
{
	RENDERVERTEX *renderVerticesPtr = VerticesBuffer;
	int i = RenderPolygon.NumberOfVertices;
	VertexNumberPtr = &polyPtr->Poly1stPt;
	RenderPolygon.TranslucencyMode = TRANSLUCENCY_OFF;

	do {
		renderVerticesPtr->X = RotatedPts[*VertexNumberPtr].vx;
		renderVerticesPtr->Y = RotatedPts[*VertexNumberPtr].vy;
		renderVerticesPtr->Z = RotatedPts[*VertexNumberPtr].vz;

		VertexIntensity(renderVerticesPtr);

		int colour = (renderVerticesPtr->B + renderVerticesPtr->R + renderVerticesPtr->G) / 3;
		renderVerticesPtr->R = colour;
		renderVerticesPtr->G = colour;
		renderVerticesPtr->B = 0;
		renderVerticesPtr++;
		VertexNumberPtr++;
	} while (--i);
}

/* GOURAUD TEXTURED POLYGONS */
static void GouraudTexturedPolygon_Construct(POLYHEADER *polyPtr)
{
	int *texture_defn_ptr;
	RENDERVERTEX *renderVerticesPtr = VerticesBuffer;
	int i = RenderPolygon.NumberOfVertices;
	/* get ptr to uv coords for this polygon */
	int texture_defn_index = (polyPtr->PolyColour >> TxDefn);
	texture_defn_ptr = Global_ShapeTextures[texture_defn_index];
	int uv_array[maxpolypts * 2];
	VertexNumberPtr = &polyPtr->Poly1stPt;

	/* If this texture is animated the UV array must be calculated */
	if (polyPtr->PolyFlags & iflag_txanim) {
		/* Create the UV array */
		CreateTxAnimUVArray(texture_defn_ptr, uv_array, (int *)polyPtr);
		texture_defn_ptr = uv_array;

		do {
			VECTORCH *vertexPtr = &(RotatedPts[*VertexNumberPtr]);

			if (TRIPTASTIC_CHEATMODE) {
				renderVerticesPtr->X = vertexPtr->vx + GetSin((CloakingPhase * 2    + vertexPtr->vz) & 4095) / 1024;
				renderVerticesPtr->Y = vertexPtr->vy + GetSin((CloakingPhase - 3000 + vertexPtr->vx) & 4095) / 1024;
				renderVerticesPtr->Z = vertexPtr->vz + GetSin((CloakingPhase * 3 + 239 + vertexPtr->vy) & 4095) / 1024;
			} else if (UNDERWATER_CHEATMODE) {
				renderVerticesPtr->X = vertexPtr->vx + (GetSin((CloakingPhase / 2   + vertexPtr->vz) & 4095)) / 1024;
				renderVerticesPtr->Y = vertexPtr->vy + (GetSin((CloakingPhase - 3000 + vertexPtr->vx) & 4095)) / 1024;
				renderVerticesPtr->Z = vertexPtr->vz + (GetSin((CloakingPhase / 3 + 239 + vertexPtr->vy) & 4095)) / 1024;
			} else {
				renderVerticesPtr->X = vertexPtr->vx;
				renderVerticesPtr->Y = vertexPtr->vy;
				renderVerticesPtr->Z = vertexPtr->vz;
			}

			renderVerticesPtr->U = texture_defn_ptr[0];
			renderVerticesPtr->V = texture_defn_ptr[1];

			if ((Global_ODB_Ptr->SpecialFXFlags & SFXFLAG_MELTINGINTOGROUND)
			    && (Global_ODB_Ptr->ObFlags2 < ONE_FIXED)) {
				renderVerticesPtr->A = Global_ODB_Ptr->ObFlags2 >> 8;
				RenderPolygon.TranslucencyMode = TRANSLUCENCY_NORMAL;
			} else if (polyPtr->PolyFlags & iflag_transparent) {
				renderVerticesPtr->A = 128;
				RenderPolygon.TranslucencyMode = TRANSLUCENCY_NORMAL;
			} else {
				if (TRIPTASTIC_CHEATMODE) {
					renderVerticesPtr->A = TripTasticPhase;
				} else if (MOTIONBLUR_CHEATMODE) {
					renderVerticesPtr->A = 128;
				} else {
					renderVerticesPtr->A = 255;
				}

				RenderPolygon.TranslucencyMode = TRANSLUCENCY_OFF;
			}

			if (polyPtr->PolyFlags & iflag_nolight) {
				switch (CurrentVisionMode) {
					default:
					case VISION_MODE_NORMAL: {
						renderVerticesPtr->R = 255;
						renderVerticesPtr->G = 255;
						renderVerticesPtr->B = 255;
						renderVerticesPtr->SpecularR = 0;
						renderVerticesPtr->SpecularG = 0;
						renderVerticesPtr->SpecularB = 0;
						break;
					}

					case VISION_MODE_IMAGEINTENSIFIER: {
						renderVerticesPtr->R = 0;
						renderVerticesPtr->G = 255;
						renderVerticesPtr->B = 0;
						renderVerticesPtr->SpecularR = 0;
						renderVerticesPtr->SpecularG = 0;
						renderVerticesPtr->SpecularB = 0;
						break;
					}

					case VISION_MODE_PRED_THERMAL: {
						renderVerticesPtr->R = 0;
						renderVerticesPtr->G = 0;
						renderVerticesPtr->B = 255;
						renderVerticesPtr->SpecularR = 0;
						renderVerticesPtr->SpecularG = 0;
						renderVerticesPtr->SpecularB = 0;
						break;
					}

					case VISION_MODE_PRED_SEEALIENS: {
						renderVerticesPtr->R = 255;
						renderVerticesPtr->G = 0;
						renderVerticesPtr->B = 0;
						renderVerticesPtr->SpecularR = 0;
						renderVerticesPtr->SpecularG = 0;
						renderVerticesPtr->SpecularB = 0;
						break;
					}

					case VISION_MODE_PRED_SEEPREDTECH: {
						renderVerticesPtr->R = 0;
						renderVerticesPtr->G = 255;
						renderVerticesPtr->B = 0;
						renderVerticesPtr->SpecularR = 255;
						renderVerticesPtr->SpecularG = 0;
						renderVerticesPtr->SpecularB = 255;
						break;
					}
				}
			} else {
				VertexIntensity(renderVerticesPtr);
			}

			renderVerticesPtr++;
			VertexNumberPtr++;
			texture_defn_ptr += 2;
		} while (--i);
	} else {
		do {
			VECTORCH *vertexPtr = &(RotatedPts[*VertexNumberPtr]);
#if UNDERWATER
			renderVerticesPtr->X = vertexPtr->vx + GetSin((CloakingPhase * 2    + vertexPtr->vz) & 4095) / 1024;
			renderVerticesPtr->Y = vertexPtr->vy + GetSin((CloakingPhase - 3000 + vertexPtr->vx) & 4095) / 1024;
			renderVerticesPtr->Z = vertexPtr->vz + GetSin((CloakingPhase * 3 + 239 + vertexPtr->vy) & 4095) / 1024;
#elif SPATIAL_SHOCKWAVE
			{
				int d = Magnitude(vertexPtr);
				int a = (CloakingPhase & 16383) + 4000;
				int u = d - a;
				int offset;

				if (u > 0 && u < 8192) {
					VECTORCH n = *vertexPtr;
					Normalise(&n);
					u <<= 3;
					offset = MUL_FIXED(MUL_FIXED(2 * u, ONE_FIXED - u), 8000) + MUL_FIXED(MUL_FIXED(u, u), 8192);
					LOCALASSERT(offset >= 0 && offset <= 8192);
					renderVerticesPtr->X = MUL_FIXED(n.vx, d); //a+offset*2);
					renderVerticesPtr->Y = MUL_FIXED(n.vy, d); //a+offset*2);
					renderVerticesPtr->Z = MUL_FIXED(n.vz, a + offset);
				} else {
					renderVerticesPtr->X = vertexPtr->vx;
					renderVerticesPtr->Y = vertexPtr->vy;
					renderVerticesPtr->Z = vertexPtr->vz;
				}
			}
#else

			if (TRIPTASTIC_CHEATMODE) {
				renderVerticesPtr->X = vertexPtr->vx + GetSin((CloakingPhase * 2    + vertexPtr->vz) & 4095) / 1024;
				renderVerticesPtr->Y = vertexPtr->vy + GetSin((CloakingPhase - 3000 + vertexPtr->vx) & 4095) / 1024;
				renderVerticesPtr->Z = vertexPtr->vz + GetSin((CloakingPhase * 3 + 239 + vertexPtr->vy) & 4095) / 1024;
			} else if (UNDERWATER_CHEATMODE) {
				renderVerticesPtr->X = vertexPtr->vx + (GetSin((CloakingPhase / 2   + vertexPtr->vz) & 4095)) / 1024;
				renderVerticesPtr->Y = vertexPtr->vy + (GetSin((CloakingPhase - 3000    + vertexPtr->vx) & 4095)) / 1024;
				renderVerticesPtr->Z = vertexPtr->vz + (GetSin((CloakingPhase / 3 + 239 + vertexPtr->vy) & 4095)) / 1024;
			} else {
				renderVerticesPtr->X = vertexPtr->vx;
				renderVerticesPtr->Y = vertexPtr->vy;
				renderVerticesPtr->Z = vertexPtr->vz;
			}

#endif
			renderVerticesPtr->U = texture_defn_ptr[0];// << 16;
			renderVerticesPtr->V = texture_defn_ptr[1];// << 16;

			if ((Global_ODB_Ptr->SpecialFXFlags & SFXFLAG_MELTINGINTOGROUND)
			    && (Global_ODB_Ptr->ObFlags2 < ONE_FIXED)) {
				renderVerticesPtr->A = Global_ODB_Ptr->ObFlags2 >> 8;
				RenderPolygon.TranslucencyMode = TRANSLUCENCY_NORMAL;
			} else if (polyPtr->PolyFlags & iflag_transparent) {
				renderVerticesPtr->A = 128;
				RenderPolygon.TranslucencyMode = TRANSLUCENCY_NORMAL;
			} else {
				if (TRIPTASTIC_CHEATMODE) {
					renderVerticesPtr->A = TripTasticPhase;
				} else if (MOTIONBLUR_CHEATMODE) {
					renderVerticesPtr->A = 128;
				} else {
					renderVerticesPtr->A = 255;
				}

				RenderPolygon.TranslucencyMode = TRANSLUCENCY_OFF;
			}

			if (polyPtr->PolyFlags & iflag_nolight) {
				switch (CurrentVisionMode) {
					default:
					case VISION_MODE_NORMAL: {
						renderVerticesPtr->R = 255;
						renderVerticesPtr->G = 255;
						renderVerticesPtr->B = 255;
						renderVerticesPtr->SpecularR = 0;
						renderVerticesPtr->SpecularG = 0;
						renderVerticesPtr->SpecularB = 0;
						break;
					}

					case VISION_MODE_IMAGEINTENSIFIER: {
						renderVerticesPtr->R = 0;
						renderVerticesPtr->G = 255;
						renderVerticesPtr->B = 0;
						renderVerticesPtr->SpecularR = 0;
						renderVerticesPtr->SpecularG = 0;
						renderVerticesPtr->SpecularB = 0;
						break;
					}

					case VISION_MODE_PRED_THERMAL: {
						renderVerticesPtr->R = 0;
						renderVerticesPtr->G = 0;
						renderVerticesPtr->B = 255;
						renderVerticesPtr->SpecularR = 0;
						renderVerticesPtr->SpecularG = 0;
						renderVerticesPtr->SpecularB = 0;
						break;
					}

					case VISION_MODE_PRED_SEEALIENS: {
						renderVerticesPtr->R = 255;
						renderVerticesPtr->G = 0;
						renderVerticesPtr->B = 0;
						renderVerticesPtr->SpecularR = 0;
						renderVerticesPtr->SpecularG = 0;
						renderVerticesPtr->SpecularB = 0;
						break;
					}

					case VISION_MODE_PRED_SEEPREDTECH: {
						renderVerticesPtr->R = 0;
						renderVerticesPtr->G = 255;
						renderVerticesPtr->B = 0;
						renderVerticesPtr->SpecularR = 255;
						renderVerticesPtr->SpecularG = 0;
						renderVerticesPtr->SpecularB = 255;
						break;
					}
				}
			} else {
				VertexIntensity(renderVerticesPtr);
			}

			renderVerticesPtr++;
			VertexNumberPtr++;
			texture_defn_ptr += 2;
		} while (--i);
	}
}

static void VertexIntensity_Pred_Thermal(RENDERVERTEX *renderVertexPtr)
{
	int redI, blueI, specular = 0;
	int vertexNumber = *VertexNumberPtr;

	if (ColourIntensityArray[vertexNumber].Stamp == ObjectCounter) {
		renderVertexPtr->R = ColourIntensityArray[vertexNumber].R;
		renderVertexPtr->G = ColourIntensityArray[vertexNumber].G;
		renderVertexPtr->B = ColourIntensityArray[vertexNumber].B;
		renderVertexPtr->SpecularR = ColourIntensityArray[vertexNumber].SpecularR;
		renderVertexPtr->SpecularG = ColourIntensityArray[vertexNumber].SpecularG;
		renderVertexPtr->SpecularB = ColourIntensityArray[vertexNumber].SpecularB;
		return;
	}

	{
		VECTORCH *vertexNormalPtr = ((VECTORCH *)Global_ShapeVNormals) + vertexNumber;
		VECTORCH *vertexPtr = ((VECTORCH *)Global_ShapePoints) + vertexNumber;
		LIGHTBLOCK **larrayptr;
		LIGHTBLOCK *lptr;
		redI = 0;
		larrayptr = LightSourcesForObject;

		for (int i = NumLightSourcesForObject; i != 0; i--) {
			VECTORCH vertexToLight;
			int distanceToLight;
			lptr = *larrayptr++;

			if (lptr->LightFlags & LFlag_PreLitSource) {
				continue;
			}

			vertexToLight.vx = lptr->LocalLP.vx - vertexPtr->vx;
			vertexToLight.vy = lptr->LocalLP.vy - vertexPtr->vy;
			vertexToLight.vz = lptr->LocalLP.vz - vertexPtr->vz;
			distanceToLight = Approximate3dMagnitude(&vertexToLight);

			if (distanceToLight < lptr->LightRange) {
				int idot = MUL_FIXED(lptr->LightRange - distanceToLight, lptr->BrightnessOverRange);

				if ((distanceToLight > 0) && (!(Global_ODB_Ptr->ObFlags3 & ObFlag3_NoLightDot))) {
					int dotproduct = MUL_FIXED(vertexNormalPtr->vx, vertexToLight.vx)
					                 + MUL_FIXED(vertexNormalPtr->vy, vertexToLight.vy)
					                 + MUL_FIXED(vertexNormalPtr->vz, vertexToLight.vz);

					if (dotproduct > 0) {
						idot = WideMulNarrowDiv(idot, dotproduct, distanceToLight);
					} else {
						idot = 0;
					}

					idot = WideMulNarrowDiv(idot, dotproduct, distanceToLight);
				}

				redI += idot;

				if (lptr->LightFlags & LFlag_Thermal) {
					specular += idot;
				}
			}
		}
	}

	blueI = ONE_FIXED / 2;

	if (renderVertexPtr->Z > 5000) {
		int a = (renderVertexPtr->Z - 5000);

		if (a > 4096) {
			blueI = (blueI * 80192) / a;
		}
	}

	blueI >>= 8;

	if (redI >= ONE_FIXED) {
		redI = (ONE_FIXED - 1);
	}

	redI >>= 8;
	specular >>= 6;

	if (specular >= 255) {
		specular = 255;
	}

	/* KJL 12:41:54 05/10/98 - red/green swapped, whilst testing colours */
	renderVertexPtr->R = 0;
	ColourIntensityArray[vertexNumber].R = 0;
	renderVertexPtr->B = blueI;
	ColourIntensityArray[vertexNumber].B = blueI;
	renderVertexPtr->G = redI / 2;
	ColourIntensityArray[vertexNumber].G = redI / 2;
	renderVertexPtr->SpecularR = specular;//specularR;
	ColourIntensityArray[vertexNumber].SpecularR = specular;//specularR;
	renderVertexPtr->SpecularG = specular;
	ColourIntensityArray[vertexNumber].SpecularG = specular;
	renderVertexPtr->SpecularB = specular;//specularB;
	ColourIntensityArray[vertexNumber].SpecularB = specular;//specularB;
	ColourIntensityArray[vertexNumber].Stamp = ObjectCounter;
}

static void VertexIntensity_Pred_SeeAliens(RENDERVERTEX *renderVertexPtr)
{
	int redI, blueI, specular = 0;
	int vertexNumber = *VertexNumberPtr;

	if (ColourIntensityArray[vertexNumber].Stamp == ObjectCounter) {
		renderVertexPtr->R = ColourIntensityArray[vertexNumber].R;
		renderVertexPtr->G = ColourIntensityArray[vertexNumber].G;
		renderVertexPtr->B = ColourIntensityArray[vertexNumber].B;
		renderVertexPtr->SpecularR = ColourIntensityArray[vertexNumber].SpecularR;
		renderVertexPtr->SpecularG = ColourIntensityArray[vertexNumber].SpecularG;
		renderVertexPtr->SpecularB = ColourIntensityArray[vertexNumber].SpecularB;
		return;
	}

	ColourIntensityArray[vertexNumber].Stamp = ObjectCounter;
	{
		VECTORCH *vertexPtr = ((VECTORCH *)Global_ShapePoints) + vertexNumber;
		LIGHTBLOCK **larrayptr;
		LIGHTBLOCK *lptr;
		redI = 0;
		larrayptr = LightSourcesForObject;

		for (int i = NumLightSourcesForObject; i != 0; i--) {
			VECTORCH vertexToLight;
			int distanceToLight;
			lptr = *larrayptr++;

			if (lptr->LightFlags & LFlag_PreLitSource) {
				continue;
			}

			vertexToLight.vx = lptr->LocalLP.vx - vertexPtr->vx;
			vertexToLight.vy = lptr->LocalLP.vy - vertexPtr->vy;
			vertexToLight.vz = lptr->LocalLP.vz - vertexPtr->vz;
			distanceToLight = Approximate3dMagnitude(&vertexToLight);

			if (distanceToLight < lptr->LightRange) {
				int idot = MUL_FIXED(lptr->LightRange - distanceToLight, lptr->BrightnessOverRange);
				redI += idot;

				if (lptr->LightFlags & LFlag_Electrical) {
					specular += idot;
				}
			}
		}
	}
	redI >>= 11;

	if (redI > 255) {
		redI = 255;
	}

	renderVertexPtr->G = redI;
	ColourIntensityArray[vertexNumber].G = redI;
	blueI = ONE_FIXED / 2;

	if (renderVertexPtr->Z > 5000) {
		int a = (renderVertexPtr->Z - 5000);

		if (a > 4096) {
			blueI = (blueI * 4096) / a;
		}
	}

	/* KJL 12:41:54 05/10/98 - red/green swapped, whilst testing colours */
	blueI >>= 9;
	renderVertexPtr->R = blueI;
	ColourIntensityArray[vertexNumber].R = blueI;
	renderVertexPtr->B = 0;
	ColourIntensityArray[vertexNumber].B = 0;
	specular >>= 10;

	if (specular > 255) {
		specular = 255;
	}

	renderVertexPtr->SpecularR = specular;//specularR;
	ColourIntensityArray[vertexNumber].SpecularR = specular;//specularR;
	renderVertexPtr->SpecularG = specular;
	ColourIntensityArray[vertexNumber].SpecularG = specular;
	renderVertexPtr->SpecularB = specular;//specularB;
	ColourIntensityArray[vertexNumber].SpecularB = specular;//specularB;
}

static void VertexIntensity_Pred_SeePredatorTech(RENDERVERTEX *renderVertexPtr)
{
	int redI, blueI;
	int vertexNumber = *VertexNumberPtr;

	if (ColourIntensityArray[vertexNumber].Stamp == ObjectCounter) {
		renderVertexPtr->R = ColourIntensityArray[vertexNumber].R;
		renderVertexPtr->G = ColourIntensityArray[vertexNumber].G;
		renderVertexPtr->B = ColourIntensityArray[vertexNumber].B;
		renderVertexPtr->SpecularR = ColourIntensityArray[vertexNumber].SpecularR;
		renderVertexPtr->SpecularG = ColourIntensityArray[vertexNumber].SpecularG;
		renderVertexPtr->SpecularB = ColourIntensityArray[vertexNumber].SpecularB;
		return;
	}

	ColourIntensityArray[vertexNumber].Stamp = ObjectCounter;
	{
		VECTORCH *vertexPtr = ((VECTORCH *)Global_ShapePoints) + vertexNumber;
		LIGHTBLOCK **larrayptr;
		LIGHTBLOCK *lptr;
		redI = 0;
		larrayptr = LightSourcesForObject;

		for (int i = NumLightSourcesForObject; i != 0; i--) {
			VECTORCH vertexToLight;
			int distanceToLight;
			lptr = *larrayptr++;

			if (lptr->LightFlags & LFlag_PreLitSource) {
				continue;
			}

			vertexToLight.vx = lptr->LocalLP.vx - vertexPtr->vx;
			vertexToLight.vy = lptr->LocalLP.vy - vertexPtr->vy;
			vertexToLight.vz = lptr->LocalLP.vz - vertexPtr->vz;
			distanceToLight = Approximate3dMagnitude(&vertexToLight);

			if (distanceToLight < lptr->LightRange) {
				int idot = MUL_FIXED(lptr->LightRange - distanceToLight, lptr->BrightnessOverRange);
				redI += idot;
			}
		}
	}
	blueI = ONE_FIXED - 1;

	if (renderVertexPtr->Z > 5000) {
		int a = (renderVertexPtr->Z - 5000);

		if (a > 4096) {
			blueI = (blueI * 4096) / a;
		}
	}

	blueI >>= 8;
	redI >>= 9;

	if (redI > 255) {
		redI = 255;
	}

	/* KJL 12:41:54 05/10/98 - red/green swapped, whilst testing colours */
	renderVertexPtr->R = 255;
	ColourIntensityArray[vertexNumber].R = 255;
	renderVertexPtr->B = 255;
	ColourIntensityArray[vertexNumber].B = 255;
	renderVertexPtr->G = blueI;
	ColourIntensityArray[vertexNumber].G = blueI;
	renderVertexPtr->SpecularR = 255;//specularR;
	ColourIntensityArray[vertexNumber].SpecularR = 255;//specularR;
	renderVertexPtr->SpecularG = redI;
	ColourIntensityArray[vertexNumber].SpecularG = redI;
	renderVertexPtr->SpecularB = 255;//specularB;
	ColourIntensityArray[vertexNumber].SpecularB = 255;//specularB;
}

static void VertexIntensity_ImageIntensifier(RENDERVERTEX *renderVertexPtr)
{
	int greenI;
	int specular;
	int vertexNumber = *VertexNumberPtr;

	if (ColourIntensityArray[vertexNumber].Stamp == ObjectCounter) {
		renderVertexPtr->R = ColourIntensityArray[vertexNumber].R;
		renderVertexPtr->G = ColourIntensityArray[vertexNumber].G;
		renderVertexPtr->B = ColourIntensityArray[vertexNumber].B;
		renderVertexPtr->SpecularR = ColourIntensityArray[vertexNumber].SpecularR;
		renderVertexPtr->SpecularG = ColourIntensityArray[vertexNumber].SpecularG;
		renderVertexPtr->SpecularB = ColourIntensityArray[vertexNumber].SpecularB;
		return;
	}

	ColourIntensityArray[vertexNumber].Stamp = ObjectCounter;
	{
		VECTORCH *vertexNormalPtr = ((VECTORCH *)Global_ShapeVNormals) + vertexNumber;
		VECTORCH *vertexPtr = ((VECTORCH *)Global_ShapePoints) + vertexNumber;
		LIGHTBLOCK **larrayptr;
		LIGHTBLOCK *lptr;
		int i;
		greenI = 0;
		specular = 0;
		larrayptr = LightSourcesForObject;

		for (i = NumLightSourcesForObject; i != 0; i--) {
			VECTORCH vertexToLight;
			int distanceToLight;
			lptr = *larrayptr++;
			vertexToLight.vx = lptr->LocalLP.vx - vertexPtr->vx;
			vertexToLight.vy = lptr->LocalLP.vy - vertexPtr->vy;
			vertexToLight.vz = lptr->LocalLP.vz - vertexPtr->vz;
			{
				int dx, dy, dz;
				dx = vertexToLight.vx;

				if (dx < 0) {
					dx = -dx;
				}

				dy = vertexToLight.vy;

				if (dy < 0) {
					dy = -dy;
				}

				dz = vertexToLight.vz;

				if (dz < 0) {
					dz = -dz;
				}

				if (dx > dy) {
					if (dx > dz) {
						distanceToLight = dx + ((dy + dz) >> 2);
					} else {
						distanceToLight = dz + ((dy + dx) >> 2);
					}
				} else {
					if (dy > dz) {
						distanceToLight = dy + ((dx + dz) >> 2);
					} else {
						distanceToLight = dz + ((dx + dy) >> 2);
					}
				}
			}

			if (distanceToLight < lptr->LightRange) {
				int idot = MUL_FIXED(lptr->LightRange - distanceToLight, lptr->BrightnessOverRange);

				if (distanceToLight > 0) {
					int dotproduct = MUL_FIXED(vertexNormalPtr->vx, vertexToLight.vx)
					                 + MUL_FIXED(vertexNormalPtr->vy, vertexToLight.vy)
					                 + MUL_FIXED(vertexNormalPtr->vz, vertexToLight.vz);

					if (dotproduct > 0) {
						idot = (WideMulNarrowDiv(idot, dotproduct, distanceToLight) + idot / 4);
					} else {
						idot /= 4;
					}
				}

				if (idot < 0) {
					LOCALASSERT(idot >= 0);
				}

				specular += idot;
			}
		}
	}
	greenI = 255;

	if (renderVertexPtr->Z > 5000) {
		int a = (renderVertexPtr->Z - 5000);

		if (a > 4096) {
			greenI = (greenI * 4096) / a;
		}
	}

	renderVertexPtr->G = greenI;
	ColourIntensityArray[vertexNumber].G = greenI;
	renderVertexPtr->R = 0;
	ColourIntensityArray[vertexNumber].R = 0;
	renderVertexPtr->B = 0;
	ColourIntensityArray[vertexNumber].B = 0;
	specular >>= 7;

	if (specular > 254) {
		specular = 254;
	}

	LOCALASSERT(specular >= 0 && specular <= 254);
	renderVertexPtr->SpecularR = specular;
	ColourIntensityArray[vertexNumber].SpecularR = specular;
	renderVertexPtr->SpecularG = specular;
	ColourIntensityArray[vertexNumber].SpecularG = specular;
	renderVertexPtr->SpecularB = specular;
	ColourIntensityArray[vertexNumber].SpecularB = specular;
}

static void VertexIntensity_Alien_Sense(RENDERVERTEX *renderVertexPtr)
{
	int intensity;
	int vertexNumber = *VertexNumberPtr;

	if (ColourIntensityArray[vertexNumber].Stamp == ObjectCounter) {
		renderVertexPtr->R = ColourIntensityArray[vertexNumber].R;
		renderVertexPtr->G = ColourIntensityArray[vertexNumber].G;
		renderVertexPtr->B = ColourIntensityArray[vertexNumber].B;
		renderVertexPtr->SpecularR = 0;
		renderVertexPtr->SpecularG = 0;
		renderVertexPtr->SpecularB = 0;
		return;
	}

	ColourIntensityArray[vertexNumber].Stamp = ObjectCounter;
	intensity = 255;

	if (renderVertexPtr->Z > 5000) {
		int a = (renderVertexPtr->Z - 5000);

		if (a > 1024) {
			intensity = (intensity * 1024) / a;
		}
	}

	renderVertexPtr->R = intensity;
	ColourIntensityArray[vertexNumber].R = intensity;
	renderVertexPtr->G = intensity;
	ColourIntensityArray[vertexNumber].G = intensity;
	renderVertexPtr->B = intensity;
	ColourIntensityArray[vertexNumber].B = intensity;
	renderVertexPtr->SpecularR = 0;
	renderVertexPtr->SpecularG = 0;
	renderVertexPtr->SpecularB = 0;
}

static void VertexIntensity_Standard_Opt(RENDERVERTEX *renderVertexPtr)
{
	int redI, greenI, blueI;
	int specularR, specularG, specularB;
	int vertexNumber = *VertexNumberPtr;

	if (ColourIntensityArray[vertexNumber].Stamp == ObjectCounter) {
		renderVertexPtr->R = ColourIntensityArray[vertexNumber].R;
		renderVertexPtr->G = ColourIntensityArray[vertexNumber].G;
		renderVertexPtr->B = ColourIntensityArray[vertexNumber].B;
		renderVertexPtr->SpecularR = ColourIntensityArray[vertexNumber].SpecularR;
		renderVertexPtr->SpecularG = ColourIntensityArray[vertexNumber].SpecularG;
		renderVertexPtr->SpecularB = ColourIntensityArray[vertexNumber].SpecularB;
		return;
	}

	ColourIntensityArray[vertexNumber].Stamp = ObjectCounter;
	{
		VECTORCH *vertexNormalPtr = ((VECTORCH *)Global_ShapeVNormals) + vertexNumber;
		VECTORCH *vertexPtr = ((VECTORCH *)Global_ShapePoints) + vertexNumber;
		LIGHTBLOCK **larrayptr;
		LIGHTBLOCK *lptr;
		int i;

		if (Global_ShapeHeaderPtr->shapeflags & ShapeFlag_PreLit) {
			unsigned int packedI = Global_EID_IPtr[vertexNumber];
			blueI = (packedI & 255) * 257;
			packedI >>= 8;
			greenI = (packedI & 255) * 257;
			packedI >>= 8;
			redI = (packedI & 255) * 257;
		} else {
			redI = 0;
			greenI = 0;
			blueI = 0;
		}

		specularR = 0;
		specularG = 0;
		specularB = 0;
		larrayptr = LightSourcesForObject;

		for (i = NumLightSourcesForObject; i != 0; i--) {
			VECTORCH vertexToLight;
			int distanceToLight;
			lptr = *larrayptr++;
			vertexToLight.vx = lptr->LocalLP.vx - vertexPtr->vx;
			vertexToLight.vy = lptr->LocalLP.vy - vertexPtr->vy;
			vertexToLight.vz = lptr->LocalLP.vz - vertexPtr->vz;
			{
				int dx, dy, dz;
				dx = vertexToLight.vx;

				if (dx < 0) {
					dx = -dx;
				}

				dy = vertexToLight.vy;

				if (dy < 0) {
					dy = -dy;
				}

				dz = vertexToLight.vz;

				if (dz < 0) {
					dz = -dz;
				}

				if (dx > dy) {
					if (dx > dz) {
						distanceToLight = dx + ((dy + dz) >> 2);
					} else {
						distanceToLight = dz + ((dy + dx) >> 2);
					}
				} else {
					if (dy > dz) {
						distanceToLight = dy + ((dx + dz) >> 2);
					} else {
						distanceToLight = dz + ((dx + dy) >> 2);
					}
				}
			}

			if (distanceToLight < lptr->LightRange) {
				int idot = MUL_FIXED(lptr->LightRange - distanceToLight, lptr->BrightnessOverRange);
				int r, g, b;

				if (distanceToLight > 0) {
					int dotproduct = MUL_FIXED(vertexNormalPtr->vx, vertexToLight.vx)
					                 + MUL_FIXED(vertexNormalPtr->vy, vertexToLight.vy)
					                 + MUL_FIXED(vertexNormalPtr->vz, vertexToLight.vz);

					if (dotproduct > 0) {
						idot = (WideMulNarrowDiv(idot, dotproduct, distanceToLight) + idot / 4) / 2;
					} else {
						idot /= 8;
					}
				}

				r = MUL_FIXED(idot, lptr->RedScale);
				g = MUL_FIXED(idot, lptr->GreenScale);
				b = MUL_FIXED(idot, lptr->BlueScale);
				redI += r;
				greenI += g;
				blueI += b;

				if (!(lptr->LightFlags & LFlag_PreLitSource)
				    && !(lptr->LightFlags & LFlag_NoSpecular)) {
					specularR += r;
					specularG += g;
					specularB += b;
				}
			}
		}
	}

	if (Global_ODB_Ptr->SpecialFXFlags & SFXFLAG_ONFIRE) {
		specularR >>= 2;
		specularG >>= 2;
		specularB >>= 2;
		redI >>= 1;
		greenI >>= 1;
		blueI >>= 1;
	}

	/* Intensity for Textures */
	redI >>= 8;

	if (redI > 255) {
		redI = 255;
	}

	renderVertexPtr->R = redI;
	ColourIntensityArray[vertexNumber].R = redI;
	greenI >>= 8;

	if (greenI > 255) {
		greenI = 255;
	}

	renderVertexPtr->G = greenI;
	ColourIntensityArray[vertexNumber].G = greenI;
	blueI >>= 8;

	if (blueI > 255) {
		blueI = 255;
	}

	renderVertexPtr->B = blueI;
	ColourIntensityArray[vertexNumber].B = blueI;
	specularR >>= 10;

	if (specularR > 255) {
		specularR = 255;
	}

	renderVertexPtr->SpecularR = specularR;
	ColourIntensityArray[vertexNumber].SpecularR = specularR;
	specularG >>= 10;

	if (specularG > 255) {
		specularG = 255;
	}

	renderVertexPtr->SpecularG = specularG;
	ColourIntensityArray[vertexNumber].SpecularG = specularG;
	specularB >>= 10;

	if (specularB > 255) {
		specularB = 255;
	}

	renderVertexPtr->SpecularB = specularB;
	ColourIntensityArray[vertexNumber].SpecularB = specularB;
}

static void VertexIntensity_FullBright(RENDERVERTEX *renderVertexPtr)
{
	int vertexNumber = *VertexNumberPtr;
	renderVertexPtr->R = 255;
	ColourIntensityArray[vertexNumber].R = 255;
	renderVertexPtr->G = 255;
	ColourIntensityArray[vertexNumber].G = 255;
	renderVertexPtr->B = 255;
	ColourIntensityArray[vertexNumber].B = 255;
	renderVertexPtr->SpecularR = 0;
	ColourIntensityArray[vertexNumber].SpecularR = 0;
	renderVertexPtr->SpecularG = 0;
	ColourIntensityArray[vertexNumber].SpecularG = 0;
	renderVertexPtr->SpecularB = 0;
	ColourIntensityArray[vertexNumber].SpecularB = 0;
}

static void VertexIntensity_DiscoInferno(RENDERVERTEX *renderVertexPtr)
{
	int redI, greenI, blueI;
	int specularR, specularG, specularB;
	int vertexNumber = *VertexNumberPtr;

	if (ColourIntensityArray[vertexNumber].Stamp == ObjectCounter) {
		renderVertexPtr->R = ColourIntensityArray[vertexNumber].R;
		renderVertexPtr->G = ColourIntensityArray[vertexNumber].G;
		renderVertexPtr->B = ColourIntensityArray[vertexNumber].B;
		renderVertexPtr->SpecularR = ColourIntensityArray[vertexNumber].SpecularR;
		renderVertexPtr->SpecularG = ColourIntensityArray[vertexNumber].SpecularG;
		renderVertexPtr->SpecularB = ColourIntensityArray[vertexNumber].SpecularB;
		return;
	}

	ColourIntensityArray[vertexNumber].Stamp = ObjectCounter;
	{
		VECTORCH *vertexNormalPtr = ((VECTORCH *)Global_ShapeVNormals) + vertexNumber;
		VECTORCH *vertexPtr = ((VECTORCH *)Global_ShapePoints) + vertexNumber;
		LIGHTBLOCK **larrayptr;
		LIGHTBLOCK *lptr;
		int i;

		if (Global_ShapeHeaderPtr->shapeflags & ShapeFlag_PreLit) {
			unsigned int packedI = Global_EID_IPtr[vertexNumber];
			blueI = (packedI & 255) * 257;
			packedI >>= 8;
			greenI = (packedI & 255) * 257;
			packedI >>= 8;
			redI = (packedI & 255) * 257;
		} else {
			redI = 0;
			greenI = 0;
			blueI = 0;
		}

		specularR = 0;
		specularG = 0;
		specularB = 0;
		larrayptr = LightSourcesForObject;

		for (i = NumLightSourcesForObject; i != 0; i--) {
			VECTORCH vertexToLight;
			int distanceToLight;
			lptr = *larrayptr++;
			vertexToLight.vx = lptr->LocalLP.vx - vertexPtr->vx;
			vertexToLight.vy = lptr->LocalLP.vy - vertexPtr->vy;
			vertexToLight.vz = lptr->LocalLP.vz - vertexPtr->vz;
			{
				int dx, dy, dz;
				dx = vertexToLight.vx;

				if (dx < 0) {
					dx = -dx;
				}

				dy = vertexToLight.vy;

				if (dy < 0) {
					dy = -dy;
				}

				dz = vertexToLight.vz;

				if (dz < 0) {
					dz = -dz;
				}

				if (dx > dy) {
					if (dx > dz) {
						distanceToLight = dx + ((dy + dz) >> 2);
					} else {
						distanceToLight = dz + ((dy + dx) >> 2);
					}
				} else {
					if (dy > dz) {
						distanceToLight = dy + ((dx + dz) >> 2);
					} else {
						distanceToLight = dz + ((dx + dy) >> 2);
					}
				}
			}

			if (distanceToLight < lptr->LightRange) {
				int idot = MUL_FIXED(lptr->LightRange - distanceToLight, lptr->BrightnessOverRange);
				int r, g, b;

				if (distanceToLight > 0) {
					int dotproduct = MUL_FIXED(vertexNormalPtr->vx, vertexToLight.vx)
					                 + MUL_FIXED(vertexNormalPtr->vy, vertexToLight.vy)
					                 + MUL_FIXED(vertexNormalPtr->vz, vertexToLight.vz);

					if (dotproduct > 0) {
						idot = (WideMulNarrowDiv(idot, dotproduct, distanceToLight) + idot / 4) / 2;
					} else {
						idot /= 8;
					}
				}

				r = MUL_FIXED(idot, lptr->RedScale);
				g = MUL_FIXED(idot, lptr->GreenScale);
				b = MUL_FIXED(idot, lptr->BlueScale);
				redI += r;
				greenI += g;
				blueI += b;

				if (!(lptr->LightFlags & LFlag_PreLitSource)
				    && !(lptr->LightFlags & LFlag_NoSpecular)) {
					specularR += r;
					specularG += g;
					specularB += b;
				}
			}
		}
	}

	if (Global_ODB_Ptr->SpecialFXFlags & SFXFLAG_ONFIRE) {
		specularR >>= 2;
		specularG >>= 2;
		specularB >>= 2;
		redI >>= 1;
		greenI >>= 1;
		blueI >>= 1;
	}

	{
		int i = (redI + greenI + blueI);
		int si = (specularR + specularG + specularB);
		VECTORCH vertex = *(((VECTORCH *)Global_ShapePoints) + vertexNumber);
		int r, g, b;
		vertex.vx += Global_ODB_Ptr->ObWorld.vx;
		vertex.vy += Global_ODB_Ptr->ObWorld.vy;
		vertex.vz += Global_ODB_Ptr->ObWorld.vz;
		r = GetSin((vertex.vx + CloakingPhase) & 4095);
		r = MUL_FIXED(r, r);
		redI = MUL_FIXED(r, i);
		specularR = MUL_FIXED(r, si);
		g = GetSin((vertex.vy + CloakingPhase / 2) & 4095);
		g = MUL_FIXED(g, g);
		greenI = MUL_FIXED(g, i);
		specularG = MUL_FIXED(g, si);
		b = GetSin((vertex.vz + CloakingPhase * 3) & 4095);
		b = MUL_FIXED(b, b);
		blueI = MUL_FIXED(b, i);
		specularB = MUL_FIXED(b, si);
	}

	/* Intensity for Textures */
	redI >>= 8;

	if (redI > 255) {
		redI = 255;
	}

	renderVertexPtr->R = redI;
	ColourIntensityArray[vertexNumber].R = redI;
	greenI >>= 8;

	if (greenI > 255) {
		greenI = 255;
	}

	renderVertexPtr->G = greenI;
	ColourIntensityArray[vertexNumber].G = greenI;
	blueI >>= 8;

	if (blueI > 255) {
		blueI = 255;
	}

	renderVertexPtr->B = blueI;
	ColourIntensityArray[vertexNumber].B = blueI;
	specularR >>= 10;

	if (specularR > 255) {
		specularR = 255;
	}

	renderVertexPtr->SpecularR = specularR;
	ColourIntensityArray[vertexNumber].SpecularR = specularR;
	specularG >>= 10;

	if (specularG > 255) {
		specularG = 255;
	}

	renderVertexPtr->SpecularG = specularG;
	ColourIntensityArray[vertexNumber].SpecularG = specularG;
	specularB >>= 10;

	if (specularB > 255) {
		specularB = 255;
	}

	renderVertexPtr->SpecularB = specularB;
	ColourIntensityArray[vertexNumber].SpecularB = specularB;
}

static void VertexIntensity_Underwater(RENDERVERTEX *renderVertexPtr)
{
	int redI, greenI, blueI;
	int specularR, specularG, specularB;
	int vertexNumber = *VertexNumberPtr;

	if (ColourIntensityArray[vertexNumber].Stamp == ObjectCounter) {
		renderVertexPtr->R = ColourIntensityArray[vertexNumber].R;
		renderVertexPtr->G = ColourIntensityArray[vertexNumber].G;
		renderVertexPtr->B = ColourIntensityArray[vertexNumber].B;
		renderVertexPtr->SpecularR = ColourIntensityArray[vertexNumber].SpecularR;
		renderVertexPtr->SpecularG = ColourIntensityArray[vertexNumber].SpecularG;
		renderVertexPtr->SpecularB = ColourIntensityArray[vertexNumber].SpecularB;
		return;
	}

	ColourIntensityArray[vertexNumber].Stamp = ObjectCounter;
	{
		VECTORCH *vertexNormalPtr = ((VECTORCH *)Global_ShapeVNormals) + vertexNumber;
		VECTORCH *vertexPtr = ((VECTORCH *)Global_ShapePoints) + vertexNumber;
		LIGHTBLOCK **larrayptr;
		LIGHTBLOCK *lptr;
		int i;

		if (Global_ShapeHeaderPtr->shapeflags & ShapeFlag_PreLit) {
			unsigned int packedI = Global_EID_IPtr[vertexNumber];
			blueI = (packedI & 255) * 257;
			packedI >>= 8;
			greenI = (packedI & 255) * 257;
			packedI >>= 8;
			redI = (packedI & 255) * 257;
		} else {
			redI = 0;
			greenI = 0;
			blueI = 0;
		}

		specularR = 0;
		specularG = 0;
		specularB = 0;
		larrayptr = LightSourcesForObject;

		for (i = NumLightSourcesForObject; i != 0; i--) {
			VECTORCH vertexToLight;
			int distanceToLight;
			lptr = *larrayptr++;
			vertexToLight.vx = lptr->LocalLP.vx - vertexPtr->vx;
			vertexToLight.vy = lptr->LocalLP.vy - vertexPtr->vy;
			vertexToLight.vz = lptr->LocalLP.vz - vertexPtr->vz;
			{
				int dx, dy, dz;
				dx = vertexToLight.vx;

				if (dx < 0) {
					dx = -dx;
				}

				dy = vertexToLight.vy;

				if (dy < 0) {
					dy = -dy;
				}

				dz = vertexToLight.vz;

				if (dz < 0) {
					dz = -dz;
				}

				if (dx > dy) {
					if (dx > dz) {
						distanceToLight = dx + ((dy + dz) >> 2);
					} else {
						distanceToLight = dz + ((dy + dx) >> 2);
					}
				} else {
					if (dy > dz) {
						distanceToLight = dy + ((dx + dz) >> 2);
					} else {
						distanceToLight = dz + ((dx + dy) >> 2);
					}
				}
			}

			if (distanceToLight < lptr->LightRange) {
				int idot = MUL_FIXED(lptr->LightRange - distanceToLight, lptr->BrightnessOverRange);
				int r, g, b;

				if (distanceToLight > 0) {
					int dotproduct = MUL_FIXED(vertexNormalPtr->vx, vertexToLight.vx)
					                 + MUL_FIXED(vertexNormalPtr->vy, vertexToLight.vy)
					                 + MUL_FIXED(vertexNormalPtr->vz, vertexToLight.vz);

					if (dotproduct > 0) {
						idot = (WideMulNarrowDiv(idot, dotproduct, distanceToLight) + idot / 4) / 2;
					} else {
						idot /= 8;
					}
				}

				r = MUL_FIXED(idot, lptr->RedScale);
				g = MUL_FIXED(idot, lptr->GreenScale);
				b = MUL_FIXED(idot, lptr->BlueScale);
				redI += r;
				greenI += g;
				blueI += b;

				if (!(lptr->LightFlags & LFlag_PreLitSource)
				    && !(lptr->LightFlags & LFlag_NoSpecular)) {
					specularR += r;
					specularG += g;
					specularB += b;
				}
			}
		}
	}

	if (Global_ODB_Ptr->SpecialFXFlags & SFXFLAG_ONFIRE) {
		specularR >>= 2;
		specularG >>= 2;
		specularB >>= 2;
		redI >>= 1;
		greenI >>= 1;
		blueI >>= 1;
	}

	if (specularB < renderVertexPtr->Z * 4) {
		specularB = renderVertexPtr->Z * 4;
	}

	/* Intensity for Textures */
	redI >>= 8;

	if (redI > 255) {
		redI = 255;
	}

	renderVertexPtr->R = redI;
	ColourIntensityArray[vertexNumber].R = redI;
	greenI >>= 8;

	if (greenI > 255) {
		greenI = 255;
	}

	renderVertexPtr->G = greenI;
	ColourIntensityArray[vertexNumber].G = greenI;
	blueI >>= 8;

	if (blueI > 255) {
		blueI = 255;
	}

	renderVertexPtr->B = blueI;
	ColourIntensityArray[vertexNumber].B = blueI;
	specularR >>= 10;

	if (specularR > 255) {
		specularR = 255;
	}

	renderVertexPtr->SpecularR = specularR;
	ColourIntensityArray[vertexNumber].SpecularR = specularR;
	specularG >>= 10;

	if (specularG > 255) {
		specularG = 255;
	}

	renderVertexPtr->SpecularG = specularG;
	ColourIntensityArray[vertexNumber].SpecularG = specularG;
	specularB >>= 10;

	if (specularB > 255) {
		specularB = 255;
	}

	renderVertexPtr->SpecularB = specularB;
	ColourIntensityArray[vertexNumber].SpecularB = specularB;
}

/*KJL***********************************************************************
* The following functions have been transplanted from the old shape.c, and *
* will probably be found a new home at some point in the future.           *
***********************************************************************KJL*/

/*

 Texture Animation

*/

int *GetTxAnimArrayZ(int shape, int item)
{
	int **item_array_ptr;
	int **shape_textures;
	int *item_ptr;
	POLYHEADER *pheader;
	SHAPEHEADER *sptr = GetShapeData(shape);

	if (sptr && sptr->sh_textures && sptr->items) {
		item_array_ptr = sptr->items;
		shape_textures = sptr->sh_textures;
		item_ptr = item_array_ptr[item];
		pheader  = (POLYHEADER *) item_ptr;

		if (pheader->PolyFlags & iflag_txanim) {
			return (int *) shape_textures[pheader->PolyColour >> TxDefn];
		}
	}

	return 0;
}

TXANIMHEADER *GetTxAnimDataZ(int shape, int item, int sequence)
{
	TXANIMHEADER **txah_ptr;
	int **item_array_ptr;
	int **shape_textures;
	int *item_ptr;
	POLYHEADER *pheader;
	SHAPEHEADER *sptr = GetShapeData(shape);

	if (sptr && sptr->sh_textures && sptr->items) {
		item_array_ptr = sptr->items;
		shape_textures = sptr->sh_textures;
		item_ptr = item_array_ptr[item];
		pheader  = (POLYHEADER *) item_ptr;

		if (pheader->PolyFlags & iflag_txanim) {
			txah_ptr = (TXANIMHEADER **) shape_textures[pheader->PolyColour >> TxDefn];
			txah_ptr++;     /* Skip sequence shadow */
			return txah_ptr[sequence];
		}
	}

	return 0;
}

/*

 This function copies the TXANIMHEADER from the shape data item sequence
 selected by the TXACTRLBLK to the TXANIMHEADER in the TXACTRLBLK

*/

TXANIMHEADER *GetTxAnimHeaderFromShape(TXACTRLBLK *taptr, int shape)
{
	TXANIMHEADER *txah = GetTxAnimDataZ(shape, taptr->tac_item, taptr->tac_sequence);

	if (txah) {
		taptr->tac_txah.txa_flags        = txah->txa_flags;
		taptr->tac_txah.txa_state        = txah->txa_state;
		taptr->tac_txah.txa_numframes    = txah->txa_numframes;
		taptr->tac_txah.txa_framedata    = txah->txa_framedata;
		taptr->tac_txah.txa_currentframe = txah->txa_currentframe;
		taptr->tac_txah.txa_maxframe     = txah->txa_maxframe;
		taptr->tac_txah.txa_speed        = txah->txa_speed;
	}

	return txah;
}


/*

 Texture Animation Control Blocks are used to update animation. At the start
 of "AddShape()" the relevant control block values are copied across to the
 item TXANIMHEADER.

*/

void UpdateTxAnim(TXANIMHEADER *txah)
{
	int UpdateRate;

	if (txah->txa_flags & txa_flag_play) {
		/* How fast do we go? */
		if (txah->txa_flags & txa_flag_quantiseframetime) {
			/* This option is still being designed and tested */
			UpdateRate = txah->txa_speed & (~4096);     /* 1/16th */

			if (UpdateRate < 4096) {
				UpdateRate = 4096;
			}
		}

		UpdateRate = MUL_FIXED(NormalFrameTime, txah->txa_speed);

		/* Update the current frame */
		if (txah->txa_flags & txa_flag_reverse) {
			txah->txa_currentframe -= UpdateRate;

			if (txah->txa_currentframe < 0) {
				if (txah->txa_flags & txa_flag_noloop) {
					txah->txa_currentframe = 0;
				} else {
					txah->txa_currentframe += txah->txa_maxframe;
				}
			}
		} else {
			txah->txa_currentframe += UpdateRate;

			if (txah->txa_currentframe >= txah->txa_maxframe) {
				if (txah->txa_flags & txa_flag_noloop) {
					txah->txa_currentframe = txah->txa_maxframe - 1;
				} else {
					txah->txa_currentframe -= txah->txa_maxframe;
				}
			}
		}
	}
}

// Display block TXACTRLBLKS pass their data on to shape TXANIMHEADERs
void ControlTextureAnimation(DISPLAYBLOCK *dptr)
{
	TXANIMHEADER *txah;
	int *iptr;
	TXACTRLBLK *taptr = dptr->ObTxAnimCtrlBlks;

	while (taptr) {
		/* Update animation for the display block TXACTRLBLK */
		LOCALASSERT(&(taptr->tac_txah));
		UpdateTxAnim(&taptr->tac_txah);
		/* Get the TXANIMHEADER from the shape data */
		txah = taptr->tac_txah_s;
		/* Copy across the current frame */
		LOCALASSERT(txah);
		txah->txa_currentframe = taptr->tac_txah.txa_currentframe;
		iptr = taptr->tac_txarray;
		LOCALASSERT(iptr);
		*iptr = taptr->tac_sequence;
		taptr = taptr->tac_next;
	}
}

void CreateTxAnimUVArray(int *txa_data, int *uv_array, int *shapeitemptr)
{
	TXANIMHEADER **txah_ptr;
	TXANIMHEADER *txah;
	TXANIMFRAME *txaf;
	TXANIMFRAME *txaf0;
	TXANIMFRAME *txaf1;
	int *txaf0_uv;
	int *txaf1_uv;
	int CurrentFrame, NextFrame, Alpha, OneMinusAlpha;
	int i;
	int *iptr;
	int Orient, Scale;
	int OrientX, OrientY;
	int ScaleX, ScaleY;
	int sin, cos;
	int x, y;
	int x1, y1;
	int o1, o2, od;
	POLYHEADER *pheader = (POLYHEADER *) shapeitemptr;
	/* The sequence # will have been copied across by the control block */
	int sequence = *txa_data++;
	txah_ptr = (TXANIMHEADER **) txa_data;
	txah = txah_ptr[sequence];
	txaf = txah->txa_framedata;

	/* Because the current frame can be set from outside, clamp it first */
	if (txah->txa_currentframe < 0) {
		txah->txa_currentframe = 0;
	}

	if (txah->txa_currentframe >= txah->txa_maxframe) {
		txah->txa_currentframe = txah->txa_maxframe - 1;
	}

	/* Frame # */
	CurrentFrame  = txah->txa_currentframe >> 16;
	Alpha         = txah->txa_currentframe - (CurrentFrame << 16);
	OneMinusAlpha = ONE_FIXED - Alpha;
	/* Start and End Frame */
	NextFrame = CurrentFrame + 1;

	if (NextFrame >= txah->txa_numframes) {
		NextFrame = 0;
	}

	txaf0 = &txaf[CurrentFrame];
	txaf1 = &txaf[NextFrame];
	/*

	Write the image index back to the item by overwriting the shape data.
	This is not elegant but it is one of the kind of things you expect to
	have happen when a major new feature is retro-fitted to a system.

	*/
	pheader->PolyColour &= ClrTxIndex;
	pheader->PolyColour |= txaf0->txf_image;
	txaf0_uv = txaf0->txf_uvdata;
	txaf1_uv = txaf1->txf_uvdata;
	/* Calculate UVs */
	iptr = uv_array;

	if (txah->txa_flags & txa_flag_interpolate_uvs) {
		for (i = txaf0->txf_numuvs; i != 0; i--) {
			iptr[0] = MUL_FIXED(txaf0_uv[0], OneMinusAlpha) + MUL_FIXED(txaf1_uv[0], Alpha);
			iptr[1] = MUL_FIXED(txaf0_uv[1], OneMinusAlpha) + MUL_FIXED(txaf1_uv[1], Alpha);
			/*textprint("%d, %d\n", iptr[0] >> 16, iptr[1] >> 16);*/
			txaf0_uv += 2;
			txaf1_uv += 2;
			iptr += 2;
		}
	} else {
		for (i = txaf0->txf_numuvs; i != 0; i--) {
			iptr[0] = txaf0_uv[0];
			iptr[1] = txaf0_uv[1];
			/*textprint("%d, %d\n", iptr[0] >> 16, iptr[1] >> 16);*/
			txaf0_uv += 2;
			iptr += 2;
		}
	}

	/* Interpolate Orient and Scale */
	o1 = txaf0->txf_orient;
	o2 = txaf1->txf_orient;

	if (o1 == o2) {
		Orient = o1;
	} else {
		od = o1 - o2;

		if (od < 0) {
			od = -od;
		}

		if (od >= deg180) {
			o1 <<= (32 - 12);
			o1 >>= (32 - 12);
			o2 <<= (32 - 12);
			o2 >>= (32 - 12);
		}

		Orient = MUL_FIXED(o1, OneMinusAlpha) + MUL_FIXED(o2, Alpha);
		Orient &= wrap360;
	}

	if (txaf0->txf_scale == txaf1->txf_scale) {
		Scale = txaf0->txf_scale;
	} else {
		Scale = WideMul2NarrowDiv(txaf0->txf_scale, OneMinusAlpha,
		                          txaf1->txf_scale, Alpha, ONE_FIXED);
	}

	/* Interpolate Orient and Scale Origins */
	if (txaf0->txf_orientx == txaf1->txf_orientx) {
		OrientX = txaf0->txf_orientx;
	} else {
		OrientX = MUL_FIXED(txaf0->txf_orientx, OneMinusAlpha)
		          + MUL_FIXED(txaf1->txf_orientx, Alpha);
	}

	if (txaf0->txf_orienty == txaf1->txf_orienty) {
		OrientY = txaf0->txf_orienty;
	} else {
		OrientY = MUL_FIXED(txaf0->txf_orienty, OneMinusAlpha)
		          + MUL_FIXED(txaf1->txf_orienty, Alpha);
	}

	if (txaf0->txf_scalex == txaf1->txf_scalex) {
		ScaleX = txaf0->txf_scalex;
	} else {
		ScaleX = MUL_FIXED(txaf0->txf_scalex, OneMinusAlpha)
		         + MUL_FIXED(txaf1->txf_scalex, Alpha);
	}

	if (txaf0->txf_scaley == txaf1->txf_scaley) {
		ScaleY = txaf0->txf_scaley;
	} else {
		ScaleY = MUL_FIXED(txaf0->txf_scaley, OneMinusAlpha)
		         + MUL_FIXED(txaf1->txf_scaley, Alpha);
	}

#if 0
	textprint("Alpha         = %d\n", Alpha);
	textprint("OneMinusAlpha = %d\n", OneMinusAlpha);
	textprint("Orient = %d\n", Orient);
	textprint("txaf0->txf_scale = %d\n", txaf0->txf_scale);
	textprint("txaf1->txf_scale = %d\n", txaf1->txf_scale);
	textprint("Scale  = %d\n", Scale);
#endif

	/* Rotate UV Array */
	if (Orient) {
		sin = GetSin(Orient);
		cos = GetCos(Orient);
		iptr = uv_array;

		for (i = txaf0->txf_numuvs; i != 0; i--) {
			x = iptr[0] - OrientX;
			y = iptr[1] - OrientY;
			x1 = MUL_FIXED(x, cos) - MUL_FIXED(y, sin);
			y1 = MUL_FIXED(x, sin) + MUL_FIXED(y, cos);
			iptr[0] = x1 + OrientX;
			iptr[1] = y1 + OrientY;
			iptr += 2;
		}
	}

	/* Scale UV Array */
	if (Scale != ONE_FIXED) {
		iptr = uv_array;

		for (i = txaf0->txf_numuvs; i != 0; i--) {
			x = iptr[0] - ScaleX;
			y = iptr[1] - ScaleY;
			x = MUL_FIXED(x, Scale);
			y = MUL_FIXED(y, Scale);
			iptr[0] = x + ScaleX;
			iptr[1] = y + ScaleY;
			iptr += 2;
		}
	}

#if 0
	textprint("Current Frame = %d\n", txah->txa_currentframe);
	textprint("Current Frame = %d\n", CurrentFrame);
	textprint("Next Frame    = %d\n", NextFrame);
	textprint("Alpha         = %d\n", Alpha);
#endif
}


/*

 Shape Points for Unrotated Sprites

*/

void ShapeSpritePointsInstr(SHAPEINSTR *shapeinstrptr)
{
	int **shapeitemarrayptr = shapeinstrptr->sh_instr_data;
	int *shapeitemptr       = *shapeitemarrayptr;
	VECTORCH *rotptsptr     = RotatedPts;
	int numitems;

	for (numitems = shapeinstrptr->sh_numitems; numitems != 0; numitems--) {
		rotptsptr->vx =  shapeitemptr[ix];
		rotptsptr->vx += Global_ODB_Ptr->ObView.vx;
		rotptsptr->vy =  shapeitemptr[iy];
		rotptsptr->vy += Global_ODB_Ptr->ObView.vy;
		rotptsptr->vz =  shapeitemptr[iz];
		rotptsptr->vz += Global_ODB_Ptr->ObView.vz;
		shapeitemptr += vsize;
		rotptsptr++;
	}
}

// FIXME
//bool IsSphereInFrustum(Sphere_t &sphere);

void AddShape(DISPLAYBLOCK *dptr, VIEWDESCRIPTORBLOCK *VDB_Ptr)
{
	SHAPEHEADER *shapeheaderptr;

	if (!dptr->ObShape && dptr->SfxPtr) {
		return;
	}

	/* KJL 12:42:38 18/05/98 - check to see if object is on fire */
	if (dptr->ObStrategyBlock) {
		if (dptr->ObStrategyBlock->SBDamageBlock.IsOnFire) {
			dptr->SpecialFXFlags |= SFXFLAG_ONFIRE;
		} else {
			dptr->SpecialFXFlags &= ~SFXFLAG_ONFIRE;
		}
	}

	/* is object a morphing one? */
	if (dptr->ObMorphCtrl) {
		LOCALASSERT(dptr->ObMorphCtrl->ObMorphHeader);
		//if(dptr->ObMorphCtrl->ObMorphHeader)
		{
			GetMorphDisplay(&MorphDisplay, dptr);
			dptr->ObShape     = MorphDisplay.md_shape1;
			dptr->ObShapeData = MorphDisplay.md_sptr1;
			shapeheaderptr    = MorphDisplay.md_sptr1;
		}
	} else {
		shapeheaderptr = GetShapeData(dptr->ObShape);
		/* It is important to pass this SHAPEHEADER* on to the display block */
		dptr->ObShapeData = shapeheaderptr;

		// I've put this inside the else so that it does
		// not conflict with morphing !!!
		// make sure dptr->ObShapeData is up to date before
		// doing CopyAnimationFrameToShape

		if (dptr->ShapeAnimControlBlock) {
			if (!(dptr->ShapeAnimControlBlock->current.empty)) {
				CopyAnimationFrameToShape(&dptr->ShapeAnimControlBlock->current, dptr);
			}
		}
	}

	ChooseLightingModel(dptr);

	/* Texture Animation Control */
	if (dptr->ObTxAnimCtrlBlks) {
		ControlTextureAnimation(dptr);
	}

	/* Global Variables */
	Global_VDB_Ptr        = VDB_Ptr;
	Global_ODB_Ptr        = dptr;
	Global_ShapeHeaderPtr = shapeheaderptr;

	// Shape Language Specific Setup
	SetupShapePipeline();

	/*
		Create the Local -> View Matrix

		LToVMat = VDB_Mat * ObMat

		"Get the points into View Space, then apply the Local Transformation"
	*/
	MatrixMultiply(&VDB_Ptr->VDB_Mat, &dptr->ObMat, &LToVMat);
	MatrixToEuler(&LToVMat, &LToVMat_Euler);
	
	/*
		Create the World -> Local Matrix

		WToLMat = Transposed Local Matrix
	*/
	CopyMatrix(&dptr->ObMat, &WToLMat);
	TransposeMatrixCH(&WToLMat);

	/*
		Transform the View World Location to Local Space

		-> Make the View Loc. relative to the Object View Space Centre
		-> Rotate this vector using WToLMat
	*/
	MakeVector(&VDB_Ptr->VDB_World, &dptr->ObWorld, &LocalView);
	RotateVector(&LocalView, &WToLMat);
	NumberOfHeatSources = 0;

	if (dptr->HModelControlBlock) {
		ObjectCentre = dptr->ObView;

		if (dptr->ObStrategyBlock) {
			HierarchicalObjectsLowestYValue = dptr->ObStrategyBlock->DynPtr->ObjectVertices[0].vy;

			if (CurrentVisionMode == VISION_MODE_NORMAL && AvP.PlayerType == I_Alien) {
				DoAlienEnergyView(dptr);
			}
		}

		if (CurrentVisionMode == VISION_MODE_PRED_THERMAL) {
			FindHeatSourcesInHModel(dptr);
		}

		DoHModel(dptr->HModelControlBlock, dptr);
		return;
	}

	/* Find out which light sources are in range of of the object */
	LightSourcesInRangeOfObject(dptr);
	/* Shape Language Execution Shell */
	{
		SHAPEINSTR *shapeinstrptr = shapeheaderptr->sh_instruction;

		/*  setup the rotated points array */
		switch (shapeinstrptr->sh_instr) {
			default:
			case I_ShapePoints: {
				if (Global_ODB_Ptr->ObMorphCtrl) {
					MorphPoints(shapeinstrptr);
				} else {
					TranslateShapeVertices(shapeinstrptr);
				}

				break;
			}
		}
	}

	// call polygon pipeline
	ShapePipeline(shapeheaderptr);

	// call sfx code
	HandleSfxForObject(dptr);

	if (dptr->ObStrategyBlock) {
		if (dptr->ObStrategyBlock->I_SBtype == I_BehaviourInanimateObject) {
			INANIMATEOBJECT_STATUSBLOCK *objStatPtr = static_cast<INANIMATEOBJECT_STATUSBLOCK *>(dptr->ObStrategyBlock->SBdataptr);

			if (objStatPtr->typeId == IOT_FieldCharge) {
				D3D_DecalSystem_Setup();

				for (int i = 0; i < 63; i++) {
					PARTICLE particle = {0};
					//                  particle.Position.vy = -280+i-GetCos((CloakingPhase/16*i + i*64+particle.Position.vz)&4095)/1024;
					particle.Position.vy = -280 + i - GetCos((CloakingPhase / 16 * i + i * 64/*+particle.Position.vz*/) & 4095) / 1024;
					particle.Position.vx = GetCos((CloakingPhase + i * 64 + particle.Position.vy) & 4095) / 512;
					particle.Position.vz = GetSin((CloakingPhase + i * 64 + particle.Position.vy) & 4095) / 512;
					RotateVector(&particle.Position, &dptr->ObMat);
					particle.Position.vx += dptr->ObWorld.vx;
					particle.Position.vy += dptr->ObWorld.vy;
					particle.Position.vz += dptr->ObWorld.vz;
					particle.ParticleID = PARTICLE_MUZZLEFLASH;
					particle.Colour = 0xff00007f + (FastRandom() & 0x7f7f7f);
					particle.Size = 40;
					RenderParticle(&particle);
				}

				D3D_DecalSystem_End();
			}
		}
	}
}

void DoAlienEnergyView(DISPLAYBLOCK *dispPtr)
{
	HMODELCONTROLLER *controllerPtr = dispPtr->HModelControlBlock;
	unsigned int colour = MARINES_LIFEFORCE_GLOW_COLOUR;
	LOCALASSERT(controllerPtr);
	/* KJL 16:36:25 10/02/98 - process model */
	{
		STRATEGYBLOCK *sbPtr = Global_ODB_Ptr->ObStrategyBlock;

		if (sbPtr) {
			switch (sbPtr->I_SBtype) {
				case I_BehaviourAlien: {
					colour = ALIENS_LIFEFORCE_GLOW_COLOUR;
					break;
				}

				case I_BehaviourPredator: {
					colour = PREDATORS_LIFEFORCE_GLOW_COLOUR;
					break;
				}

				case I_BehaviourMarine:
				case I_BehaviourSeal: {
					MARINE_STATUS_BLOCK *marineStatusPointer = (MARINE_STATUS_BLOCK *)(sbPtr->SBdataptr);
					GLOBALASSERT(marineStatusPointer);

					if (marineStatusPointer->Android) {
						return;
					}

					colour = MARINES_LIFEFORCE_GLOW_COLOUR;
				}

				case I_BehaviourNetGhost: {
					NETGHOSTDATABLOCK *ghostDataPtr = (NETGHOSTDATABLOCK *)Global_ODB_Ptr->ObStrategyBlock->SBdataptr;

					if (ghostDataPtr->type == I_BehaviourAlienPlayer || ghostDataPtr->type == I_BehaviourAlien
					    || (ghostDataPtr->type == I_BehaviourNetCorpse && ghostDataPtr->subtype == I_BehaviourAlienPlayer)) {
						colour = ALIENS_LIFEFORCE_GLOW_COLOUR;
					} else if (ghostDataPtr->type == I_BehaviourPredatorPlayer || ghostDataPtr->type == I_BehaviourPredator
					           || (ghostDataPtr->type == I_BehaviourNetCorpse && ghostDataPtr->subtype == I_BehaviourPredatorPlayer)) {
						colour = PREDATORS_LIFEFORCE_GLOW_COLOUR;
					}

					break;
				}

				case I_BehaviourNetCorpse: {
					NETCORPSEDATABLOCK *corpseDataPtr = (NETCORPSEDATABLOCK *)sbPtr->SBdataptr;

					if (corpseDataPtr->Android) {
						return;
					}

					if (corpseDataPtr->Type == I_BehaviourAlienPlayer || corpseDataPtr->Type == I_BehaviourAlien) {
						colour = ALIENS_LIFEFORCE_GLOW_COLOUR;
					} else if (corpseDataPtr->Type == I_BehaviourPredatorPlayer || corpseDataPtr->Type == I_BehaviourPredator) {
						colour = PREDATORS_LIFEFORCE_GLOW_COLOUR;
					}

					break;
				}

				case I_BehaviourHierarchicalFragment: {
					HDEBRIS_BEHAV_BLOCK *debrisDataPtr = (HDEBRIS_BEHAV_BLOCK *)sbPtr->SBdataptr;

					if (debrisDataPtr->Type == I_BehaviourAutoGun || debrisDataPtr->Android) {
						return;
					} else if (debrisDataPtr->Type == I_BehaviourAlien) {
						colour = ALIENS_LIFEFORCE_GLOW_COLOUR;
					} else if (debrisDataPtr->Type == I_BehaviourPredator) {
						colour = PREDATORS_LIFEFORCE_GLOW_COLOUR;
					} else if ((debrisDataPtr->Type == I_BehaviourMarine) || (debrisDataPtr->Type == I_BehaviourSeal)) {
						colour = MARINES_LIFEFORCE_GLOW_COLOUR;
					} else {
						return;
					}

					break;
				}

				case I_BehaviourAutoGun: {
					/* KJL 19:31:53 25/01/99 - organics only, please */
					return;
					break;
				}

				default:
					break;
			}
		}
	}

	if ((Global_ODB_Ptr->SpecialFXFlags & SFXFLAG_MELTINGINTOGROUND)
	    && (Global_ODB_Ptr->ObFlags2 < ONE_FIXED)) {
		unsigned int alpha = MUL_FIXED(Global_ODB_Ptr->ObFlags2, colour >> 24);
		colour = (colour & 0xffffff) + (alpha << 24);
	}

	/* KJL 16:36:12 10/02/98 - check positions are up to date */
	ProveHModel(controllerPtr, dispPtr);
	D3D_DecalSystem_Setup();
	FindAlienEnergySource_Recursion(controllerPtr, controllerPtr->section_data, colour);
	D3D_DecalSystem_End();
}

static void FindAlienEnergySource_Recursion(HMODELCONTROLLER *controllerPtr, SECTION_DATA *sectionDataPtr, unsigned int colour)
{
	/* KJL 16:29:40 10/02/98 - Recurse through hmodel */
	if ((sectionDataPtr->First_Child != NULL) && (!(sectionDataPtr->flags & section_data_terminate_here))) {
		SECTION_DATA *childSectionPtr = sectionDataPtr->First_Child;

		while (childSectionPtr != NULL) {
			LOCALASSERT(childSectionPtr->My_Parent == sectionDataPtr);
			FindAlienEnergySource_Recursion(controllerPtr, childSectionPtr, colour);
			childSectionPtr = childSectionPtr->Next_Sibling;
		}
	}

	if (sectionDataPtr->Shape && sectionDataPtr->Shape->shaperadius > LocalDetailLevels.AlienEnergyViewThreshold) {
		PARTICLE particle;
		particle.Position = sectionDataPtr->World_Offset;
		particle.ParticleID = PARTICLE_MUZZLEFLASH;
		particle.Colour = colour;//0x208080ff;
		//      particle.Colour = 0x20ff8080;
		//      particle.Size = sectionDataPtr->Shape->shaperadius*3;
		//      particle.Colour = 0x20ffffff;
		particle.Size = sectionDataPtr->Shape->shaperadius * 2;
		RenderParticle(&particle);
	}
}

void AddHierarchicalShape(DISPLAYBLOCK *dptr, VIEWDESCRIPTORBLOCK *VDB_Ptr)
{
	// players gun, player, alien, predator models..
	SHAPEHEADER *shapeheaderptr;
	SHAPEINSTR  *shapeinstrptr;
	GLOBALASSERT(!dptr->HModelControlBlock);

	if (!ObjectWithinFrustum(dptr)) {
		return;
	}

	shapeheaderptr = dptr->ObShapeData;

	/* Texture Animation Control */
	if (dptr->ObTxAnimCtrlBlks) {
		ControlTextureAnimation(dptr);
	}

	/* Global Variables */
	Global_VDB_Ptr        = VDB_Ptr;
	Global_ODB_Ptr        = dptr;
	Global_ShapeHeaderPtr = shapeheaderptr;
	//  if((Global_ODB_Ptr->ObStrategyBlock)&&(Global_ODB_Ptr->ObStrategyBlock->I_SBtype == I_BehaviourAlien))
	// textprint("hier alien part\n");
	/* Shape Language Specific Setup */
	SetupShapePipeline();
	/*

	Create the Local -> View Matrix

	LToVMat = VDB_Mat * ObMat

	"Get the points into View Space, then apply the Local Transformation"

	*/
	MatrixMultiply(&VDB_Ptr->VDB_Mat, &dptr->ObMat, &LToVMat);
	MatrixToEuler(&LToVMat, &LToVMat_Euler);
	/*

	Create the World -> Local Matrix

	WToLMat = Transposed Local Matrix

	*/
	CopyMatrix(&dptr->ObMat, &WToLMat);
	TransposeMatrixCH(&WToLMat);
	/*

	Transform the View World Location to Local Space

	-> Make the View Loc. relative to the Object View Space Centre
	-> Rotate this vector using WToLMat

	*/
	MakeVector(&VDB_Ptr->VDB_World, &dptr->ObWorld, &LocalView);
	RotateVector(&LocalView, &WToLMat);

	if (!(PIPECLEANER_CHEATMODE || BALLSOFFIRE_CHEATMODE) || !dptr->ObStrategyBlock) {
		/* Find out which light sources are in range of of the object */
		LightSourcesInRangeOfObject(dptr);
		/* Shape Language Execution Shell */
		shapeinstrptr = shapeheaderptr->sh_instruction;

		/*  setup the rotated points array */
		if ((dptr->SpecialFXFlags & SFXFLAG_MELTINGINTOGROUND)
		    && (dptr->ObFlags2 <= ONE_FIXED)) {
			SquishPoints(shapeinstrptr);
		} else {
			TranslateShapeVertices(shapeinstrptr);
		}

		/* call polygon pipeline */
		ShapePipeline(shapeheaderptr);
	}

	if (BALLSOFFIRE_CHEATMODE && dptr->ObStrategyBlock) {
		HandleObjectOnFire(dptr);
	}

	/* call sfx code */
	HandleSfxForObject(dptr);
}


float ViewMatrix[12];
float ObjectViewMatrix[12];
float Source[3];
float Dest[3];

float p = 1.0f;
float o = 1.0f;

extern void TranslationSetup(void)
{
	VECTORCH v = Global_VDB_Ptr->VDB_World;
	/*float*/ p = PredatorVisionChangeCounter / 65536.0f;
	/*float*/ o = 1.0f;
	p = 1.0f + p;

	if (NAUSEA_CHEATMODE) {
		p = (GetSin((CloakingPhase / 3) & 4095)) / 65536.0f;
		p = 1.0f + p * p;
		o = (GetCos((CloakingPhase / 5) & 4095)) / 65536.0f;
		o = 1.0f + o * o;
	}

	// right vector
	ViewMatrix[0] = (float)(Global_VDB_Ptr->VDB_Mat.mat11) / 65536.0f; //*o;
	ViewMatrix[1] = (float)(Global_VDB_Ptr->VDB_Mat.mat21) / 65536.0f; //*o;
	ViewMatrix[2] = (float)(Global_VDB_Ptr->VDB_Mat.mat31) / 65536.0f; //*o;
	// up vector
	ViewMatrix[4] = (float)(Global_VDB_Ptr->VDB_Mat.mat12) / 65536.0f; //*p;
	ViewMatrix[5] = (float)(Global_VDB_Ptr->VDB_Mat.mat22) / 65536.0f; //*p;
	ViewMatrix[6] = (float)(Global_VDB_Ptr->VDB_Mat.mat32) / 65536.0f; //*p;
	// lookat vector
	ViewMatrix[8]  = (float)(Global_VDB_Ptr->VDB_Mat.mat13) / 65536.0f;
	ViewMatrix[9]  = (float)(Global_VDB_Ptr->VDB_Mat.mat23) / 65536.0f;
	ViewMatrix[10] = (float)(Global_VDB_Ptr->VDB_Mat.mat33) / 65536.0f;
#ifndef USE_D3DVIEWTRANSFORM
	RotateVector(&v, &Global_VDB_Ptr->VDB_Mat);
#endif
	// position
#ifndef USE_D3DVIEWTRANSFORM // negate values
	ViewMatrix[3] = ((float) -v.vx);// * o;
	ViewMatrix[7] = ((float) -v.vy);// * p;
	ViewMatrix[11] = ((float)-v.vz);// * CameraZoomScale;
#else
	ViewMatrix[3]  = ((float)v.vx);//*o;
	ViewMatrix[7]  = ((float)v.vy);//*p;
	ViewMatrix[11] = ((float)v.vz);
#endif

	if (MIRROR_CHEATMODE) {
		ViewMatrix[0] = -ViewMatrix[0];
		ViewMatrix[1] = -ViewMatrix[1];
		ViewMatrix[2] = -ViewMatrix[2];
		ViewMatrix[3] = -ViewMatrix[3];
	}

	UpdateViewMatrix(&ViewMatrix[0]);
	UpdateProjectionMatrix();
	BuildFrustum();
}

static void TranslatePoint(const float *source, float *dest, const float *matrix)
{
	dest[0] = matrix[ 0] * source[0] + matrix[ 1] * source[1] + matrix[ 2] * source[2] + matrix[ 3];
	dest[1] = matrix[ 4] * source[0] + matrix[ 5] * source[1] + matrix[ 6] * source[2] + matrix[ 7];
	dest[2] = matrix[ 8] * source[0] + matrix[ 9] * source[1] + matrix[10] * source[2] + matrix[11];
}

void TranslatePointIntoViewspace(VECTORCH *pointPtr)
{
#ifndef USE_D3DVIEWTRANSFORM
	Source[0] = (float)pointPtr->vx;
	Source[1] = (float)pointPtr->vy;
	Source[2] = (float)pointPtr->vz;
	TranslatePoint(Source, Dest, ViewMatrix);
	f2i(pointPtr->vx, Dest[0]);
	f2i(pointPtr->vy, Dest[1]);
	f2i(pointPtr->vz, Dest[2]);
#endif
}

void TranslatePointIntoViewspace2(VECTORCH *pointPtr)
{
	Source[0] = (float)pointPtr->vx;
	Source[1] = (float)pointPtr->vy;
	Source[2] = (float)pointPtr->vz;
	TranslatePoint(Source, Dest, ViewMatrix);
	pointPtr->vx = (int)Dest[0];
	pointPtr->vy = (int)Dest[1];
	pointPtr->vz = (int)Dest[2];
}

void TranslatePointIntoViewspaceF(VECTORCHF *pointPtr)
{
	float Dest[3];
	float Src[3];
	Src[0] = pointPtr->vx;
	Src[1] = pointPtr->vy;
	Src[2] = pointPtr->vz;
	TranslatePoint(Src, Dest, ViewMatrix);
	pointPtr->vx = Dest[0];
	pointPtr->vy = Dest[1];
	pointPtr->vz = Dest[2];
}

void SquishPoints(SHAPEINSTR *shapeinstrptr)
{
	int **shapeitemarrayptr = shapeinstrptr->sh_instr_data;
	VECTORCH *shapePts = (VECTORCH *)*shapeitemarrayptr;
	{
		int scale = Global_ODB_Ptr->ObFlags2;

		for (int i = 0; i < Global_ShapeHeaderPtr->numpoints; i++) {
			VECTORCH point = shapePts[i];
			RotateVector(&point, &Global_ODB_Ptr->ObMat);
			point.vx = MUL_FIXED(point.vx, ONE_FIXED * 3 / 2 - scale / 2);
			point.vx += Global_ODB_Ptr->ObWorld.vx;
			point.vz = MUL_FIXED(point.vz, ONE_FIXED * 3 / 2 - scale / 2);
			point.vz += Global_ODB_Ptr->ObWorld.vz;
			point.vy += Global_ODB_Ptr->ObWorld.vy;
			point.vy = HierarchicalObjectsLowestYValue + MUL_FIXED(point.vy - HierarchicalObjectsLowestYValue, scale);
#ifndef USE_D3DVIEWTRANSFORM
			Source[0] = (float)point.vx;
			Source[1] = (float)point.vy;
			Source[2] = (float)point.vz;
			TranslatePoint(Source, Dest, ViewMatrix);
			f2i(RotatedPts[i].vx, Dest[0]);
			f2i(RotatedPts[i].vy, Dest[1]);
			f2i(RotatedPts[i].vz, Dest[2]);
#else   // bjd - view matrix test
			RotatedPts[i].vx = point.vx;
			RotatedPts[i].vy = point.vy;
			RotatedPts[i].vz = point.vz;
#endif
		}
	}
}

void MorphPoints(SHAPEINSTR *shapeinstrptr)
{
	VECTORCH *srcPtr;
	{
		SHAPEHEADER *shape1Ptr;
		VECTORCH *shape1PointsPtr;
		VECTORCH *shape2PointsPtr;
		/* Set up the morph data */
		GetMorphDisplay(&MorphDisplay, Global_ODB_Ptr);
		shape1Ptr = MorphDisplay.md_sptr1;

		if (MorphDisplay.md_lerp == 0x0000) {
			srcPtr = (VECTORCH *)*shape1Ptr->points;
		} else if (MorphDisplay.md_lerp == 0xffff) {
			SHAPEHEADER *shape2Ptr;
			shape2Ptr = MorphDisplay.md_sptr2;
			srcPtr = (VECTORCH *)*shape2Ptr->points;
			Global_ShapePoints = *(shape2Ptr->points);
		} else {
			SHAPEHEADER *shape2Ptr;
			shape2Ptr = MorphDisplay.md_sptr2;
			shape1PointsPtr = (VECTORCH *)(*shape1Ptr->points);
			shape2PointsPtr = (VECTORCH *)(*shape2Ptr->points);
			{
				int numberOfPoints = shape1Ptr->numpoints;
				VECTORCH *morphedPointsPtr = (VECTORCH *) MorphedPts;

				while (numberOfPoints--) {
					VECTORCH vertex1 = *shape1PointsPtr;
					VECTORCH vertex2 = *shape2PointsPtr;

					if ((vertex1.vx == vertex2.vx && vertex1.vy == vertex2.vy && vertex1.vz == vertex2.vz)) {
						*morphedPointsPtr = vertex1;
					} else {
						/* KJL 15:27:20 05/22/97 - I've changed this to speed things up, If a vertex
						component has a magnitude greater than 32768 things will go wrong. */
						morphedPointsPtr->vx = vertex1.vx + (((vertex2.vx - vertex1.vx) * MorphDisplay.md_lerp) >> 16);
						morphedPointsPtr->vy = vertex1.vy + (((vertex2.vy - vertex1.vy) * MorphDisplay.md_lerp) >> 16);
						morphedPointsPtr->vz = vertex1.vz + (((vertex2.vz - vertex1.vz) * MorphDisplay.md_lerp) >> 16);
					}

					shape1PointsPtr++;
					shape2PointsPtr++;
					morphedPointsPtr++;
				}
			}
			Global_ShapePoints = (int *)MorphedPts;
			srcPtr = (VECTORCH *)MorphedPts;
		}
	}
	{
		VECTORCH *destPtr = RotatedPts;
		int i;

		for (i = shapeinstrptr->sh_numitems; i != 0; i--) {
#ifndef USE_D3DVIEWTRANSFORM
			Source[0] = (float)(srcPtr->vx + Global_ODB_Ptr->ObWorld.vx);
			Source[1] = (float)(srcPtr->vy + Global_ODB_Ptr->ObWorld.vy);
			Source[2] = (float)(srcPtr->vz + Global_ODB_Ptr->ObWorld.vz);
			TranslatePoint(Source, Dest, ViewMatrix);
			f2i(destPtr->vx, Dest[0]);
			f2i(destPtr->vy, Dest[1]);
			f2i(destPtr->vz, Dest[2]);
#else  // bjd - view matrix test
			destPtr->vx = srcPtr->vx + Global_ODB_Ptr->ObWorld.vx;
			destPtr->vy = srcPtr->vy + Global_ODB_Ptr->ObWorld.vy;
			destPtr->vz = srcPtr->vz + Global_ODB_Ptr->ObWorld.vz;
#endif
			srcPtr++;
			destPtr++;
		}
	}
}

void TranslateShapeVertices(SHAPEINSTR *shapeinstrptr)
{
	VECTORCH *destPtr = RotatedPts;
	int **shapeitemarrayptr;
	VECTORCH *srcPtr;
	int i;
	shapeitemarrayptr = shapeinstrptr->sh_instr_data;
	srcPtr = (VECTORCH *)*shapeitemarrayptr;

	if (Global_ODB_Ptr->ObFlags & ObFlag_ArbRot) {
		for (i = shapeinstrptr->sh_numitems; i != 0; i--) {
			destPtr->vx = (srcPtr->vx + Global_ODB_Ptr->ObView.vx);
			//          destPtr->vy = ((srcPtr->vy + Global_ODB_Ptr->ObView.vy)*4)/3;
			destPtr->vy = (srcPtr->vy + Global_ODB_Ptr->ObView.vy);
			destPtr->vz = (srcPtr->vz + Global_ODB_Ptr->ObView.vz);
			srcPtr++;
			destPtr++;
		}
	} else {
		ObjectViewMatrix[0] = (float)(Global_ODB_Ptr->ObMat.mat11) / 65536.0f;
		ObjectViewMatrix[1] = (float)(Global_ODB_Ptr->ObMat.mat21) / 65536.0f;
		ObjectViewMatrix[2] = (float)(Global_ODB_Ptr->ObMat.mat31) / 65536.0f;
		ObjectViewMatrix[4] = (float)(Global_ODB_Ptr->ObMat.mat12) / 65536.0f;
		ObjectViewMatrix[5] = (float)(Global_ODB_Ptr->ObMat.mat22) / 65536.0f;
		ObjectViewMatrix[6] = (float)(Global_ODB_Ptr->ObMat.mat32) / 65536.0f;
		ObjectViewMatrix[8] = (float)(Global_ODB_Ptr->ObMat.mat13) / 65536.0f;
		ObjectViewMatrix[9] = (float)(Global_ODB_Ptr->ObMat.mat23) / 65536.0f;
		ObjectViewMatrix[10] = (float)(Global_ODB_Ptr->ObMat.mat33) / 65536.0f;
		ObjectViewMatrix[3] = (float)Global_ODB_Ptr->ObWorld.vx;
		ObjectViewMatrix[7] = (float)Global_ODB_Ptr->ObWorld.vy;
		ObjectViewMatrix[11] = (float)Global_ODB_Ptr->ObWorld.vz;

		for (i = shapeinstrptr->sh_numitems; i != 0; i--) {
			Source[0] = (float)srcPtr->vx;
			Source[1] = (float)srcPtr->vy;
			Source[2] = (float)srcPtr->vz;
			// static void TranslatePoint(const float *source, float *dest, const float *matrix)
			TranslatePoint(Source, Dest, ObjectViewMatrix); // local to world?
#ifndef USE_D3DVIEWTRANSFORM
			TranslatePoint(Dest, Source, ViewMatrix); // world to view?
			f2i(destPtr->vx, Source[0]);
			f2i(destPtr->vy, Source[1]);
			f2i(destPtr->vz, Source[2]);
#else // bjd - view matrix test
			//          f2i(destPtr->vx, Dest[0]);
			//          f2i(destPtr->vy, Dest[1]);
			//          f2i(destPtr->vz, Dest[2]);
			destPtr->vx = (int)Dest[0];
			destPtr->vy = (int)Dest[1];
			destPtr->vz = (int)Dest[2];
#endif
			srcPtr++;
			destPtr++;
		}
	}
}

void RenderDecal(DECAL *decalPtr)
{
#ifndef USE_D3DVIEWTRANSFORM
	VECTORCH translatedPosition;
	/* translate decal into view space */
	translatedPosition = decalPtr->Vertices[0];
	TranslatePointIntoViewspace2(&translatedPosition);

	VerticesBuffer[0].X = translatedPosition.vx;
	VerticesBuffer[0].Y = translatedPosition.vy;
	VerticesBuffer[0].Z = translatedPosition.vz;

	translatedPosition = decalPtr->Vertices[1];
	TranslatePointIntoViewspace2(&translatedPosition);
	VerticesBuffer[1].X = translatedPosition.vx;
	VerticesBuffer[1].Y = translatedPosition.vy;
	VerticesBuffer[1].Z = translatedPosition.vz;

	translatedPosition = decalPtr->Vertices[2];
	TranslatePointIntoViewspace2(&translatedPosition);
	VerticesBuffer[2].X = translatedPosition.vx;
	VerticesBuffer[2].Y = translatedPosition.vy;
	VerticesBuffer[2].Z = translatedPosition.vz;

	translatedPosition = decalPtr->Vertices[3];
	TranslatePointIntoViewspace2(&translatedPosition);
	VerticesBuffer[3].X = translatedPosition.vx;
	VerticesBuffer[3].Y = translatedPosition.vy;
	VerticesBuffer[3].Z = translatedPosition.vz;
#else
	VerticesBuffer[0].X = decalPtr->Vertices[0].vx;
	VerticesBuffer[0].Y = decalPtr->Vertices[0].vy;
	VerticesBuffer[0].Z = decalPtr->Vertices[0].vz;
	VerticesBuffer[1].X = decalPtr->Vertices[1].vx;
	VerticesBuffer[1].Y = decalPtr->Vertices[1].vy;
	VerticesBuffer[1].Z = decalPtr->Vertices[1].vz;
	VerticesBuffer[2].X = decalPtr->Vertices[2].vx;
	VerticesBuffer[2].Y = decalPtr->Vertices[2].vy;
	VerticesBuffer[2].Z = decalPtr->Vertices[2].vz;
	VerticesBuffer[3].X = decalPtr->Vertices[3].vx;
	VerticesBuffer[3].Y = decalPtr->Vertices[3].vy;
	VerticesBuffer[3].Z = decalPtr->Vertices[3].vz;
#endif
	//  int outcode = DecalWithinFrustum(decalPtr);
	//  if (outcode) // bjd
	{
		switch (decalPtr->DecalID) {
			default:
			case DECAL_SCORCHED: {
				DecalPolygon_Construct(decalPtr);
				/*
				                if (0)//(outcode != 2) // bjd
				                {
				                    TexturedPolygon_ClipWithZ();
				                    if (RenderPolygon.NumberOfVertices < 3)
				                        return;

				                    TexturedPolygon_ClipWithNegativeX();
				                    if (RenderPolygon.NumberOfVertices < 3)
				                        return;

				                    TexturedPolygon_ClipWithPositiveY();
				                    if (RenderPolygon.NumberOfVertices < 3)
				                        return;

				                    TexturedPolygon_ClipWithNegativeY();
				                    if (RenderPolygon.NumberOfVertices < 3)
				                        return;

				                    TexturedPolygon_ClipWithPositiveX();
				                    if (RenderPolygon.NumberOfVertices < 3)
				                        return;

				                    D3D_Decal_Output(decalPtr, RenderPolygon.Vertices);
				                }
				                else*/ D3D_Decal_Output(decalPtr, VerticesBuffer);
				break;
			}
		}
	}
#if MIRRORING_ON

	if (MirroringActive) {
		RenderMirroredDecal(decalPtr);
	}

#endif
}

void RenderParticle(PARTICLE *particlePtr)
{
	int particleSize = particlePtr->Size;

	VECTORCH translatedPosition = particlePtr->Position;

	// do view transform
	TranslatePointIntoViewspace(&translatedPosition);
#if 0
	VECTORCHF tempVector;

	tempVector.vx = (float)particlePtr->Position.vx;
	tempVector.vy = (float) -particlePtr->Position.vy;
	tempVector.vz = (float)particlePtr->Position.vz;

	TransformToViewspace(&tempVector);
	translatedPosition.vx = (int)tempVector.vx;
	translatedPosition.vy = (int)tempVector.vy;
	translatedPosition.vz = (int)tempVector.vz;
#endif
	VerticesBuffer[0].X = translatedPosition.vx;
	VerticesBuffer[3].X = translatedPosition.vx;
	VerticesBuffer[0].Y = translatedPosition.vy;
	VerticesBuffer[3].Y = translatedPosition.vy;
	VerticesBuffer[0].Z = translatedPosition.vz;
	VerticesBuffer[3].Z = translatedPosition.vz;

	if ((particlePtr->ParticleID == PARTICLE_EXPLOSIONFIRE)
	    || (particlePtr->ParticleID == PARTICLE_RICOCHET_SPARK)
	    || (particlePtr->ParticleID == PARTICLE_SPARK)
	    || (particlePtr->ParticleID == PARTICLE_ORANGE_SPARK)
	    || (particlePtr->ParticleID == PARTICLE_ORANGE_PLASMA)
	    || (particlePtr->ParticleID == PARTICLE_ALIEN_BLOOD)
	    || (particlePtr->ParticleID == PARTICLE_PREDATOR_BLOOD)
	    || (particlePtr->ParticleID == PARTICLE_HUMAN_BLOOD)
	    || (particlePtr->ParticleID == PARTICLE_WATERFALLSPRAY)
	    || (particlePtr->ParticleID == PARTICLE_LASERBEAM)
	    || (particlePtr->ParticleID == PARTICLE_PLASMABEAM)
	    || (particlePtr->ParticleID == PARTICLE_TRACER)
	    || (particlePtr->ParticleID == PARTICLE_PREDPISTOL_FLECHETTE)
	    || (particlePtr->ParticleID == PARTICLE_PREDPISTOL_FLECHETTE_NONDAMAGING)
	   ) {
		VECTORCH translatedPosition = particlePtr->Offset;
		TranslatePointIntoViewspace(&translatedPosition);
#if 0
		{
			tempVector.vx = (float)particlePtr->Offset.vx;
			tempVector.vy = (float)-particlePtr->Offset.vy;
			tempVector.vz = (float)particlePtr->Offset.vz;
			TransformToViewspace(&tempVector);
			translatedPosition.vx = (int)tempVector.vx;
			translatedPosition.vy = (int)tempVector.vy;
			translatedPosition.vz = (int)tempVector.vz;
		}
#endif
		VerticesBuffer[1].X = translatedPosition.vx;
		VerticesBuffer[2].X = translatedPosition.vx;
		VerticesBuffer[1].Y = translatedPosition.vy;
		VerticesBuffer[2].Y = translatedPosition.vy;
		VerticesBuffer[1].Z = translatedPosition.vz;
		VerticesBuffer[2].Z = translatedPosition.vz;
		{
			int deltaX = VerticesBuffer[1].X - VerticesBuffer[0].X;
			int deltaY = VerticesBuffer[1].Y - VerticesBuffer[0].Y;
			int splitY = 0;

			if (deltaX >= 0) {
				if (deltaY >= 0) {
					if (deltaX > deltaY) {
						splitY = 1;
					}
				} else if (deltaX > -deltaY) {
					splitY = 1;
				}
			} else {
				if (deltaY >= 0) {
					if (-deltaX > deltaY) {
						splitY = 1;
					}
				} else if (-deltaX > -deltaY) {
					splitY = 1;
				}
			}

			if (splitY) {
				if (deltaX > 0) {
					/* 1 & 2 are more +ve in X */
					VerticesBuffer[0].X -= particleSize;
					VerticesBuffer[0].Y -= particleSize;//MUL_FIXED(particleSize,87381);
					VerticesBuffer[1].X += particleSize;
					VerticesBuffer[1].Y -= particleSize;//MUL_FIXED(particleSize,87381);
					VerticesBuffer[2].X += particleSize;
					VerticesBuffer[2].Y += particleSize;//MUL_FIXED(particleSize,87381);
					VerticesBuffer[3].X -= particleSize;
					VerticesBuffer[3].Y += particleSize;//MUL_FIXED(particleSize,87381);
				} else {
					/* 1 & 2 are more -ve in X */
					VerticesBuffer[0].X += particleSize;
					VerticesBuffer[0].Y -= particleSize;//MUL_FIXED(particleSize,87381);
					VerticesBuffer[1].X -= particleSize;
					VerticesBuffer[1].Y -= particleSize;//MUL_FIXED(particleSize,87381);
					VerticesBuffer[2].X -= particleSize;
					VerticesBuffer[2].Y += particleSize;//MUL_FIXED(particleSize,87381);
					VerticesBuffer[3].X += particleSize;
					VerticesBuffer[3].Y += particleSize;//MUL_FIXED(particleSize,87381);
				}
			} else {
				if (deltaY > 0) {
					/* 1 & 2 are more +ve in Y */
					VerticesBuffer[0].X -= particleSize;
					VerticesBuffer[0].Y -= particleSize;//MUL_FIXED(particleSize,87381);
					VerticesBuffer[1].X -= particleSize;
					VerticesBuffer[1].Y += particleSize;//MUL_FIXED(particleSize,87381);
					VerticesBuffer[2].X += particleSize;
					VerticesBuffer[2].Y += particleSize;//MUL_FIXED(particleSize,87381);
					VerticesBuffer[3].X += particleSize;
					VerticesBuffer[3].Y -= particleSize;//MUL_FIXED(particleSize,87381);
				} else {
					/* 1 & 2 are more -ve in Y */
					VerticesBuffer[0].X -= particleSize;
					VerticesBuffer[0].Y += particleSize;//MUL_FIXED(particleSize,87381);
					VerticesBuffer[1].X -= particleSize;
					VerticesBuffer[1].Y -= particleSize;//MUL_FIXED(particleSize,87381);
					VerticesBuffer[2].X += particleSize;
					VerticesBuffer[2].Y -= particleSize;//MUL_FIXED(particleSize,87381);
					VerticesBuffer[3].X += particleSize;
					VerticesBuffer[3].Y += particleSize;//MUL_FIXED(particleSize,87381);
				}
			}
		}
	} else { // pulse rifle muzzle flash handled here - bjd
		VECTOR2D offset[4];
		VerticesBuffer[1].X = VerticesBuffer[0].X;
		VerticesBuffer[2].X = VerticesBuffer[0].X;
		VerticesBuffer[1].Y = VerticesBuffer[0].Y;
		VerticesBuffer[2].Y = VerticesBuffer[0].Y;
		VerticesBuffer[1].Z = VerticesBuffer[0].Z;
		VerticesBuffer[2].Z = VerticesBuffer[0].Z;
		offset[0].vx = -particleSize;
		offset[0].vy = -particleSize;
		offset[1].vx = +particleSize;
		offset[1].vy = -particleSize;
		offset[2].vx = +particleSize;
		offset[2].vy = +particleSize;
		offset[3].vx = -particleSize;
		offset[3].vy = +particleSize;

		if ((particlePtr->ParticleID == PARTICLE_MUZZLEFLASH)) {
			int theta = FastRandom() & 4095;
			RotateVertex(&offset[0], theta);
			RotateVertex(&offset[1], theta);
			RotateVertex(&offset[2], theta);
			RotateVertex(&offset[3], theta);
		} else if ((particlePtr->ParticleID == PARTICLE_SMOKECLOUD)
		           || (particlePtr->ParticleID == PARTICLE_GUNMUZZLE_SMOKE)
		           || (particlePtr->ParticleID == PARTICLE_PARGEN_FLAME)
		           || (particlePtr->ParticleID == PARTICLE_FLAME)) {
			int theta = (particlePtr->Offset.vx + MUL_FIXED(CloakingPhase, particlePtr->Offset.vy)) & 4095;
			RotateVertex(&offset[0], theta);
			RotateVertex(&offset[1], theta);
			RotateVertex(&offset[2], theta);
			RotateVertex(&offset[3], theta);
		}

		VerticesBuffer[0].X += offset[0].vx;
		VerticesBuffer[0].Y += offset[0].vy;
		VerticesBuffer[1].X += offset[1].vx;
		VerticesBuffer[1].Y += offset[1].vy;
		VerticesBuffer[2].X += offset[2].vx;
		VerticesBuffer[2].Y += offset[2].vy;
		VerticesBuffer[3].X += offset[3].vx;
		VerticesBuffer[3].Y += offset[3].vy;
	}

	{
		/* bjd - bypass this clipping stuff
		        int outcode = QuadWithinFrustum();

		        if (outcode)
		        {
		            ParticlePolygon_Construct(particlePtr);

		            if (outcode != 2)
		            {
		                TexturedPolygon_ClipWithZ();
		                if (RenderPolygon.NumberOfVertices<3) return;
		                TexturedPolygon_ClipWithNegativeX();
		                if (RenderPolygon.NumberOfVertices<3) return;
		                TexturedPolygon_ClipWithPositiveY();
		                if (RenderPolygon.NumberOfVertices<3) return;
		                TexturedPolygon_ClipWithNegativeY();
		                if (RenderPolygon.NumberOfVertices<3) return;
		                TexturedPolygon_ClipWithPositiveX();
		                if (RenderPolygon.NumberOfVertices<3) return;
		//              D3D_Particle_Output(particlePtr,RenderPolygon.Vertices);
		                AddParticle(particlePtr, &RenderPolygon.Vertices[0]);
		            }
		            else AddParticle(particlePtr, &VerticesBuffer[0]);//D3D_Particle_Output(particlePtr,VerticesBuffer);
		        }
		*/
		ParticlePolygon_Construct(particlePtr); // bjd - copy from commented out code above, remove this is above is uncommented
		AddParticle(particlePtr, VerticesBuffer);
	}
}

extern void RenderFlechetteParticle(PARTICLE *particlePtr)
{
	VECTORCH vertices[5];
	MATRIXCH mat;
	MakeMatrixFromDirection(&particlePtr->Velocity, &mat);
	mat.mat11 >>= 12;
	mat.mat12 >>= 12;
	mat.mat13 >>= 12;
	mat.mat21 >>= 12;
	mat.mat22 >>= 12;
	mat.mat23 >>= 12;
	mat.mat31 >>= 9;
	mat.mat32 >>= 9;
	mat.mat33 >>= 9;
	vertices[0].vx = particlePtr->Position.vx - mat.mat31 + mat.mat11;
	vertices[0].vy = particlePtr->Position.vy - mat.mat32 + mat.mat12;
	vertices[0].vz = particlePtr->Position.vz - mat.mat33 + mat.mat13;
	vertices[1].vx = particlePtr->Position.vx - mat.mat31 - mat.mat11;
	vertices[1].vy = particlePtr->Position.vy - mat.mat32 - mat.mat12;
	vertices[1].vz = particlePtr->Position.vz - mat.mat33 - mat.mat13;
	vertices[2] = particlePtr->Position;
	vertices[3].vx = particlePtr->Position.vx - mat.mat31 + mat.mat21;
	vertices[3].vy = particlePtr->Position.vy - mat.mat32 + mat.mat22;
	vertices[3].vz = particlePtr->Position.vz - mat.mat33 + mat.mat23;
	vertices[4].vx = particlePtr->Position.vx - mat.mat31 - mat.mat21;
	vertices[4].vy = particlePtr->Position.vy - mat.mat32 - mat.mat22;
	vertices[4].vz = particlePtr->Position.vz - mat.mat33 - mat.mat23;

	TranslatePointIntoViewspace(&vertices[0]);
	TranslatePointIntoViewspace(&vertices[1]);
	TranslatePointIntoViewspace(&vertices[2]);
	TranslatePointIntoViewspace(&vertices[3]);
	TranslatePointIntoViewspace(&vertices[4]);

	for (int i = 0; i < 3; i++) {
		VerticesBuffer[i].X = vertices[i].vx;
		VerticesBuffer[i].Y = vertices[i].vy;
		VerticesBuffer[i].Z = vertices[i].vz;
		VerticesBuffer[i].R = (particlePtr->Colour >> 16) & 255;
		VerticesBuffer[i].G = (particlePtr->Colour >> 8) & 255;
		VerticesBuffer[i].B = (particlePtr->Colour) & 255;
		VerticesBuffer[i].A = (particlePtr->Colour >> 24) & 255;
	}

	RenderPolygon.NumberOfVertices = 3;
	RenderPolygon.TranslucencyMode = TRANSLUCENCY_NORMAL;
	int outcode = TriangleWithinFrustum();
	POLYHEADER fakeHeader;
	fakeHeader.PolyFlags  = iflag_transparent;

	do {
		if (/*outcode*/1) {
			if (/*outcode!=2*/0) {
				GouraudPolygon_ClipWithZ();

				if (RenderPolygon.NumberOfVertices < 3) {
					continue;
				}

				GouraudPolygon_ClipWithNegativeX();

				if (RenderPolygon.NumberOfVertices < 3) {
					continue;
				}

				GouraudPolygon_ClipWithPositiveY();

				if (RenderPolygon.NumberOfVertices < 3) {
					continue;
				}

				GouraudPolygon_ClipWithNegativeY();

				if (RenderPolygon.NumberOfVertices < 3) {
					continue;
				}

				GouraudPolygon_ClipWithPositiveX();

				if (RenderPolygon.NumberOfVertices < 3) {
					continue;
				}

				D3D_ZBufferedGouraudPolygon_Output(&fakeHeader, RenderPolygon.Vertices);
			} else {
				D3D_ZBufferedGouraudPolygon_Output(&fakeHeader, VerticesBuffer);
			}
		}
	} while (0);

	for (int i = 0; i < 3; i++) {
		VerticesBuffer[i].X = vertices[i + 2].vx;
		VerticesBuffer[i].Y = vertices[i + 2].vy;
		VerticesBuffer[i].Z = vertices[i + 2].vz;
		VerticesBuffer[i].R = (particlePtr->Colour >> 16) & 255;
		VerticesBuffer[i].G = (particlePtr->Colour >> 8) & 255;
		VerticesBuffer[i].B = (particlePtr->Colour) & 255;
		VerticesBuffer[i].A = (particlePtr->Colour >> 24) & 255;
	}

	RenderPolygon.NumberOfVertices = 3;
	RenderPolygon.TranslucencyMode = TRANSLUCENCY_NORMAL;
	outcode = TriangleWithinFrustum();
	fakeHeader.PolyFlags = iflag_transparent;

	do {
		if (/*outcode*/1) {
			if (/*outcode!=2*/0) {
				GouraudPolygon_ClipWithZ();

				if (RenderPolygon.NumberOfVertices < 3) {
					continue;
				}

				GouraudPolygon_ClipWithNegativeX();

				if (RenderPolygon.NumberOfVertices < 3) {
					continue;
				}

				GouraudPolygon_ClipWithPositiveY();

				if (RenderPolygon.NumberOfVertices < 3) {
					continue;
				}

				GouraudPolygon_ClipWithNegativeY();

				if (RenderPolygon.NumberOfVertices < 3) {
					continue;
				}

				GouraudPolygon_ClipWithPositiveX();

				if (RenderPolygon.NumberOfVertices < 3) {
					continue;
				}

				D3D_ZBufferedGouraudPolygon_Output(&fakeHeader, RenderPolygon.Vertices);
			} else {
				D3D_ZBufferedGouraudPolygon_Output(&fakeHeader, VerticesBuffer);
			}
		}
	} while (0);
}

static void ParticlePolygon_Construct(PARTICLE *particlePtr)
{
	PARTICLE_DESC *particleDescPtr = &ParticleDescription[particlePtr->ParticleID];
	RenderPolygon.NumberOfVertices = 4;
	VerticesBuffer[0].U = particleDescPtr->StartU;
	VerticesBuffer[0].V = particleDescPtr->StartV;
	VerticesBuffer[1].U = particleDescPtr->EndU;
	VerticesBuffer[1].V = particleDescPtr->StartV;
	VerticesBuffer[2].U = particleDescPtr->EndU;
	VerticesBuffer[2].V = particleDescPtr->EndV;
	VerticesBuffer[3].U = particleDescPtr->StartU;
	VerticesBuffer[3].V = particleDescPtr->EndV;
}

void RenderMirroredDecal(DECAL *decalPtr)
{
	VECTORCH translatedPosition;
	/* translate decal into view space */
	translatedPosition = decalPtr->Vertices[0];
	translatedPosition.vx = MirroringAxis - translatedPosition.vx;
	TranslatePointIntoViewspace(&translatedPosition);
	VerticesBuffer[0].X = translatedPosition.vx;
	VerticesBuffer[0].Y = translatedPosition.vy;
	VerticesBuffer[0].Z = translatedPosition.vz;
	translatedPosition = decalPtr->Vertices[1];
	translatedPosition.vx = MirroringAxis - translatedPosition.vx;
	TranslatePointIntoViewspace(&translatedPosition);
	VerticesBuffer[1].X = translatedPosition.vx;
	VerticesBuffer[1].Y = translatedPosition.vy;
	VerticesBuffer[1].Z = translatedPosition.vz;
	translatedPosition = decalPtr->Vertices[2];
	translatedPosition.vx = MirroringAxis - translatedPosition.vx;
	TranslatePointIntoViewspace(&translatedPosition);
	VerticesBuffer[2].X = translatedPosition.vx;
	VerticesBuffer[2].Y = translatedPosition.vy;
	VerticesBuffer[2].Z = translatedPosition.vz;
	translatedPosition = decalPtr->Vertices[3];
	translatedPosition.vx = MirroringAxis - translatedPosition.vx;
	TranslatePointIntoViewspace(&translatedPosition);
	VerticesBuffer[3].X = translatedPosition.vx;
	VerticesBuffer[3].Y = translatedPosition.vy;
	VerticesBuffer[3].Z = translatedPosition.vz;

	//  int outcode = DecalWithinFrustum(decalPtr);
	//  if (outcode) // bjd
	{
		switch (decalPtr->DecalID) {
			default:
			case DECAL_SCORCHED: {
				DecalPolygon_Construct(decalPtr);

				if (/*outcode!=2*/0) { // bjd
					TexturedPolygon_ClipWithZ();

					if (RenderPolygon.NumberOfVertices < 3) {
						return;
					}

					TexturedPolygon_ClipWithNegativeX();

					if (RenderPolygon.NumberOfVertices < 3) {
						return;
					}

					TexturedPolygon_ClipWithPositiveY();

					if (RenderPolygon.NumberOfVertices < 3) {
						return;
					}

					TexturedPolygon_ClipWithNegativeY();

					if (RenderPolygon.NumberOfVertices < 3) {
						return;
					}

					TexturedPolygon_ClipWithPositiveX();

					if (RenderPolygon.NumberOfVertices < 3) {
						return;
					}

					D3D_Decal_Output(decalPtr, RenderPolygon.Vertices);
				} else {
					D3D_Decal_Output(decalPtr, VerticesBuffer);
				}

				break;
			}
		}
	}
}

static void DecalPolygon_Construct(DECAL *decalPtr)
{
	DECAL_DESC *decalDescPtr = &DecalDescription[decalPtr->DecalID];
	RenderPolygon.NumberOfVertices = 4;
	VerticesBuffer[0].U = decalDescPtr->StartU + decalPtr->UOffset;
	VerticesBuffer[0].V = decalDescPtr->StartV;
	VerticesBuffer[1].U = decalDescPtr->EndU + decalPtr->UOffset;
	VerticesBuffer[1].V = decalDescPtr->StartV;
	VerticesBuffer[2].U = decalDescPtr->EndU + decalPtr->UOffset;
	VerticesBuffer[2].V = decalDescPtr->EndV;
	VerticesBuffer[3].U = decalDescPtr->StartU + decalPtr->UOffset;
	VerticesBuffer[3].V = decalDescPtr->EndV;
}

void FindIntersectionWithYPlane(VECTORCH *startPtr, VECTORCH *directionPtr, VECTORCH *intersectionPtr)
{
	int lambda = DIV_FIXED(intersectionPtr->vy - startPtr->vy, directionPtr->vy);
	intersectionPtr->vx = startPtr->vx + MUL_FIXED(directionPtr->vx, lambda);
	intersectionPtr->vz = startPtr->vz + MUL_FIXED(directionPtr->vz, lambda);
	//  textprint("%d %d %d\n",intersectionPtr->vx,intersectionPtr->vy,intersectionPtr->vz);
}

void FindZFromXYIntersection(VECTORCH *startPtr, VECTORCH *directionPtr, VECTORCH *intersectionPtr)
{
	float a = (float)(intersectionPtr->vx - startPtr->vx);
	a /= directionPtr->vx;
	intersectionPtr->vz = (float)(startPtr->vz + (directionPtr->vz * a));
	//  textprint("%d %d %d\n",intersectionPtr->vx,intersectionPtr->vy,intersectionPtr->vz);
}

void ClearTranslucentPolyList(void)
{
	CurrentNumberOfTranslucentPolygons = 0;
}

void AddToTranslucentPolyList(POLYHEADER *inputPolyPtr, RENDERVERTEX *renderVerticesPtr)
{
	/* copy the data to the list for processing later */
	int i = RenderPolygon.NumberOfVertices;
	int maxZ = 0;
	RENDERVERTEX *vertexPtr = TranslucentPolygons[CurrentNumberOfTranslucentPolygons].Vertices;
	TranslucentPolygons[CurrentNumberOfTranslucentPolygons].NumberOfVertices = i;

	do {
#ifdef USE_D3DVIEWTRANSFORM // bjd - temp fix to get glass to render correctly. should I need to view transform these?
		VECTORCHF test;
		test.vx = renderVerticesPtr->X;
		test.vy = renderVerticesPtr->Y;
		test.vz = renderVerticesPtr->Z;
		TransformToViewspace(&test);

		if (maxZ < test.vz) {
			maxZ = test.vz;
		}

#else

		if (maxZ < renderVerticesPtr->Z) {
			maxZ = renderVerticesPtr->Z;
		}

#endif
		*vertexPtr++ = *renderVerticesPtr++;
	} while (--i);

	TranslucentPolygons[CurrentNumberOfTranslucentPolygons].MaxZ = maxZ;
	TranslucentPolygonHeaders[CurrentNumberOfTranslucentPolygons] = *inputPolyPtr;

	CurrentNumberOfTranslucentPolygons++;
	LOCALASSERT(CurrentNumberOfTranslucentPolygons < MAX_NO_OF_TRANSLUCENT_POLYGONS);
}

void OutputTranslucentPolyList(void)
{
	int i = CurrentNumberOfTranslucentPolygons;

	while (i--) {
		int k = CurrentNumberOfTranslucentPolygons;
		int maxFound = 0;

		while (k--) {
			if (TranslucentPolygons[k].MaxZ > TranslucentPolygons[maxFound].MaxZ) {
				maxFound = k;
			}
		}

		RenderAllParticlesFurtherAwayThan(TranslucentPolygons[maxFound].MaxZ);
		RenderPolygon.NumberOfVertices = TranslucentPolygons[maxFound].NumberOfVertices;
		RenderPolygon.TranslucencyMode = TRANSLUCENCY_NORMAL;
		D3D_ZBufferedGouraudTexturedPolygon_Output(&TranslucentPolygonHeaders[maxFound], TranslucentPolygons[maxFound].Vertices);
		TranslucentPolygons[maxFound].MaxZ = 0;
	}

	RenderAllParticlesFurtherAwayThan(-0x7fffffff);
}

EULER CubeOrient = {0, 0, 0};
int CuboidPolyVertexU[][4] = {
	{1, 1, 1, 1},

	{127, 127, 0, 0},
	{128, 128, 255, 255},

	{127, 127, 0, 0},
	{128, 128, 255, 255},
};

int CuboidPolyVertexV[][4] = {
	{1, 1, 1, 1},

	{127, 0, 0, 127},
	{127, 0, 0, 127},
	{128, 255, 255, 128},
	{128, 255, 255, 128},
};

// Draws some dirt on the smaller of the two mirrors in
// first room of marine level 1
void RenderMirrorSurface(void)
{
	VECTORCH translatedPts[4] = {
		{ -5596, -932, -1872},
		{ -5596, -932, -702},
		{ -5596, 1212, -702},
		{ -5596, 1212, -1872},
	};
	int mirrorUV[] =
	{ 0, 0, 127, 0, 127, 127, 0, 127};
	POLYHEADER fakeHeader;
	fakeHeader.PolyFlags  = iflag_transparent;
	fakeHeader.PolyColour = CloudyImageNumber;

	for (int i = 0; i < 4; i++) {
		TranslatePointIntoViewspace(&translatedPts[i]);
		VerticesBuffer[i].X = translatedPts[i].vx;
		VerticesBuffer[i].Y = translatedPts[i].vy;
		VerticesBuffer[i].Z = translatedPts[i].vz;
		VerticesBuffer[i].U = mirrorUV[i * 2];
		VerticesBuffer[i].V = mirrorUV[i * 2 + 1];
		VerticesBuffer[i].R = 255;
		VerticesBuffer[i].G = 255;
		VerticesBuffer[i].B = 255;
		VerticesBuffer[i].A = 128;
		VerticesBuffer[i].SpecularR = 0;
		VerticesBuffer[i].SpecularG = 0;
		VerticesBuffer[i].SpecularB = 0;
	}

	RenderPolygon.NumberOfVertices = 4;
	RenderPolygon.TranslucencyMode = TRANSLUCENCY_COLOUR;
	/* bjd - bypass clipping
	    GouraudTexturedPolygon_ClipWithZ();
	    if(RenderPolygon.NumberOfVertices<3) return;
	    GouraudTexturedPolygon_ClipWithNegativeX();
	    if(RenderPolygon.NumberOfVertices<3) return;
	    GouraudTexturedPolygon_ClipWithPositiveY();
	    if(RenderPolygon.NumberOfVertices<3) return;
	    GouraudTexturedPolygon_ClipWithNegativeY();
	    if(RenderPolygon.NumberOfVertices<3) return;
	    GouraudTexturedPolygon_ClipWithPositiveX();
	    if(RenderPolygon.NumberOfVertices<3) return;
	    D3D_ZBufferedGouraudTexturedPolygon_Output(&fakeHeader,RenderPolygon.Vertices);
	*/
	D3D_ZBufferedGouraudTexturedPolygon_Output(&fakeHeader, /*RenderPolygon.Vertices*/&VerticesBuffer[0]);
}

// Draws some dirt on the larger of the two mirrors in
// first room of marine level 1
void RenderMirrorSurface2(void)
{
	VECTORCH translatedPts[4] = {
		{ -5596, -592, 562},
		{ -5596, -592, 1344},
		{ -5596, 140,  1344},
		{ -5596, 140,  562},
	};
	int mirrorUV[] =
	{ 0, 0, 127, 0, 127, 127, 0, 127};
	POLYHEADER fakeHeader;
	fakeHeader.PolyFlags  = iflag_transparent;
	fakeHeader.PolyColour = CloudyImageNumber;

	for (int i = 0; i < 4; i++) {
		TranslatePointIntoViewspace(&translatedPts[i]);
		VerticesBuffer[i].X = translatedPts[i].vx;
		VerticesBuffer[i].Y = translatedPts[i].vy;
		VerticesBuffer[i].Z = translatedPts[i].vz;
		VerticesBuffer[i].U = mirrorUV[i * 2];
		VerticesBuffer[i].V = mirrorUV[i * 2 + 1];
		VerticesBuffer[i].R = 255;
		VerticesBuffer[i].G = 255;
		VerticesBuffer[i].B = 255;
		VerticesBuffer[i].A = 128;
		VerticesBuffer[i].SpecularR = 0;
		VerticesBuffer[i].SpecularG = 0;
		VerticesBuffer[i].SpecularB = 0;
	}

	RenderPolygon.NumberOfVertices = 4;
	RenderPolygon.TranslucencyMode = TRANSLUCENCY_COLOUR;
	/* bjd - bypass clipping
	    GouraudTexturedPolygon_ClipWithZ();
	    if(RenderPolygon.NumberOfVertices<3) return;
	    GouraudTexturedPolygon_ClipWithNegativeX();
	    if(RenderPolygon.NumberOfVertices<3) return;
	    GouraudTexturedPolygon_ClipWithPositiveY();
	    if(RenderPolygon.NumberOfVertices<3) return;
	    GouraudTexturedPolygon_ClipWithNegativeY();
	    if(RenderPolygon.NumberOfVertices<3) return;
	    GouraudTexturedPolygon_ClipWithPositiveX();
	    if(RenderPolygon.NumberOfVertices<3) return;
	    D3D_ZBufferedGouraudTexturedPolygon_Output(&fakeHeader,RenderPolygon.Vertices);
	*/
	D3D_ZBufferedGouraudTexturedPolygon_Output(&fakeHeader, /*RenderPolygon.Vertices*/&VerticesBuffer[0]);
}

void RenderWaterFall(int xOrigin, int yOrigin, int zOrigin)
{
	VECTORCH v[4];
	int i, z;
	int waterfallX[9];
	int waterfallY[9];
	int waterfallZ[9];
	int waterfallZScale[9];

	for (i = 0; i < 9; i++) {
		int u = (i * 65536) / 8;
		int b = MUL_FIXED(2 * u, (65536 - u));
		int c = MUL_FIXED(u, u);
		int y3 = (4742 - yOrigin);
		int x3 = 2000;
		int y2 = 2000;
		int x2 = 1500;
		waterfallX[i] = MUL_FIXED(b, x2) + MUL_FIXED(c, x3);
		waterfallY[i] = yOrigin + MUL_FIXED(b, y2) + MUL_FIXED(c, y3);
		waterfallZ[i] = zOrigin + MUL_FIXED((66572 - zOrigin), u);
		waterfallZScale[i] = ONE_FIXED + b / 2 - c;

		if (i != 8) {
			waterfallZScale[i] += (FastRandom() & 8191);
			waterfallY[i] -= (FastRandom() & 127);
		}
	}

	for (z = 0; z < 8; z++)
		for (i = 0; i < 8; i++) {
			v[0].vx = xOrigin + MUL_FIXED(waterfallX[i], waterfallZScale[z]);
			v[1].vx = xOrigin + MUL_FIXED(waterfallX[i], waterfallZScale[z + 1]);
			v[2].vx = xOrigin + MUL_FIXED(waterfallX[i + 1], waterfallZScale[z + 1]);
			v[3].vx = xOrigin + MUL_FIXED(waterfallX[i + 1], waterfallZScale[z]);
			v[0].vy = waterfallY[i];
			v[1].vy = waterfallY[i];
			v[2].vy = waterfallY[i + 1];
			v[3].vy = waterfallY[i + 1];
			v[0].vz = waterfallZ[z];
			v[1].vz = waterfallZ[z + 1];
			v[2].vz = v[1].vz;
			v[3].vz = v[0].vz;
			DrawWaterFallPoly(v);
		}

	for (z = 0; z < 3; z++) {
		v[0].vx = xOrigin + MUL_FIXED(waterfallX[8], waterfallZScale[z + 1]);
		v[1].vx = xOrigin + MUL_FIXED(waterfallX[8], waterfallZScale[z]);
		v[2].vx = 179450;
		v[3].vx = 179450;
		v[0].vy = 4742;
		v[1].vy = 4742;
		v[2].vy = 4742;
		v[3].vy = 4742;
		v[0].vz = waterfallZ[z];
		v[1].vz = waterfallZ[z + 1];
		v[2].vz = v[1].vz;
		v[3].vz = v[0].vz;
		DrawWaterFallPoly(v);
	}

	for (z = 0; z < 8; z++)
		for (i = 0; i < 16; i++) {
			int xOffset, xOffset2;

			if (z < 3) {
				xOffset = 179450;
			} else {
				xOffset = xOrigin + MUL_FIXED(waterfallX[8], waterfallZScale[z]);
			}

			if (z < 2) {
				xOffset2 = 179450;
			} else {
				xOffset2 = xOrigin + MUL_FIXED(waterfallX[8], waterfallZScale[z + 1]);
			}

			v[0].vx = xOffset;
			v[1].vx = xOffset2;
			v[2].vx = xOffset2;
			v[3].vx = xOffset;
			v[0].vy = 4742 + i * 4096;
			v[1].vy = 4742 + i * 4096;
			v[2].vy = 4742 + (i + 1) * 4096;
			v[3].vy = 4742 + (i + 1) * 4096;
			v[0].vz = waterfallZ[z];
			v[1].vz = waterfallZ[z + 1];
			v[2].vz = v[1].vz;
			v[3].vz = v[0].vz;
			DrawWaterFallPoly(v);
		}
}

void DrawWaterFallPoly(VECTORCH *v)
{
	POLYHEADER fakeHeader;
	fakeHeader.PolyFlags = iflag_transparent;
	fakeHeader.PolyColour = CloudyImageNumber;
	RenderPolygon.TranslucencyMode = TRANSLUCENCY_NORMAL;
	static int wv = 0;

	for (unsigned int a = 0; a < 4; a++) {
		VerticesBuffer[a].A = 128;
		VerticesBuffer[a].U = (v[a].vz) << 11;
		VerticesBuffer[a].V = (v[a].vy << 10) - wv;
		TranslatePointIntoViewspace(&v[a]);
		VerticesBuffer[a].X = v[a].vx;
		VerticesBuffer[a].Y = v[a].vy;
		VerticesBuffer[a].Z = v[a].vz;
		VerticesBuffer[a].R = 200;
		VerticesBuffer[a].G = 200;
		VerticesBuffer[a].B = 255;
		VerticesBuffer[a].SpecularR = 0;
		VerticesBuffer[a].SpecularG = 0;
		VerticesBuffer[a].SpecularB = 0;
	}

	wv += NormalFrameTime * 2;
	RenderPolygon.NumberOfVertices = 4;
	//  GouraudTexturedPolygon_ClipWithZ();
	D3D_ZBufferedGouraudTexturedPolygon_Output(&fakeHeader, VerticesBuffer);
	/*
	    if (RenderPolygon.NumberOfVertices>=3)
	    {
	        GouraudTexturedPolygon_ClipWithNegativeX();
	        if (RenderPolygon.NumberOfVertices>=3)
	        {
	            GouraudTexturedPolygon_ClipWithPositiveY();
	            if (RenderPolygon.NumberOfVertices>=3)
	            {
	                GouraudTexturedPolygon_ClipWithNegativeY();
	                if (RenderPolygon.NumberOfVertices>=3)
	                {
	                    GouraudTexturedPolygon_ClipWithPositiveX();
	                    if (RenderPolygon.NumberOfVertices>=3)
	                    {
	                        D3D_ZBufferedGouraudTexturedPolygon_Output(&fakeHeader,RenderPolygon.Vertices);
	                    }
	                }
	            }
	        }
	    }
	*/
}

void RenderPredatorTargetingSegment(int theta, int scale, int drawInRed)
{

#ifdef USE_D3DVIEWTRANSFORM
	VECTOR2D offset[4];
	int centreX, centreY;
	int z = ONE_FIXED - scale;

	// Mel - In the end, all we needed to do was to subtract screen's width and height
	// from the SmartTargetSight, it's later used to aim predator shoulder cannon and
	// marine smartgun, so we won't alter it to keep those from breaking, still need
	// to fix targeting segment's size, but the vertical error in widescreen is almost
	// unoticeable
	fixed_t centeredSmartTargetSightX = SmartTargetSightX - (ScreenDescriptorBlock.SDB_Width << 15);
	fixed_t centeredSmartTargetSightY = SmartTargetSightY - (ScreenDescriptorBlock.SDB_Height << 15);
	z = MUL_FIXED(MUL_FIXED(z, z), 2048);
	{
		centreY = MUL_FIXED((centeredSmartTargetSightY - (ScreenDescriptorBlock.SDB_Height)) / Global_VDB_Ptr->VDB_ProjY, z);

		if (MIRROR_CHEATMODE) {
			centreX = MUL_FIXED((- (centeredSmartTargetSightX - (ScreenDescriptorBlock.SDB_Width))) / Global_VDB_Ptr->VDB_ProjX, z);
		} else {
			centreX = MUL_FIXED((centeredSmartTargetSightX - (ScreenDescriptorBlock.SDB_Width)) / Global_VDB_Ptr->VDB_ProjX, z);
		}
	}
	float posZ = z * CameraZoomScale;
	RHW_VERTEX list[4];
	{
		int a = 160 * CameraZoomScale;
		int b = 40  * CameraZoomScale;
		/* tan(30) = 1/sqrt(3), & 65536/(sqrt(3)) = 37837 */
		int y = MUL_FIXED(37837, a + 20);
		offset[0].vx = -a + MUL_FIXED(113512, b);
		offset[0].vy = y - b;
		offset[1].vx = -offset[0].vx;
		offset[1].vy = y - b;
		offset[2].vx = a;
		offset[2].vy = y;
		offset[3].vx = -a;
		offset[3].vy = y;

		if (theta) {
			RotateVertex(&offset[0], theta);
			RotateVertex(&offset[1], theta);
			RotateVertex(&offset[2], theta);
			RotateVertex(&offset[3], theta);
		}

		if (MIRROR_CHEATMODE) {
			offset[0].vx = -offset[0].vx;
			offset[1].vx = -offset[1].vx;
			offset[2].vx = -offset[2].vx;
			offset[3].vx = -offset[3].vx;
		}

		// Send our quad to the center of the targetting system
		for (int i = 0; i < 4; i++) {
			VerticesBuffer[i].X = (offset[i].vx + centreX);
			VerticesBuffer[i].Y = (offset[i].vy + centreY);
		}
	}
	{
		for (int i = 0; i < 4; i++) {
			VerticesBuffer[i].Z = z;

			if (drawInRed) {
				VerticesBuffer[i].R = 255;
				VerticesBuffer[i].G = 0;
				VerticesBuffer[i].B = 0;
				VerticesBuffer[i].A = 128;
			} else {
				VerticesBuffer[i].R = 255;
				VerticesBuffer[i].G = 255;
				VerticesBuffer[i].B = 255;
				VerticesBuffer[i].A = 128;
			}

			list[i].x = VerticesBuffer[i].X;
			list[i].y = VerticesBuffer[i].Y;
			list[i].z = posZ;
			list[i].color = RGBA_MAKE(VerticesBuffer[i].R, VerticesBuffer[i].G, VerticesBuffer[i].B, VerticesBuffer[i].A);
			list[i].rhw = posZ;
			list[i].u = 0.5f;
			list[i].v = 0.5f;
		}
	}

	if (drawInRed) {
		VerticesBuffer[0].X = MUL_FIXED(offset[3].vx, scale * 8) + centreX;
		VerticesBuffer[0].Y = MUL_FIXED(offset[3].vy, scale * 8) + centreY;
		VerticesBuffer[1].X = MUL_FIXED(offset[2].vx, scale * 8) + centreX;
		VerticesBuffer[1].Y = MUL_FIXED(offset[2].vy, scale * 8) + centreY;
		VerticesBuffer[2].X = offset[2].vx + centreX;
		VerticesBuffer[2].Y = offset[2].vy + centreY;
		VerticesBuffer[3].X = offset[3].vx + centreX;
		VerticesBuffer[3].Y = offset[3].vy + centreY;

		for (int i = 0; i < 2; i++) {
			VerticesBuffer[i].Z = z;
			VerticesBuffer[i].R = 255;
			VerticesBuffer[i].G = 0;
			VerticesBuffer[i].B = 0;
			VerticesBuffer[i].A = 0;
		}

		for (int i = 2; i < 4; i++) {
			VerticesBuffer[i].Z = z;
			VerticesBuffer[i].R = 255;
			VerticesBuffer[i].G = 0;
			VerticesBuffer[i].B = 0;
			VerticesBuffer[i].A = 128;
		}

		RenderPolygon.NumberOfVertices = 4;
	}

	ChangeZWriteEnable(ZWRITE_ENABLED);
	d3d.rhwDecl->Set();
	d3d.effectSystem->SetActive(d3d.rhwEffect);
	d3d.lpD3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, &list[0], sizeof(RHW_VERTEX));
#else

	VECTOR2D offset[4];
	POLYHEADER fakeHeader;
	int centreX, centreY;

	int z = ONE_FIXED-scale;

	z = MUL_FIXED(MUL_FIXED(z, z), 2048);

	centreY = MUL_FIXED((SmartTargetSightY-(ScreenDescriptorBlock.SDB_Height<<15)) / Global_VDB_Ptr->VDB_ProjY, z);
	if (MIRROR_CHEATMODE) {
		centreX = MUL_FIXED(( - (SmartTargetSightX-(ScreenDescriptorBlock.SDB_Width<<15))) / Global_VDB_Ptr->VDB_ProjX, z);
	}
	else {
		centreX = MUL_FIXED((SmartTargetSightX-(ScreenDescriptorBlock.SDB_Width<<15)) / Global_VDB_Ptr->VDB_ProjX, z);
	}

	z = (float)z*CameraZoomScale;

	{
		int a = 160;
		int b = 40;

		/* tan(30) = 1/sqrt(3), & 65536/(sqrt(3)) = 37837 */

		int y = MUL_FIXED(37837,a+20);

		offset[0].vx = -a+MUL_FIXED(113512,b);
		offset[0].vy = y-b;

		offset[1].vx = -offset[0].vx;
		offset[1].vy = y-b;

		offset[2].vx = a;
		offset[2].vy = y;

		offset[3].vx = -a;
		offset[3].vy = y;

		if (theta)
		{
			RotateVertex(&offset[0],theta);
			RotateVertex(&offset[1],theta);
			RotateVertex(&offset[2],theta);
			RotateVertex(&offset[3],theta);
		}

		if (MIRROR_CHEATMODE)
		{
			offset[0].vx = -offset[0].vx;
			offset[1].vx = -offset[1].vx;
			offset[2].vx = -offset[2].vx;
			offset[3].vx = -offset[3].vx;
		}
		VerticesBuffer[0].X = offset[0].vx+centreX;
		VerticesBuffer[0].Y = offset[0].vy+centreY;

		VerticesBuffer[1].X = offset[1].vx+centreX;
		VerticesBuffer[1].Y = offset[1].vy+centreY;

		VerticesBuffer[2].X = offset[2].vx+centreX;
		VerticesBuffer[2].Y = offset[2].vy+centreY;

		VerticesBuffer[3].X = offset[3].vx+centreX;
		VerticesBuffer[3].Y = offset[3].vy+centreY;
	}

	fakeHeader.PolyFlags = iflag_transparent;
	RenderPolygon.TranslucencyMode = TRANSLUCENCY_GLOWING;

	for (int i = 0; i < 4; i++)
	{
		VerticesBuffer[i].Z	= z;
		VerticesBuffer[i].R = 255;

		if (drawInRed)
		{
			VerticesBuffer[i].G	= 0;
			VerticesBuffer[i].B = 0;
		}
		else
		{
			VerticesBuffer[i].G	= 255;
			VerticesBuffer[i].B = 255;
		}

		VerticesBuffer[i].A = 128;
	}

	RenderPolygon.NumberOfVertices = 4;
/*
	GouraudPolygon_ClipWithZ();
	if(RenderPolygon.NumberOfVertices<3) return;
	GouraudPolygon_ClipWithNegativeX();
	if(RenderPolygon.NumberOfVertices<3) return;
	GouraudPolygon_ClipWithPositiveY();
	if(RenderPolygon.NumberOfVertices<3) return;
	GouraudPolygon_ClipWithNegativeY();
	if(RenderPolygon.NumberOfVertices<3) return;
	GouraudPolygon_ClipWithPositiveX();
	if(RenderPolygon.NumberOfVertices<3) return;
	D3D_ZBufferedGouraudPolygon_Output(&fakeHeader,RenderPolygon.Vertices);
*/
	D3D_ZBufferedGouraudPolygon_Output(&fakeHeader, VerticesBuffer);

	if (drawInRed)
	{
		VerticesBuffer[0].X = MUL_FIXED(offset[3].vx,scale*8)+centreX;
		VerticesBuffer[0].Y = MUL_FIXED(offset[3].vy,scale*8)+centreY;

		VerticesBuffer[1].X = MUL_FIXED(offset[2].vx,scale*8)+centreX;
		VerticesBuffer[1].Y = MUL_FIXED(offset[2].vy,scale*8)+centreY;

		VerticesBuffer[2].X = offset[2].vx+centreX;
		VerticesBuffer[2].Y = offset[2].vy+centreY;

		VerticesBuffer[3].X = offset[3].vx+centreX;
		VerticesBuffer[3].Y = offset[3].vy+centreY;
	 
		for (int i = 0; i < 2; i++)
		{
			VerticesBuffer[i].Z	= z;
			VerticesBuffer[i].R = 255;
			VerticesBuffer[i].G	= 0;
			VerticesBuffer[i].B = 0;
			VerticesBuffer[i].A = 0;
		}
		for (int i = 2; i < 4; i++)
		{
			VerticesBuffer[i].Z	= z;
			VerticesBuffer[i].R = 255;
			VerticesBuffer[i].G	= 0;
			VerticesBuffer[i].B = 0;
			VerticesBuffer[i].A = 128;
		}
		RenderPolygon.NumberOfVertices = 4;
/*
		GouraudPolygon_ClipWithZ();
		if(RenderPolygon.NumberOfVertices<3) return;
		GouraudPolygon_ClipWithNegativeX();
		if(RenderPolygon.NumberOfVertices<3) return;
		GouraudPolygon_ClipWithPositiveY();
		if(RenderPolygon.NumberOfVertices<3) return;
		GouraudPolygon_ClipWithNegativeY();
		if(RenderPolygon.NumberOfVertices<3) return;
		GouraudPolygon_ClipWithPositiveX();
		if(RenderPolygon.NumberOfVertices<3) return;
		D3D_ZBufferedGouraudPolygon_Output(&fakeHeader,RenderPolygon.Vertices);
*/
		D3D_ZBufferedGouraudPolygon_Output(&fakeHeader, VerticesBuffer);
	}

#endif
}

void RenderPredatorPlasmaCasterCharge(int value, VECTORCH *worldOffsetPtr, MATRIXCH *orientationPtr)
{
	POLYHEADER fakeHeader;
	VECTORCH translatedPts[4];
	int halfWidth = 100;
	int halfHeight = 4;
	int z = -1;
	translatedPts[0].vx = -halfWidth;
	translatedPts[0].vy = z;
	translatedPts[0].vz = -halfHeight - 4;
	translatedPts[1].vx = -halfWidth + MUL_FIXED(value, 2 * halfWidth - 10);
	translatedPts[1].vy = z;
	translatedPts[1].vz = -halfHeight - 4;
	translatedPts[2].vx = -halfWidth + MUL_FIXED(value, 2 * halfWidth - 10);
	translatedPts[2].vy = z;
	translatedPts[2].vz = halfHeight - 4;
	translatedPts[3].vx = -halfWidth;
	translatedPts[3].vy = z;
	translatedPts[3].vz = halfHeight - 4;
	fakeHeader.PolyFlags = iflag_transparent;
	RenderPolygon.TranslucencyMode = TRANSLUCENCY_GLOWING;
	{
		int i;

		for (i = 0; i < 4; i++) {
			VerticesBuffer[i].A = 255;
			RotateVector(&(translatedPts[i]), orientationPtr);
			translatedPts[i].vx += worldOffsetPtr->vx;
			translatedPts[i].vy += worldOffsetPtr->vy;
			translatedPts[i].vz += worldOffsetPtr->vz;
			TranslatePointIntoViewspace(&translatedPts[i]);
			VerticesBuffer[i].X = translatedPts[i].vx;
			VerticesBuffer[i].Y = translatedPts[i].vy;
			VerticesBuffer[i].Z = translatedPts[i].vz;
			VerticesBuffer[i].R = 32;
			VerticesBuffer[i].G = 0;
			VerticesBuffer[i].B = 0;
		}

		RenderPolygon.NumberOfVertices = 4;
	}
	{
		int outcode = QuadWithinFrustum();

		if (/*outcode*/1) {
			if (/*outcode!=2*/0) {
				GouraudPolygon_ClipWithZ();

				if (RenderPolygon.NumberOfVertices < 3) {
					return;
				}

				GouraudPolygon_ClipWithNegativeX();

				if (RenderPolygon.NumberOfVertices < 3) {
					return;
				}

				GouraudPolygon_ClipWithPositiveY();

				if (RenderPolygon.NumberOfVertices < 3) {
					return;
				}

				GouraudPolygon_ClipWithNegativeY();

				if (RenderPolygon.NumberOfVertices < 3) {
					return;
				}

				GouraudPolygon_ClipWithPositiveX();

				if (RenderPolygon.NumberOfVertices < 3) {
					return;
				}

				D3D_ZBufferedGouraudPolygon_Output(&fakeHeader, RenderPolygon.Vertices);
			} else {
				D3D_ZBufferedGouraudPolygon_Output(&fakeHeader, VerticesBuffer);
			}
		}
	}
}

void RenderLightFlare(VECTORCH *positionPtr, uint32_t colour)
{
	int z;
	PARTICLE particle;
	VECTORCH point = *positionPtr;
	VECTORCHF tempVector;

#ifndef USE_D3DVIEWTRANSFORM
	TranslatePointIntoViewspace2(&point);

	tempVector.vx = point.vx;
	tempVector.vy = -point.vy;
	tempVector.vz = point.vz;
#else
	tempVector.vx = (float)point.vx;
	tempVector.vy = (float)-point.vy;
	tempVector.vz = (float)point.vz;

	TransformToViewspace(&tempVector);
#endif

	if (tempVector.vz < 64) {
		return;
	}

	particle.ParticleID = PARTICLE_LIGHTFLARE;
	particle.Colour = colour;
	//  sprintf(buf, "render fn %d %d %d\n",positionPtr->vx, positionPtr->vy, positionPtr->vz);
	//  OutputDebugString(buf);
	//  textprint("render fn %d %d %d\n",positionPtr->vx,positionPtr->vy,positionPtr->vz);
	z = point.vz;
	//  z = ONE_FIXED;
	//  centreX = DIV_FIXED(point.vx, point.vz);
	//  centreY = DIV_FIXED(point.vy, point.vz);
	//  sizeX = (ScreenDescriptorBlock.SDB_Width<<13) / Global_VDB_Ptr->VDB_ProjX;
	//  sizeY = MUL_FIXED(ScreenDescriptorBlock.SDB_Height<<13, 87381) / Global_VDB_Ptr->VDB_ProjY;
	{
		int outcode = QuadWithinFrustum();

		if (/*outcode*/1) { // bjd
			RenderPolygon.NumberOfVertices = 4;
			//          textprint("On Screen!\n");
			/*
			            // top left
			            VerticesBuffer[1].U = 192;
			            VerticesBuffer[1].V = 0;

			            // top right
			            VerticesBuffer[3].U = 255;
			            VerticesBuffer[3].V = 0;

			            // bottom right
			            VerticesBuffer[2].U = 255;
			            VerticesBuffer[2].V = 63;

			            // bottom left
			            VerticesBuffer[0].U = 192;
			            VerticesBuffer[0].V = 63;
			*/
			AddCorona(&particle, &tempVector);
		}
	}
}

#if VOLUMETRIC_FOG

int FogValue(VECTORCH *vertexPtr)
{
	float a, b, c, d, lMax, lMin;
	VECTORCHF v;
	v.vx = vertexPtr->vx;
	v.vy = vertexPtr->vy;
	v.vz = vertexPtr->vz;
	a = (v.vx * v.vx + v.vy * v.vy + v.vz * v.vz) * 2.0;
	b = -2.0 * (v.vx * FogPosition.vx + v.vy * FogPosition.vy + v.vz * FogPosition.vz);
	{
		//      float s = MUL_FIXED(GetSin(CloakingPhase&4095),GetSin(CloakingPhase&4095));
		c = FogMagnitude - 10000.0 * 10000.0;
	}
	d = b * b - 2.0 * a * c;

	if (d < 0) {
		return 0;
	}

	d = sqrt(d);
	lMin = (-b - d) / (a);

	if (lMin > 1.0) {
		return 0;
	}

	lMax = (-b + d) / (a);

	if (lMax < 0.0) {
		return 0;
	} else if (lMax > 1.0) {
		if (lMin > 0.0) {
			float m;
			int f;
			m = Approximate3dMagnitude(vertexPtr);
			f2i(f, (lMax - lMin)*m);
			return f;
		} else {
			return Approximate3dMagnitude(vertexPtr);
		}
	} else { //(lMax<1.0)
		if (lMin > 0.0) {
			float m;
			int f;
			m = Approximate3dMagnitude(vertexPtr);
			f2i(f, (lMax - lMin)*m);
			return f;
		} else {
			float m;
			int f;
			m = Approximate3dMagnitude(vertexPtr);
			f2i(f, (lMax)*m);
			return f;
		}
	}
}
#endif

int Alpha[SPHERE_VERTICES];

void RenderExplosionSurface(VOLUMETRIC_EXPLOSION *explosionPtr)
{
	int red, green, blue;

	switch (CurrentVisionMode) {
		default:
		case VISION_MODE_NORMAL: {
			red   = 255;
			green = 255;
			blue  = 255;
			break;
		}

		case VISION_MODE_IMAGEINTENSIFIER: {
			red   = 0;
			green = 255;
			blue  = 0;
			break;
		}

		case VISION_MODE_PRED_THERMAL:
		case VISION_MODE_PRED_SEEALIENS:
		case VISION_MODE_PRED_SEEPREDTECH: {
			red   = 255;
			green = 0;
			blue  = 255;
			break;
		}
	}

	{
		POLYHEADER fakeHeader;
		VECTORCH *vSphere = SphereRotatedVertex;
		static int o = 0;
		o++;

		if (explosionPtr->ExplosionPhase) {
			fakeHeader.PolyFlags  = iflag_transparent;
			fakeHeader.PolyColour = BurningImageNumber;
			RenderPolygon.TranslucencyMode = TRANSLUCENCY_NORMAL;
		} else {
			fakeHeader.PolyFlags = iflag_transparent;
			fakeHeader.PolyColour = CloudyImageNumber;
			RenderPolygon.TranslucencyMode = TRANSLUCENCY_INVCOLOUR;
			red = explosionPtr->LifeTime / 256;
			green = explosionPtr->LifeTime / 256;
			blue = explosionPtr->LifeTime / 256;
		}

		for (int f = 0; f < SPHERE_VERTICES; f++) {
			*vSphere = explosionPtr->Position[f];
			TranslatePointIntoViewspace(vSphere);
			vSphere++;
		}

		for (int f = 0; f < SPHERE_FACES; f++) 
		{
			VECTORCH vertex[3];

			for (int i = 0; i < 3; i++) {
				int n = SphereFace[f].v[i];
				vertex[i] = SphereRotatedVertex[n];
			}

			for (int i = 0; i < 3; i++) 
			{
				int n = SphereFace[f].v[i];
				VerticesBuffer[i].X = vertex[i].vx;
				VerticesBuffer[i].Y = vertex[i].vy;
				VerticesBuffer[i].Z = vertex[i].vz;
				{
					int u = -(ONE_FIXED - explosionPtr->LifeTime) * 128 * 2;
					VerticesBuffer[i].U = (SphereAtmosU[n]);
					VerticesBuffer[i].V = (SphereAtmosV[n] + u);
				}
				{
					int d1 = VerticesBuffer[0].U - VerticesBuffer[1].U;
					int d2 = VerticesBuffer[0].U - VerticesBuffer[2].U;
					int d3 = VerticesBuffer[1].U - VerticesBuffer[2].U;
					int ad1 = d1, ad2 = d2, ad3 = d3;
					int i1 = 0, i2 = 0, i3 = 0;

					if (ad1 < 0) {
						ad1 = -ad1;
					}

					if (ad2 < 0) {
						ad2 = -ad2;
					}

					if (ad3 < 0) {
						ad3 = -ad3;
					}

					if (ad1 > (128 * (SPHERE_TEXTURE_WRAP - 1) + 64) * 65536) {
						if (d1 > 0) {
							i2 = 1;
						} else {
							i1 = 1;
						}
					}

					if (ad2 > (128 * (SPHERE_TEXTURE_WRAP - 1) + 64) * 65536) {
						if (d2 > 0) {
							i3 = 1;
						} else {
							i1 = 1;
						}
					}

					if (ad3 > (128 * (SPHERE_TEXTURE_WRAP - 1) + 64) * 65536) {
						if (d3 > 0) {
							i3 = 1;
						} else {
							i2 = 1;
						}
					}

					if (i1) {
						VerticesBuffer[0].U += 128 * 65536 * SPHERE_TEXTURE_WRAP;
					}

					if (i2) {
						VerticesBuffer[1].U += 128 * 65536 * SPHERE_TEXTURE_WRAP;
					}

					if (i3) {
						VerticesBuffer[2].U += 128 * 65536 * SPHERE_TEXTURE_WRAP;
					}
				}

				VerticesBuffer[i].A = explosionPtr->LifeTime / 256;
				VerticesBuffer[i].R = red;
				VerticesBuffer[i].G = green;
				VerticesBuffer[i].B = blue;
				VerticesBuffer[i].SpecularR = 0;
				VerticesBuffer[i].SpecularG = 0;
				VerticesBuffer[i].SpecularB = 0;

				// D3D_ZBufferedGouraudTexturedPolygon_Output no longer expects fixed point texture coordinates so convert them here
				VerticesBuffer[i].U >>= 16;
				VerticesBuffer[i].V >>= 16;
			}

			RenderPolygon.NumberOfVertices = 3;
			D3D_ZBufferedGouraudTexturedPolygon_Output(&fakeHeader, VerticesBuffer);
			/*
			            GouraudTexturedPolygon_ClipWithZ();
			            if (RenderPolygon.NumberOfVertices>=3)
			            {
			                GouraudTexturedPolygon_ClipWithNegativeX();
			                if (RenderPolygon.NumberOfVertices>=3)
			                {
			                    GouraudTexturedPolygon_ClipWithPositiveY();
			                    if (RenderPolygon.NumberOfVertices>=3)
			                    {
			                        GouraudTexturedPolygon_ClipWithNegativeY();
			                        if (RenderPolygon.NumberOfVertices>=3)
			                        {
			                            GouraudTexturedPolygon_ClipWithPositiveX();
			                            if (RenderPolygon.NumberOfVertices>=3)
			                            {
			                                D3D_ZBufferedGouraudTexturedPolygon_Output(&fakeHeader, RenderPolygon.Vertices);
			                            }
			                        }
			                    }
			                }
			            }
			*/
		}
	}
}

void RenderInsideAlienTongue(int offset)
{
#define TONGUE_SCALE 1024
	int TonguePolyVertexList[4][4] = {
		{0, 3, 7, 4}, //+ve y
		{1, 2, 3, 0}, //+ve x
		{5, 6, 7, 4}, //-ve x
		{1, 2, 6, 5}, //-ve y
	};
	VECTORCH vertices[8] = {
		{ +TONGUE_SCALE, -TONGUE_SCALE, 0},
		{ +TONGUE_SCALE, +TONGUE_SCALE, 0},
		{ +TONGUE_SCALE, +TONGUE_SCALE, +TONGUE_SCALE * 4},
		{ +TONGUE_SCALE, -TONGUE_SCALE, +TONGUE_SCALE * 4},

		{ -TONGUE_SCALE, -TONGUE_SCALE, 0},
		{ -TONGUE_SCALE, +TONGUE_SCALE, 0},
		{ -TONGUE_SCALE, +TONGUE_SCALE, +TONGUE_SCALE * 4},
		{ -TONGUE_SCALE, -TONGUE_SCALE, +TONGUE_SCALE * 4},
	};
	VECTORCH translatedPts[8];
	POLYHEADER fakeHeader;
	int polyNumber;
	{
		int i = 7;

		do {
			translatedPts[i] = vertices[i];
			translatedPts[i].vz -= (ONE_FIXED - offset) / 16;
		} while (i--);
	}
	{
		fakeHeader.PolyFlags = 0;
		fakeHeader.PolyColour = AlienTongueImageNumber;
		RenderPolygon.TranslucencyMode = TRANSLUCENCY_GLOWING;
	}

	for (polyNumber = 0; polyNumber < 4; polyNumber++) {
		for (int i = 0; i < 4; i++) {
			int v = TonguePolyVertexList[polyNumber][i];

			VerticesBuffer[i].X = translatedPts[v].vx;
			VerticesBuffer[i].Y = translatedPts[v].vy;
			VerticesBuffer[i].Z = translatedPts[v].vz;
			VerticesBuffer[i].U = CuboidPolyVertexU[3][i];
			VerticesBuffer[i].V = CuboidPolyVertexV[3][i];
			VerticesBuffer[i].R = offset / 2048;
			VerticesBuffer[i].G = offset / 2048;
			VerticesBuffer[i].B = offset / 2048;
			VerticesBuffer[i].A = 255;
			VerticesBuffer[i].SpecularR = 0;
			VerticesBuffer[i].SpecularG = 0;
			VerticesBuffer[i].SpecularB = 0;
		}

		RenderPolygon.NumberOfVertices = 4;
		{
			D3D_ZBufferedGouraudTexturedPolygon_Output(&fakeHeader, VerticesBuffer);
			/*
			            GouraudTexturedPolygon_ClipWithZ();
			            if (RenderPolygon.NumberOfVertices<3) continue;
			            GouraudTexturedPolygon_ClipWithNegativeX();
			            if (RenderPolygon.NumberOfVertices<3) continue;
			            GouraudTexturedPolygon_ClipWithPositiveY();
			            if (RenderPolygon.NumberOfVertices<3) continue;
			            GouraudTexturedPolygon_ClipWithNegativeY();
			            if (RenderPolygon.NumberOfVertices<3) continue;
			            GouraudTexturedPolygon_ClipWithPositiveX();
			            if (RenderPolygon.NumberOfVertices<3) continue;
			            D3D_ZBufferedGouraudTexturedPolygon_Output(&fakeHeader,RenderPolygon.Vertices);
			*/
		}
	}
}

#define OCTAVES 3
int u[OCTAVES];
int v[OCTAVES];
int du[OCTAVES];
int dv[OCTAVES];

bool skySetup = false;

int SkyColour_R = 200;
int SkyColour_G = 200;
int SkyColour_B = 200; 

void RenderSky(void)
{
	POLYHEADER fakeHeader;
	int o = 0;

	// if this is our first time in the function, initialise some values just the once
	if (!skySetup) {
		for (int i = 0; i < OCTAVES; i++) {
			u[i] = (FastRandom() & 65535) * 128;
			v[i] = (FastRandom() & 65535) * 128;
			du[i] = (((FastRandom() & 65535) - 32768) * (i + 1)) * 8;
			dv[i] = (((FastRandom() & 65535) - 32768) * (i + 1)) * 8;
		}
		skySetup = true;
	}

	// disable z-writes
	mainList->AddCommand(kCommandZWriteDisable);

	// set render states and texture
	fakeHeader.PolyFlags  = iflag_transparent;
	fakeHeader.PolyColour = CloudyImageNumber;
	RenderPolygon.TranslucencyMode = TRANSLUCENCY_GLOWING;

	for (o = 0; o < OCTAVES; o++) {
		u[o] += MUL_FIXED(du[o], NormalFrameTime);
		v[o] += MUL_FIXED(dv[o], NormalFrameTime);
	}

	for (int x = -10; x <= 10; x++)
	{
		for (int z = -10; z <= 10; z++)
		{
			int t = 255;
			int size = 65536 * 128;

			for (o = 0; o < OCTAVES; o++)
			{
				VECTORCH translatedPts[4] = {
					// x   // y  // z
					{-1024,-1000,-1024},
					{-1024,-1000, 1024},
					{ 1024,-1000, 1024},
					{ 1024,-1000,-1024},
				};

				for (int i = 0; i < 4; i++)
				{
					translatedPts[i].vx += 2048 * x;
					translatedPts[i].vz += 2048 * z;

					translatedPts[i].vx += Global_VDB_Ptr->VDB_World.vx;
					translatedPts[i].vy += Global_VDB_Ptr->VDB_World.vy;
					translatedPts[i].vz += Global_VDB_Ptr->VDB_World.vz;
					TranslatePointIntoViewspace(&translatedPts[i]);
					VerticesBuffer[i].X = translatedPts[i].vx;
					VerticesBuffer[i].Y = translatedPts[i].vy;
					VerticesBuffer[i].Z = translatedPts[i].vz;

					switch (CurrentVisionMode) {
						default:
						case VISION_MODE_NORMAL: {
							VerticesBuffer[i].R = SkyColour_R;
							VerticesBuffer[i].G = SkyColour_G;
							VerticesBuffer[i].B = SkyColour_B;
							break;
						}

						case VISION_MODE_IMAGEINTENSIFIER: {
							VerticesBuffer[i].R = 0;
							VerticesBuffer[i].G = 255;
							VerticesBuffer[i].B = 0;
							break;
						}

						case VISION_MODE_PRED_THERMAL:
						case VISION_MODE_PRED_SEEALIENS:
						case VISION_MODE_PRED_SEEPREDTECH: {
							VerticesBuffer[i].R = 0;
							VerticesBuffer[i].G = 0;
							VerticesBuffer[i].B = 255;
							break;
						}
					}

					VerticesBuffer[i].A = t;
					VerticesBuffer[0].U = (u[o] + size * x);
					VerticesBuffer[0].V = (v[o] + size * z);
					VerticesBuffer[1].U = (u[o] + size * x);
					VerticesBuffer[1].V = (v[o] + size * (z + 1));
					VerticesBuffer[2].U = (u[o] + size * (x + 1));
					VerticesBuffer[2].V = (v[o] + size * (z + 1));
					VerticesBuffer[3].U = (u[o] + size * (x + 1));
					VerticesBuffer[3].V = (v[o] + size * z);
					RenderPolygon.NumberOfVertices = 4;
				}

				D3D_SkyPolygon_Output(&fakeHeader, VerticesBuffer);
				/*
				                GouraudTexturedPolygon_ClipWithZ();
				                if (RenderPolygon.NumberOfVertices>=3)
				                {
				                    GouraudTexturedPolygon_ClipWithNegativeX();
				                    if (RenderPolygon.NumberOfVertices>=3)
				                    {
				                        GouraudTexturedPolygon_ClipWithPositiveY();
				                        if (RenderPolygon.NumberOfVertices>=3)
				                        {
				                            GouraudTexturedPolygon_ClipWithNegativeY();
				                            if (RenderPolygon.NumberOfVertices>=3)
				                            {
				                                GouraudTexturedPolygon_ClipWithPositiveX();
				                                if (RenderPolygon.NumberOfVertices>=3)
				                                {
				                                    D3D_SkyPolygon_Output(&fakeHeader, RenderPolygon.Vertices);
				                                }
				                            }
				                        }
				                    }
				                }
				*/
				t /= 2;
				size *= 2;
			}
		}
	}

	// enable z-writes
	mainList->AddCommand(kCommandZWriteEnable);
}

void CreateStarArray(void)
{
	SetSeededFastRandom(FastRandom());

	for (int i = 0; i < kNumStars; i++)
	{
		int phi = SeededFastRandom()&4095;

		StarArray[i].Position.vy = ONE_FIXED-(SeededFastRandom() & 131071);
		{
			float y = ((float)StarArray[i].Position.vy) / 65536.0f;
			y = sqrt(1-y*y);

			f2i(StarArray[i].Position.vx,(float)GetCos(phi)*y);
			f2i(StarArray[i].Position.vz,(float)GetSin(phi)*y);
		}
		StarArray[i].Colour = 0xff000000 + (FastRandom()&0x7f7f7f)+0x7f7f7f;
	}

#if 0 // test code, disabled
	SetSeededFastRandom(FastRandom());

	for (int i = 0; i < kNumStars; i++) {
		int phi = SeededFastRandom() & 4095;
		StarArray[i].Position.vy = ONE_FIXED - (SeededFastRandom() & 131071);
		{
			float y = ((float)StarArray[i].Position.vy) / 65536.0f;
			y = (float)(sqrt(1 - y * y));
			StarArray[i].Position.vx = ((float)GetCos(phi)) * y;
			StarArray[i].Position.vz = ((float)GetSin(phi)) * y;
		}
		StarArray[i].Colour = 0xff000000 + (FastRandom() & 0x7f7f7f) + 0x7f7f7f;
		/* bjd - unused
		        StarArray[i].Frequency = (FastRandom()&4095);
		        StarArray[i].Phase = FastRandom()&4095;
		*/
	}

	// load vbs
	D3DLVERTEX *starsVertex = NULL;
	uint16_t *starsIndex = NULL;
	d3d.starsVB->Lock((void **)&starsVertex);
	d3d.starsIB->Lock(&starsIndex);
	uint32_t sizeX = 256;

	// load em up
	for (int i = 0; i < kNumStars; i++) {
		starsList->AddItem(4, SpecialFXImageNumber, TRANSLUCENCY_OFF);
		// top left?
		starsVertex[0].x = StarArray[i].Position.vx;// - sizeX;
		starsVertex[0].y = StarArray[i].Position.vy;// - sizeX;
		starsVertex[0].z = StarArray[i].Position.vz;
		// top right
		starsVertex[1].x = StarArray[i].Position.vx;//+sizeX;
		starsVertex[1].y = StarArray[i].Position.vy;//-sizeX;
		starsVertex[1].z = StarArray[i].Position.vz;
		// bottom right
		starsVertex[2].x = StarArray[i].Position.vx;//+sizeX;
		starsVertex[2].y = StarArray[i].Position.vy;//+sizeX;
		starsVertex[2].z = StarArray[i].Position.vz;
		// bottom left
		starsVertex[3].x = StarArray[i].Position.vx;//-sizeX;
		starsVertex[3].y = StarArray[i].Position.vy;//+sizeX;
		starsVertex[3].z = StarArray[i].Position.vz;
		starsVertex[0].u = 192.0f;
		starsVertex[0].v = 0.0f;
		starsVertex[1].u = 255.0f;
		starsVertex[1].v = 0.0f;
		starsVertex[2].u = 255.0f;
		starsVertex[2].v = 63.0f;
		starsVertex[3].u = 192.0f;
		starsVertex[3].v = 63.0f;
		starsVertex[0].color = StarArray[i].Colour;
		starsVertex[1].color = StarArray[i].Colour;
		starsVertex[2].color = StarArray[i].Colour;
		starsVertex[3].color = StarArray[i].Colour;
		starsVertex[0].specular = RCOLOR_ARGB(0, 0, 0, 0);
		starsVertex[1].specular = RCOLOR_ARGB(255, 0, 0, 0);
		starsVertex[2].specular = RCOLOR_ARGB(255, 255, 0, 0);
		starsVertex[3].specular = RCOLOR_ARGB(0, 255, 0, 0);
		starsList->CreateIndices(starsIndex, 4);
	}

	d3d.starsVB->Unlock();
	d3d.starsIB->Unlock();
#endif
}

void RenderStarfield()
{
	int sizeX;
	int sizeY;
	sizeX = 256;
	//  sizeY = MUL_FIXED(sizeX,87381);
	sizeY = sizeX;

	for (int i = 0; i < kNumStars; i++) {
		VECTORCH position = StarArray[i].Position;
		PARTICLE particle;
		particle.ParticleID = PARTICLE_STAR;
		particle.Colour = StarArray[i].Colour;
		position.vx += Global_VDB_Ptr->VDB_World.vx;
		position.vy += Global_VDB_Ptr->VDB_World.vy;
		position.vz += Global_VDB_Ptr->VDB_World.vz;

#ifdef USE_D3DVIEWTRANSFORM
		VECTORCHF tempVector;

		//      VECTORCH translatedPosition = particlePtr->Position;
		tempVector.vx = (float)position.vx;
		tempVector.vy = (float)-position.vy;
		tempVector.vz = (float)position.vz;
		TransformToViewspace(&tempVector);

		position.vx = (int)tempVector.vx;
		position.vy = (int)tempVector.vy;
		position.vz = (int)tempVector.vz;
		//      TranslatePointIntoViewspace(&position);
		//      RotateVector(&position,&(Global_VDB_Ptr->VDB_Mat));
#else
		TranslatePointIntoViewspace(&position);
#endif

		VerticesBuffer[0].X = position.vx - sizeX;
		VerticesBuffer[0].Y = position.vy - sizeY;
		VerticesBuffer[0].Z = position.vz;
		VerticesBuffer[1].X = position.vx + sizeX;
		VerticesBuffer[1].Y = position.vy - sizeY;
		VerticesBuffer[1].Z = position.vz;
		VerticesBuffer[2].X = position.vx + sizeX;
		VerticesBuffer[2].Y = position.vy + sizeY;
		VerticesBuffer[2].Z = position.vz;
		VerticesBuffer[3].X = position.vx - sizeX;
		VerticesBuffer[3].Y = position.vy + sizeY;
		VerticesBuffer[3].Z = position.vz;

		int outcode = QuadWithinFrustum();

		if (/*outcode*/1) {
			RenderPolygon.NumberOfVertices = 4;
			//          textprint("On Screen!\n");
			VerticesBuffer[0].U = 192;
			VerticesBuffer[0].V = 0;
			VerticesBuffer[1].U = 255;
			VerticesBuffer[1].V = 0;
			VerticesBuffer[2].U = 255;
			VerticesBuffer[2].V = 63;
			VerticesBuffer[3].U = 192;
			VerticesBuffer[3].V = 63;

			if (/*outcode!=2*/0) {
				TexturedPolygon_ClipWithZ();

				if (RenderPolygon.NumberOfVertices < 3) {
					return;
				}

				TexturedPolygon_ClipWithNegativeX();

				if (RenderPolygon.NumberOfVertices < 3) {
					return;
				}

				TexturedPolygon_ClipWithPositiveY();

				if (RenderPolygon.NumberOfVertices < 3) {
					return;
				}

				TexturedPolygon_ClipWithNegativeY();

				if (RenderPolygon.NumberOfVertices < 3) {
					return;
				}

				TexturedPolygon_ClipWithPositiveX();

				if (RenderPolygon.NumberOfVertices < 3) {
					return;
				}

				//                  D3D_Particle_Output(&particle,RenderPolygon.Vertices);
				AddParticle(&particle, &RenderPolygon.Vertices[0]);
			}
			//              else D3D_Particle_Output(&particle,VerticesBuffer);
			else {
				AddParticle(&particle, VerticesBuffer);
			}
		}
	}
}

#if 0 // unused
signed int ForceFieldPointDisplacement[15 * 3 + 1][16];
signed int ForceFieldPointDisplacement2[15 * 3 + 1][16];
signed int ForceFieldPointVelocity[15 * 3 + 1][16];
unsigned char ForceFieldPointColour1[15 * 3 + 1][16];
unsigned char ForceFieldPointColour2[15 * 3 + 1][16];

void InitForceField()
{
	for (int x = 0; x < 15 * 3 + 1; x++) {
		for (int y = 0; y < 16; y++) {
			ForceFieldPointDisplacement[x][y] = 0;
			ForceFieldPointDisplacement2[x][y] = 0;
			ForceFieldPointVelocity[x][y] = 0;
		}
	}
}
#endif

int LightIntensityAtPoint(VECTORCH *pointPtr)
{
	int intensity = 0;
	int dist;
	DISPLAYBLOCK **activeBlockListPtr = ActiveBlockList;

	for (int i = NumActiveBlocks; i != 0; i--) {
		DISPLAYBLOCK *dispPtr = *activeBlockListPtr++;

		if (dispPtr->ObNumLights) {
			for (int j = 0; j < dispPtr->ObNumLights; j++) {
				LIGHTBLOCK *lptr = dispPtr->ObLights[j];
				VECTORCH disp = lptr->LightWorld;
				disp.vx -= pointPtr->vx;
				disp.vy -= pointPtr->vy;
				disp.vz -= pointPtr->vz;
				dist = Approximate3dMagnitude(&disp);

				if (dist < lptr->LightRange) {
					intensity += WideMulNarrowDiv(lptr->LightBright, lptr->LightRange - dist, lptr->LightRange);
				}
			}
		}
	}

	if (intensity > ONE_FIXED) {
		intensity = ONE_FIXED;
	} else if (intensity < GlobalAmbience) {
		intensity = GlobalAmbience;
	}

	/* KJL 20:31:39 12/1/97 - limit how dark things can be so blood doesn't go green */
	if (intensity < 10 * 256) {
		intensity = 10 * 256;
	}

	return intensity;
}
