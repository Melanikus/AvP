// What the common weapon code offers the guns that have files of their own: the shotgun, sniper rifle, revolver, M79 and staff. Written 2026-11-06 by Melanikus for the AvP port.
//
// HOW A GUN JOINS IN: its row in equipment.cpp points at a WEAPON_HOOKS (in equipment.h) in the gun's own file, and only says what it needs. A gun with no hooks works just as it always did.

#pragma once

#include "equipment.h"

// Can this weapon be drawn? The new guns' models are in files of their own, which may not be there, or may have failed to load. Every other weapon says yes.
bool Weapon_CanBeDrawn(enum WEAPON_ID weapon);

// Which loaded file a gun's model comes from: the one its row names, or our own MelPyth_MarWep.rif for the four marine guns
// that file has its own versions of, when it's loaded.
char *Weapon_ModelFileFor(TEMPLATE_WEAPON_DATA const *row);

// What a slot should hold, for a slot that's meant to hold this weapon: the weapon, or an empty slot if it can't be drawn, so that nothing can hand him a gun that isn't there.
enum WEAPON_ID Weapon_ForSlot(enum WEAPON_ID weapon);

// Puts a gun into a state, with the state's whole time to run.
void Weapon_EnterState(PLAYER_WEAPON_DATA *weaponPtr, int state);

// Tells the gun in his hands if it has just gone into a new state. The common weapon code calls this once a frame.
void Weapon_TellOfNewState(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);

// Throws a spent case out of the gun in his hands, from the part of its model of this name, as a shape of that name. Up and out, with something added for how fast he's moving. If the model or the shape isn't there, nothing happens.
void Weapon_ThrowCase(const char *shapeName, const char *sectionName);

// Has he just asked for another weapon, or did he ask earlier and is it still waiting? If he's just asked, the common code remembers which, and swaps once the gun is idle again. For a gun that's busy, whose state the common code doesn't look at the keys in. The file that holds this is weapons.cpp, because the search for the next weapon is private to it.
int Weapon_AskedForAnotherWeapon(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);

// Starts the change to the weapon asked for, as the common code does when an idle gun gets as far as the weapon keys. For a gun whose own key stops it getting that far.
void Weapon_StartChangingGun(void *playerStatus, PLAYER_WEAPON_DATA *weaponPtr);

// He's pressed the reload key. The state machine looks at it next, at the place where a state's function will see that its state has only just begun. If the gun in his hands is idle, isn't full, and he has something to load, it reloads. Nothing is lost: a gun that loads a round at a time keeps what's in it, and what's left in any other gun's magazine goes in a pile for its next reload. See weapon_reload.h. The grenade launcher, with its kinds of grenade, only reloads when it's empty.
void Weapon_ReloadKeyPressed(void *playerStatus);

// A player has just started: every gun forgets what it was doing.
void Guns_AtPlayerStart(void);

// Each gun's own state goes into a save in a block of its own, and comes back from one. A block that's missing, the wrong size or fails the gun's own check is left out, and the gun stays as it was at the player's start. So an older save, which has none, simply starts these guns afresh.
void Guns_SaveOwnStates(void);
void Guns_LoadOwnStates(void);

// The same states, kept in memory from one level of a co-op campaign to the next. See campaign_kit.h. They never leave this machine.
void Guns_KeepOwnStates(void);
void Guns_GiveBackOwnStates(void);
