#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <endian.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <math.h>

#include "libetc2.h"

/* License: I have no earthly idea. Whatever Khronos allows for works derived
 * from their specifications. Good luck! */

#define DEBUG (0)

/**=== DATA TABLES ===**/

/** NB: The modifier table is laid out in the spec as -N -n +p +P,
 * from smallest to largest. However, the pixel index is defined to be
 * +p +P -n -N, so lay the table out that way instead so we can use the
 * pixel index value as a direct offset into the codeword table. */
int16_t codeword[8][4] = {{  2,   8,  -2,   -8},
                          {  5,  17,  -5,  -17},
                          {  9,  29,  -9,  -29},
                          { 13,  42, -13,  -42},
                          { 18,  60, -18,  -60},
                          { 24,  80, -24,  -80},
                          { 33, 106, -33, -106},
                          { 47, 183, -47, -183}};

uint8_t distance[8] = {3, 6, 11, 16, 23, 32, 41, 64};

/**=== MACROS AND PRIMITIVE BIT OPERATIONS ===**/

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((b) > (a) ? (b) : (a))
#define BITN(val64, n) (((val64) >> (n)) & 0x01)
#define PIXEL(val64, n) (BITN((val64), (n)) | (BITN((val64), ((n) + 16)) << 1))
#define DELTA(uval8, idelta16) ((int16_t)(uval8) + idelta16)
#define CLAMP(ival16) MIN(255, MAX(0, (ival16)))

#define RGBDX(rgb, delta) (rgb8){ CLAMP(DELTA(rgb.r, delta)),	\
                                  CLAMP(DELTA(rgb.g, delta)),	\
                                  CLAMP(DELTA(rgb.b, delta)) }


/* Extend 0000abcd to abcdABCD */
uint8_t _4to8bits(uint8_t v)
{
    return (v << 4) | (v & 0x0F);
}

void extend_4to8bits(uint8_t *v)
{
    *v = _4to8bits(*v);
}

/* Extend 000abcde to abcdeABC */
uint8_t _5to8bits(uint8_t v)
{
    return (v << 3) | (v >> 2);
}

void extend_5to8bits(uint8_t *v)
{
    *v = _5to8bits(*v);
}

/* Extend 00abcdef to abcdefAB */
uint8_t _6to8bits(uint8_t v)
{
  return (v << 2) | (v >> 4);
}

void extend_6to8bits(uint8_t *v)
{
  *v = _6to8bits(*v);
}

/* Extend 0abcdefg to abcdefgA */
uint8_t _7to8bits(uint8_t v)
{
  return (v << 1) | (v >> 6);
}

void extend_7to8bits(uint8_t *v)
{
  *v = _7to8bits(*v);
}

/** Add signed 3bit to unsigned 5bit.
 * interpret result as unsigned 8bit. */
uint8_t add_3to5(uint8_t _3bit, uint8_t _5bit)
{
  /* sign extend 3bit to 8bit */
  if (_3bit & 0x04) {
    _3bit = _3bit | 0xF8;
  }
  return _3bit + _5bit;
}

void extend_rgb444(rgb8 *v)
{
    extend_4to8bits(&v->r);
    extend_4to8bits(&v->g);
    extend_4to8bits(&v->b);
}

void extend_rgb555(rgb8 *v)
{
    extend_5to8bits(&v->r);
    extend_5to8bits(&v->g);
    extend_5to8bits(&v->b);
}

void extend_rgb676(rgb8 *v)
{
    extend_6to8bits(&v->r);
    extend_7to8bits(&v->g);
    extend_6to8bits(&v->b);
}

/* Fill pixel data for ETC1 modes */
extern rgb8 *deblock_etc1(uint64_t block, rgb8 base[3])
{
    rgb8 *pixels = (rgb8 *)malloc(sizeof(rgb8) * 16);
    uint8_t subblock;
    uint8_t cw[2];
    bool flip;

    /* determine 3 bit table code words for sub-block 1 and 2 */
    cw[0] = (block & 0x000000E000000000ULL) >> 37;
    cw[1] = (block & 0x0000001C00000000ULL) >> 34;
    /* vertical or horizontal split */
    flip = ((1ULL << 32) & block);

    if (DEBUG) {
      fprintf(stdout, "raw 0x%016lx flip [%d] table1[%d] table2[%d]\n", block, flip, cw[0], cw[1]);
      fprintf(stdout, "base1 0x%02x%02x%02x\n", base[0].r, base[0].g, base[0].b);
      fprintf(stdout, "base2 0x%02x%02x%02x\n", base[1].r, base[1].g, base[1].b);
    }

    /* [flipbit = 0]
     * s  1 | s  2
     * -----------
     * a  e | i  m
     * b  f | j  n
     * c  g | k  o
     * d  h | l  p
     */

    /* [flipbit = 1]
     * s | a  e  i  m
     * 1 | b  f  j  n
     * --------------
     * s | c  g  k  o
     * 2 | d  h  l  p
     */

    /* flipbit=0; 2x4, col1 = abcdefgh; col2 = ijklmnop */
    /* flipbit=1; 4x2, col1 = aeimbfjn; col2 = cgkodhlp */
    for (int i = 0; i < 16; i++) {
        int index = PIXEL(block, i);
        int16_t delta;
        int16_t r, g, b;

        subblock = flip ? ((i / 2) %2) : (i / 8);
        assert((subblock & ~0x01) == 0);
        assert((cw[subblock] & ~0x07) == 0);
        assert((index & ~0x03) == 0);
        delta = codeword[cw[subblock]][index];
        r = DELTA(base[subblock].r, delta);
        g = DELTA(base[subblock].g, delta);
        b = DELTA(base[subblock].b, delta);
        pixels[i] = (rgb8){ .r = CLAMP(r),
                            .g = CLAMP(g),
                            .b = CLAMP(b) };
        if (DEBUG) {
          fprintf(stdout, "RESULT 0x%02x%02x%02x; UNCLAMPED(%d,%d,%d) pixelidx[%d]: %d; subblock: %d; cw[%d]: %d; delta: %d\n",
                  pixels[i].r,
                  pixels[i].g,
                  pixels[i].b,
                  r,g,b,
                  i, index,
                  subblock,
                  subblock, cw[subblock],
                  delta);
        }
    }

    return pixels;
}

extern rgb8 *deblock_etc2(uint64_t block, rgb8 base[3], char mode, uint8_t dist)
{
    assert(dist < 8);
    assert(mode == 'T' || mode == 'H');
    rgb8 *pixels = (rgb8 *)malloc(sizeof(rgb8) * 16);
    int16_t d = distance[dist];
    rgb8 paint[4];

    if (mode == 'T') {
      paint[0] = base[0];
      paint[1] = RGBDX(base[1], d);
      paint[2] = base[1];
      paint[3] = RGBDX(base[1], -d);
    } else if (mode == 'H') {
      paint[0] = RGBDX(base[0], d);
      paint[1] = RGBDX(base[0], -d);
      paint[2] = RGBDX(base[1], d);
      paint[3] = RGBDX(base[1], -d);
    }

    if (DEBUG) {
      for (int i = 0; i < 4; i++) {
	fprintf(stdout, "paint[%d] (dist:%d) 0x%02x%02x%02x\n",
		i, d, paint[i].r, paint[i].g, paint[i].b);
      }
    }
    for (int i = 0; i < 16; i++) {
        int index = PIXEL(block, i);
        assert((index & ~0x03) == 0);
        pixels[i] = paint[index];
	if (DEBUG) {
	  fprintf(stdout, "pixels[%d] 0x%02x%02x%02x\n",
		  i, pixels[i].r, paint[i].g, paint[i].b);
	}
    }

    return pixels;
}

extern rgb8 *deblock_planar(uint64_t block, rgb8 base[3])
{
    rgb8 *pixels = (rgb8 *)malloc(sizeof(rgb8) * 16);
    /* O, H, V map to base[0], base[1], base[2] */

    if (DEBUG) {
      fprintf(stdout, "O 0x%02x%02x%02x\n", base[0].r, base[0].g, base[0].b);
      fprintf(stdout, "H 0x%02x%02x%02x\n", base[1].r, base[1].g, base[1].b);
      fprintf(stdout, "V 0x%02x%02x%02x\n", base[2].r, base[2].g, base[2].b);
      fprintf(stdout, "H-O (%d,%d,%d)\n",
	      base[1].r - base[0].r,
	      base[1].g - base[0].g,
	      base[1].b - base[0].b);
      fprintf(stdout, "V-O (%d,%d,%d)\n",
	      base[2].r - base[0].r,
	      base[2].g - base[0].g,
	      base[2].b - base[0].b);
    }

    for (int x = 1; x <= 4; x++) {
      for (int y = 1; y <= 4; y++) {
	int r1 = (x * (base[1].r - base[0].r) + y * (base[2].r - base[0].r) + 4 * base[0].r + 2) >> 2;
	int g1 = (x * (base[1].g - base[0].g) + y * (base[2].g - base[0].g) + 4 * base[0].g + 2) >> 2;
	int b1 = (x * (base[1].b - base[0].b) + y * (base[2].b - base[0].b) + 4 * base[0].b + 2) >> 2;
	pixels[(x - 1) * 4 + (y - 1)] = (rgb8){ CLAMP(r1), CLAMP(g1), CLAMP(b1) };
      }
    }

    return pixels;
}

extern rgb8 *deblock(FILE *fh)
{
    uint64_t block;
    size_t ret;
    bool diff;
    rgb8 base[3];

    ret = fread(&block, sizeof(block), 1, fh);
    if (ret != 1) {
        return NULL;
    }
    block = htobe64(block);

    diff = ((1ULL << 33) & block);
    if (!diff) {
        /* Individual mode */
        base[0].r = (block & (0xFULL << 60)) >> 60;
        base[1].r = (block & (0xFULL << 56)) >> 56;
        base[0].g = (block & (0xFULL << 52)) >> 52;
        base[1].g = (block & (0xFULL << 48)) >> 48;
        base[0].b = (block & (0xFULL << 44)) >> 44;
        base[1].b = (block & (0xFULL << 40)) >> 40;
        /* Inflate Columns 1 and 2 */
        if (DEBUG) {
          fprintf(stdout, "block: 0x%016lx\n", block);
          fprintf(stdout, "rgbA 0x%02x 0x%02x 0x%02x\n", base[0].r, base[0].g, base[0].b);
          fprintf(stdout, "rgbB 0x%02x 0x%02x 0x%02x\n", base[1].r, base[1].g, base[1].b);
        }
        extend_rgb444(&base[0]);
        extend_rgb444(&base[1]);
        if (DEBUG) {
          fprintf(stdout, "rgb0 0x%02x 0x%02x 0x%02x\n", base[0].r, base[0].g, base[0].b);
          fprintf(stdout, "rgb1 0x%02x 0x%02x 0x%02x\n", base[1].r, base[1].g, base[1].b);
        }
        return deblock_etc1(block, base);
    }

    base[0].r = (block & 0xF800000000000000ULL) >> 59;
    base[1].r = (block & 0x0700000000000000ULL) >> 56;
    base[1].r = add_3to5(base[1].r, base[0].r);
    if (base[1].r & 0xE0) {
        /* T Mode */
        uint8_t dist, tmp;

        tmp = (block & (0xFFULL << 56)) >> 56;
        base[0].r = ((tmp >> 1) & 0x0C) | (tmp & 0x03);
        base[0].g = (block & (0xFULL << 52)) >> 52;
        base[0].b = (block & (0xFULL << 48)) >> 48;
        base[1].r = (block & (0xFULL << 44)) >> 44;
        base[1].g = (block & (0xFULL << 40)) >> 40;
        base[1].b = (block & (0xFULL << 36)) >> 36;
        /* inflate 4to8 */
        extend_rgb444(&base[0]);
        extend_rgb444(&base[1]);
        /* select [aa1b] */
        dist = (block & (0xFULL << 32)) >> 32;
        assert((dist & ~0xF) == 0);
        assert(dist < 16);
        dist = ((dist & 0x0C) >> 1) | (dist & 0x01);
        assert(dist < 8);
        return deblock_etc2(block, base, 'T', dist);
    }

    base[0].g = (block & 0x00F8000000000000ULL) >> 51;
    base[1].g = (block & 0x0007000000000000ULL) >> 48;
    base[1].g = add_3to5(base[1].g, base[0].g);
    if (base[1].g & 0xE0) {
        /* H Mode */
        base[0].r = (block & (0xFULL << 59)) >> 59;
        base[0].g = (block & (0x7ULL << 56)) >> 55;
        base[0].g = base[0].g | ((block & (1ULL << 52)) >> 52);
        base[0].b = (block & (1ULL << 51)) >> 48;
        base[0].b = base[0].b | ((block & (0x7ULL << 47)) >> 47);
        base[1].r = (block & (0xFULL << 43)) >> 43;
        base[1].g = (block & (0xFULL << 39)) >> 39;
        base[1].b = (block & (0xFULL << 35)) >> 35;
        /* inflate 4to8 */
        extend_rgb444(&base[0]);
        extend_rgb444(&base[1]);
        /* dist */
        uint8_t dist = (block & (0xFULL << 32)) >> 32;
        dist = (dist & 0x04) | ((dist & 0x01) << 1);
        uint32_t val1 = (uint32_t)base[0].r << 16 | (uint32_t)base[0].g << 8 | (uint32_t)base[0].b;
        uint32_t val2 = (uint32_t)base[1].r << 16 | (uint32_t)base[1].g << 8 | (uint32_t)base[1].b;
        if (val1 >= val2) {
          dist = dist | 0x01;
        }
        assert(dist < 8);
        return deblock_etc2(block, base, 'H', dist);
    }

    base[0].b = (block & 0x0000F80000000000ULL) >> 43;
    base[1].b = (block & 0x0000070000000000ULL) >> 40;
    base[1].b = add_3to5(base[1].b, base[0].b);
    if (base[1].b & 0xE0) {
      /* Planar Mode */
      uint16_t tmp;

      /* RO, GO, BO */
      base[0].r = (block & (0x3FULL << 57)) >> 57;
      tmp = (block & (0xFFULL << 49)) >> 49;
      base[0].g = ((tmp & 0x80) >> 1) | (tmp & 0x3F);
      tmp = (block & (0x3FFULL << 39)) >> 39;
      base[0].b = ((tmp & 0x200) >> 4) | ((tmp & 0x30) >> 1) | (tmp & 0x07);

      /* RH, GH, BH */
      tmp = (block & (0x7FULL << 32)) >> 32;
      base[1].r = ((tmp & 0x78) >> 1) | (tmp & 0x01);
      base[1].g = (block & (0x7F << 25)) >> 25;
      base[1].b = (block & (0x3F << 19)) >> 19;

      /* RV, GV, BV */
      base[2].r = (block & (0x3F << 13)) >> 13;
      base[2].g = (block & (0x7F << 6)) >> 6;
      base[2].b = (block & 0x3F);

      /* Inflate colors */
      extend_rgb676(&base[0]);
      extend_rgb676(&base[1]);
      extend_rgb676(&base[2]);

      return deblock_planar(block, base);
    }

    /* Differential Mode */
    extend_rgb555(&base[1]);
    extend_rgb555(&base[0]);
    return deblock_etc1(block, base);
}

extern uint8_t *decomp(FILE *fh, int width, int height, size_t *len)
{
    int xblocks = (width + 3) / 4;
    int yblocks = (height + 3) / 4;
    uint8_t *buffer;
    rgb8 **groups;
    size_t i = 0;
    size_t buflen;

    buflen = width * height * 3;
    buffer = (uint8_t *)malloc(buflen);
    if (buffer == NULL) {
        return NULL;
    }
    groups = (rgb8 **)calloc(xblocks, sizeof(rgb8 *));
    if (groups == NULL) {
        free(buffer);
        return NULL;
    }

    /* each block has a pixel layout like this: */
    /*  _______
     * |0|4|8|c|
     * |1|5|9|d|
     * |2|6|a|e|
     * |3|7|b|f|
     * ---------
     */

    /* So for every four rows of the image, we need to get a full row of blocks,
     * and then serialize them in an interleaved order:
     * X0:[0,4,8,c] X1:[0,4,8,c] X2:[0,4,8,c] ..... XN:[0,4,8,c]
     * X0:[1,5,9,d] X1:[1,5,9,d] ...
     * X0:[2,6,a,e] X1:[2,6,a,e] ...
     * X0:[3,7,b,f] X1:[3,7,b,f] ...
     */

    for (int y = 0; y < yblocks; y++) {
        for (int x = 0; x < xblocks; x++) {
            groups[x] = deblock(fh);
            if (groups[x] == NULL) {
                goto fail;
            }
        }

        for (int j = 0; j < 4; j++) {
            for (int x = 0; x < xblocks; x++) {
                for (int k = 0; k < 4; k++) {
                    if (x * 4 + k >= width) {
                        continue;
                    }
                    buffer[i++] = groups[x][k * 4 + j].r;
                    buffer[i++] = groups[x][k * 4 + j].g;
                    buffer[i++] = groups[x][k * 4 + j].b;
                }
            }
        }

        for (int x = 0; x < xblocks; x++) {
            free(groups[x]);
            groups[x] = NULL;
        }
    }

    free(groups);
    *len = buflen;
    return buffer;

 fail:
    if (groups) {
        for (int x = 0; x < xblocks; x++) {
            free(groups[x]);
        }
        free(groups);
    }
    free(buffer);
    return NULL;
}

extern ssize_t fdecomp(FILE *fh, size_t width, size_t height, uint8_t **outbuffer)
{
    int fd, rc;
    struct stat buf = { 0 };
    size_t nblocks;
    size_t minblocks, minbytes;
    uint8_t *buffer;
    size_t buflen;

    if (!fh) {
        return -EINVAL;
    }

    fd = fileno(fh);
    if (fd == -1) {
      rc = errno;
      perror("Couldn't get file descriptor for file");
      return rc;
    }

    if (fstat(fd, &buf) == -1) {
      rc = errno;
      perror("Couldn't fstat() file");
      return rc;
    }

    nblocks = buf.st_size / sizeof(uint64_t);
    if (DEBUG) {
      fprintf(stderr, "filesize is %zd\n", buf.st_size);
      fprintf(stderr, "nblocks is %zd\n", nblocks);
    }

    if (width == 0) {
      size_t wblocks = sqrt(nblocks);
      width = wblocks * 4;
      if (DEBUG) {
        fprintf(stdout, "guessing width: %zd blocks, %zd pixels\n", wblocks, width);
      }
    }

    if (height == 0) {
      size_t wblocks = ((width + 3) / 4);
      size_t hblocks = nblocks / wblocks;
      height = hblocks * 4;
      if (DEBUG) {
	fprintf(stdout, "guessing height: %zd blocks, %zd pixels\n", hblocks, height);
      }
    }

    minblocks = ((width + 3) / 4) * ((height + 3) / 4);
    minbytes = minblocks * sizeof(uint64_t);

    if (DEBUG) {
      fprintf(stderr, "estimated blocksize is %zd\n", minblocks);
      fprintf(stderr, "minimum bitesize is %zd\n", minbytes);
    }

    if (minbytes > buf.st_size) {
      fprintf(stderr, "The resolution specified (%zd x %zd) "
	      "requires a minimum of %zd bytes, "
	      "but the file passed has only %zd bytes\n",
	      width, height, minbytes, buf.st_size);
      return -EINVAL;
    }

    buffer = decomp(fh, width, height, &buflen);
    if (!buffer) {
      fprintf(stderr, "Decomposition failed\n");
      return -1;
    }

    *outbuffer = buffer;
    return buflen;
}
