// The parts the game can't do without, model by model. Written 2026-11-04 by Melanikus for the AvP port.

/*
An animated model in this game is a tree of named parts, built from a file: a chest, an arm on the chest, a gun in the
hand. The game's code asks for some of those parts by name, such as the tip of the gun, so that a bullet knows where to
start. If a damaged or edited file leaves that part out, the answer is "nothing there", and a game that goes on to use
"nothing" crashes.

So each model is held against this list as it's built from its file (Global_Hierarchy_Store::add_hierarchy in
win95/projload.cpp). A model that falls short stops the game, with a message naming the file and the part. Stopping is
kinder than it sounds: the other choice is a level that quietly loses its characters, with nothing to say why.

WHAT THIS LIST DOESN'T DO. It only sees a model as it leaves its file. In play, parts really do get cut off, and a save
file says which ones have gone, so any part on this list except the very first one in a model can still be missing
later on. That's why the code checks for "nothing there" as well, wherever it asks for a part. The list and those
checks do different jobs: the list reports a broken file clearly, and the checks stop a missing part from crashing the
game.

HOW THE LIST IS LAID OUT. One row per model, not one row per kind of character, because models in the same family
differ: the marine with a pistol has "dum flash", and the bare marine model it's based on doesn't.

A row names only parts the game asks for by name. A wider list would be a mistake: the code also asks for a few names
that no model in the game has, such as "SoundSource", and always copes without them. Requiring those would refuse the
game's own files.

Capitals count in part names and model names, because the game's own lookups use strcmp. "dum flash" and "Dum flash" are
different parts. File names are the exception: the game finds files whatever their capitals (see
GetNamedHierarchyFromLibrary), so this list does too.

The row for a model has to match what's in the game's files. All the rows were checked against all 63 models in the
game's 18 model files on 2026-11-04. Anyone changing a row needs to repeat that, because a row that asks for too much
stops the game for everybody.
*/

#include "3dc.h"
#include "hmodel.h"
#include "model_needs.h"
#include <string.h>

const MODEL_NEEDS model_needs[] =
{
	// Soldiers. "head" is where a marine looks from. "dum flash" (on the SADAR, "dum flame") is the tip of the gun, where
	// shots and the muzzle flash start. Multiplayer ghosts use the Cudgel model too, and ask it for the same part.
	{ "hnpcmarine", "marine with pulse rifle", { "head", "dum flash" } },
	{ "hnpcmarine", "marine with flame thrower", { "head", "dum flash" } },
	{ "hnpcmarine", "marine with smart gun", { "head", "dum flash" } },
	{ "hnpcmarine", "marine with SADAR", { "head", "dum flame" } },
	{ "hnpcmarine", "marine + grenade launcher", { "head", "dum flash" } },
	{ "hnpcmarine", "Marine with Mini Gun", { "head", "dum flash" } },
	{ "hnpcmarine", "PISTOL", { "head", "dum flash" } },
	{ "hnpcmarine", "Two Pistol", { "head", "dum flash" } },
	{ "hnpcmarine", "skeeter", { "head", "dum flash" } },
	{ "hnpcmarine", "Cudgel", { "head", "dum flash" } },
	{ "hnpcmarine", "Template", { "head" } },

	// Civilians and androids. Here the tip of the gun is "flash dummy", and a Molotov is thrown from "bottle".
	{ "hnpc_civvie", "male_shotgun", { "head", "flash dummy" } },
	{ "hnpc_civvie", "male_pistol", { "head", "flash dummy" } },
	{ "hnpc_civvie", "male_flamer", { "head", "flash dummy" } },
	{ "hnpc_civvie", "male_bottle", { "head", "bottle" } },
	{ "hnpc_civvie", "male_unarmed", { "head" } },
	{ "hnpc_civvie", "Android shotgun", { "head", "flash dummy" } },
	{ "hnpc_civvie", "Android Shotgun Special", { "head", "flash dummy" } },
	{ "hnpc_civvie", "Android Pistol Special", { "head", "flash dummy" } },
	{ "hnpc_civvie", "TEMPLATE", { "head" } },
	{ "hnpc_civvie", "Android template", { "head" } },

	// The Predator's flechette pistol fires from "dum flash".
	{ "hnpcpredator", "pred + pistol", { "dum flash" } },

	// A heavy blow aimed at an alien lands on its chest.
	{ "hnpcalien", "alien", { "chest" } },
	{ "hnpcpred_alien", "TEMPLATE", { "chest" } },
	{ "hnpcpretorian", "Template", { "chest" } },

	// The Queen walks by planting one foot at a time, swings from either hand, and carries things in "rrrr fing".
	{ "queen", "Template", { "left foot", "right foot", "left hand", "right hand", "rrrr fing" } },

	// The Xenoborg aims with its neck and turns on its hips. "pelvis presley" is the first part of the model, so nothing
	// can ever cut it off, which makes this list the only check it needs.
	{ "hnpc_xenoborg", "xenobasic", { "pelvis presley", "neck" } },

	// The sentry gun points its "gun" part at its target.
	{ "sentry", "gun", { "gun" } },

	// The Predator's disc, in flight and in the Predator's hand.
	{ "disk", "Disk", { "disk" } },
	{ "pred_HUD", "disk", { "disk" } },
};

const int model_needs_count = sizeof(model_needs) / sizeof(model_needs[0]);

// A part holds a list of its children, ended by an empty entry. Looking through them finds a part at any depth, the same
// way the game's own lookups do.
bool Model_Has_Part(const SECTION *root, const char *name)
{
	if (strcmp(root->Section_Name, name) == 0)
	{
		return true;
	}

	if (root->Children)
	{
		for (SECTION **child = root->Children; *child; child++)
		{
			if (Model_Has_Part(*child, name))
			{
				return true;
			}
		}
	}

	return false;
}

const char *Model_First_Missing_Part(const SECTION *root, const char *file, const char *hierarchy)
{
	for (int row = 0; row < model_needs_count; row++)
	{
		const MODEL_NEEDS *needs = &model_needs[row];

		if (_stricmp(needs->file, file) != 0 || strcmp(needs->hierarchy, hierarchy) != 0)
		{
			continue;
		}

		for (int part = 0; part < MODEL_NEEDS_MOST_PARTS && needs->parts[part]; part++)
		{
			if (!Model_Has_Part(root, needs->parts[part]))
			{
				return needs->parts[part];
			}
		}
	}

	return NULL;
}
