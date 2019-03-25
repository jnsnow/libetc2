LDFLAGS += -lm
CFLAGS += -std=gnu99 -Wall -O2

etcf: etcf.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o etcf etcf.c

.PHONY: clean
clean:
	rm -f etcf
