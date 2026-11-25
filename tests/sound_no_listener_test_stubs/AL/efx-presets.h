// Stand-in for OpenAL's room echo presets, for tests/sound_no_listener_test.cpp. Written 2026-11-25 by Melanikus (Vasquez) for the AvP port.
//
// The shape of a preset is OpenAL's own (OpenAL Soft's include/AL/efx-presets.h). The numbers in each are all 0 here: the test's pretend
// device has no effects, so openal_audio.cpp never hands a preset to it, and only its table of rooms needs them to exist.
#ifndef _sound_no_listener_test_efx_presets_h
#define _sound_no_listener_test_efx_presets_h 1

typedef struct {
    float flDensity;
    float flDiffusion;
    float flGain;
    float flGainHF;
    float flGainLF;
    float flDecayTime;
    float flDecayHFRatio;
    float flDecayLFRatio;
    float flReflectionsGain;
    float flReflectionsDelay;
    float flReflectionsPan[3];
    float flLateReverbGain;
    float flLateReverbDelay;
    float flLateReverbPan[3];
    float flEchoTime;
    float flEchoDepth;
    float flModulationTime;
    float flModulationDepth;
    float flAirAbsorptionGainHF;
    float flHFReference;
    float flLFReference;
    float flRoomRolloffFactor;
    int   iDecayHFLimit;
} EFXEAXREVERBPROPERTIES;

#define SOUND_NO_LISTENER_TEST_PRESET { 0.0f }

#define EFX_REVERB_PRESET_GENERIC          SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_PADDEDCELL       SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_ROOM             SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_BATHROOM         SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_LIVINGROOM       SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_STONEROOM        SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_AUDITORIUM       SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_CONCERTHALL      SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_CAVE             SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_ARENA            SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_HANGAR           SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_CARPETEDHALLWAY  SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_HALLWAY          SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_STONECORRIDOR    SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_ALLEY            SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_FOREST           SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_CITY             SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_MOUNTAINS        SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_QUARRY           SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_PLAIN            SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_PARKINGLOT       SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_SEWERPIPE        SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_UNDERWATER       SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_DRUGGED          SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_DIZZY            SOUND_NO_LISTENER_TEST_PRESET
#define EFX_REVERB_PRESET_PSYCHOTIC        SOUND_NO_LISTENER_TEST_PRESET

#endif
