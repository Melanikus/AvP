// Stand-in for OpenAL's device header, for tests/sound_no_listener_test.cpp. Written 2026-11-25 by Melanikus (Vasquez) for the AvP port.
//
// Only what openal_audio.cpp uses. The numbers are OpenAL's own (OpenAL Soft's include/AL/alc.h, which is in the public domain). The
// functions are stand-ins in the test: one pretend device, with no effects (no ALC_EXT_EFX).
#ifndef _sound_no_listener_test_alc_h
#define _sound_no_listener_test_alc_h 1

typedef struct ALCdevice ALCdevice;
typedef struct ALCcontext ALCcontext;
typedef char ALCboolean;
typedef char ALCchar;
typedef int ALCint;
typedef int ALCsizei;
typedef int ALCenum;

#define ALC_DEFAULT_DEVICE_SPECIFIER             0x1004
#define ALC_DEVICE_SPECIFIER                     0x1005
#define ALC_MONO_SOURCES                         0x1010
#define ALC_STEREO_SOURCES                       0x1011

const ALCchar *alcGetString(ALCdevice *device, ALCenum param);
ALCdevice *alcOpenDevice(const ALCchar *devicename);
ALCboolean alcCloseDevice(ALCdevice *device);
ALCcontext *alcCreateContext(ALCdevice *device, const ALCint *attrlist);
ALCboolean alcMakeContextCurrent(ALCcontext *context);
void alcDestroyContext(ALCcontext *context);
ALCboolean alcIsExtensionPresent(ALCdevice *device, const ALCchar *extname);
void alcGetIntegerv(ALCdevice *device, ALCenum param, ALCsizei size, ALCint *values);

#endif
