OBJS=sdl1
CFLAGS=-g
LDFLAGS=-lSDL2

all: $(OBJS)

sdl1.o: sdl1.c
	$(CC) $(CFLAGS) -c $^ 

sdl1: sdl1.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)


.PHONY: clean

clean:
	rm -f $(OBJS)
