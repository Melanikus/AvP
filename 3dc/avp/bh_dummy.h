
#ifndef _bhdummy_h_
	#define _bhdummy_h_ 1

	#include "bh_ais.h"

	typedef struct dummyStatusBlock 
	{
		I_PLAYER_TYPE PlayerType;
		int incidentFlag;
		int incidentTimer;
		HMODELCONTROLLER HModelController;
	} DUMMY_STATUS_BLOCK;


	extern void MakeDummyNear(STRATEGYBLOCK *sbPtr);
	extern void MakeDummyFar(STRATEGYBLOCK *sbPtr);
	extern void DummyBehaviour(STRATEGYBLOCK *sbPtr);

#endif
