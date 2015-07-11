OBJS=nbtool 
all: $(OBJS) tests

LIB=../nbtlib
CFLAGS=-g
CFLAGS+=-I$(LIB)
LDFLAGS=-L$(LIB) -lnbt -lz

nbtool: nbtool.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

nbtool.o: nbtool.c
	$(CC) $(CFLAGS) -c $<

.PHONY: clean tests

tests:
	make -C tests

clean: 
	rm -f $(OBJS) *.o
	make -C tests clean
