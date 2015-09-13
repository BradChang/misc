all: libtconf.a redis-udp redis-fast

CFLAGS=-I/usr/local/include 
CFLAGS+=-O2
LDFLAGS=-lhiredis -L. -ltconf

redis-udp: redis-udp.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

redis-fast: redis-fast.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

libtconf.a: tconf.c
	$(CC) $(LIBCFLAGS) -c $<
	ar cru libtconf.a tconf.o

clean:
	rm -f *.o redis-udp redis-fast libtconf.a 

