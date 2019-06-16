#include <assert.h>
#include <endian.h>
#include <errno.h>
#include <math.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "libetc2.h"

int main(int argc, char *argv[])
{
    FILE *fh, *fout;
    const char *infile;
    const char *outfile;

    size_t width = 0;
    size_t height = 0;

    uint8_t *buffer;
    size_t outsize;
    size_t ret;
    int rc = EXIT_SUCCESS;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <filename> <output_file> [width [height]]\n", argv[0]);
        return EXIT_FAILURE;
    } else {
        infile = argv[1];
	outfile = argv[2];
    }

    if (argc >= 4) {
      width = atoi(argv[3]);
    }

    if (argc >= 5) {
      height = atoi(argv[4]);
    }

    fh = fopen(infile, "r");
    if (!fh) {
        perror("Couldn't open input file");
        return EXIT_FAILURE;
    }

    fout = fopen(outfile, "w");
    if (!fout) {
      rc = EXIT_FAILURE;
      perror("Couldn't open output file for writing");
      goto out1;
    }

    outsize = fdecomp(fh, &width, &height, &buffer);
    if (outsize < 0) {
      fprintf(stderr, "Decomposition failed\n");
      goto out2;
    }

    fprintf(stderr, "width: %zd; height: %zd\n", width, height);

    if (strstr(outfile, ".png")) {
      png_image *info = calloc(1, sizeof(png_image));
      info->version = PNG_IMAGE_VERSION;
      info->width = width;
      info->height = height;
      info->format = PNG_FORMAT_RGB;
      png_image_write_to_stdio(info, fout, 0, buffer, 0, NULL);

      if (info->warning_or_error) {
	fprintf(stderr, "woe: 0x%08x\n", info->warning_or_error);
	fprintf(stderr, "%s", info->message);
	rc = EXIT_FAILURE;
      }
      png_image_free(info);
      free(info);
    } else {
      ret = fwrite(buffer, 1, outsize, fout);
      if (ret != outsize) {
        fprintf(stderr, "Failed to write %s\n", outfile);
	rc = EXIT_FAILURE;
      } else {
        fprintf(stderr, "Wrote %zd bytes to %s\n", outsize, outfile);
      }
    }

    free(buffer);
 out2:
    fclose(fout);
 out1:
    fclose(fh);
    return rc;
}
