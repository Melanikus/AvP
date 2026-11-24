// See device_reset_hooks.h. Written 2026-11-23 by Melanikus (Frost) for the AvP port.
//
// A fixed list of at most kMostDeviceResetHooks owners and their hooks, in the order they were added. While DeviceReset_ReleaseAll walks it, nothing may
// change it (a hook that tries is refused), so the walk always sees the list it began with, and every hook is called exactly once.

#include "device_reset_hooks.h"

namespace
{
	struct Hook
	{
		void *owner;
		DeviceResetRelease release;
	};

	Hook hooks[kMostDeviceResetHooks];
	size_t held = 0;
	bool calling = false;   // DeviceReset_ReleaseAll is walking the list
}

bool DeviceReset_Add(void *owner, DeviceResetRelease release)
{
	if (owner == nullptr || release == nullptr || calling || held >= kMostDeviceResetHooks)
	{
		return false;
	}

	for (size_t i = 0; i < held; i++)
	{
		if (hooks[i].owner == owner)
		{
			return false;
		}
	}

	hooks[held].owner = owner;
	hooks[held].release = release;
	held++;
	return true;
}

bool DeviceReset_Remove(void *owner)
{
	if (calling)
	{
		return false;
	}

	for (size_t i = 0; i < held; i++)
	{
		if (hooks[i].owner == owner)
		{
			// the ones after it move up one place, so the order of adding is kept
			for (size_t j = i + 1; j < held; j++)
			{
				hooks[j - 1] = hooks[j];
			}

			held--;
			return true;
		}
	}

	return false;
}

void DeviceReset_ReleaseAll()
{
	calling = true;

	for (size_t i = held; i > 0; i--)
	{
		hooks[i - 1].release(hooks[i - 1].owner);
	}

	calling = false;
}

size_t DeviceReset_Count()
{
	return held;
}
