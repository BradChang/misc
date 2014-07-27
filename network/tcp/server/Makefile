OBJS=tcp-server
ALL: $(OBJS)

CFLAGS=-Wall
CFLAGS+=-g

tcp-server: tcp-server.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tcp-server.o: tcp-server.c
	$(CC) -c $(CFLAGS) $<

.PHONY: clean

clean:
	rm -f *.o $(OBJS)
