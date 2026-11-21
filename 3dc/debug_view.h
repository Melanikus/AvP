// debug_view.h: THE DEBUG VIEWS for Hudson's runs. Written 2026-11-20 by Melanikus (Drake) for the AvP port.
//
// WHY: the owner, through Bishop: "when testing with hudson, can we turn on some wireframe or debug mode that allows me to see the aliens in the dark?",
// "Showing normals would be enough/look cool". Two views, each asked for by a run's own word, OFF unless asked:
//   - normals=yes: the NORMAL LINES view (normal_lines.h; J with -dev): each polygon's normals stood up as short lines over the normally lit scene, coloured by
//     how they agree with the polygon (the owner's correction 11-20: "display normals like we did on debugging shaders, not the ugly normal maps view"). It
//     draws only in the final view of the per-pixel lighting, and a network game has none.
//   - normalcolours=yes: the main list (the level's polygons, the aliens, the player's weapon) drawn with shaders/debugNormals.vsh/.psh in place of the
//     main shaders, each pixel coloured by which way its polygon faces, at full brightness, so nothing is lost in the dark.
//   - wireframe=yes: everything drawn as lines, Rebellion's own WIREFRAMEMODE 3 (the console's cheat: the environment and the objects).
// RENDER ONLY: they change how a pixel looks, never what the game decides. Nothing in the AI, the movement, the timing or the level reads them, so a
// stock run (stock_flags.h) stays one. Each run writes their state in its first log line beside the stock flags, and the lane probe in its MODE line.
#pragma once

#include <string>

// Sets both views, before any level (hudson.cpp, from the run's words)
void DebugView_Set(bool normals, bool normalColours, bool wireframe);

bool DebugView_Normals();
bool DebugView_NormalColours();
bool DebugView_Wireframe();

// "view normals=on|off normalcolours=on|off wireframe=on|off", for the log lines
std::string DebugView_Words();
