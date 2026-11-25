// Stand-in for OpenAL's extensions header, for tests/sound_no_listener_test.cpp. Written 2026-11-25 by Melanikus (Vasquez) for the AvP port.
//
// Only what openal_audio.cpp uses: the room echo's (EFX) names and the types of its functions. OpenAL Soft's alext.h has these through its
// efx.h; here they are in this one file. The numbers are OpenAL's own (OpenAL Soft's include/AL/efx.h). The test's pretend device has no
// effects (alcIsExtensionPresent says no to ALC_EXT_EFX), so none of these functions is ever asked for or called.
#ifndef _sound_no_listener_test_alext_h
#define _sound_no_listener_test_alext_h 1

#include "al.h"
#include "alc.h"

#define ALC_EXT_EFX 1

#define AL_EFFECT_TYPE                           0x8001
#define AL_EFFECT_NULL                           0x0000
#define AL_EFFECT_REVERB                         0x0001
#define AL_EFFECT_EAXREVERB                      0x8000
#define AL_EFFECTSLOT_NULL                       0x0000
#define AL_EFFECTSLOT_EFFECT                     0x0001
#define AL_EFFECTSLOT_GAIN                       0x0002
#define AL_FILTER_NULL                           0x0000
#define AL_AUXILIARY_SEND_FILTER                 0x20006

#define AL_REVERB_DENSITY                        0x0001
#define AL_REVERB_DIFFUSION                      0x0002
#define AL_REVERB_GAIN                           0x0003
#define AL_REVERB_GAINHF                         0x0004
#define AL_REVERB_DECAY_TIME                     0x0005
#define AL_REVERB_DECAY_HFRATIO                  0x0006
#define AL_REVERB_REFLECTIONS_GAIN               0x0007
#define AL_REVERB_REFLECTIONS_DELAY              0x0008
#define AL_REVERB_LATE_REVERB_GAIN               0x0009
#define AL_REVERB_LATE_REVERB_DELAY              0x000A
#define AL_REVERB_AIR_ABSORPTION_GAINHF          0x000B
#define AL_REVERB_ROOM_ROLLOFF_FACTOR            0x000C
#define AL_REVERB_DECAY_HFLIMIT                  0x000D

#define AL_EAXREVERB_DENSITY                     0x0001
#define AL_EAXREVERB_DIFFUSION                   0x0002
#define AL_EAXREVERB_GAIN                        0x0003
#define AL_EAXREVERB_GAINHF                      0x0004
#define AL_EAXREVERB_GAINLF                      0x0005
#define AL_EAXREVERB_DECAY_TIME                  0x0006
#define AL_EAXREVERB_DECAY_HFRATIO               0x0007
#define AL_EAXREVERB_DECAY_LFRATIO               0x0008
#define AL_EAXREVERB_REFLECTIONS_GAIN            0x0009
#define AL_EAXREVERB_REFLECTIONS_DELAY           0x000A
#define AL_EAXREVERB_REFLECTIONS_PAN             0x000B
#define AL_EAXREVERB_LATE_REVERB_GAIN            0x000C
#define AL_EAXREVERB_LATE_REVERB_DELAY           0x000D
#define AL_EAXREVERB_LATE_REVERB_PAN             0x000E
#define AL_EAXREVERB_ECHO_TIME                   0x000F
#define AL_EAXREVERB_ECHO_DEPTH                  0x0010
#define AL_EAXREVERB_MODULATION_TIME             0x0011
#define AL_EAXREVERB_MODULATION_DEPTH            0x0012
#define AL_EAXREVERB_AIR_ABSORPTION_GAINHF       0x0013
#define AL_EAXREVERB_HFREFERENCE                 0x0014
#define AL_EAXREVERB_LFREFERENCE                 0x0015
#define AL_EAXREVERB_ROOM_ROLLOFF_FACTOR         0x0016
#define AL_EAXREVERB_DECAY_HFLIMIT               0x0017

typedef void (*LPALGENEFFECTS)(ALsizei n, ALuint *effects);
typedef void (*LPALDELETEEFFECTS)(ALsizei n, const ALuint *effects);
typedef void (*LPALEFFECTI)(ALuint effect, ALenum param, ALint iValue);
typedef void (*LPALEFFECTF)(ALuint effect, ALenum param, ALfloat flValue);
typedef void (*LPALEFFECTFV)(ALuint effect, ALenum param, const ALfloat *pflValues);
typedef void (*LPALGENAUXILIARYEFFECTSLOTS)(ALsizei n, ALuint *effectslots);
typedef void (*LPALDELETEAUXILIARYEFFECTSLOTS)(ALsizei n, const ALuint *effectslots);
typedef void (*LPALAUXILIARYEFFECTSLOTI)(ALuint effectslot, ALenum param, ALint iValue);
typedef void (*LPALAUXILIARYEFFECTSLOTF)(ALuint effectslot, ALenum param, ALfloat flValue);

#endif
