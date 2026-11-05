// Putting back the numbers for trying out how shadows look. Written 2026-11-05 by Melanikus for the AvP port.
//
// With -dev, the command line can change how shadows and lights are drawn: how dark a shadow is, how bright the world is, which things cast one, whether walls
// come from the fixed buffer. In a network game that could change what one player sees compared with the others. A paler shadow or a brighter world is an
// advantage, and a route nobody has proved could draw something wrong. So a network game puts them all back to what the game was built with. WinMain in winmain.cpp
// reads the switches, and KeepDeveloperToolsOutOfNetworkGames in d3d_render.cpp puts them back.
//
// It's on its own so that a test can try it with no game.
#ifndef SHADOW_TUNING_H
#define SHADOW_TUNING_H

struct ShadowTuningNumber
{
	int *value;
	int asBuilt;
};

// Puts each one back. It says whether any had been changed, so that the log can say so once and not every frame.
inline bool ShadowTuning_PutBack(const ShadowTuningNumber *numbers, int count)
{
	bool anyChanged = false;

	for (int i = 0; i < count; i++)
	{
		if (*numbers[i].value != numbers[i].asBuilt)
		{
			*numbers[i].value = numbers[i].asBuilt;
			anyChanged = true;
		}
	}

	return anyChanged;
}

#endif
