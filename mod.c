#include "mod.h"
#include "types.h"
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL_audio.h>


#define MIN(x,y) ((x)<(y)?(x):(y))
#define MAX(x,y) ((x)>(y)?(x):(y))
#define SAMPLING_SHIFT 2
#define	FRACBITS 28
#define	FRACMASK ((1<<28)-1)

enum {
	WAVEFORM_SINE = 0,
	WAVEFORM_RAMP_DOWN,
	WAVEFORM_SQUARE,
	NUM_WAVEFORMS,
};

enum {
	ARPEGGIO = 1,
	SLIDE_UP,
	SLIDE_DOWN,
	SLIDE_TO_NOTE,
	VIBRATO,
	SLIDE_TO_NOTE_VOL_SLIDE,
	VIBRATO_VOL_SLIDE,
	TREMOLO,
	SET_PANNING,
	SET_SAMPLE_OFFSET,
	VOL_SLIDE,
	POS_JUMP,
	SET_VOL,
	PATTERN_BRK,
	EFFECTS_14,
	SET_SPEED,
	EFFECTS_14_START = SET_SPEED,
	EFFECT14_SET_FILTER = EFFECTS_14_START, // unset
	EFFECT14_FINESLIDE_UP,					// untested
	EFFECT14_FINESLIDE_DOWN,  				// untested
	EFFECT14_SET_GLISSANDO, 				// untested
	EFFECT14_SET_VIBRATO_WAVEFORM, 			// untested
	EFFECT14_SET_FINETUNE_VALUE, 			// untested
	EFFECT14_SET_LOOP_PATTERN, 				// untested
	EFFECT14_SET_TREMOLO_WAVEFORM, 			// untested
	EFFECT14_UNUSED,						// unset
	EFFECT14_RETRIGGER_SAMPLE, 				// untested
	EFFECT14_FINE_VOLUME_UP, 				// untested
	EFFECT14_FINE_VOLUME_DOWN, 				// untested
	EFFECT14_FINE_CUT_SAMPLE, 				// unset
	EFFECT14_FINE_DELAY_SAMPLE, 			// unset
	EFFECT14_FINE_DELAY_PATTERN, 			// unset
	EFFECT14_FINE_INVERT_LOOP, 				// unset
};

static mod_u64 GetPeriodIncrement(mod_s8 finetune, mod_u16 period);
static mod_u32 GetPeriodUpSemitones(mod_u16 period, mod_u8 semitones);
static void Player_Division(MODPlayer_t *player);
static mod_u32 GetPeriodDownSemitones(mod_u16 period, mod_u8 semitones);

static const mod_u16 semitonePowers[] = {
	10595, 11225, 11893, 12600,
	13350, 14145, 14986, 15878,
	16823, 17824, 18884, 20008,
	21198, 22460, 23796, 25212,
};

static const mod_u8 sine[] = {
	0x00, 0x0C, 0x18, 0x25, 0x31, 0x3D, 0x49, 0x55, 0x61, 0x6C, 0x78, 0x83, 0x8D, 0x97, 0xA1, 0xAB, 
	0xB4, 0xBC, 0xC5, 0xCC, 0xD3, 0xDA, 0xE0, 0xE6, 0xEB, 0xF0, 0xF3, 0xF7, 0xFA, 0xFC, 0xFD, 0xFE, 
	0xFE, 0xFE, 0xFD, 0xFC, 0xFA, 0xF7, 0xF4, 0xF0, 0xEB, 0xE6, 0xE1, 0xDA, 0xD4, 0xCC, 0xC5, 0xBD, 
	0xB4, 0xAB, 0xA2, 0x98, 0x8D, 0x83, 0x78, 0x6D, 0x61, 0x56, 0x4A, 0x3E, 0x32, 0x25, 0x19, 0x0C, 
};

static const mod_u8 rampDown[] = {
	0xFF, 0xFB, 0xF7, 0xF3, 0xEF, 0xEB, 0xE7, 0xE3, 0xDF, 0xDB, 0xD7, 0xD3, 0xCF, 0xCB, 0xC7, 0xC3, 
	0xBF, 0xBB, 0xB7, 0xB3, 0xAF, 0xAB, 0xA7, 0xA3, 0x9F, 0x9B, 0x97, 0x93, 0x8F, 0x8B, 0x87, 0x83, 
	0x7F, 0x7B, 0x77, 0x73, 0x6F, 0x6B, 0x67, 0x63, 0x5F, 0x5B, 0x57, 0x53, 0x4F, 0x4B, 0x47, 0x43, 
	0x3F, 0x3B, 0x37, 0x33, 0x2F, 0x2B, 0x27, 0x23, 0x1F, 0x1B, 0x17, 0x13, 0x0F, 0x0B, 0x07, 0x03, 
};

static const mod_u8 square[] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const mod_u8 *Waveforms[] = { sine, rampDown, square };

// static const mod_u8 Sine[] = 
// {
//     0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x10,
//     0x10, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x30,
//     0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x40, 0x40,
//     0x40, 0x40, 0x40, 0x40, 0x40, 0x50, 0x50, 0x50,
//     0x50, 0x50, 0x50, 0x50, 0x50, 0x60, 0x60, 0x60, 
//     0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
//     0x60, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
//     0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
//     0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
//     0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
//     0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
//     0x60, 0x60, 0x60, 0x60, 0x50, 0x50, 0x50, 0x50,
//     0x50, 0x50, 0x50, 0x50, 0x40, 0x40, 0x40, 0x40,
//     0x40, 0x40, 0x40, 0x30, 0x30, 0x30, 0x30, 0x30,
//     0x30, 0x30, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
//     0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00,
//     0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
//     0xF0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xD0,
//     0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xC0, 0xC0,
//     0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xB0, 0xB0, 0xB0,
//     0xB0, 0xB0, 0xB0, 0xB0, 0xB0, 0xA0, 0xA0, 0xA0,
//     0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0,
//     0xA0, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
//     0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
//     0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
//     0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
//     0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0,
//     0xA0, 0xA0, 0xA0, 0xA0, 0xB0, 0xB0, 0xB0, 0xB0,
//     0xB0, 0xB0, 0xB0, 0xB0, 0xC0, 0xC0, 0xC0, 0xC0,
//     0xC0, 0xC0, 0xC0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0,
//     0xD0, 0xD0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0,
//     0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0x00, 0x00
// };

// static const mod_u8 Sqaure[] = 
// {
//     0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
//     0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
//     0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
//     0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
//     0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
//     0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
//     0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
//     0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
//     0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
//     0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
//     0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
//     0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
//     0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
//     0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
//     0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
//     0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
//     0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
//     0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
//     0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
//     0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
//     0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
//     0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
//     0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
//     0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
//     0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
//     0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
//     0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
//     0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
//     0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
//     0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
//     0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
//     0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70
// };

// static const mod_u8 RampDown[] = 
// {
// 	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 
// 	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 
// 	0x90, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 
// 	0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 
// 	0xA0, 0xA0, 0xB0, 0xB0, 0xB0, 0xB0, 0xB0, 0xB0, 
// 	0xB0, 0xB0, 0xB0, 0xB0, 0xB0, 0xB0, 0xB0, 0xB0, 
// 	0xB0, 0xB0, 0xB0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 
// 	0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 
// 	0xC0, 0xC0, 0xC0, 0xC0, 0xD0, 0xD0, 0xD0, 0xD0, 
// 	0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 
// 	0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xE0, 0xE0, 0xE0, 
// 	0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 
// 	0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xF0, 0xF0, 
// 	0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 
// 	0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0x00, 
// 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
// 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
// 	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 
// 	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 
// 	0x10, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
// 	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
// 	0x20, 0x20, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 
// 	0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 
// 	0x30, 0x30, 0x30, 0x40, 0x40, 0x40, 0x40, 0x40, 
// 	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 
// 	0x40, 0x40, 0x40, 0x40, 0x50, 0x50, 0x50, 0x50, 
// 	0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 
// 	0x50, 0x50, 0x50, 0x50, 0x50, 0x60, 0x60, 0x60, 
// 	0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 
// 	0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x70, 0x70, 
// 	0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 
// 	0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70
// };

// static const mod_u8 WaveMultTable[] = 
// {
// 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
// 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
// 	0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x03, 
// 	0x04, 0x04, 0x05, 0x05, 0x06, 0x06, 0x07, 0x07, 
// 	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
// 	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 
// 	0x00, 0x01, 0x03, 0x04, 0x06, 0x07, 0x09, 0x0A, 
// 	0x0C, 0x0D, 0x0F, 0x10, 0x12, 0x13, 0x15, 0x16, 
// 	0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 
// 	0x10, 0x12, 0x14, 0x16, 0x18, 0x1A, 0x1C, 0x1E, 
// 	0x00, 0x02, 0x05, 0x07, 0x0A, 0x0C, 0x0F, 0x11, 
// 	0x14, 0x16, 0x19, 0x1B, 0x1E, 0x20, 0x23, 0x25, 
// 	0x00, 0x03, 0x06, 0x09, 0x0C, 0x0F, 0x12, 0x15, 
// 	0x18, 0x1B, 0x1E, 0x21, 0x24, 0x27, 0x2A, 0x2D, 
// 	0x00, 0x03, 0x07, 0x0A, 0x0E, 0x11, 0x15, 0x18, 
// 	0x1C, 0x1F, 0x23, 0x26, 0x2A, 0x2D, 0x31, 0x34, 
// 	0x00, 0xFC, 0xF8, 0xF4, 0xF0, 0xEC, 0xE8, 0xE4, 
// 	0xE0, 0xDC, 0xD8, 0xD4, 0xD0, 0xCC, 0xC8, 0xC4, 
// 	0x00, 0xFC, 0xF9, 0xF5, 0xF2, 0xEE, 0xEB, 0xE7, 
// 	0xE4, 0xE0, 0xDD, 0xD9, 0xD6, 0xD2, 0xCF, 0xCB, 
// 	0x00, 0xFD, 0xFA, 0xF7, 0xF4, 0xF1, 0xEE, 0xEB, 
// 	0xE8, 0xE5, 0xE2, 0xDF, 0xDC, 0xD9, 0xD6, 0xD3, 
// 	0x00, 0xFD, 0xFB, 0xF8, 0xF6, 0xF3, 0xF1, 0xEE, 
// 	0xEC, 0xE9, 0xE7, 0xE4, 0xE2, 0xDF, 0xDD, 0xDA, 
// 	0x00, 0xFE, 0xFC, 0xFA, 0xF8, 0xF6, 0xF4, 0xF2, 
// 	0xF0, 0xEE, 0xEC, 0xEA, 0xE8, 0xE6, 0xE4, 0xE2, 
// 	0x00, 0xFE, 0xFD, 0xFB, 0xFA, 0xF8, 0xF7, 0xF5, 
// 	0xF4, 0xF2, 0xF1, 0xEF, 0xEE, 0xEC, 0xEB, 0xE9, 
// 	0x00, 0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 
// 	0xF8, 0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 
// 	0x00, 0xFF, 0xFF, 0xFE, 0xFE, 0xFD, 0xFD, 0xFC, 
// 	0xFC, 0xFB, 0xFB, 0xFA, 0xFA, 0xF9, 0xF9, 0xF8
// };

static mod_u64 GetPeriodIncrement(mod_s8 finetune, mod_u16 period){
	
	if(period == 0) return 0;

	if(finetune > 0)
		period = GetPeriodUpSemitones(period, finetune);
	else if(finetune < 0)
		period = GetPeriodDownSemitones(period, finetune);

	return ((mod_u64)(8363*1712/period) << (FRACBITS-SAMPLING_SHIFT))/MOD_SAMPLERATE;
}

static void Channel_GetSamples(MODPlayer_t *player, MODChannelPlayer_t *ch, mod_s8 *samples, mod_u16 len){

	if(!ch->instrument) return;

	MODInstrument_t *inst = &player->file->instruments[ch->instrument-1];

	for(; len > 0; len--){

		mod_u32 i = ch->onSample >> FRACBITS;

		if(i >= ch->repeatEnd){
			ch->onSample = (mod_u64)inst->repeat << FRACBITS;
			ch->repeatEnd = inst->repeat + inst->repeatLen;
			i = inst->repeat;
		}

		*samples++ += ((inst->samples[i] * inst->volume * ch->volume * player->volume) / (64*64*64*2));
		// *samples++ += ((inst->samples[i] * inst->volume * ch->volume * player->volume) / (64*64*4));

		ch->onSample += ch->increment;
	}

}

// untested
static mod_u32 GetPeriodDownSemitones(mod_u16 period, mod_u8 semitones){
	
	if(semitones == 0)
		return period;

	return (mod_u32)(semitonePowers[semitones-1] * period) / 10000;
}

static mod_u32 GetPeriodUpSemitones(mod_u16 period, mod_u8 semitones){
	
	if(semitones == 0)
		return period;

	return (mod_u32)((period*10000) / semitonePowers[semitones-1]);
}

static void Channel_Division(MODPlayer_t *player, MODChannelPlayer_t *ch, mod_u16 index){

	MODFile_t *file = player->file;


	mod_u32 division = file->divisions[index];

	mod_u8 inst = MODFILE_DIVISION_SAMPLE(division);
	mod_u16 period = MODFILE_DIVISION_PERIOD(division);
	mod_u16 effectValue = MODFILE_DIVISION_EFFECT(division);

	// printf("\x1b[%im", 34);
	// printf("%8.8X ", ROTATE_INT(file->divisions[index])); 
	// printf("\x1b[%im", 31);
	// printf("%2.2i ", MODFILE_DIVISION_SAMPLE(file->divisions[index])); 
	// printf("\x1b[%im", 32);
	// printf("%4.4i ", MODFILE_DIVISION_PERIOD(file->divisions[index]));
	// printf("\x1b[%im", 33);
	// printf("%3.3x ", MODFILE_DIVISION_EFFECT(file->divisions[index]));
	// printf("| ");
	// printf("\x1b[%im", 0);

	if(inst > 0){ // set playing instrument
		ch->onSample = 0;
		ch->instrument = inst;
		ch->repeatEnd = file->instruments[inst - 1].len;
		ch->volume = 64;
	}

	ch->effect.type = 0;

	if(effectValue > 0){

		MODPlayedEffect_t effect;

		effect.type = ((effectValue >> 8) & 0xF) + 1;
		effect.x = (effectValue >> 4) & 0xF;
		effect.y = effectValue & 0xF;

		effect.division = index;

		mod_u8 speed;

		switch(effect.type){
			case ARPEGGIO:
				// ch->arpeggio[0] = period > 0 ? period : ch->note.period;
				ch->arpeggio[0] = period > 0 ? period : ch->period;
				ch->arpeggio[1] = GetPeriodUpSemitones(ch->arpeggio[0], effect.x);
				ch->arpeggio[2] = GetPeriodUpSemitones(ch->arpeggio[0], effect.y);
				break;
			case SLIDE_UP:
				ch->noteSlide.speed = (effect.x * 16) + effect.y;
				break;
			case SLIDE_DOWN:
				ch->noteSlide.speed = (effect.x * 16) + effect.y;
				break;
			case VOL_SLIDE:

				if(effect.x > 0)
					ch->volSlide.speed = effect.x;
				else
					ch->volSlide.speed = -effect.y;

				break;
			case SLIDE_TO_NOTE:

				if(effect.x != 0 || effect.y != 0)
					ch->noteSlide.speed = (effect.x * 16) + effect.y;

				if(period)
					ch->noteSlide.to = period;

				period = 0;
			
				break;
			case VIBRATO:


				if(effect.x != 0 && effect.y != 0){
	
					ch->vibrato.start = period > 0 ? period : ch->period;
	
					if(ch->vibrato.waveform <= 3)
						ch->vibrato.curr = 0;
	
					ch->vibrato.amplitude = effect.y;
					ch->vibrato.rate = effect.x;
				}

				if(!ch->vibrato.rate)
					effect.type = 0;

				break;
			case TREMOLO:

				if(effect.x != 0 && effect.y != 0){

					if(ch->tremolo.waveform <= 3)
						ch->tremolo.curr = 0;

					ch->tremolo.start = ch->volume;
					ch->tremolo.amplitude = effect.y;
					ch->tremolo.rate = effect.x;
				}
				break;

			case SET_SAMPLE_OFFSET:
				
				ch->onSample = ((effect.x*4096 + effect.y*256) * 2) << FRACBITS;
				break;

			case SLIDE_TO_NOTE_VOL_SLIDE:

				if(effect.x > 0)
					ch->volSlide.speed = effect.x;
				else
					ch->volSlide.speed = -effect.y;

				break;

			case VIBRATO_VOL_SLIDE:

				if(!ch->vibrato.rate)
					effect.type = 0;

				if(effect.x > 0)
					ch->volSlide.speed = effect.x;
				else
					ch->volSlide.speed = -effect.y;

				break;
			case SET_VOL:
				ch->volume = (effect.x*16)+effect.y;
				break;
			case EFFECTS_14:

				switch(effect.x + EFFECTS_14_START){
					case EFFECT14_FINESLIDE_UP:
						ch->period = MIN(113, ch->period - effect.y); 
						break;
					case EFFECT14_FINESLIDE_DOWN:
						ch->period = MAX(856, ch->period + effect.y); 
						break;
					case EFFECT14_SET_GLISSANDO:
						ch->glissando = effect.y ? 1 : 0;
						break;
					case EFFECT14_SET_VIBRATO_WAVEFORM:
						ch->vibrato.waveform = effect.y;
						break;
					case EFFECT14_RETRIGGER_SAMPLE:
						ch->onSample = 0;
						ch->repeatEnd = file->instruments[inst - 1].len;
						break;
					case EFFECT14_FINE_VOLUME_UP:
						ch->volume = MIN(64, ch->volume + effect.y); 
						break;
					case EFFECT14_FINE_VOLUME_DOWN:
						ch->volume = MAX(0, ch->volume - effect.y); 
						break;
					case EFFECT14_SET_TREMOLO_WAVEFORM:
						ch->tremolo.waveform = effect.y;
						break;
					case EFFECT14_SET_FINETUNE_VALUE:
						ch->finetune = effect.y - 8;
						break;
					case EFFECT14_SET_LOOP_PATTERN:

						if(effect.y != 0){
	
							if(player->loopCount == 0)
								player->loopCount = effect.y+1;

							if(player->loopCount == 1){
								player->loopDivision = 0;
								player->loopPosition = player->onPosition;
							} else if(player->loopCount > 1){
								player->onDivision = player->loopDivision;
								player->onPosition = player->loopPosition;
								--player->onDivision; // incremented in Player_Division after
							}

							--player->loopCount;

						} else {

							player->loopDivision = player->onDivision;
							player->loopPosition = player->onPosition;
						}

						break;
				}
				break;

			case PATTERN_BRK:

				++player->onPosition;
				player->onDivision = (effect.x*10)+effect.y;
				--player->onDivision; // incremented in Player_Division after
				if(player->onPosition >= player->file->nPositions)
					player->onPosition = 0;

				break;
			case POS_JUMP:
				player->onPosition = (effect.x*16)+effect.y;
				break;
			case SET_SPEED:

				speed = (effect.x*16)+effect.y;

				if(speed < 32)
					player->ticksPerDivision = speed;
				else
					player->beatsPerMinute = speed;

				player->samplesPerDivision = ((MOD_SAMPLERATE*60)/((24*player->beatsPerMinute)/player->ticksPerDivision));
				player->samplesPerTick = player->samplesPerDivision / player->ticksPerDivision;
				
				break;
		}
	
		ch->effect = effect;
	}

	if(period > 0){ // set playing period
		ch->note.period = period;
		ch->note.division = index;
		ch->period = period;
	}

	ch->increment = GetPeriodIncrement(ch->finetune, ch->period);
}

static void SlideToNote(MODChannelPlayer_t *ch){
	if(!ch->glissando){
		if(ch->period > ch->noteSlide.to)
			ch->period = MAX(ch->period - ch->noteSlide.speed, ch->noteSlide.to);
		else
			ch->period = MIN(ch->period + ch->noteSlide.speed, ch->noteSlide.to);
	} else {

		if(ch->period > ch->noteSlide.to)
			ch->period = MAX(GetPeriodDownSemitones(ch->period, MAX(ch->noteSlide.speed, 16)), ch->noteSlide.to);
		else
			ch->period = MIN(GetPeriodUpSemitones(ch->period, MAX(ch->noteSlide.speed, 16)), ch->noteSlide.to);
	}

}

static void Channel_Tick(MODPlayer_t *player, MODChannelPlayer_t *ch, mod_u8 tick){

	(void)player;
	(void)tick;

	switch(ch->effect.type){
		case SLIDE_DOWN:
			ch->period = MIN(856, ch->period + ch->noteSlide.speed);
			break;
		case SLIDE_UP:
			ch->period = MAX(113, ch->period - ch->noteSlide.speed);
			break;
		case SLIDE_TO_NOTE:
			SlideToNote(ch);
			break;
		case SLIDE_TO_NOTE_VOL_SLIDE:
			SlideToNote(ch);
			ch->volume = MAX(0, MIN(64, ch->volume + ch->volSlide.speed));
			break;
		case VIBRATO_VOL_SLIDE:
			ch->volume = MAX(0, MIN(64, ch->volume + ch->volSlide.speed));
			break;
		case VOL_SLIDE:
			ch->volume = MAX(0, MIN(64, ch->volume + ch->volSlide.speed));
			break;
		default:
			return;
	}

	ch->increment = GetPeriodIncrement(ch->finetune, ch->period);
}

static void Channel_Init(MODChannelPlayer_t *ch){
	ch->volume = 64;
	ch->finetune = 0;
}

static void Player_Tick(MODPlayer_t *player, mod_u8 tick){

	mod_u32 j;
	for(j = 0; j < 4; j++)
		Channel_Tick(player, &player->channels[j], tick);
}

static void Player_Division(MODPlayer_t *player){

	mod_u32 offset = (player->file->patternTable[player->onPosition] * 64 * 4) + (player->onDivision * 4);

	int j;
	for(j = 0; j < 4; j++)
		Channel_Division(player, &player->channels[j], offset + j);

	++player->onDivision;

	if(player->onDivision >= 64){
		
		player->onDivision = 0;
		++player->onPosition;

		if(player->onPosition >= player->file->nPositions)
			player->onPosition = 0;
	
		if(player->loopCount == 0){
			player->loopDivision = 0;
			player->loopPosition = player->onPosition;
		}
	}

	// printf("\n");
}

static void Player_Arpeggio(MODPlayer_t *player){

	MODChannelPlayer_t *ch;

	mod_u32 j;
	for(j = 0; j < 4; j++){
		
		ch = &player->channels[j];
		
		if(ch->effect.type == ARPEGGIO){
			ch->period = ch->arpeggio[player->onArpeggio];
			ch->increment = GetPeriodIncrement(ch->finetune, ch->period);
		}
	}
}

void MODPlayer_MixAudio(MODPlayer_t *player, mod_s8 *stream, mod_u32 len){

	mod_u8 k;

	MODChannelPlayer_t *ch;

	mod_u32 samplesPerArpeggio = player->samplesPerDivision / 3;

	mod_u16 remaining;

	memset(stream, 0, len);

	do {

		// if(player->onSample % player->samplesPerDivision >= player->onArpeggio * samplesPerArpeggio){
		if(player->onSample % samplesPerArpeggio == 0){
			Player_Arpeggio(player);
			player->onArpeggio = (player->onArpeggio+1) % 3;
		}

		if(player->onSample > player->onTick * player->samplesPerTick){

			++player->onTick;

			if((player->onTick-1) % player->ticksPerDivision == 0)
				Player_Division(player);
			else
				Player_Tick(player, (player->onTick-1) % player->ticksPerDivision);

		}

		remaining = player->samplesPerTick - (player->onSample % player->samplesPerTick);
		remaining = MIN(remaining, samplesPerArpeggio - (player->onSample % samplesPerArpeggio));
		remaining = MIN(remaining, len);
		len -= remaining;

		for(k = 0; k < 4; k++){
			
			ch = &player->channels[k];

			mod_u16 lenTmp = remaining;
			mod_s8 *streamTmp = stream;
			mod_u16 remainingTmp;
			mod_u16 onSampleTmp = player->onSample;
			mod_u32 rate;

			do {
	
				if(ch->effect.type == VIBRATO || ch->effect.type == VIBRATO_VOL_SLIDE){
			
					rate = player->samplesPerTick / ch->vibrato.rate;

					remainingTmp = rate - (player->onSample % rate);
	
				} else if(ch->effect.type == TREMOLO){

					rate = player->samplesPerTick / ch->tremolo.rate;

					remainingTmp = rate - (player->onSample % rate);

				} else {

					Channel_GetSamples(player, ch, stream, remaining);
					break;
				}

				remainingTmp = MIN(remainingTmp, lenTmp);

				Channel_GetSamples(player, ch, streamTmp, remainingTmp);

				lenTmp -= remainingTmp;
				streamTmp += remainingTmp;
				onSampleTmp += remainingTmp;

				if(ch->effect.type == VIBRATO || ch->effect.type == VIBRATO_VOL_SLIDE){
			
					mod_u8 semitones = (Waveforms[ch->vibrato.waveform % 3][ch->vibrato.curr % 64]
						* ch->vibrato.amplitude) / 0xFF;

					mod_u32 period;

					if(semitones < ch->vibrato.amplitude / 2){
						period = GetPeriodDownSemitones(ch->period, semitones/2);
					} else {
						period = GetPeriodUpSemitones(ch->period, semitones/2);
					}

					ch->increment = GetPeriodIncrement(ch->finetune, period);

					++ch->vibrato.curr;
					ch->vibrato.curr %= 64;

				} else if(ch->effect.type == TREMOLO){

					mod_u8 amplitude = (Waveforms[ch->tremolo.waveform % 3][ch->tremolo.curr % 64]
						* ch->tremolo.amplitude) / 0xFF;

					ch->volume = ch->tremolo.start + amplitude - (ch->tremolo.amplitude/2);

					++ch->tremolo.curr;
					ch->tremolo.curr %= 64;
				}

			} while(lenTmp > 0);

		}

		player->onSample += remaining;

		stream += remaining;


	} while(len > 0);
}



int MODFile_Load(MODFile_t *file, const char *path){

	FILE *fp = fopen(path, "rb");

	if(!fp) return 0;

	fread(file->title, 1, sizeof(file->title), fp);

	int k;
	for(k = 0; k < 31; k++){

		MODInstrument_t *inst = &file->instruments[k];

		fread(inst->name, 1, sizeof(inst->name), fp);

		fread(&inst->len, 1, sizeof(inst->len), fp);
		inst->len = ROTATE_WORD(inst->len) * 2;

		fread(&inst->finetune, 1, sizeof(inst->finetune), fp);

		fread(&inst->volume, 1, sizeof(inst->volume), fp);

		fread(&inst->repeat, 1, sizeof(inst->repeat), fp);
		inst->repeat = ROTATE_WORD(inst->repeat) * 2;

		fread(&inst->repeatLen, 1, sizeof(inst->repeatLen), fp);
		inst->repeatLen = ROTATE_WORD(inst->repeatLen) * 2;
	}

	fread(&file->nPositions, 1, sizeof(file->nPositions), fp);

	fseek(fp, 1, SEEK_CUR); // skip unused

	fread(file->patternTable, 1, sizeof(file->patternTable), fp);

	fseek(fp, 4, SEEK_CUR); // skip initials

	file->nPatterns = 0;

	for(k = 0; k < file->nPositions; k++)
		if(file->patternTable[k]+1 > file->nPatterns)
			file->nPatterns = file->patternTable[k]+1;

	fread(file->divisions, sizeof(mod_u32), file->nPatterns * 64 * 4, fp);


	for(k = 0; k < 31; k++){
	
		MODInstrument_t *inst = &file->instruments[k];

		if(inst->len <= 1)
			continue;

		fread(inst->samples, 1, inst->len, fp);
	}


	fclose(fp);

	return 1;
}

void MODPlayer_Play(MODPlayer_t *player, MODFile_t *file){

	mod_u32 k;

	memset(player, 0, sizeof(MODPlayer_t));

	player->volume = 64;
	player->onTick = 0;
	player->file = file;
	player->onDivision = 0;
	player->onArpeggio = 0;
	player->ticksPerDivision = 6;
	player->beatsPerMinute = 125;
	player->samplesPerTick = (MOD_SAMPLERATE*60)/(24*player->beatsPerMinute);
	player->samplesPerDivision = player->samplesPerTick * player->ticksPerDivision;

	for(k = 0; k < 4; k++)
		Channel_Init(&player->channels[k]);

}