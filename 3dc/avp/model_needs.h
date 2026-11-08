// The parts the game can't do without, model by model. Written 2026-11-04 by Melanikus for the AvP port.
// model_needs.cpp holds the list and explains why it exists.
#ifndef MODEL_NEEDS_H
#define MODEL_NEEDS_H

struct section;

// The most parts any one row of the list names. The compiler refuses a row with more.
#define MODEL_NEEDS_MOST_PARTS 5

// One model the game asks for by name, and the parts it must have. Unused places in parts stay empty (NULL).
struct MODEL_NEEDS
{
	const char *file;
	const char *hierarchy;
	const char *parts[MODEL_NEEDS_MOST_PARTS];
};

extern const MODEL_NEEDS model_needs[];
extern const int model_needs_count;

// Is there a part with exactly this name anywhere in this model, or in this branch of it?
bool Model_Has_Part(const struct section *root, const char *name);

// The first part the list wants from this model and doesn't find, or NULL when nothing is missing. A model the list
// doesn't mention has nothing to miss.
const char *Model_First_Missing_Part(const struct section *root, const char *file, const char *hierarchy);

#endif
