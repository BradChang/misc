OBJS=libnbt.a
all: $(OBJS)
CFLAGS=-g
#CFLAGS+=-Wall
LDFLAGS=-lz

libnbt.a: ungz.o nbt.o record.o utvector.o 
	ar r $@ $^

nbt.o: nbt.c nbt.h
	$(CC) $(CFLAGS) -c $< 

utvector.o: utvector.c utvector.h
	$(CC) $(CFLAGS) -c $<

record.o: record.c record.h nbt.h
	$(CC) $(CFLAGS) -c $<

ungz.o: ungz.c
	$(CC) $(CFLAGS) -c $^ 

.PHONY: clean

clean: 
	rm -f $(OBJS) *.o
