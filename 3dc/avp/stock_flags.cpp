// stock_flags.cpp: the stock switch's flags (stock_flags.h says what each one is and why). Written 2026-11-20 by Melanikus (Vasquez) for the
// AvP port.
#include "stock_flags.h"

#include "floor_is_lava.h"
#include "new_dynamic_collisions.h"

// On, as every run of the game starts: the port's ways. The stock switch, or a test, turns them off for 1999's.
int FarLocsFourthCornerOn = 1;
int PheromoneDoorOn = 1;
int NearWakeOn = 1;

std::string StockFlags_Words()
{
	return StockFlags_WordsOf(FloorIsLavaAsked, NewDynamicCollisionsOn, FarLocsFourthCornerOn, PheromoneDoorOn, NearWakeOn);
}

void StockFlags_AllStock()
{
	// the two with a console command go through its own rule, as a player typing them would
	FloorIsLava_FromWords("OFF", &FloorIsLavaAsked);
	NewDynamicCollisions_FromWords("OFF", &NewDynamicCollisionsOn);
	FarLocsFourthCornerOn = 0;
	PheromoneDoorOn = 0;
	NearWakeOn = 0;
}
