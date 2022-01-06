#ifndef MOD_DEF
#define MOD_DEF

#include <stdint.h>

#define MODFILE_DIVISION_EFFECT(d) ((((d) & 0xFF000000) >> 24) | (((d) & 0x000F0000) >> 8))
#define MODFILE_DIVISION_PERIOD(d) ((((d) & 0x0000000F) << 8) | (((d) & 0x0000FF00) >> 8))
#define MODFILE_DIVISION_SAMPLE(d) (((d) & 0x000000F0) | ((((d) & 0x00F00000) >> 20)))
#define MOD_SAMPLERATE 44100
#define MOD_TRUE 1
#define MOD_FALSE 0

typedef uint64_t 	mod_u64;
typedef uint32_t 	mod_u32;
typedef uint16_t 	mod_u16;
typedef uint8_t 	mod_u8;
typedef int64_t 	mod_s64;
typedef int32_t 	mod_s32;
typedef int16_t 	mod_s16;
typedef int8_t 		mod_s8;

typedef struct {
	mod_u8 		finetune;
	mod_u16 	len;
	mod_u8 		volume;
	mod_u16 	repeat;
	mod_u16 	repeatLen;
	char 		name[22];
	mod_s8 		samples[65535*2];
} MODInstrument_t;

typedef struct {
	mod_u8 				nPatterns;
	mod_u8 				nPositions;
	char 				title[20];
	MODInstrument_t 	instruments[32];
	mod_u32 			divisions[64*64*4];
	mod_u8 				patternTable[128];
} MODFile_t;

typedef struct {
	mod_u8 				division;
	mod_u8 				type;
	mod_u8 				x;
	mod_u8 				y;
} MODPlayedEffect_t;

typedef struct {
	mod_u8 				division;
	mod_u16 			period;
} MODPlayedNote_t;

typedef struct {
	mod_u16 			to;
	mod_s16 			speed;
} MODSlide_t;

typedef struct {
	mod_u8 				rate;
	mod_u8				waveform;
	mod_u16 			amplitude;
	mod_u16 			start;
	mod_u32 			curr;
} MODVibrato_t;

typedef struct {
	mod_u8 				glissando;
	mod_u8 				instrument;
	mod_u8				volume;
	mod_s8 				finetune;
	mod_u16				period;
	mod_u16 			repeatEnd;

	MODPlayedEffect_t 	effect;
	MODPlayedNote_t		note;

	MODSlide_t 			noteSlide;
	MODSlide_t 			volSlide;
	MODVibrato_t		vibrato;
	MODVibrato_t		tremolo;

	union {
		mod_u16 		arpeggio[3];
	};

	mod_u64				increment;
	mod_u64 			onSample;
} MODChannelPlayer_t;

typedef struct {
	mod_u8 				volume;
	mod_s8 				onDivision;
	mod_u8 				onPosition;
	mod_u8				ticksPerDivision;
	mod_u8				loopCount;
	mod_u8				beatsPerMinute;
	mod_u32 			samplesPerTick;
	mod_u32 			samplesPerDivision;
	mod_u32 			lastPosJump;
	mod_u32				loopDivision;
	mod_u32				loopPosition;
	mod_u32 			onTick;
	mod_u32 			onSample;
	mod_u32 			onArpeggio;
	MODFile_t 			*file;
	MODChannelPlayer_t 	channels[4];
} MODPlayer_t;

int MODFile_Load(MODFile_t *file, const char *path);
void MODPlayer_Play(MODPlayer_t *player, MODFile_t *file);
void MODPlayer_MixAudio(MODPlayer_t *player, mod_s8 *stream, mod_u32 len);


#endif