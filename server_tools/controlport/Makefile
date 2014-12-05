TESTS=$(patsubst %.c,%,$(wildcard test*.c))
OBJS=libcontrolport.a 
OBJS+=$(TESTS)
#OBJS+=xcon
all: $(OBJS)
CFLAGS=-I./include
CFLAGS+=-g
#CFLAGS+=-O2
TESTS_LDFLAGS=-L. -lcontrolport

xcon.o: xcon.c
	$(CC) $(CFLAGS) -o $@ $< 

xcon: xcon.o
	$(CC) $(CFLAGS) -o $@ $< -lreadline

libcontrolport.a: libcontrolport.o tpl.o
	ar r $@ $^

tpl.o: tpl.c 
	$(CC) $(CFLAGS) -c $< 

libcontrolport.o: libcontrolport.c libcontrolport.h
	$(CC) $(CFLAGS) -c $< 

# test apps

test%: test%.o libcontrolport.a
	$(CC) $(CFLAGS) -o $@ $< $(TESTS_LDFLAGS) $(LDFLAGS) 

test%.o: test%.c
	$(CC) $(CFLAGS) -c $< 

.PHONY: clean

clean:
	rm -f *.o $(OBJS)
