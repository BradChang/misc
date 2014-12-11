OBJS=key-responder
all: $(OBJS)
CFLAGS=-g

key-responder: key-responder.o tconf.o
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: clean

clean:
	rm -f $(OBJS) *.o
