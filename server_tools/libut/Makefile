OBJS=libut.a
SUBDIRS=tests
all: $(OBJS) $(SUBDIRS)
CFLAGS=-I./include
CFLAGS+=-Wall
CFLAGS+=-g

libut.a: libut.o utvector.o 
	ar r $@ $^

libut.o: libut.c include/libut.h
	$(CC) $(CFLAGS) -c $<

utvector.o: utvector.c include/utvector.h
	$(CC) $(CFLAGS) -c $<

.PHONY: clean $(SUBDIRS)

$(SUBDIRS):
	for d in $(SUBDIRS); do $(MAKE) -C $$d; done

clean:
	rm -f $(OBJS) *.o
	for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done
