#ifndef __LIBETC2_H
#define __LIBETC2_H

#include <stdio.h>
#include <stdint.h>

typedef struct rgb8 {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb8;

uint8_t _4to8bits(uint8_t v);
void extend_4to8bits(uint8_t *v);
uint8_t _5to8bits(uint8_t v);
void extend_5to8bits(uint8_t *v);
uint8_t _6to8bits(uint8_t v);
void extend_6to8bits(uint8_t *v);
uint8_t _7to8bits(uint8_t v);
void extend_7to8bits(uint8_t *v);
uint8_t add_3to5(uint8_t _3bit, uint8_t _5bit);
void extend_rgb444(rgb8 *v);
void extend_rgb555(rgb8 *v);
void extend_rgb676(rgb8 *v);

extern rgb8 *deblock_etc1(uint64_t block, rgb8 base[3]);
extern rgb8 *deblock_etc2(uint64_t block, rgb8 base[3], char mode, uint8_t dist);
extern rgb8 *deblock_planar(uint64_t block, rgb8 base[3]);
extern rgb8 *deblock(FILE *fh);
extern uint8_t *decomp(FILE *fh, int width, int height, size_t *len);
extern ssize_t fdecomp(FILE *fh, size_t width, size_t height, uint8_t **outbuffer);

#endif
