// device_reset_hooks.h: what else lives on the card alone, let go before the device is reset. Written 2026-11-23 by Melanikus (Frost) for the AvP port.
//
// When Direct3D loses its device (another program takes the screen, the window is resized, the computer sleeps), everything on the card alone
// (D3DPOOL_DEFAULT) has to be let go before the device can be reset: Direct3D refuses the reset while one such thing is held (D3DERR_INVALIDCALL), and the
// game then stays lost, every frame (Drake's second hole in the level editor's live picker panes' safety case). The game's own (its card-only textures,
// vertex and index buffers, queries) are let go by d3_func.cpp ReleaseVolatileResources itself. A part of the program the game knows nothing of adds a hook
// here for its own: the level editor's live picker panes keep their atlas's depth surface and the state block that holds the device's settings this way.
// ReleaseVolatileResources calls every hook first, before the game's own are let go. Each owner makes its own again when it next draws (Direct3D allows that
// at any time after the reset), so there is no hook for after.
//
// The list is the program's main thread's only, as the device is (it isn't D3DCREATE_MULTITHREADED).

#ifndef _device_reset_hooks_h_
#define _device_reset_hooks_h_

#include <stddef.h>

// at most this many owners at once (the editor adds one)
const size_t kMostDeviceResetHooks = 16;

// let go of everything `owner` keeps on the card alone
typedef void (*DeviceResetRelease)(void *owner);

// owner's hook added: false (and nothing added) for no owner or no hook, an owner already added, the most reached, or while the hooks are being called
bool DeviceReset_Add(void *owner, DeviceResetRelease release);

// owner's hook taken away: false for an owner not added, or while the hooks are being called
bool DeviceReset_Remove(void *owner);

// every hook called once, the last added first (an owner added later may hold what an earlier one made). A hook that adds or takes away one is refused.
void DeviceReset_ReleaseAll();

// how many owners have a hook now
size_t DeviceReset_Count();

#endif
