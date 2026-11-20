// stock_flags.h: THE OWNER'S STOCK SWITCH (2026-11-20, through Bishop: "we need any new behavior we added behind a flag in AVPX or these tests mean nothing").
// Written 2026-11-20 by Melanikus (Vasquez) for the AvP port.
//
// WHY: the lane probe measures what Rebellion's own aliens do with a level's waypoints. Five things this port changed still touch aliens
// (Frost's audit 11-19, a diff of this code against AvP-assembla branches/reb with the two below already off):
//   - FLOORISLAVA (floor_is_lava.h): the alien ways, the port's own alien movement. Off: 1999's aliens.
//   - NEWDYNAMICCOLLISIONS (new_dynamic_collisions.h): the smallest move that counts shrinks with a short frame. Off: 1999's 16 mm.
//   - FARLOCS4THCORNER (pfarlocs.cpp, #161): a four-cornered ceiling's or floor's 4th corner counts for its height where a far alien may
//     be put. Off: 1999's slip, the 3rd corner counted twice.
//   - PHEROMONEDOOR (pheromone.cpp, ai_module_door.h): whether the scent goes through an AI module that is a door is asked of the module
//     that is the door. Off: 1999's way, asked of the AI module's FIRST module, whatever it is (read safely here: a first module that is
//     no proximity door, where 1999 read its block as one's, counts as unlocked; one that is no lift or switch door counts as shut).
//   - NEARWAKE (near_wake.h, #130): a sleeping NPC near a player wakes through its walk-in box. Off: only the box itself, as in 1999.
// Each is on as every run of the game starts, as before. The stock switch (Hudson's stock=yes) turns all five off before any level, and
// every Hudson run writes each flag's state to the log before any level: StockFlags_Words, for the run's reader to check.
#ifndef STOCK_FLAGS_H
#define STOCK_FLAGS_H

#include <string>

extern int FarLocsFourthCornerOn;   // 1: the 4th corner counts (#161); 0: 1999's
extern int PheromoneDoorOn;         // 1: the door module is asked; 0: 1999's first module, read safely
extern int NearWakeOn;              // 1: a sleeper near a player wakes (#130); 0: only its walk-in box

// The five flags' words, as the log gets them: "flags: floorislava=on newdynamiccollisions=on farlocs4thcorner=on pheromonedoor=on nearwake=on"
// (each "on" or "off"); stock when all five say off
inline std::string StockFlags_WordsOf(int floorIsLava, int newDynamicCollisions, int farLocsFourthCorner, int pheromoneDoor, int nearWake)
{
	auto const word = [](int on) { return std::string(on ? "on" : "off"); };
	return "flags: floorislava=" + word(floorIsLava) + " newdynamiccollisions=" + word(newDynamicCollisions) + " farlocs4thcorner=" + word(farLocsFourthCorner) +
		" pheromonedoor=" + word(pheromoneDoor) + " nearwake=" + word(nearWake) + ((floorIsLava || newDynamicCollisions || farLocsFourthCorner || pheromoneDoor || nearWake) ?
		" (not stock)" : " (stock)");
}

// The game's own flags now, in those words
std::string StockFlags_Words();

// The stock switch: all five off (FLOORISLAVA through the console's own rule)
void StockFlags_AllStock();

#endif
