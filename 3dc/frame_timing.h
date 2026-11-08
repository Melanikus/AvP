// How long each part of a frame takes, on the graphics card and on the processor. Written 2026-11-04 by Melanikus for the AvP port.
// See frame_timing.cpp for how it works. None of this waits for the card, so it's cheap enough to leave on.

#ifndef FRAME_TIMING_H
#define FRAME_TIMING_H

#include <d3d9.h>
#include <stdint.h>

// The parts of a frame on the card. Marking one says that everything the card has done since the last mark belongs to it.
enum FrameStage
{
	kStageScene,            // the solid polygons and decals
	kStageSurfacePictures,  // the second draw of the solid polygons, into the pictures
	kStageLightTotal,
	kStagePrismsSent,       // shadow prisms sent to the card this frame
	kStagePrismsKept,       // shadow prisms already on the card
	kStageClearing,         // clearing the stencil, before each pass
	kStageShading,          // putting each light's share into the mask
	kStageSoftening,        // blurring the mask and applying it
	kStageDimming,
	kStageAddingLights,
	kStageAfter,            // glass, particles, the weapon and the display
	kNumFrameStages
};

// Starts a frame, after the scene has begun. Most frames this only asks whether any answers have come in.
void FrameTiming_StartFrame(LPDIRECT3DDEVICE9 device);

// On a frame being timed, says that what the card has done since the last mark belongs to this stage. On any other frame it does nothing.
void FrameTiming_Mark(FrameStage stage);

// Ends the card's half of the frame, before the frame is shown.
void FrameTiming_EndFrame();

// Ends the frame for the processor, once the frame has been shown. A frame that took too long goes in the stall list, and every so often the averages
// and that list go to the log.
void FrameTiming_FrameShown();

// Writes the stall list to the log now, so that the last of a level isn't lost when it ends.
void FrameTiming_WriteStalls();

// What a stall's entry says about its frame.
void FrameTiming_SetRoom(const char *room);
void FrameTiming_SetPolygonsCollected(int polygons);
void FrameTiming_SetFrameNumber(int frame);   // the number the shadow pass's reports and the tag on the screen use for this frame
void FrameTiming_CountPlacesChanged(int places);
void FrameTiming_CountKeptGroupsMade();

// Says whether this is the first time this texture has been used. The first use of a texture is when the card gets its copy of it.
bool FrameTiming_TextureIsNew(uint32_t texture);
void FrameTiming_NoteNewTexture(const char *name, uint32_t bytes);

// Notes that this effect is being used. Its first draw is when a driver may finish building it.
void FrameTiming_NoteEffectUsed(uint32_t effect, const char *name);

// Counts how many shadow prism corners a frame draws, of each kind. Only a frame being timed keeps them, to go with its times.
void FrameTiming_CountPrismCorners(int sent, int kept);

// Counts the vertices the game's own lists held when they were drawn.
void FrameTiming_CountMainVertices(int vertices);

// Counts one indexed draw of the game's own lists. The scene and the surface pictures each make their own, and shadow draws aren't counted here.
void FrameTiming_CountDraw();

// A frame with other marines' lamp beams: how many, how many walls were kept for them, and how many there wasn't room for. See kshape.cpp.
void FrameTiming_CountLampBeams(int beams, int wallsKept, int wallsLeftOut);

// Gives back what's held on the graphics card, which has to go whenever the device resets.
void FrameTiming_Release();

// The parts of a frame on the processor. They can overlap: a level drawn inside the drawing counts in both.
enum ProcessorStage
{
	kProcessorShapes,       // turning, lighting and writing out each shape's polygons
	kProcessorLocks,        // opening and closing the lists of vertices
	kProcessorDrawing,      // handing the lists to the card, the shadow pass included
	kProcessorShadowPass,   // the shadow pass alone
	kProcessorPresent,      // waiting to show the frame: for the card to catch up, and for the screen
	kProcessorStateBlock,   // saving and putting back all the card's settings around the shadow pass, which is part of that pass
	kProcessorBeginScene,   // starting a frame. A driver that's holding a frame back to match the screen can wait here and not in present
	kProcessorGame,         // the game itself: animation, movement, what everything does
	kProcessorMessages,     // what Windows has to say, and anything else at the top of the loop
	kProcessorViews,        // working out what's on screen. Shapes are part of this
	kProcessorHud,          // the display, and looking after the music
	kProcessorMenus,        // the menus, which are asked every frame
	kProcessorEndScene,     // finishing a frame
	kProcessorLampBeams,    // the beams of other marines' lamps: working out their shapes, and handing them to the card. Part of the drawing
	kProcessorLog,          // handing lines to the log. It should never be more than a moment: see logString.cpp
	kNumProcessorStages
};

// Counts the time from here to the end of the block as this stage.
class ProcessorStageClock
{
public:
	explicit ProcessorStageClock(ProcessorStage stage);
	~ProcessorStageClock();

private:
	ProcessorStage stage;
	int64_t started;
};

#endif
