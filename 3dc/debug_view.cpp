// debug_view.cpp: see debug_view.h. Written 2026-11-20 by Melanikus (Drake) for the AvP port.

#include "debug_view.h"

static bool normalsAsked = false;
static bool wireframeAsked = false;

void DebugView_Set(bool normals, bool wireframe)
{
	normalsAsked = normals;
	wireframeAsked = wireframe;
}

bool DebugView_Normals()
{
	return normalsAsked;
}

bool DebugView_Wireframe()
{
	return wireframeAsked;
}

std::string DebugView_Words()
{
	return std::string("view normals=") + (normalsAsked ? "on" : "off") + " wireframe=" + (wireframeAsked ? "on" : "off");
}
