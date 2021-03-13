/* Sequnces.h CDF 8/12/97 */

#ifndef _sequnces_h_

	#define _sequnces_h_ 1

	/* Sequences enums. */

	typedef enum HModelSequenceTypes {
		HMSQT_AlienRun=0,
		HMSQT_AlienCrawl,
		HMSQT_AlienStand,
		HMSQT_AlienCrouch,
		HMSQT_Hugger,
		HMSQT_MarineRun,
		HMSQT_MarineCrawl,
		HMSQT_MarineStand,
		HMSQT_MarineCrouch,
		HMSQT_PredatorHUD,
		HMSQT_MarineHUD,
		HMSQT_PredatorRun,
		HMSQT_PredatorCrawl,
		HMSQT_PredatorStand,
		HMSQT_PredatorCrouch,
		HMSQT_QueenLeftStanceTemplate,
		HMSQT_QueenLeftStanceFull,
		HMSQT_QueenRightStanceTemplate,
		HMSQT_QueenRightStanceFull,
		HMSQT_QueenGeneral,
		HMSQT_AlienHUD,
		HMSQT_Xenoborg,
	} HMODEL_SEQUENCE_TYPES;
	
	typedef enum AlienRunSubSequences {
		ARSS_Standard=0,
		ARSS_Dies,
		ARSS_Attack_Swipe,
		ARSS_Jump,
		ARSS_Standard_II,
		ARSS_Left_Hobble,
		ARSS_Right_Hobble,
		ARSS_end,
	} ALIENRUN_SUBSEQUENCES;

	typedef enum AlienCrawlSubSequences {
		ACSS_Standard=0,
		ACSS_Dies,
		ACSS_Attack_Bite,
		ACSS_Attack_Tail,
		ACSS_Pain_Fall_Fwd,
		ACSS_Pain_Fall_Back,
		ACSS_Pain_Fall_Left,
		ACSS_Pain_Fall_Right,
		ACSS_Boom_Fall_Fwd,
		ACSS_Boom_Fall_Back,
		ACSS_Boom_Fall_Left,
		ACSS_Boom_Fall_Right,
		ACSS_Attack_Swipe,
		ACSS_Crawl_Hurt,
		ACSS_Scamper,
		ACSS_end,
	} ALIENCRAWL_SUBSEQUENCES;

	typedef enum AlienStandSubSequences {
		ASSS_Standard=0,
		ASSS_Dies,
		ASSS_Attack_Right_Swipe_In,
		ASSS_Attack_Bite,
		ASSS_Pain_Fall_Fwd,
		ASSS_Pain_Fall_Back,
		ASSS_Pain_Fall_Left,
		ASSS_Pain_Fall_Right,
		ASSS_Boom_Fall_Fwd,
		ASSS_Boom_Fall_Back,
		ASSS_Boom_Fall_Left,
		ASSS_Boom_Fall_Right,
		ASSS_Spin_Clockwise,
		ASSS_Spin_Anticlockwise,
		ASSS_Feed,
		ASSS_Taunt,
		ASSS_BurningDeath,
		ASSS_Standard_Elevation,
		ASSS_FidgetA,
		ASSS_FidgetB,
		ASSS_Attack_Left_Swipe_In,
		ASSS_Attack_Tail,
		ASSS_Dormant,
		ASSS_Unfurl,
		ASSS_Attack_Both_In,
		ASSS_Attack_Both_Down,
		ASSS_Spasm,
		ASSS_SpearFlyFwrd,
		ASSS_SpearFlyBack,
		ASSS_SpearHitBck,
		ASSS_SpearHitFrnt,
		ASSS_Attack_Low_Left_Swipe,
		ASSS_Attack_Low_Right_Swipe,
		ASSS_Taunt2,
		ASSS_Taunt3,
		ASSS_Fear,
		ASSS_Hit_Left,
		ASSS_Hit_Right,
		ASSS_end,
	} ALIENSTAND_SUBSEQUENCES;

	typedef enum AlienCrouchSubSequences {
		ACrSS_Standard=0,
		ACrSS_Dies,
		ACrSS_Attack_Bite,
		ACrSS_Attack_Tail,
		ACrSS_Attack_Swipe,
		ACrSS_Dies_Thrash,
		ACrSS_Standard_Elevation,
		ACrSS_Pounce,
		ACrSS_Hit_Left,
		ACrSS_Hit_Right,
		ACrSS_Taunt,
		ACrSS_end,
	} ALIENCROUCH_SUBSEQUENCES;

	typedef enum HuggerSubSequences {
		HSS_Stand=0,
		HSS_Run,
		HSS_Dies,
		HSS_Jump,
		HSS_Attack,
		HSS_DieOnFire,
		HSS_Floats,
		HSS_end,
	} HUGGER_SUBSEQUENCES;

	typedef enum MarineRunSubSequences {
		MRSS_Standard=0,
		MRSS_Dies_Standard,
		MRSS_Jump,
		MRSS_Attack_Primary,
		MRSS_Elevation,
		MRSS_Walk,
		MRSS_Tem_Run_On_Fire,
		MRSS_Tem_Run_On_FireB,
		MRSS_Tem_Run_On_FireC,
		MRSS_Mooch_Bored,
		MRSS_Mooch_Alert,
		MRSS_Sprint,
		MRSS_SprintHeadDelta,
		MRSS_Fire_From_Hips,
		MRSS_end,
	} MARINERUN_SUBSEQUENCES;

	typedef enum MarineCrawlSubSequences {
		MCSS_Standard=0,
		MCSS_Dies_Standard,
		MCSS_Jump,
		MCSS_Attack_Primary,
		MCSS_Elevation,
		MCSS_FireFromHips,
		MCSS_end,
	} MARINECRAWL_SUBSEQUENCES;

	typedef enum MarineStandSubSequences {
		MSSS_Standard=0,
		MSSS_Dies_Standard,
		MSSS_Jump,
		MSSS_Attack_Primary,
		MSSS_Elevation,
		MSSS_DieSecondary,
		MSSS_Fidget_A,
		MSSS_Fidget_B,
		MSSS_Fidget_C,
		MSSS_Tem_Back_Death,
		MSSS_Tem_Front_Death,
		MSSS_Tem_Sum_Death,
		MSSS_HitLeftLeg,
		MSSS_HitRightLeg,
		MSSS_HitLeftArm,
		MSSS_HitRightArm,
		MSSS_HitChestFront,
		MSSS_HitChestBack,
		MSSS_HitHeadFront,
		MSSS_HitHeadBack,
		MSSS_Attack_Secondary,
		MSSS_Stand_To_Fidget,
		MSSS_Tem_LeftSholdr,
		MSSS_Tem_RightSholdr,
		MSSS_Tem_LeftThigh,
		MSSS_Tem_RightThigh,
		MSSS_Tem_LeftForarm,
		MSSS_Tem_RightForarm,
		MSSS_Tem_LeftShin,
		MSSS_Tem_RightShin,
		MSSS_Tem_Burning,
		MSSS_Taunt_One,
		MSSS_Wait_Alert,
		MSSS_Minigun_Delta,
		MSSS_WildFire_0,
		MSSS_SpearFlyFwrd,
		MSSS_SpearFlyBack,
		MSSS_SpearHitWallB,
		MSSS_SpearHitWallF,
		MSSS_Spasm,
		MSSS_FireFromHips,
		MSSS_Hip_Fire_Elevation,
		MSSS_WildFire_45,
		MSSS_WildFire_67,
		MSSS_WildFire_90,
		MSSS_Reload,
		MSSS_PumpAction,
		MSSS_Get_Weapon,
		MSSS_Panic_One,
		MSSS_Panic_Two,
		MSSS_Tem_Electric_Death_One,
		MSSS_Tem_Electric_Death_Two,
		MSSS_WildFire_22,
		MSSS_Panic_Reload,
		MSSS_end,
	} MARINESTAND_SUBSEQUENCES;

	typedef enum MarineCrouchSubSequences {
		MCrSS_Standard=0,
		MCrSS_Dies_Standard,
		MCrSS_Jump,
		MCrSS_Attack_Primary,
		MCrSS_Elevation,
		MCrSS_HitLeftLeg,
		MCrSS_HitRightLeg,
		MCrSS_HitLeftArm,
		MCrSS_HitRightArm,
		MCrSS_HitChestFront,
		MCrSS_HitChestBack,
		MCrSS_HitHeadFront,
		MCrSS_HitHeadBack,
		MCrSS_Attack_Secondary,
		MCrSS_PumpAction,
		MCrSS_Tem_Electric_Death_One,
		MCrSS_FireFromHips,
		MCrSS_Hip_Fire_Elevation,
		MCrSS_end,
	}MARINECROUCH_SUBSEQUENCES;

	typedef enum PredatorHUDSubSequences {
		PHSS_Stand=0,	
		PHSS_Run,	
		PHSS_Come,	
		PHSS_Go,	
		PHSS_Attack_Primary,
		PHSS_Attack_Secondary,
		PHSS_Program,
		PHSS_Attack_Jab,
		PHSS_Fidget,
		PHSS_Attack_Primary_Two,
		PHSS_PullBack,
		PHSS_Hold,
		PHSS_Attack_Secondary_Weak_One,
		PHSS_Attack_Secondary_Weak_Two,
		PHSS_Attack_Secondary_Strong_One,
		PHSS_Attack_Secondary_Strong_Two,
		PHSS_end,
	} PREDATORHUD_SUBSEQUENCES;

	typedef enum MarineHUDSubSequences {
		MHSS_Stationary=0,
		MHSS_Standard_Reload,
		MHSS_Standard_Fire,
		MHSS_Come,
		MHSS_Go,
		MHSS_Fidget,
		MHSS_Secondary_Fire,
		MHSS_Tertiary_Fire,
		MHSS_Right_Out,
		MHSS_Left_Out,
		MHSS_end,
	} MARINEHUD_SUBSEQUENCES;

	typedef enum PredatorRunSubSequences {
		PRSS_Standard=0,
		PRSS_Dies_Standard,
		PRSS_Jump,
		PRSS_Attack_Primary,
		PRSS_Elevation,
		PRSS_Attack_Offence_Sweep,
		PRSS_Attack_Defence_Stab,
		PRSS_Attack_Defence_Sweep,
		PRSS_Walk,
		PRSS_end,
	} PREDATORRUN_SUBSEQUENCES;

	typedef enum PredatorCrawlSubSequences {
		PCSS_Standard=0,
		PCSS_Dies_Standard,
		PCSS_Jump,
		PCSS_Attack_Primary,
		PCSS_Elevation,
		PCSS_Attack_Offence_Sweep,
		PCSS_Attack_Defence_Stab,
		PCSS_Attack_Defence_Sweep,
		PCSS_end,
	} PREDATORCRAWL_SUBSEQUENCES;

	typedef enum PredatorStandSubSequences {
		PSSS_Standard=0,
		PSSS_Dies_Standard,
		PSSS_Jump,
		PSSS_Attack_Primary,
		PSSS_Elevation,
		PSSS_Get_Weapon,
		PSSS_HitLeftLeg,
		PSSS_HitRightLeg,
		PSSS_HitLeftArm,
		PSSS_HitRightArm,
		PSSS_HitChestFront,
		PSSS_HitChestBack,
		PSSS_HitHeadFront,
		PSSS_HitHeadBack,
		PSSS_TemDeath_Fwrd,
		PSSS_TemDeath_Bwrd,
		PSSS_Tem_LeftArm,
		PSSS_Tem_LeftLeg,
		PSSS_Tem_RightArm,
		PSSS_Tem_RightLeg,
		PSSS_Tem_Riddled,
		PSSS_Tem_Burning,
		PSSS_Taunt_One,
		PSSS_Attack_Offense_Sweep,
		PSSS_Attack_Defence_Stab,
		PSSS_Attack_Defence_Sweep,
		PSSS_Attack_Quick_Jab,
		PSSS_Attack_Uppercut,
		PSSS_Jump_Up,
		PSSS_Spasm,
		PSSS_end,
	} PREDATORSTAND_SUBSEQUENCES;

	typedef enum PredatorCrouchSubSequences {
		PCrSS_Standard=0,
		PCrSS_Dies_Standard,
		PCrSS_Jump,
		PCrSS_Attack_Primary,
		PCrSS_Elevation,
		PCrSS_Get_Weapon,
		PCrSS_HitLeftLeg,
		PCrSS_HitRightLeg,
		PCrSS_HitLeftArm,
		PCrSS_HitRightArm,
		PCrSS_HitChestFront,
		PCrSS_HitChestBack,
		PCrSS_HitHeadFront,
		PCrSS_HitHeadBack,
		PCrSS_Attack_Offence_Sweep,
		PCrSS_Attack_Defence_Stab,
		PCrSS_Attack_Defence_Sweep,
		PCrSS_Det_Prog,
		PCrSS_Det_Laugh,
		PCrSS_Det_Die1,
		PCrSS_end,
	} PREDATORCROUCH_SUBSEQUENCES;

	typedef enum QueenLeftStanceTemplateSubSequences {
		QLSTSS_Standard,
		QLSTSS_Forward_L2R,
		QLSTSS_Backward_L2R,
		QLSTSS_Right_L2L,
		QLSTSS_Left_L2L,
		QLSTSS_LeftSwipe,
		QLSTSS_RightSwipe,
		QLSTSS_RightHit,
		QLSTSS_LeftHit,
		QLSTSS_end,
	} QLST_SUBSEQUENCES;

	typedef enum QueenLeftStanceFull_SubSequences {
		QLSFSS_Standard_Hiss,
		QLSFSS_Taunt,
		QLSFSS_Forward_L2R,
		QLSFSS_Backward_L2R,
		QLSFSS_Right_L2L,
		QLSFSS_Left_L2L,
		QLSFSS_end,
	} QLSF_SUBSEQUENCES;

	typedef enum QueenRightStanceTemplateSubSequences {
		QRSTSS_Standard,
		QRSTSS_Forward_R2L,
		QRSTSS_Backward_R2L,
		QRSTSS_Right_R2R,
		QRSTSS_Left_R2R,
		QRSTSS_LeftSwipe,
		QRSTSS_RightSwipe,
		QRSTSS_RightHit,
		QRSTSS_LeftHit,
		QRSTSS_LeftSwipe_Low,
		QRSTSS_RightSwipe_Low,
		QRSTSS_end,
	} QRST_SUBSEQUENCES;

	typedef enum QueenRightStanceFull_SubSequences {
		QRSFSS_Standard_Hiss,
		QRSFSS_Taunt,
		QRSFSS_Forward_R2L,
		QRSFSS_Backward_R2L,
		QRSFSS_Right_R2R,
		QRSFSS_Left_R2R,
		QRSFSS_end,
	} QRSF_SUBSEQUENCES;

	typedef enum QueenGeneral_SubSequences {
		QGSS_SquashDeath,
		QGSS_FaceDeath,
		QGSS_FallDeath,
		QGSS_ButtConnect,
		QGSS_RunButtAttack,
		QGSS_Walk,
		QGSS_Explode_Death,
		QGSS_Sprint,
		QGSS_Stop_To_Left,
		QGSS_Stop_To_Right,
		QGSS_Walk_II,
		QGSS_Sprint_II,
		QGSS_Spine_Elevation,
		QGSS_Search_Floor,
		QGSS_Fire_Flinch,
		QGSS_Fire_Steps,
		QGSS_Sprint_Full,
		QGSS_Explosion_Stun,
		QGSS_ClimbOut,
		QGSS_end,
	} QG_SUBSEQUENCES;

	typedef enum AlienHUDSubSequences {
		AHSS_LeftSwipeDown=0,	
		AHSS_RightSwipeDown,	
		AHSS_LeftSwipeIn,	
		AHSS_RightSwipeIn,	
		AHSS_Both_In,
		AHSS_Both_Down,
		AHSS_TailCome,
		AHSS_TailHold,
		AHSS_TailStrike,
		AHSS_PounceIn,
		AHSS_PounceDown,
		AHSS_Hor_Delta,
		AHSS_Ver_Delta,
		AHSS_Eat,
		AHSS_end, 
	} ALIENHUD_SUBSEQUENCES;

	typedef enum XenoborgSubSequences {
		XBSS_Die_Backwards=0,
		XBSS_Die_Forwards,
		XBSS_Fire_Bolter,
		XBSS_Head_Horizontal_Delta,
		XBSS_Head_Vertical_Delta,
		XBSS_LeftArm_Horizontal_Delta,
		XBSS_LeftArm_Vertical_Delta,
		XBSS_Fire_Plasma,
		XBSS_Power_Up,
		XBSS_Power_Down,
		XBSS_RightArm_Horizontal_Delta,
		XBSS_RightArm_Vertical_Delta,
		XBSS_Powered_Down_Standard,
		XBSS_Powered_Up_Standard,
		XBSS_Standing_Death,
		XBSS_Torso_Delta,
		XBSS_Turn_Left,
		XBSS_Turn_Right,
		XBSS_Walking,
		XBSS_LeftLegMissingDeath,
		XBSS_RightLegMissingDeath,
		XBSS_end,
	} XENOBORG_SUBSEQUENCES;

#endif
