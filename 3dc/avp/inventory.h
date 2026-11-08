// Modified 2026-11-06 by Melanikus for the AvP port:
//   unlimited pistol ammunition doesn't apply in a game that plays as single player
#ifndef INVENTRY_H
#define INVENTRY_H
/*KJL*****************************************************
* INVENTRY.H - contains externs to the fns in INVENTRY.C *
*****************************************************KJL*/

/*KJL****************************************************************************************
*                                    P R O T O T Y P E S	                                *
****************************************************************************************KJL*/
extern void InitialisePlayersInventory(PLAYER_STATUS *playerStatusPtr);

// What a player keeps between the levels of a co-op campaign. See campaign_kit.h.
extern void Campaign_NoteKit(int place);                                 // a level is ending, and place comes next
extern void Campaign_ForgetKit(void);                                    // a new game has begun
extern void Campaign_GiveKeptKit(PLAYER_STATUS *playerStatusPtr);        // this player has just been given the level's own kit
extern void MaintainPlayersInventory(void);

/*-------------------------------Patrick 11/3/97--------------------------------
  Protoypes for a couple of little functions: see inventry.c for details...
  ------------------------------------------------------------------------------*/
extern void SetPlayerSecurityClearance(STRATEGYBLOCK *sbPtr, unsigned int securityLevel);
extern int ReturnPlayerSecurityClearance(STRATEGYBLOCK *sbPtr, unsigned int securityLevel);



//structure for starting equipment information loaded from rif files
typedef struct player_starting_equipment
{
	unsigned int marine_jetpack :1;
	
	unsigned int predator_pistol :1;
	unsigned int predator_plasmacaster :1;
	unsigned int predator_disc :1;
	unsigned int predator_medicomp :1;
	unsigned int predator_grappling_hook :1;
	int predator_num_spears;

	
}PLAYER_STARTING_EQUIPMENT;

extern PLAYER_STARTING_EQUIPMENT StartingEquipment;

#define PISTOL_INFINITE_AMMO	(netGameData.pistolInfiniteAmmo && !SameAsSinglePlayer())

#endif
