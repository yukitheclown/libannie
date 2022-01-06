#ifndef TYPES_DEF
#define TYPES_DEF

#include <stdint.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define ROTATE_WORD(w) ((((w) >> 8) & 0xFF) | ((w) << 8))

#define ROTATE_INT(i) ((((i) & 0xff000000) >> 24) | (((i) & 0x00ff0000) >>  8) | \
					  (((i) & 0x0000ff00) <<  8) | (((i) & 0x000000ff) << 24))

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;

#endif