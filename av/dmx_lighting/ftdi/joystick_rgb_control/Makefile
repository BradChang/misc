EXES=jrgb rgb-mmap
all: $(EXES) track track-mmap

CFLAGS=-g
CFLAGS+=-I/usr/include/libftdi1
LDFLAGS=-lftdi1

$(EXES): %: %.c
	$(CC) $(CFLAGS) -o $@ $< tpl.c $(LDFLAGS)

track: %: %.c
	$(CC) $(CFLAGS) -o $@ $< tpl.c -lSDL

track-mmap: %: %.c
	$(CC) $(CFLAGS) -o $@ $< -lSDL

clean:
	rm -f $(EXES) track track-mmap *.o
