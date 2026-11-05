// Which shader model a shader file needs. Written 2026-11-04 by Melanikus for the AvP port.
//
// This is on its own so that there's only one copy of the rule: the game uses it to compile its shaders, and the shader test and
// tests/compile-shaders.ps1 use it to compile them the same way.

#ifndef SHADER_MODEL_H
#define SHADER_MODEL_H

#include <string>

// The game's shaders all fit shader model 2. The ones for shadows and lighting need 3, for their length and to draw on several pictures at
// once. Those are told apart by the start of the file's name.
inline bool ShaderNeedsModel3(const std::string &fileName)
{
	return fileName.compare(0, 6, "shadow") == 0 || fileName.compare(0, 7, "surface") == 0 || fileName.compare(0, 5, "light") == 0 || fileName.compare(0, 6, "relief") == 0;
}

#endif
