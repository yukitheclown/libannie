#ifndef AUDIO_DEF
#define AUDIO_DEF

#include "types.h"
#include "mod.h"

#define ANNIEAUDIO_MAX_VOLUME 64
#define ANNIEAUDIO_MAX_SAMPLES 1024

typedef struct {
	u8 			volume;
	u16 		length;
	const s8 	*samples;
	u64 		curr;
	u64 		increment;
} SoundEffect;

typedef struct {
	u8 				soundsVolume;
	SoundEffect 	effect;
	MODPlayer_t 	modPlayer;
	s8 				samples[ANNIEAUDIO_MAX_SAMPLES];
	u32 			nSamples;
} AnnieAudioThread;

void AnnieAudio_Init(AnnieAudioThread *at);
void AnnieAudio_SetMusicVolume(AnnieAudioThread *at, u8 vol);
void AnnieAudio_SetSoundsVolume(AnnieAudioThread *at, u8 vol);
void AnnieAudio_Close(AnnieAudioThread *at);
void AnnieAudio_Play(AnnieAudioThread *at, const s8 *samples, u16 len, u8 volume, u16 freq);
void AnnieAudio_PlayMOD(AnnieAudioThread *at, MODFile_t *file);
void AnnieAudio_StopMOD(AnnieAudioThread *at);

#endif