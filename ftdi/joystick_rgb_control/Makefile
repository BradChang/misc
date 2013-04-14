EXES=jrgb
all: $(EXES) track

CFLAGS=-g
CFLAGS+=-I/usr/include/libftdi1
LDFLAGS=-lftdi1

$(EXES): %: %.c
	$(CC) $(CFLAGS) -o $@ $< tpl.c $(LDFLAGS)

track: %: %.c
	$(CC) $(CFLAGS) -o $@ $< tpl.c -lSDL

clean:
	rm -f $(EXES) track *.o
