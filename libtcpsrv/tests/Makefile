TESTS=$(patsubst %.c,%,$(wildcard test*.c))
OBJS=$(TESTS)
all: $(OBJS)
CFLAGS=-pthread -I.. -I../controlport
CFLAGS+=-g
#CFLAGS+=-O2
LDFLAGS=-L.. -ltcpsrv -L../controlport -lcontrolport

# test apps

test%: test%.o ../libtcpsrv.a
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) 

test%.o: test%.c
	$(CC) $(CFLAGS) -c $< 

.PHONY: clean 

clean:
	rm -f *.o $(OBJS)
