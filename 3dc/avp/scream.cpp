// AvP port by Barry Duncan.
// Modified 2026-11-03 by Melanikus for the AvP port:
//   the folder name for voice sounds is copied with a size limit, so it can never overflow
#include "3dc.h"
#include "ourasert.h"
#include "psndplat.h"
#include "jsndsup.h"
#include "mempool.h"
#include "scream.h"
#include "dxlog.h"
#include "menus.h"
#include "logString.h"

extern DISPLAYBLOCK* Player;


struct ScreamSound
{
	struct loaded_sound const * sound_loaded;
	int pitch;
	int volume;	
};

struct ScreamSoundCategory
{
	int num_sounds;
	ScreamSound* sounds;

	SOUNDINDEX last_sound;
};
 
struct ScreamVoiceType
{
	ScreamSoundCategory* category;
};

struct CharacterSoundEffects
{
	int num_voice_types;
	int num_voice_cats;
	ScreamVoiceType* voice_types;
	SOUNDINDEX global_last_sound;

	void PlaySound(int VoiceType,int SoundCategory,int PitchShift,int* ExternalRef,VECTORCH* Location);
	void UnloadSounds();
	void LoadSounds(const char* filename,const char* directory);
};

static CharacterSoundEffects MarineSounds={0,0,0,SID_NOSOUND};
static CharacterSoundEffects AlienSounds={0,0,0,SID_NOSOUND};
static CharacterSoundEffects PredatorSounds={0,0,0,SID_NOSOUND};
static CharacterSoundEffects QueenSounds={0,0,0,SID_NOSOUND};

#if ALIEN_DEMO
#define ScreamFilePath "alienfastfile/"
#elif LOAD_SCREAMS_FROM_FASTFILES
#define ScreamFilePath "fastfile/"
#else
#define ScreamFilePath "sound/"
#endif

// far above the stock files (a few KB), and small enough that no count bounded by the file size below can overflow an
// allocation size
static const uint32_t kMaxScreamFileSize = 0x10000000;

// reads an int of a character sound file at pos, if 4 bytes are left before end
static bool ReadScreamInt(const char *&pos, const char *end, int &value)
{
	if (end - pos < 4)
	{
		return false;
	}
	memcpy(&value, pos, 4);
	pos += 4;
	return true;
}

/*
	Checks the layout of a character sound file (after its 8-byte "MARSOUND" tag) before LoadSounds parses it:
	num_voice_types and num_voice_cats, then for each voice type and category a sound count and that many entries of
	a 0-terminated WAV name, a pitch and a volume (all ints). False if a count is negative or does not fit in the
	data, or if anything lies past end. Nothing is changed.
*/
static bool ScreamFileLayoutIsValid(const char *pos, const char *end)
{
	int numVoiceTypes, numVoiceCats;

	if (!ReadScreamInt(pos, end, numVoiceTypes) || !ReadScreamInt(pos, end, numVoiceCats))
	{
		return false;
	}

	// every category of every voice type takes at least 4 bytes (its count): this also bounds the loops and the
	// allocations in LoadSounds
	if (numVoiceTypes < 0 || numVoiceCats < 0 ||
		numVoiceTypes > (end - pos) / 4 || numVoiceCats > (end - pos) / 4 ||
		(long long)numVoiceTypes * numVoiceCats > (end - pos) / 4)
	{
		return false;
	}

	for (int i = 0; i < numVoiceTypes; i++)
	{
		for (int j = 0; j < numVoiceCats; j++)
		{
			int numSounds;

			// every entry takes at least 9 bytes (an empty name, a pitch and a volume)
			if (!ReadScreamInt(pos, end, numSounds) || numSounds < 0 || numSounds > (end - pos) / 9)
			{
				return false;
			}

			for (int k = 0; k < numSounds; k++)
			{
				const char *nameEnd = (const char *)memchr(pos, 0, end - pos);
				int pitch, volume;

				if (!nameEnd)
				{
					return false;
				}
				pos = nameEnd + 1;

				if (!ReadScreamInt(pos, end, pitch) || !ReadScreamInt(pos, end, volume))
				{
					return false;
				}
			}
		}
	}
	return true;
}

void CharacterSoundEffects::LoadSounds(const char* filename, const char* directory)
{
	if (voice_types) 
		return;

	char path[MAX_PATH] = ScreamFilePath;
	strcat(path, filename);

	HANDLE file = avp_CreateFile(path, GENERIC_READ, 0, 0, OPEN_EXISTING,FILE_FLAG_RANDOM_ACCESS, 0);
	if (file == INVALID_HANDLE_VALUE)
	{
		LOGDXFMT(("Failed to open %s",path));
		return;
	}

	char* buffer;
	uint32_t file_size;
	DWORD bytes_read;

	file_size = GetFileSize(file, 0);
	if (file_size == INVALID_FILE_SIZE || file_size > kMaxScreamFileSize)
	{
		CloseHandle(file);
		LogErrorString(std::string("LoadSounds - cannot read or too large, no sounds loaded from it: ") + path);
		return;
	}
	buffer = new char[file_size+1];
	if (!ReadFile(file,buffer, file_size, &bytes_read, 0))
	{
		bytes_read = 0;
	}
	CloseHandle(file);

	// everything below stays inside the bytes read; the 0 after them ends a name cut off by the end of the file
	buffer[bytes_read] = 0;

	if (bytes_read < 8 || strncmp("MARSOUND", buffer, 8))
	{
		delete[] buffer;
		return;
	}

	// the counts and names come from the file: check that all of it lies inside the data before parsing it (the parse
	// below reads exactly what the check walked through)
	if (!ScreamFileLayoutIsValid(buffer + 8, buffer + bytes_read))
	{
		LogErrorString(std::string("LoadSounds - damaged file, no sounds loaded from it: ") + path);
		delete[] buffer;
		return;
	}

	char* bufpos = buffer+8;

	num_voice_types = *(int*)bufpos;
	bufpos+=4;
	num_voice_cats = *(int*)bufpos;
	bufpos+=4;
	
	voice_types = (ScreamVoiceType*) PoolAllocateMem(num_voice_types * sizeof(ScreamVoiceType));
	
	char wavpath[MAX_PATH];
	size_t const directory_length = strnlen(directory, MAX_PATH - 1);
	memcpy(wavpath, directory, directory_length);
	wavpath[directory_length] = '\0';
	char* wavname = &wavpath[directory_length];
	
	for (int i=0;i<num_voice_types;i++)	
	{
		voice_types[i].category = (ScreamSoundCategory*) PoolAllocateMem( num_voice_cats * sizeof(ScreamSoundCategory));
		for (int j=0;j<num_voice_cats;j++)
		{
			ScreamSoundCategory* cat=&voice_types[i].category[j];
			cat->last_sound = SID_NOSOUND;
			cat->num_sounds = *(int*)bufpos;
			bufpos+=4;

			if (cat->num_sounds)
			{
				cat->sounds = (ScreamSound*) PoolAllocateMem(cat->num_sounds * sizeof(ScreamSound));
			}
			else
			{
				cat->sounds = 0;
			}

			for (int k = 0;k < cat->num_sounds;)
			{
				ScreamSound * sound=&cat->sounds[k];

				// bounded: the name must fit in wavpath after the directory, or this entry is skipped like a sound
				// that does not load
				size_t nameLength = strlen(bufpos);
				bool nameFits = nameLength < (size_t)(&wavpath[MAX_PATH] - wavname);
				if (nameFits)
				{
					memcpy(wavname, bufpos, nameLength + 1);
				}
				else
				{
					LogErrorString(std::string("LoadSounds - sound name too long, skipped: ") + path + ": " + bufpos);
				}
				bufpos+=nameLength+1;

				sound->pitch=*(int*)bufpos;
				bufpos+=4;
				sound->volume=*(int*)bufpos;
				bufpos+=4;

				sound->sound_loaded = nameFits ? GetSound(wavpath) : 0;
				if (sound->sound_loaded)
				{
					k++;
				}
				else
				{
					cat->num_sounds--;
				}
			}
		}
	}

	delete[] buffer;
}

void CharacterSoundEffects::UnloadSounds()
{
	if(!voice_types) return;
	#if !NEW_DEALLOCATION_ORDER
	for(int i=0;i<num_voice_types;i++)
	{
		for(int j=0;j<num_voice_cats;j++)
		{
			ScreamSoundCategory* cat=&voice_types[i].category[j];
			for(int k=0;k<cat->num_sounds;k++)
			{
				LoseSound(cat->sounds[k].sound_loaded);
			}
		}
	}
	#endif
	voice_types=0;
	num_voice_types=0;
	num_voice_cats=0;
}

void CharacterSoundEffects::PlaySound(int VoiceType,int SoundCategory,int PitchShift,int* ExternalRef,VECTORCH* Location)
{
//	GLOBALASSERT(Location);
	
	if(!voice_types) return;
	//make sure the values are within bounds
	if(VoiceType<0 || VoiceType>=num_voice_types) return;
	if(SoundCategory<0 || SoundCategory>=num_voice_cats) return;
	if(ExternalRef)
	{
		if(*ExternalRef!=SOUND_NOACTIVEINDEX)
		{
			//already playing a sound
			return;
		}
	}

	#if 0
	if(Location)
	{
		//need to make sure this sound is close enough to be heard
		VECTORCH seperation=Player->ObWorld;
		SubVector(Location,&seperation);
		
		//(default sound range is 32 metres)
		if(Approximate3dMagnitude(&seperation)>32000)
		{
			//too far away . don't bother playing a sound.
			return;
		}
	}
	#endif

	ScreamSoundCategory* cat=&voice_types[VoiceType].category[SoundCategory];
	//make sure there are some sound for this category
	if(!cat->num_sounds) return;

	int index=FastRandom()% cat->num_sounds;
	int num_checked=0;

	//pick a sound , trying to avoid the last one picked
	if(cat->num_sounds>2)
	{
		while(num_checked<cat->num_sounds)
		{
			SOUNDINDEX sound_ind=(SOUNDINDEX)cat->sounds[index].sound_loaded->sound_num;
			if(sound_ind!=cat->last_sound && sound_ind!=global_last_sound) break;
			
			index++;
			num_checked++;
			if(index==cat->num_sounds) index=0;	
		}
	}

	ScreamSound* sound=&cat->sounds[index];

	int pitch=sound->pitch+PitchShift;

	if(Location)
		Sound_Play((SOUNDINDEX)sound->sound_loaded->sound_num,"dvpe",Location,sound->volume,pitch,ExternalRef);
	else
		Sound_Play((SOUNDINDEX)sound->sound_loaded->sound_num,"vpe",sound->volume,pitch,ExternalRef);

	//take note of the last sound played
	global_last_sound=cat->last_sound=(SOUNDINDEX)sound->sound_loaded->sound_num;
}

void UnloadScreamSounds()
{
	MarineSounds.UnloadSounds();
	AlienSounds.UnloadSounds();
	PredatorSounds.UnloadSounds();
	QueenSounds.UnloadSounds();
}

void LoadMarineScreamSounds()
{
	MarineSounds.LoadSounds("marsound.dat","npc/marinevoice/");
}

void LoadAlienScreamSounds()
{
	AlienSounds.LoadSounds("aliensound.dat","npc/alienvoice/");
}

void LoadPredatorScreamSounds()
{
	PredatorSounds.LoadSounds("predsound.dat","npc/predatorvoice/");
}

void LoadQueenScreamSounds()
{
	QueenSounds.LoadSounds("queensound.dat","npc/queenvoice/");
}

void PlayMarineScream(int VoiceType,int SoundCategory,int PitchShift,int* ExternalRef,VECTORCH* Location)
{
	MarineSounds.PlaySound(VoiceType,SoundCategory,PitchShift,ExternalRef,Location);
}

void PlayAlienSound(int VoiceType,int SoundCategory,int PitchShift,int* ExternalRef,VECTORCH* Location)
{
	AlienSounds.PlaySound(VoiceType,SoundCategory,PitchShift,ExternalRef,Location);
}

void PlayPredatorSound(int VoiceType,int SoundCategory,int PitchShift,int* ExternalRef,VECTORCH* Location)
{
	PredatorSounds.PlaySound(VoiceType,SoundCategory,PitchShift,ExternalRef,Location);
}

void PlayQueenSound(int VoiceType,int SoundCategory,int PitchShift,int* ExternalRef,VECTORCH* Location)
{
	QueenSounds.PlaySound(VoiceType,SoundCategory,PitchShift,ExternalRef,Location);
}
