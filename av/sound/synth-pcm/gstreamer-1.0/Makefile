OBJS=sines gst-beep
all: $(OBJS)

CFLAGS=-Wall -Iinclude
CFLAGS+=-g 
GST_CFLAGS := $(shell pkg-config --cflags gstreamer-1.0 glib-2.0 gio-2.0)
GST_LIBS := $(shell pkg-config --libs gstreamer-1.0 glib-2.0 gio-2.0)

gst-beep: gst-beep.c
	$(CC) $(CFLAGS) $(GST_CFLAGS) -o $@ $< $(GST_LIBS)

gst.o: gst.c
	$(CC) $(CFLAGS) $(GST_CFLAGS) -c $< 

sines.o: sines.c
	$(CC) $(CFLAGS) -pthread -c $< 

sines: sines.o gst.o
	$(CC) $(CFLAGS) $(GST_CFLAGS) -o $@ $< $(GST_LIBS)

.PHONY: clean

clean:
	rm -f $(OBJS) *.o
