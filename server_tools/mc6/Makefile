TESTS=$(patsubst %.c,%,$(wildcard test*.c))
OBJS=libmc6.a 
OBJS+=$(TESTS)
OBJS+=mc6x
all: $(OBJS)
CFLAGS=-I./include -I./tpl
CFLAGS+=-g
#CFLAGS+=-O2
TESTS_LDFLAGS=-L. -lmc6

mc6x.o: mc6x.c
	$(CC) $(CFLAGS) -c $< 

mc6x: mc6x.o tpl/tpl.o
	$(CC) $(CFLAGS) -o $@ $^ -lreadline 

libmc6.a: libmc6.o tpl/tpl.o
	ar r $@ $^

libmc6.o: libmc6.c libmc6.h
	$(CC) $(CFLAGS) -c $< 

# test apps

test%: test%.o libmc6.a
	$(CC) $(CFLAGS) -o $@ $< $(TESTS_LDFLAGS) $(LDFLAGS) 

test%.o: test%.c
	$(CC) $(CFLAGS) -c $< 

.PHONY: clean 

clean:
	rm -f *.o $(OBJS) tpl/*.o
