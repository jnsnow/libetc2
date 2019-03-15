cc=gcc
cflags=-std=gnu99 -Wall -O2

etcf: etcf.c
	$(cc) $(cflags) $(ldflags) -o etcf etcf.c

.PHONY: clean
clean:
	rm etcf
