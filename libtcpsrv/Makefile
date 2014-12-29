OBJS=libtcpsrv.a 
SUBDIRS=controlport
all: $(SUBDIRS) $(OBJS)
CFLAGS=-pthread -I./controlport
CFLAGS+=-g
#CFLAGS+=-O2

libtcpsrv.a: libtcpsrv.o cmds.o
	ar r $@ $^

libtcpsrv.o: libtcpsrv.c libtcpsrv.h internal.h
	$(CC) $(CFLAGS) -c $< 

cmds.o: cmds.c libtcpsrv.h internal.h
	$(CC) $(CFLAGS) -c $< 

.PHONY: clean $(SUBDIRS) tests

tests:
	make -C tests

$(SUBDIRS):
	for d in $(SUBDIRS); do make -C $$d; done

clean:
	rm -f *.o $(OBJS)
	for d in $(SUBDIRS); do make -C $$d clean; done
	make -C tests clean
