OBJS = gtk-demo
ALL: $(OBJS)
CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
LDFLAGS = $(shell pkg-config --libs gtk+-3.0)

gtk-demo: gtk-demo.o
	$(CC) -o $@ $< $(LDFLAGS)

gtk-demo.o: gtk-demo.c
	$(CC) -c $(CFLAGS) $<

.PHONY: clean

clean:
	rm -f *.o $(OBJS)
