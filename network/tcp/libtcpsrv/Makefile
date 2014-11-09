OBJS=libtcpsrv.a tcpsrv-test1
all: $(OBJS)
CFLAGS=-pthread
CFLAGS+=-g
#CFLAGS+=-O2
LDFLAGS=-pthread
TESTS_LDFLAGS=-L. -ltcpsrv

libtcpsrv.a: libtcpsrv.o
	ar r $@ $^

libtcpsrv.o: libtcpsrv.c libtcpsrv.h
	$(CC) $(CFLAGS) -c $< 

# test apps

tcpsrv-test1: tcpsrv-test1.o libtcpsrv.a
	$(CC) $(CFLAGS) -o $@ $< $(TESTS_LDFLAGS) $(LDFLAGS) 

tcpsrv-test1.o: tcpsrv-test1.c
	$(CC) $(CFLAGS) -c $< 

.PHONY: clean

clean:
	rm -f *.o $(OBJS)
