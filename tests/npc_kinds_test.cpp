// The numbers the host's NPC messages carry: see 3dc/avp/npc_kinds.h. Written 2026-11-06 by Melanikus for the AvP port.

// Modified 2026-11-08 by Melanikus for the AvP port:
//   its Between is the shared one in source_slice.h, so a marker that isn't there is a failed check of its own (SOURCE-SLICE-1, #128)
#include <limits.h>
#include <fstream>
#include <sstream>
#include <string>
#include <stdio.h>


#include "npc_kinds.h"
#include "source_slice.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// One of the game's source files. This test can't compile a facehugger, because it needs the whole game, so it reads what the file says.
static std::string ReadSourceFile(char const *name)
{
	std::string path = __FILE__;
	std::string::size_type const tests = path.rfind("tests");

	if (tests == std::string::npos)
	{
		return "";
	}

	std::ifstream file((path.substr(0, tests) + name).c_str(), std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();

	// The game's files end each line with two characters, a return and a new line, but the checks below that cover
	// several lines are written with the new line only. Taking the returns out lets those checks match.
	std::string const text = all.str();
	std::string withoutReturns;

	for (char const c : text)
	{
		if (c != '\r')
		{
			withoutReturns += c;
		}
	}

	return withoutReturns;
}

// The text from one marker up to the next (source_slice.h). A marker that isn't there is a failed check of its own, named by the marker.
static std::string Between(std::string const &text, std::string const &from, std::string const &to)
{
	return SourceSlice(text, from, to, kSliceEndAfterTheStart, [](const std::string &why) { Check(false, why.c_str()); });
}

static bool Has(std::string const &text, std::string const &part)
{
	return text.find(part) != std::string::npos;
}

// Is the first one there, the second one there too, and the first earlier?
static bool ComesBefore(std::string const &text, std::string const &first, std::string const &second)
{
	std::string::size_type const a = text.find(first);
	std::string::size_type const b = text.find(second);

	return a != std::string::npos && b != std::string::npos && a < b;
}

int main()
{
	puts("the kinds");
	Check(NpcKind_IsKnown(kNpcKindMarine), "the first kind is known");
	Check(NpcKind_IsKnown(kNpcKindCount - 1), "and the last");
	Check(!NpcKind_IsKnown(kNpcKindCount), "one past the end isn't");
	Check(!NpcKind_IsKnown(-1), "nor one below nothing");
	Check(!NpcKind_IsKnown(255), "nor the most a byte can say");
	Check(kNpcKindMarine == 0 && kNpcKindPredator == 1 && kNpcKindXenoborg == 2 && kNpcKindQueen == 3 && kNpcKindFacehugger == 4 && kNpcKindSentry == 5, "the numbers go over the network, so they stay as they are");

	puts("the names in a message");
	Check(NpcGuid_IsGood(0), "the lowest name is good");
	Check(NpcGuid_IsGood(0xffffff), "and the highest the game gives");
	Check(!NpcGuid_IsGood(-1), "-1 means a player, so it isn't an NPC");
	Check(!NpcGuid_IsGood(INT_MIN), "and nothing with its top bit set is");

	puts("the length of a sequence");
	Check(NpcSequenceLength_IsGood(1), "the shortest is good");
	Check(!NpcSequenceLength_IsGood(0), "none at all isn't");

	{
		std::string const ghosts = ReadSourceFile("3dc/avp/win95/npc_ghosts.cpp");
		std::string const marine = ReadSourceFile("3dc/avp/bh_marine.cpp");

		puts("a queen's swipes");
		Check(ghosts.find("state->extra2 = (unsigned char)(attack->sub_sequence + 1);") != std::string::npos, "SOURCE: a queen's message says which swipe she's in");
		Check(ghosts.find("ShowTheSwipe(ghostData, state);") != std::string::npos, "SOURCE: and her ghost shows it");
		Check(ghosts.find("!HModelSequence_Exists(model, type, number)") != std::string::npos, "SOURCE: an animation her model doesn't have is never asked for");

		puts("a marine's clothes");
		Check(ghosts.find("state->clothes = (unsigned char)marine->ClothesSet;") != std::string::npos, "SOURCE: a marine's message says which clothes he has");
		Check(ghosts.find("DressTheMarine(ghostData, state->clothes);") != std::string::npos, "SOURCE: and his ghost puts them on");
		Check(marine.find("marineStatusPointer->ClothesSet=GetHierarchyAlternateShapeSetCollectionNumber(") != std::string::npos, "SOURCE: the marine remembers which set he got");
	}

	puts("an NPC's voice");
	Check(NpcSoundPitch_IsGood(0), "its usual pitch is good");
	Check(NpcSoundPitch_IsGood(-32768) && NpcSoundPitch_IsGood(32767), "and so is as far as a pitch may go");
	Check(!NpcSoundPitch_IsGood(-32769) && !NpcSoundPitch_IsGood(32768), "but no further");
	Check(!NpcSoundPitch_IsGood(INT_MAX) && !NpcSoundPitch_IsGood(INT_MIN), "nor the most a message can say");

	{
		char const *const files[] = { "3dc/avp/bh_marine.cpp", "3dc/avp/bh_predator.cpp", "3dc/avp/bh_queen.cpp" };
		char const *const kinds[] = { "kNpcSoundMarine", "kNpcSoundPredator", "kNpcSoundQueen" };

		for (int i = 0; i < 3; i++)
		{
			Check(ReadSourceFile(files[i]).find(std::string("AddNetMsg_NpcSound(") + kinds[i]) != std::string::npos, std::string("SOURCE: ") + files[i] + " tells other machines its NPC's voice");
		}
	}

	{
		std::string const ghosts = ReadSourceFile("3dc/avp/win95/npc_ghosts.cpp");

		puts("a death whose animation is on the second model");
		Check(ghosts.find("if (thisDeath && thisDeath->Template)") != std::string::npos, "SOURCE: the ghost looks at whether this way of dying needs it");
		// The ghost changes to its second model in two places, so this is the death's, found by the comment just before it
		std::string const deathSwap = "// The same call a real corpse makes. Any part the new model doesn't have falls off as debris.\n\t\t\tTransmogrify_HModels(ghost, &ghostData->HModelController, templateModel, 1, 0, 0);";

		Check(ghosts.find(deathSwap) != std::string::npos, "SOURCE: and changes model, as a real corpse does");
		Check(ghosts.find("GetNamedHierarchyFromLibrary(\"hnpcpredator\", \"Template\")") != std::string::npos, "SOURCE: a predator's is this one");
		Check(ghosts.find("GetNamedHierarchyFromLibrary(weapon->Riffname, weapon->TemplateName)") != std::string::npos, "SOURCE: a marine's goes with his gun");
		Check(ghosts.find("thisDeath = deaths;") != std::string::npos, "SOURCE: if the level hasn't got it, he dies the plain way");
		Check(ghosts.find("if (templateModel)") != std::string::npos, "SOURCE: a level without it doesn't stop the game");

		int const swap = (int)ghosts.find(deathSwap);
		int const animation = (int)ghosts.find("UpdateAlienAIGhostAnimSequence(ghost, (HMODEL_SEQUENCE_TYPES)thisDeath->Sequence_Type", swap < 0 ? 0 : swap);

		Check(swap >= 0 && animation > swap, "SOURCE: and changes model before asking for the animation");
	}

	{
		std::string const ghosts = ReadSourceFile("3dc/avp/win95/npc_ghosts.cpp");
		std::string const network = ReadSourceFile("3dc/avp/win95/pldnet.cpp");
		std::string const drawing = ReadSourceFile("3dc/avp/particle.cpp");

		puts("a xenoborg's beams");
		Check(NpcXenoBeams_AreGood(0), "none on is fine");
		Check(NpcXenoBeams_AreGood(kNpcXenoBeamsThatExist), "and so are all five");
		Check(!NpcXenoBeams_AreGood(32), "a sixth doesn't exist");
		Check(!NpcXenoBeams_AreGood(255), "and nor does every bit there is");
		Check((kNpcXenoBeamHead | kNpcXenoBeamLeftArm | kNpcXenoBeamRightArm | kNpcXenoBeamLeftGun | kNpcXenoBeamRightGun) == kNpcXenoBeamsThatExist, "the five make up the whole list");
		Check(ghosts.find("deltas->beams = (unsigned char)NpcGhosts_XenoBeamsOf(sbPtr);") != std::string::npos, "SOURCE: the host says which are on");
		Check(network.find("if (!NpcXenoBeams_AreGood(messagePtr->beams))") != std::string::npos, "SOURCE: and a message naming one he hasn't got is refused");
		Check(ghosts.find("ghostData->npcXenoBeams = deltas->beams;") != std::string::npos, "SOURCE: the ghost remembers them");
		Check(ghosts.find("ghostData->integrity < GHOST_INTEGRITY - kLongestABeamStaysOnWithoutWord") != std::string::npos, "SOURCE: and they stop if the messages do");
		Check(drawing.find("NpcGhosts_XenoBeamsToDraw(sbPtr, aiming, &leftGun, &rightGun)") != std::string::npos, "SOURCE: the ghost's beams are drawn");
		Check(drawing.find("DrawXenoborgBeams(statusPtr->TargetingLaser, &statusPtr->LeftMainBeam, &statusPtr->RightMainBeam);") != std::string::npos, "SOURCE: and the real one's still are");
	}

	{
		std::string const network = ReadSourceFile("3dc/avp/win95/pldnet.cpp");
		std::string const queen = ReadSourceFile("3dc/avp/bh_queen.cpp");

		puts("what a queen throws");
		Check(queen.find("AddNetMsg_NpcThrownObject(QueenObjectList[i]);") != std::string::npos, "SOURCE: she tells the other machines when one moves");
		Check(queen.find("QueenObjectLastSaidAt[NumQueenObjects]=sbPtr->DynPtr->Position;") != std::string::npos, "SOURCE: and it starts where the level put it, so nothing's sent until it moves");
		Check(network.find("!strstr(object->name, \"QueenAmmo\")") != std::string::npos, "SOURCE: nothing but what she throws can be moved by this message");
		Check(network.find("object->I_SBtype != I_BehaviourInanimateObject || !object->DynPtr") != std::string::npos, "SOURCE: a light, lift or switch with that name isn't touched");
		Check(network.find("a thrown object's message put it outside the level") != std::string::npos, "SOURCE: and it can't be put outside the level");
		Check(network.find("messagePtr->xOrient & 4095") != std::string::npos && network.find("messagePtr->yOrient & 4095") != std::string::npos && network.find("messagePtr->zOrient & 4095") != std::string::npos, "SOURCE: and its angles are kept inside the table they're looked up in");
		Check(network.find("object->containingModule = room;") != std::string::npos, "SOURCE: and it's in the room it was moved to");
	}

	{
		std::string const targeting = ReadSourceFile("3dc/avp/targeting.cpp");
		std::string const generators = ReadSourceFile("3dc/avp/bh_generator.cpp");
		std::string const smell = ReadSourceFile("3dc/avp/pheromone.cpp");
		std::string const tracker = ReadSourceFile("3dc/avp/hud.cpp");
		std::string const marine = ReadSourceFile("3dc/avp/bh_marine.cpp");
		std::string const predator = ReadSourceFile("3dc/avp/bh_predator.cpp");

		puts("what a player on another machine can do to NPCs, and they to him");
		Check(targeting.find("ghostDataPtr->type==I_BehaviourXenoborg") != std::string::npos, "SOURCE: a shoulder cannon can lock on to a xenoborg's ghost");
		Check(targeting.find("ghostDataPtr->type==I_BehaviourMarine && NpcGhosts_MarineIsAnAndroid(ghostDataPtr->subtype))\n\t\t\t\t\t{\n\t\t\t\t\t\treturn 0;") != std::string::npos, "SOURCE: and not to an android's in the heat view");
		Check(targeting.find("ghostDataPtr->type==I_BehaviourMarine && NpcGhosts_MarineIsAnAndroid(ghostDataPtr->subtype))\n\t\t\t\t\t{\n\t\t\t\t\t\treturn 1;") != std::string::npos, "SOURCE: but yes in the view for machines");
		Check(targeting.find("if (ghostDataPtr->type==I_BehaviourPredator)\n\t\t\t\t\t{\n\t\t\t\t\t\treturn 1;") != std::string::npos, "SOURCE: an NPC predator's ghost is an enemy in any game");
		Check(targeting.find("if (SameAsSinglePlayer()) {\n\t\t\tif (netGameData.disableFriendlyFire && candidate->I_SBtype==I_BehaviourNetGhost") != std::string::npos, "SOURCE: friendly fire off only protects other players, and an NPC's block isn't read as a ghost's");
		Check(targeting.find("case I_BehaviourQueenAlien:\n\t\t\t\t\tcase I_BehaviourFaceHugger:\n\t\t\t\t\tcase I_BehaviourXenoborg:\n\t\t\t\t\tcase I_BehaviourMarine:\n\t\t\t\t\t\treturn(1);") != std::string::npos, "SOURCE: a smart gun can lock on to their ghosts");
		// The same test is written twice, so each is found by the line that follows it
		Check(generators.find("if(UseGeneratorBalance && AvP.Network != I_No_Network && !SameAsSinglePlayer())\n\t{\n\t\tgenBlock->Timer") != std::string::npos, "SOURCE: a generator's rate stays the level's own");
		Check(generators.find("if(UseGeneratorBalance && AvP.Network != I_No_Network && !SameAsSinglePlayer())\n\t{\n\t\tif(genBlock->use_own_max_npc)") != std::string::npos, "SOURCE: and so does the most NPCs it keeps at once");
		Check(generators.find("if(AvP.Network != I_No_Network && !SameAsSinglePlayer())\n\t{\n\t\tGeneratorBalance_PerFrameMaintenance") != std::string::npos, "SOURCE: and no balance is kept");
		Check(smell.find("ghostData->type==I_BehaviourAlienPlayer && SameAsSinglePlayer()") != std::string::npos, "SOURCE: an alien on another machine leaves a trail");
		Check(tracker.find("ghostData->HModelController.Sub_Sequence == ASSS_Dormant") != std::string::npos, "SOURCE: the ghost of an alien lying in wait isn't on the tracker");
		Check(marine.find("((NETGHOSTDATABLOCK *)otherPlayer->SBdataptr)->myGunFlash") != std::string::npos, "SOURCE: a marine can hear another machine's player firing");
		Check(marine.find("marineStatusPointer->suspect_point=playerHeardAt;") != std::string::npos, "SOURCE: and goes to look where he heard it");
		Check(predator.find("(SbPtr->I_SBtype==I_BehaviourNetGhost && NPC_IsAPlayer(SbPtr))") != std::string::npos, "SOURCE: a predator's self-destruct reaches other machines' players");
	}

	{
		std::string const ghosts = ReadSourceFile("3dc/avp/win95/npc_ghosts.cpp");

		puts("a dead facehugger or sentry gun");
		Check(ghosts.find("UpdateAlienAIGhostAnimSequence(ghost, HMSQT_Hugger, ghost->SBDamageBlock.IsOnFire ? HSS_DieOnFire : HSS_Dies,") != std::string::npos, "SOURCE: a facehugger's ghost plays its death");
		Check(ghosts.find("KillRandomSections(ghostData->HModelController.section_data, ONE_FIXED >> 1);") != std::string::npos, "SOURCE: a gun's loses pieces, as the real one does");
		Check(ghosts.find("ghostData->HModelController.Looped = 0;") != std::string::npos, "SOURCE: and stops its animation");
		Check(ghosts.find("if (type == I_BehaviourFaceHugger || type == I_BehaviourAutoGun)\n\t{\n\t\tghostData->integrity = deathTime;") != std::string::npos, "SOURCE: and each lasts as long as the host said, with nothing else to end it");
	}

	{
		std::string const ghosts = ReadSourceFile("3dc/avp/win95/npc_ghosts.cpp");

		puts("an NPC outside every room");
		Check(ghosts.find("if (!ModuleFromPosition(&place, sbPtr->containingModule) && ModuleFromPosition(&startOfFrame, sbPtr->containingModule))\n\t{\n\t\tplace = startOfFrame;\n\t}") != std::string::npos, "SOURCE: the host sends the place the game keeps it at, not one outside every room");
		Check(ghosts.find("int const x = place.vx, y = place.vy, z = place.vz;") != std::string::npos, "SOURCE: and that's the place in the message");
		Check(ghosts.find("ghostData->integrity = GHOST_INTEGRITY;\n\n\t// A marine changes model") != std::string::npos, "SOURCE: a ghost lives on while the host speaks of it, wherever the message puts it");
		Check(ghosts.find("if (module)\n\t{\n\t\tghost->containingModule = module;\n\t\tdynPtr->Position = *position;\n\t}") != std::string::npos, "SOURCE: and a place outside the level is the only part of the message not used");

		puts("a predator arming his self-destruct");
		Check(ghosts.find("if (templateModel && HModelSequence_Exists_FromRoot(templateModel, state->sequence_type, state->sub_sequence))") != std::string::npos, "SOURCE: his ghost changes model for an animation only the Template has");
		Check(ghosts.find("\t\t\tRemoveAllDeltas(&ghostData->HModelController);\n\t\t\tTransmogrify_HModels(ghost, &ghostData->HModelController, templateModel, 1, 0, 0);") != std::string::npos, "SOURCE: the same way the real one does");
	}

	{
		std::string const ghosts = ReadSourceFile("3dc/avp/win95/npc_ghosts.cpp");
		std::string const gun = ReadSourceFile("3dc/avp/bh_agun.cpp");

		puts("a destroyed sentry gun's wreck");
		Check(gun.find("AddNetMsg_NpcKilled(sbPtr, agunStatusPointer->HModelController.Sub_Sequence, AGUN_DYINGTIME, 0);") != std::string::npos, "SOURCE: its death message carries the animation it stops on");
		Check(ghosts.find("if (HModelSequence_Exists(&ghostData->HModelController, sequenceType, deathCode))\n\t\t{\n\t\t\tUpdateAlienAIGhostAnimSequence(ghost, (HMODEL_SEQUENCE_TYPES)sequenceType, deathCode, -1, ONE_FIXED >> 3);") != std::string::npos, "SOURCE: its ghost stops on the same one, if its model has it");
	}

	{
		std::string const ghosts = ReadSourceFile("3dc/avp/win95/npc_ghosts.cpp");

		puts("a facehugger that dies on someone's face");
		Check(ghosts.find("if (PlayerStatusPtr && PlayerStatusPtr->MyFaceHugger == ghost)\n\t\t{\n\t\t\tPlayerStatusPtr->MyFaceHugger = NULL;") != std::string::npos, "SOURCE: its ghost lets go of this machine's player as it dies, as the real one does");
	}

	{
		std::string const net = ReadSourceFile("3dc/avp/win95/pldnet.cpp");
		std::string const lift = ReadSourceFile("3dc/avp/bh_platform_lift.cpp");
		std::string const received = Between(net, "static void ProcessNetMsg_LiftState(NETMESSAGE_LIFTSTATE *messagePtr)\n{", "// A thing the queen throws has moved.");
		std::string const take = "PlatformLift_TakeTheHostsCar(lift, messagePtr->y, messagePtr->state, messagePtr->end);";

		puts("a joiner's lift car following the host's");
		Check(Has(received, "Net_NoteRefused(\"a lift's sync message named something that isn't a platform lift\");\n\t\treturn;") && ComesBefore(received, "Net_NoteRefused(\"a lift's sync message named something that isn't a platform lift\");", take), "SOURCE: a lift sync message naming something that isn't a platform lift is refused before the car takes it");
		Check(Has(received, "Net_NoteRefused(\"a lift's sync message named a lift whose name another lift shares\");\n\t\treturn;") && ComesBefore(received, "Net_NoteRefused(\"a lift's sync message named a lift whose name another lift shares\");", take), "SOURCE: a lift sync message naming a shared name is refused before the car takes it");
		Check(Has(received, "Net_NoteRefused(\"a lift's sync message named a state lifts don't have\");\n\t\treturn;") && ComesBefore(received, "Net_NoteRefused(\"a lift's sync message named a state lifts don't have\");", take), "SOURCE: a lift sync message naming a state lifts don't have is refused before the car takes it");
		Check(Has(received, "Net_NoteRefused(\"a lift's sync message named an end a lift doesn't have\");\n\t\treturn;") && ComesBefore(received, "Net_NoteRefused(\"a lift's sync message named an end a lift doesn't have\");", take), "SOURCE: a lift sync message naming an end a lift doesn't have is refused before the car takes it");
		Check(Has(received, "Net_NoteRefused(\"a lift's sync message put the car off its track\");\n\t\treturn;") && ComesBefore(received, "Net_NoteRefused(\"a lift's sync message put the car off its track\");", take), "SOURCE: a lift sync message naming a car off its track is refused before the car takes it");
		Check(Has(lift, "BOOL const followsTheHost = AvP.Network == I_Peer && SameAsSinglePlayer() && platformliftdata->syncHeard;") && Has(lift, "\telse switch(platformliftdata->state)\n"), "SOURCE: a joiner's car only follows the host's in the mode, once the host has spoken, and then doesn't move by itself");
		Check(Has(lift, "if ((int)platformliftdata->state != platformliftdata->syncLastSentState || platformliftdata->syncSinceSent >= ONE_FIXED)"), "SOURCE: the host sends when the state changes, and once a second");
		Check(Has(lift, "NetLiftFollowStep(dynPtr->Position.vy, hostHeight, mostThisFrame, someoneOnIt != FALSE, MAXIMUM_STEP_HEIGHT);") && Has(lift, "if (!someoneOnIt && (gap > MAXIMUM_STEP_HEIGHT || -gap > MAXIMUM_STEP_HEIGHT))"), "SOURCE: a car with someone on it travels, and only one with nobody on it goes straight there");
		Check(Has(lift, "\tif (SameAsSinglePlayer() && platformliftdata->syncHeard)\n\t{\n\t\treturn;\n\t}\n"), "SOURCE: once the host's sync has spoken, the old state message leaves the car alone");
		Check(Has(lift, "LogString(\"lift: two platform lifts in this level share a name, so a network game can't keep their cars together\");"), "SOURCE: a level whose lifts share a name says so as it loads");
	}

	puts("how cloaked a predator is");
	Check(NpcCloak_ToByte(0) == 0, "not cloaked stays not cloaked");
	Check(NpcCloak_ToByte(-1) == 0, "and below nothing is the same");
	Check(NpcCloak_ToByte(1) == 1, "the least bit of cloak still counts as a cloak");
	Check(NpcCloak_ToByte(32768) == 128, "half cloaked is about half");
	Check(NpcCloak_ToByte(65535) == 255, "nearly full");
	Check(NpcCloak_ToByte(65536) == 255, "full is the most a byte can say");
	Check(NpcCloak_ToByte(INT_MAX) == 255, "and more than that doesn't wrap round");
	Check(NpcCloak_FromByte(0) == 0, "no cloak comes back as none");
	Check(NpcCloak_FromByte(128) == 32768, "half comes back as half");
	Check(NpcCloak_FromByte(255) == 65280, "and full as nearly the whole of it");

	{
		std::string const ghosts = ReadSourceFile("3dc/avp/win95/npc_ghosts.cpp");

		Check(ghosts.find("NpcCloak_ToByte(predator->CloakingEffectiveness)") != std::string::npos, "SOURCE: a predator's message says how cloaked it is");
		Check(ghosts.find("NpcCloak_FromByte(state->extra1)") != std::string::npos, "SOURCE: and its ghost is drawn by that");
		Check(ghosts.find("ghostData->timer = deathTime;") != std::string::npos && ghosts.find("ghostData->CloakingEffectiveness = 0;", ghosts.find("ghostData->timer = deathTime;")) != std::string::npos, "SOURCE: a dead predator's ghost loses its cloak, as the real one does");
	}

	puts("how long a corpse lasts");
	Check(NpcDeathTime_IsGood(1), "the shortest is good");
	Check(NpcDeathTime_IsGood(4 * 65536), "a facehugger's 4 seconds are good");
	Check(NpcDeathTime_IsGood(16 * 65536), "a predator's and a marine's 16 seconds are good");
	Check(!NpcDeathTime_IsGood(16 * 65536 + 1), "longer than any the game gives isn't");
	Check(!NpcDeathTime_IsGood(0), "none at all isn't");
	Check(!NpcDeathTime_IsGood(-1), "below nothing isn't");
	Check(!NpcDeathTime_IsGood(INT_MIN), "nor the lowest a message can say");
	Check(!NpcDeathTime_IsGood(INT_MAX), "nor the highest");

	{
		std::string const corpses = ReadSourceFile("3dc/avp/bh_corpse.cpp");
		std::string const network = ReadSourceFile("3dc/avp/win95/pldnet.cpp");

		Check(corpses.find("corpseDataPtr->timer, corpseDataPtr->GibbFactor") == std::string::npos, "SOURCE: a death message doesn't send a timer nothing has set yet");
		Check(network.find("NpcDeathTime_IsGood(messagePtr->death_time)") != std::string::npos, "SOURCE: a death's time from another machine is checked");
	}

	puts("a bone's position in one byte");
	Check(NpcDelta_ToByte(0) == 0, "nothing stays nothing");
	Check(NpcDelta_ToByte(65535) == 255, "the end of the range is the top of the byte");
	Check(NpcDelta_ToByte(32767) == 127 && NpcDelta_FromByte(127) == 32512, "the middle comes back close to the middle");
	Check(NpcDelta_ToByte(-5) == 0, "below nothing is held at nothing");
	Check(NpcDelta_ToByte(70000) == 255, "past the end is held at the end");
	Check(NpcDelta_FromByte(255) < 65536, "and what comes back always fits a delta's timer");

	puts("what a new NPC starts with");
	std::string const hugger = ReadSourceFile("3dc/avp/bh_facehugger.cpp");
	Check(hugger.find("facehuggerStatus->Target = NULL;") != std::string::npos, "a new facehugger is after nobody, because its memory holds whatever was there before, and the pointer gets followed");
	Check(hugger.find("memset(facehuggerStatus->Target_SBname, 0,") != std::string::npos, "and it has no target's name either");
	Check(hugger.find("memset(facehuggerStatus, 0, sizeof(FACEHUGGER_STATUS_BLOCK));") != std::string::npos, "and the rest of it starts empty too");
	Check(ReadSourceFile("3dc/avp/bh_queen.cpp").find("memset(queenStatus, 0, sizeof(QUEEN_STATUS_BLOCK));") != std::string::npos, "so does a queen");
	Check(ReadSourceFile("3dc/avp/win95/pldghost.cpp").find("memset(ghostData, 0, sizeof(NETGHOSTDATABLOCK));") != std::string::npos, "and a ghost");
	Check(ReadSourceFile("3dc/avp/strategy_def.cpp").find("sptr->SBflags.placed_in_the_level = 0;") != std::string::npos, "and a block used again isn't still marked as placed in the level");

	// The flags a state message may set: 1, 2 and 4. 8 was an NPC marine's lamp on one test branch; each machine decides that for itself now.
	Check(kNpcFlagsKnown == 7, "the flags known are 1, 2 and 4");
	Check(NpcFlags_AreKnown(0) && NpcFlags_AreKnown(7), "no flags, or all three, are known");
	Check(!NpcFlags_AreKnown(8) && !NpcFlags_AreKnown(16) && !NpcFlags_AreKnown(255) && !NpcFlags_AreKnown(7 | 128), "and any other bit isn't, the lamp's old 8 included");
	{
		std::string const net = ReadSourceFile("3dc/avp/win95/pldnet.cpp");
		std::string const ghosts = ReadSourceFile("3dc/avp/win95/npc_ghosts.cpp");
		std::string const marine = ReadSourceFile("3dc/avp/bh_marine.cpp");

		Check(Has(net, "if (!NpcFlags_AreKnown(messagePtr->flags))"), "SOURCE: a state message with a flag nobody knows is refused");
		Check(ComesBefore(net, "if (!NpcFlags_AreKnown(messagePtr->flags))", "NpcGhosts_ApplyState(sbPtr, messagePtr, &position, &orientation);"), "SOURCE: before anything in it is used");
		Check(!Has(ghosts, "kNpcFlagLampOn") && !Has(ReadSourceFile("3dc/avp/npc_kinds.h"), "kNpcFlagLampOn"), "SOURCE: nothing about an NPC marine's lamp crosses the network");
		Check(Has(ghosts, "if (ghostData->type == I_BehaviourMarine && !NpcGhosts_MarineIsAnAndroid(state->model)"), "SOURCE: each machine decides it for the NPC marines it shows, and only for a marine");
		Check(Between(marine, "MARINE_STATUS_BLOCK *marineStatus = (MARINE_STATUS_BLOCK *)sbPtr->SBdataptr;", "marineStatus->lampOn = 0;").size() < 200, "SOURCE: a marine's lamp starts off");
		Check(Has(net, "messagePtr->lampOn = ThisPlayersLampShows() ? 1 : 0;"), "SOURCE: a player's lamp bit is always written");
		Check(Has(net, "return SameAsSinglePlayer() && AvP.PlayerType == I_Marine && status && status->IsAlive && ShoulderLamp_IsOn();"), "SOURCE: and only says on for a live marine, in the mode that plays as single player");
		Check(Has(net, "if (messagePtr->lampOn && !(SameAsSinglePlayer() && messagePtr->characterType == NGCT_Marine && messagePtr->IAmAlive))"), "SOURCE: a player's message saying a lamp is on, from anyone who can't have one, is refused");
		Check(ComesBefore(net, "a player's message said his lamp is on, and he can't have one, so none of it was used", "netGameData.playerData[playerIndex].lastKnownPosition = position;"), "SOURCE: whole, before even the place it gives is written down");
		Check(Has(net, "ghostData->lampOn = (messagePtr->lampOn && ghostData->type == I_BehaviourMarinePlayer) ? 1 : 0;"), "SOURCE: and a player's ghost only stores it");
		Check(Has(ReadSourceFile("3dc/avp/win95/pldnet.h"), "static_assert(sizeof(NETMESSAGE_PLAYERSTATE) == 25,"), "SOURCE: and the player's message didn't grow");
	}

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	puts("All checks passed");
	return 0;
}
