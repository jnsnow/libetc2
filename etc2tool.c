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

    rc = fdecomp(fh, width, height, &buffer);
    if (rc != 0) {
      fprintf(stderr, "Decomposition failed\n");
      goto out2;
    }

    /* FIXME: Probably want to return the number of pixels directly from decomp. */
    outsize = width * height * 3;
    ret = fwrite(buffer, 1, outsize, fout);
    if (ret != outsize) {
        fprintf(stderr, "Failed to write %s\n", outfile);
	rc = EXIT_FAILURE;
    } else {
        fprintf(stderr, "Wrote %zd bytes to %s\n", outsize, outfile);
    }

    free(buffer);
 out2:
    fclose(fout);
 out1:
    fclose(fh);
    return rc;
}
