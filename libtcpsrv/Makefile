TESTS=$(patsubst %.c,%,$(wildcard test*.c))
OBJS=libtcpsrv.a $(TESTS)
SUBDIRS=controlport
all: $(SUBDIRS) $(OBJS)
CFLAGS=-pthread
CFLAGS+=-g
#CFLAGS+=-O2
LDFLAGS=-pthread
TESTS_LDFLAGS=-L. -ltcpsrv

libtcpsrv.a: libtcpsrv.o
	ar r $@ $^

libtcpsrv.o: libtcpsrv.c libtcpsrv.h
	$(CC) $(CFLAGS) -c $< 

# test apps

test%: test%.o libtcpsrv.a
	$(CC) $(CFLAGS) -o $@ $< $(TESTS_LDFLAGS) $(LDFLAGS) 

test%.o: test%.c
	$(CC) $(CFLAGS) -c $< 

.PHONY: clean $(SUBDIRS)

$(SUBDIRS):
	for d in $(SUBDIRS); do make -C $$d; done

clean:
	rm -f *.o $(OBJS)
	for d in $(SUBDIRS); do make -C $$d clean; done
