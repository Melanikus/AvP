/* CDF 20/10/98 - Single file for the movement stat lists. */

#include "3dc.h"
#include "inline.h"
#include "module.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "compiled_shapes.h"
#include "dynblock.h"
#include "dynamics.h"
#include "pfarlocs.h"
#include "pheromone.h"
#include "bh_types.h"
#include "pvisible.h"
#include "bh_far.h"
#include "bh_debris.h"
#include "bh_predator.h"
#include "bh_paq.h"
#include "bh_queen.h"
#include "bh_marine.h"
#include "bh_alien.h"
#include "lighting.h"
#include "bh_weapon.h"
#include "weapons.h"
#include "psnd.h"
#include "equipment.h"
#include "los.h"
#include "ai_sight.h"
#include "targeting.h"
#include "dxlog.h"
#include "showcmds.h"

#define UseLocalAssert TRUE
#include "ourasert.h"

/* First number is max speed, in mm/s. */
/* Second number is acceleration, in mm/s^2. */
/* Individual marines vary this by +/- 12.5%. */
/* Predators and xenoborgs don't at the moment. */

static const MOVEMENT_DATA Movement_Stats[] = {
	{
		MDI_Marine_Mooch_Bored,
		1500,
		1500,
	},
	{
		MDI_Marine_Mooch_Alert,
		1500,
		1500,
	},
	{
		MDI_Marine_Combat,
		4000,
		4000,
	},
	{
		MDI_Marine_Sprint,
		10000,
		10000,
	},
	{
		MDI_Civilian_Mooch_Bored,
		1500,
		1500,
	},
	{
		MDI_Civilian_Mooch_Alert,
		1500,
		1500,
	},
	{
		MDI_Civilian_Combat,
		4000,
		4000,
	},
	{
		MDI_Civilian_Sprint,
		10000,
		10000,
	},
	{
		MDI_Predator,
		8000,
		10000,
	},
	{
		MDI_Casual_Predator,
		3000,
		3000,
	},
	{
		MDI_Xenoborg,
		1000,
		1000,
	},
	{
		MDI_End,
		0,
		0,
	},
};

const MOVEMENT_DATA *GetThisMovementData(MOVEMENT_DATA_INDEX index) 
{	
	if (index < 0) 
	{
		return NULL;
	}
	
	int a=0;
	while (Movement_Stats[a].index!=MDI_End) 
	{
		if (Movement_Stats[a].index==index) 
		{
			return(&Movement_Stats[a]);
		}		
		a++;	
		GLOBALASSERT(a<1000);
	}
	return NULL;	
}
