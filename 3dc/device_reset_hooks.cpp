// See device_reset_hooks.h. Written 2026-11-23 by Melanikus (Frost) for the AvP port.
//
// RED STUB (tests/device_reset_hooks_test.cpp): no hook is kept yet, so none is ever called.

#include "device_reset_hooks.h"

bool DeviceReset_Add(void *, DeviceResetRelease)
{
	return false;
}

bool DeviceReset_Remove(void *)
{
	return false;
}

void DeviceReset_ReleaseAll()
{
}

size_t DeviceReset_Count()
{
	return 0;
}
