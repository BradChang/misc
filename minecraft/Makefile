OBJS=read_schematic
all: $(OBJS)
CFLAGS=-g
LDFLAGS=-lz

read_schematic: read_schematic.o tpl.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

read_schematic.o: read_schematic.c
	$(CC) $(CFLAGS) -c $^ 

tpl.o: tpl.c
	$(CC) $(CFLAGS) -c $^ 

.PHONY: clean

clean: 
	rm -f $(OBJS) *.o
