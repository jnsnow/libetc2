LDFLAGS += -lm
CFLAGS += -std=gnu99 -Wall -O2

etc2tool: etc2tool.c libetc2.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o etc2tool etc2tool.c libetc2.o

libetc2.o: libetc2.c libetc2.h
	$(CC) $(CFLAGS) -c -o libetc2.o libetc2.c

.PHONY: clean
clean:
	rm -f etc2tool libetc2.o
