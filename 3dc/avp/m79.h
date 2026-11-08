// The sums for the marine's M79 grenade launcher, which the game takes from MelPyth M5 by Mupp. Only how the gun behaves
// was read from that mod, and this is written new. Written 2026-11-06 by Melanikus for the AvP port.
//
// WHAT IT IS: a gun that holds one grenade at a time, of three kinds: standard, fire and frag. Each kind has its own
// count of spares. A tap of secondary fire picks the next kind that has any left, and half a second after the last tap,
// he changes to it. That half second is so that a few quick taps can skip past a kind without changing to each one.
//
// WHAT HAPPENS TO THE ROUNDS: changing kind puts a loaded grenade back with its spares, and loads one of the new kind.
//
// Unlike the old mod, all of this goes into a save, and comes back only if it passes M79_OwnStateIsGood.
//
// This file knows nothing about the game, so that tests/m79_test.cpp can try all of it.

#pragma once

enum M79Kind
{
	kM79Standard = 0,
	kM79Fire = 1,
	kM79Frag = 2,
	kM79Kinds = 3
};

enum
{
	kM79MostSpares = 99,          // the most of one kind he can carry
	kM79SparesPerPickup = 5,      // what one box of ammunition holds
	kM79ChangeDelay = 65536 / 2   // half a second, in the game's time, where 65536 is a second
};

// The gun's own state, which goes into a save: plain numbers and nothing else.
struct M79OwnState
{
	int Kind;                     // which kind is in the gun
	int RoundsInGun[kM79Kinds];   // 0 or 1 for each kind: what's loaded when that kind is chosen
	int Spares[kM79Kinds];        // how many of each kind he carries, as well as what's loaded
	int ChangeIsWaiting;          // 1 after a tap of the key, until the change has happened
	int KindToChangeTo;           // which kind the taps have reached so far
	int ChangeCountdown;          // how long until the change happens
	int GunWasEmptyAtChange;      // 1 if the last change found the gun empty: he has a spent case to take out
};

inline bool M79_KindIsReal(int kind)
{
	return kind >= 0 && kind < kM79Kinds;
}

inline bool M79_KindHasAmmo(const M79OwnState &state, int kind)
{
	return M79_KindIsReal(kind) && (state.RoundsInGun[kind] > 0 || state.Spares[kind] > 0);
}

// The next kind after this one with any ammunition, going round in order. This one is only counted after going all the
// way round, so a gun with one kind left stays on it. It gives -1 if no other kind has any.
inline int M79_NextKindWithAmmo(const M79OwnState &state, int after)
{
	if (!M79_KindIsReal(after))
	{
		return -1;
	}

	for (int step = 1; step < kM79Kinds; step++)
	{
		int const kind = (after + step) % kM79Kinds;

		if (M79_KindHasAmmo(state, kind))
		{
			return kind;
		}
	}

	return -1;
}

// Changes to a kind: a grenade in the gun goes back with its spares, and one of the new kind goes in. It gives false if
// there's nothing to do, because that's the kind already there, or it isn't a real one.
inline bool M79_ChangeKind(M79OwnState &state, int newKind)
{
	state.ChangeIsWaiting = 0;
	state.ChangeCountdown = 0;

	if (!M79_KindIsReal(newKind) || !M79_KindIsReal(state.Kind) || newKind == state.Kind)
	{
		return false;
	}

	int const oldKind = state.Kind;

	state.GunWasEmptyAtChange = state.RoundsInGun[oldKind] > 0 ? 0 : 1;
	state.Spares[oldKind] += state.RoundsInGun[oldKind];
	state.RoundsInGun[oldKind] = 0;

	if (state.Spares[oldKind] > kM79MostSpares)
	{
		state.Spares[oldKind] = kM79MostSpares;
	}

	state.Kind = newKind;

	if (state.RoundsInGun[newKind] == 0 && state.Spares[newKind] > 0)
	{
		state.RoundsInGun[newKind] = 1;
		state.Spares[newKind]--;
	}

	return true;
}

// Adds a box of ammunition. It gives false if he's already carrying all he can of that kind, and the box stays where it
// is for later.
inline bool M79_AddPickup(M79OwnState &state, int kind)
{
	if (!M79_KindIsReal(kind) || state.Spares[kind] >= kM79MostSpares)
	{
		return false;
	}

	state.Spares[kind] += kM79SparesPerPickup;

	if (state.Spares[kind] > kM79MostSpares)
	{
		state.Spares[kind] = kM79MostSpares;
	}

	return true;
}

// Starts a change to the next kind with ammunition, after the delay. Further taps carry on from the kind the last one
// reached. It gives false if no other kind has any, and then nothing changes.
inline bool M79_TapChangeKey(M79OwnState &state)
{
	int const from = state.ChangeIsWaiting ? state.KindToChangeTo : state.Kind;
	int const next = M79_NextKindWithAmmo(state, from);

	if (next < 0)
	{
		return false;
	}

	state.ChangeIsWaiting = 1;
	state.KindToChangeTo = next;
	state.ChangeCountdown = kM79ChangeDelay;

	return true;
}

// Is this copy, read from a save, safe to use? A save is a file that anybody could have changed. The kinds get used as
// places in a list, so a kind that isn't real must never get through. The other numbers are only checked for being what
// the gun could really hold.
inline bool M79_OwnStateIsGood(const M79OwnState &state)
{
	if (!M79_KindIsReal(state.Kind) || !M79_KindIsReal(state.KindToChangeTo))
	{
		return false;
	}

	for (int kind = 0; kind < kM79Kinds; kind++)
	{
		if (state.RoundsInGun[kind] < 0 || state.RoundsInGun[kind] > 1 || state.Spares[kind] < 0 || state.Spares[kind] > kM79MostSpares)
		{
			return false;
		}
	}

	return (state.ChangeIsWaiting == 0 || state.ChangeIsWaiting == 1)
		&& (state.GunWasEmptyAtChange == 0 || state.GunWasEmptyAtChange == 1)
		&& state.ChangeCountdown >= 0 && state.ChangeCountdown <= kM79ChangeDelay;
}
