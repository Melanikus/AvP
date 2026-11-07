// The rules for the keys on the rendering page of the menus. Written 2026-11-05 by Melanikus for the AvP port. See view_keys.h.
#include "view_keys.h"

bool ViewKey_IsInList(int key, const unsigned char *list, int count)
{
	for (int i = 0; i < count; i++)
	{
		if (list[i] == key)
		{
			return true;
		}
	}

	return false;
}

ViewKeyAnswer ViewKey_Judge(int line, int key, const int keys[kViewKeyLines], const ViewKeyRules &rules)
{
	if (key == rules.noKey)
	{
		return kViewKeyTaken;
	}

	if (key < 0 || key > rules.lastRealKey)
	{
		return kViewKeyNotAKey;
	}

	if (ViewKey_IsInList(key, rules.gameKeeps, rules.gameKeepsCount))
	{
		return kViewKeyTheGameKeepsIt;
	}

	for (int other = 0; other < kViewKeyLines; other++)
	{
		if (other != line && keys[other] == key)
		{
			return kViewKeyIsOnAnotherLine;
		}
	}

	if (ViewKey_IsInList(key, rules.controls, rules.controlsCount))
	{
		return kViewKeyIsAControl;
	}

	return kViewKeyTaken;
}

bool ViewKey_FromSavedWord(const char *word, int *key)
{
	if (!word || word[0] == 0)
	{
		return false;
	}

	// A zero in front could only come from someone's editing, and "only the exact words count". A zero by itself is a real key: Escape. The judge refuses it for that.
	if (word[0] == '0' && word[1] != 0)
	{
		return false;
	}

	int number = 0;

	for (int i = 0; word[i] != 0; i++)
	{
		// Three digits are enough for 255, so nothing longer gets as far as being added up
		if (i >= 3 || word[i] < '0' || word[i] > '9')
		{
			return false;
		}

		number = number * 10 + (word[i] - '0');
	}

	if (number > 255)
	{
		return false;
	}

	*key = number;
	return true;
}

void ViewKey_SettleSaved(const char *const words[kViewKeyLines], const int usual[kViewKeyLines], const ViewKeyRules &rules, int keys[kViewKeyLines], bool wasRefused[kViewKeyLines])
{
	// Nothing has a key yet, so each line is only compared with the lines above it, which have been settled
	for (int line = 0; line < kViewKeyLines; line++)
	{
		keys[line] = rules.noKey;
		wasRefused[line] = false;
	}

	for (int line = 0; line < kViewKeyLines; line++)
	{
		int saved = 0;

		if (ViewKey_FromSavedWord(words[line], &saved) && ViewKey_Judge(line, saved, keys, rules) == kViewKeyTaken)
		{
			keys[line] = saved;
			continue;
		}

		wasRefused[line] = true;
		keys[line] = ViewKey_Judge(line, usual[line], keys, rules) == kViewKeyTaken ? usual[line] : rules.noKey;
	}
}

bool ViewKey_Acts(int key, bool pressed, const ViewKeyRules &rules)
{
	return pressed && key != rules.noKey && !ViewKey_IsInList(key, rules.controls, rules.controlsCount);
}
