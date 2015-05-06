all: libtconf.a redis-udp 

CFLAGS=-I/usr/local/include 
LDFLAGS=-lhiredis -L. -ltconf

redis-udp: redis-udp.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

libtconf.a: tconf.c
	$(CC) $(LIBCFLAGS) -c $<
	ar cru libtconf.a tconf.o

clean:
	rm -f *.o redis-udp libtconf.a 

