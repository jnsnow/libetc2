LDFLAGS += -lm
CFLAGS += -std=gnu99 -Wall -O2

etc2tool: etc2tool.c libetc2.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o etc2tool etc2tool.c libetc2.o

libetc2.o: libetc2.c libetc2.h
	$(CC) $(CFLAGS) -c -o libetc2.o libetc2.c

libetc2.a: libetc2.o
	ar rcs libetc2.a libetc2.o

libetc2.so: libetc2.c libetc2.h
	$(CC) $(CFLAGS) -fPIC -shared -Wl,-soname,libetc2.so -o libetc2.so libetc2.c $(LDFLAGS) -lc

.PHONY: clean
clean:
	rm -f etc2tool libetc2.o libetc2.a libetc2.so
