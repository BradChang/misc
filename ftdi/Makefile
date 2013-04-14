EXES=rgb

CFLAGS=-g
CFLAGS+=-I/usr/include/libftdi1
LDFLAGS=-lftdi1

$(EXES): %: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(EXES) *.o
