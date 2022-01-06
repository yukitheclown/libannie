#include <SDL2/SDL_audio.h>
#include <string.h>
#include "audio.h"

#define MIN(x,y) ((x)<(y)?(x):(y))

#define SAMPLERATE 44100
#define	FRACBITS 28
#define SAMPLING_SHIFT 2

static void AudioThread(void *data, Uint8 *ustream, int len){

	AnnieAudioThread *at = (AnnieAudioThread *)data;

	s8 *stream = (s8 *)ustream;

	memset(stream, 0, len);

	if(at->modPlayer.file)
		MODPlayer_MixAudio(&at->modPlayer, stream, len);

	memcpy(at->samples, stream, len);
	at->nSamples = len;

	if(at->effect.curr >> FRACBITS < at->effect.length){
	
		u32 k, i;

		s8 curr; 
		
		s32 maxVolumeSq = ANNIEAUDIO_MAX_VOLUME*ANNIEAUDIO_MAX_VOLUME;
		s32 volume = at->soundsVolume * at->effect.volume;
		// max = MAX_ANNIEAUDIO_VOLUME * MAX_ANNIEAUDIO_VOLUME = maxVolumeSq
		s32 maxMinuxVolume = ANNIEAUDIO_MAX_VOLUME - volume;

		for(k = 0; k < (u32)len; k++){

			at->effect.curr += at->effect.increment;

			i = at->effect.curr >> FRACBITS;

			if(i >= at->effect.length)
				break;

			curr = (((s32)(at->effect.samples[i] * volume)) / maxVolumeSq);
			
			curr += ((s32)stream[k]*maxMinuxVolume) / maxVolumeSq;

			stream[k] = curr;
		}
	}

}


void AnnieAudio_Play(AnnieAudioThread *at, const s8 *samples, u16 len, u8 volume, u16 freq){
	SDL_LockAudio();
	at->effect.samples = samples;
	at->effect.curr = 0;
	at->effect.volume = volume;
	at->effect.length = len;
	at->effect.increment = (freq << FRACBITS)/SAMPLERATE;
	at->effect.increment = ((mod_u64)(8363*1712/freq) << (FRACBITS-SAMPLING_SHIFT))/SAMPLERATE;
	SDL_UnlockAudio();
}

void AnnieAudio_SetMusicVolume(AnnieAudioThread *at, u8 vol){
	SDL_LockAudio();
	at->modPlayer.volume = MIN(vol, ANNIEAUDIO_MAX_VOLUME);
	SDL_UnlockAudio();
}

void AnnieAudio_SetSoundsVolume(AnnieAudioThread *at, u8 vol){
	SDL_LockAudio();
	at->soundsVolume = MIN(vol, ANNIEAUDIO_MAX_VOLUME);
	SDL_UnlockAudio();
}

void AnnieAudio_PlayMOD(AnnieAudioThread *at, MODFile_t *file){
	SDL_LockAudio();
	MODPlayer_Play(&at->modPlayer, file);
	SDL_UnlockAudio();
}

void AnnieAudio_StopMOD(AnnieAudioThread *at){
	SDL_LockAudio();
	at->modPlayer.file = NULL;
	SDL_UnlockAudio();
}

void AnnieAudio_Init(AnnieAudioThread *at){

	memset(at, 0, sizeof(AnnieAudioThread));

	at->soundsVolume = 64;

	SDL_AudioSpec fmt;

	fmt.freq = SAMPLERATE;
	fmt.format = AUDIO_S8;
	fmt.channels = 1;
	fmt.samples = ANNIEAUDIO_MAX_SAMPLES;
	fmt.callback = AudioThread;
	fmt.userdata = at;

	if (SDL_OpenAudio(&fmt, NULL) < 0){
		printf("ANNIE: Unable to open audio: %s\n", SDL_GetError());
		return;
	}

	SDL_PauseAudio(0);
}

void Audio_Close(AnnieAudioThread *at){
	(void)at;
	SDL_CloseAudio();
}