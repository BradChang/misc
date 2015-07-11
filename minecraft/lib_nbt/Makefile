OBJS=libnbt.a
all: $(OBJS)
CFLAGS=-g
#CFLAGS+=-Wall
LDFLAGS=-lz

libnbt.a: ungz.o nbt.o utvector.o
	ar r $@ $^

nbt.o: nbt.c
	$(CC) $(CFLAGS) -c $^ 

utvector.o: utvector.c
	$(CC) $(CFLAGS) -c $^ 

ungz.o: ungz.c
	$(CC) $(CFLAGS) -c $^ 

#read_schematic: read_schematic.o tpl.o ungz.o schem.o utvector.o
#	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
#
#read_schematic.o: read_schematic.c
#	$(CC) $(CFLAGS) -c $^ 
#
#tpl.o: tpl.c
#	$(CC) $(CFLAGS) -c $^ 
#

.PHONY: clean

clean: 
	rm -f $(OBJS) *.o
